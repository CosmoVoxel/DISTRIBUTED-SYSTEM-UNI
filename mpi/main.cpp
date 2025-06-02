#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <format>
#include <iostream>
#include <mpi.h>
#include <unistd.h>

#define REQUEST 0
#define RELEASE 2
#define GRANT 3
#define DONE 4

int rank, size;
int lamport_clock = 0;

#define GATE_CAPACITY 2 // Default Y=2
typedef enum { NONE = 0, DIR_LEFT = 1, DIR_RIGHT = 2 } Direction;

class Logger {
  std::chrono::steady_clock::time_point start_time{
      std::chrono::steady_clock::now()};

public:
  std::string getCurrentTimestamp() const {
    using namespace std::chrono;
    auto ms =
        duration_cast<milliseconds>(steady_clock::now() - start_time).count();
    auto min = ms / 60000 % 60;
    auto sec = ms / 1000 % 60;
    auto milli = ms % 1000;
    return std::format("{:02}:{:02}:{:03}", min, sec, milli);
  }
  void log(std::string_view state, std::string_view message) const {
    std::cout << std::format("[{}][Process {}][{}] {}\n", getCurrentTimestamp(),
                             rank, state, message);
  }
  static constexpr std::string_view directionToString(Direction dir) {
    switch (dir) {
    case DIR_LEFT:
      return "Left";
    case DIR_RIGHT:
      return "Right";
    case NONE:
      return "None";
    default:
      return "Unknown";
    }
  }
};

Logger logger;

typedef struct {
  int timestamp;
  int rank;
  Direction direction;

} Request;

Request start_state;
Request request_queue[100];
int queue_size = 0;

Direction gate_direction = NONE;

void update_clock(int other) {
  if (other > lamport_clock)
    lamport_clock = other;
  lamport_clock++;
}

int compare_requests(Request a, Request b) {
  if (a.timestamp != b.timestamp)
    return a.timestamp - b.timestamp;
  return a.rank - b.rank;
}

#include <algorithm>
void sort_queue() {
  std::ranges::sort(request_queue, request_queue + queue_size, [](const Request& a, const Request& b) {
    return compare_requests(a, b) < 0;
  });
}

void add_request(Request req) {
  request_queue[queue_size++] = req;
  sort_queue();
}

void remove_request(int from_rank) {
  auto it = std::ranges::find_if(request_queue, request_queue + queue_size, [from_rank](const Request& req) {
    return req.rank == from_rank;
  });
  if (it != request_queue + queue_size) {
    std::ranges::move(it + 1, request_queue + queue_size, it);
    --queue_size;
  }
}

int count_top_n_same_direction() {
  if (gate_direction == NONE)
    return 0;

  return std::ranges::count_if(
    request_queue,
    request_queue + std::min(queue_size, GATE_CAPACITY),
    [](const Request& req) {
      return req.direction == gate_direction;
    }
  );
}

int is_among_top_n(int rank_to_check) {
  if (gate_direction == NONE)
    return 0;

  int count = 0;
  for (int i = 0; i < std::min(queue_size, GATE_CAPACITY); ++i) {
    if (request_queue[i].direction == gate_direction) {
      if (request_queue[i].rank == rank_to_check)
        return 1;
      count++;
    }
  }
  return 0;
}

void try_change_direction() {
  // INIT STATE
  if (queue_size == 0) {
    if (gate_direction != NONE) {
      gate_direction = NONE;
    }
    return;
  }

  // CURRENT DIRECTION
  Direction top_dir = request_queue[0].direction;

  // NOT SET YET
  if (gate_direction == NONE) {
    gate_direction = top_dir;
    logger.log("DIRECTION",
               std::format("Gate direction set to {}",
                           logger.directionToString(gate_direction)));
    return;
  }

  if (top_dir != gate_direction) {
    gate_direction = top_dir;
    logger.log("DIRECTION",
               std::format("Gate direction changed to {}",
                           logger.directionToString(gate_direction)));
  }
}

int can_enter() {
  if (gate_direction == NONE)
    return 0;

  if (queue_size == 0 || gate_direction != request_queue[0].direction)
    return 0;

  if (start_state.direction != gate_direction)
    return 0;

  if (!is_among_top_n(rank))
    return 0;

  return 1;
}

void send_request_to_all() {
  lamport_clock++;
  start_state.timestamp = lamport_clock;
  start_state.rank = rank;

  for (int i = 0; i < size; i++) {
    if (i == rank)
      continue;
    int data[3] = {start_state.timestamp, rank, start_state.direction};
    MPI_Send(&data, 3, MPI_INT, i, REQUEST, MPI_COMM_WORLD);
  }
}

void send_release_to_all() {
  lamport_clock++;
  for (int i = 0; i < size; i++) {
    if (i == rank)
      continue;
    int data[1] = {lamport_clock};
    MPI_Send(&data, 1, MPI_INT, i, RELEASE, MPI_COMM_WORLD);
  }
}

