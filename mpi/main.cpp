#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <format>
#include <iostream>
#include <mpi.h>
#include <unistd.h>

#define REQUEST 0
#define ACK 1
#define RELEASE 2

int rank, size;
int lamport_clock = 0;
int ack_count = 0;

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

void sort_queue() {
  for (int i = 0; i < queue_size - 1; i++) {
    for (int j = i + 1; j < queue_size; j++) {
      if (compare_requests(request_queue[i], request_queue[j]) > 0) {
        Request temp = request_queue[i];
        request_queue[i] = request_queue[j];
        request_queue[j] = temp;
      }
    }
  }
}

void add_request(Request req) {
  request_queue[queue_size++] = req;
  sort_queue();
}

void remove_request(int from_rank) {
  int i;
  for (i = 0; i < queue_size; i++) {
    if (request_queue[i].rank == from_rank)
      break;
  }
  for (; i < queue_size - 1; i++) {
    request_queue[i] = request_queue[i + 1];
  }
  queue_size--;
}

int count_top_n_same_direction() {
  // CURRENTLY IN INITIAL STATE
  if (gate_direction == NONE)
    return 0;

  int count = 0;
  for (int i = 0; i < queue_size && count < GATE_CAPACITY; i++) {
    if (request_queue[i].direction == gate_direction)
      count++;
    else
      break;
  }
  return count;
}

int is_among_top_n(int rank_to_check) {
  // CURRENTLY IN INITIAL STATE
  if (gate_direction == NONE)
    return 0;

  int count = 0;
  for (int i = 0; i < queue_size; i++) {
    if (request_queue[i].direction == gate_direction) {
      if (request_queue[i].rank == rank_to_check)
        return count < GATE_CAPACITY;
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

  if (ack_count < size - 1)
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

void send_ack(int dest) {
  lamport_clock++;
  int data[1] = {lamport_clock};
  MPI_Send(&data, 1, MPI_INT, dest, ACK, MPI_COMM_WORLD);
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

void enter_critical_section() {
  auto log = std::format(
      "GATE DIRECTION: {}, TOP DIRECTION: {}, LAMPORT CLOCK: {}",
      logger.directionToString(gate_direction),
      logger.directionToString(request_queue[0].direction), lamport_clock);
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

  send_request_to_all();
  add_request(start_state);

  MPI_Status status;

  while (1) {
    int buffer[3];
    MPI_Recv(buffer, 3, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD,
             &status);

    update_clock(buffer[0]);

    // RECV
    if (status.MPI_TAG == REQUEST) {
      Request req = {buffer[0], buffer[1], static_cast<Direction>(buffer[2])};
      add_request(req);
      send_ack(buffer[1]);
      try_change_direction();

    } else if (status.MPI_TAG == ACK) {
      ack_count++;
    } else if (status.MPI_TAG == RELEASE) {
      int releasing_rank = status.MPI_SOURCE;
      remove_request(releasing_rank);
      try_change_direction();
    }

    // TRY
    if (can_enter()) {
      enter_critical_section();

      remove_request(rank);
      send_release_to_all();

      break;
    }
  }

  MPI_Finalize();
  return 0;
}