void send_done_to_all() {
  for (int i = 0; i < size; i++) {
    int dummy[1] = {0};
    MPI_Send(&dummy, 1, MPI_INT, i, DONE, MPI_COMM_WORLD);
  }
}

void log_enter_leave(Direction gate_dir, Direction top_dir) {
  auto log =
      std::format("GATE DIRECTION: {}, TOP DIRECTION: {}, LAMPORT CLOCK: {}",
                  logger.directionToString(gate_dir),
                  logger.directionToString(top_dir), lamport_clock);
  logger.log("ENTERING", log);
  sleep(2);
  logger.log("LEAVING", std::format("Gate at Lamport {}", lamport_clock));
}

int main(int argc, char **argv) {
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  // Half go left (DIR_LEFT=1), half go right (DIR_RIGHT=2)
  start_state.direction = (rank % 2 == 0) ? DIR_LEFT : DIR_RIGHT;

  srand(rank + time(NULL));
  sleep(rand() % 3);

  MPI_Status status;

  if (rank == size - 1) { // CORDINATOR

    int in_cs_count = 0;
    int done_count = 0;
    int total_p_count = size;

    while (true) {
      int buffer[3];
      MPI_Recv(buffer, 3, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD,
               &status);
      update_clock(buffer[0]);
      
      if (status.MPI_TAG == REQUEST) {
        add_request(
            Request{buffer[0], buffer[1], static_cast<Direction>(buffer[2])});
        try_change_direction();
      } 
      else if (status.MPI_TAG == RELEASE) { // P LEAVES CS
        remove_request(status.MPI_SOURCE);
        in_cs_count--;
        done_count++;
        if (in_cs_count == 0) // NO ONE IN CS CAN CHANGE DIR
          try_change_direction();
        if (done_count == total_p_count) { // EXIT PROGRAM 
          send_done_to_all();
          logger.log("Finaly", "All processes have finished. Exiting.");
          break;
        }

      }

      if (in_cs_count != 0 || queue_size == 0) {
        if (queue_size == 0)
          gate_direction = Direction::NONE;
        continue;
      }
      
      gate_direction = request_queue[0].direction;

      int granted = 0;
      for (size_t i = 0; i < queue_size && granted < GATE_CAPACITY; ++i) {
        if (request_queue[i].direction != gate_direction)
          continue;
        if (request_queue[i].rank == rank) {
          log_enter_leave(gate_direction, request_queue[0].direction);
          remove_request(rank);
          in_cs_count++;
          granted++;
          in_cs_count--;
          done_count++;
          if (in_cs_count == 0)
            try_change_direction();
          if (done_count == total_p_count) {
            send_done_to_all();
            logger.log("Finaly", "All processes have finished. Exiting.");
            break;
          }
          break;
        } else {
          int data[3] = {lamport_clock, (int)gate_direction,
                         (int)request_queue[0].direction};
          MPI_Send(&data, 3, MPI_INT, request_queue[i].rank, GRANT,
                   MPI_COMM_WORLD);
          granted++;
          in_cs_count++;
        }
      }
      int found = 0;
      for (int i = 0; i < queue_size; i++) {
        if (request_queue[i].rank == rank) {
          found = 1;
          break;
        }
      }
      if (done_count < total_p_count && !found) {
        lamport_clock++;
        add_request(Request{lamport_clock, rank,
                            (rank % 2 == 0) ? DIR_LEFT : DIR_RIGHT});
      }
    }
  } else { // SLAVE ;]
    lamport_clock++;
    start_state.timestamp = lamport_clock;
    start_state.rank = rank;

    // SEND CORDINATOR REQUEST 
    int data[3] = {start_state.timestamp, rank, (int)start_state.direction};
    MPI_Send(&data, 3, MPI_INT, size - 1, REQUEST, MPI_COMM_WORLD);

    while (true) {
      int buffer[3];
      MPI_Recv(buffer, 3, MPI_INT, size - 1, MPI_ANY_TAG, MPI_COMM_WORLD,
               &status);
      update_clock(buffer[0]);
      if (status.MPI_TAG == GRANT) {
        Direction granted_gate_direction = (Direction)buffer[1];
        Direction granted_top_direction = (Direction)buffer[2];

        log_enter_leave(granted_gate_direction, granted_top_direction);

        lamport_clock++;
        int rel[1] = {lamport_clock};
        MPI_Send(&rel, 1, MPI_INT, size - 1, RELEASE, MPI_COMM_WORLD);
        break;
      }
    }
    int dummy[1];
    MPI_Recv(dummy, 1, MPI_INT, size - 1, DONE, MPI_COMM_WORLD, &status);
    logger.log("Finaly", "All processes have finished. Exiting.");
  }
  MPI_Finalize();
  return 0;
}
