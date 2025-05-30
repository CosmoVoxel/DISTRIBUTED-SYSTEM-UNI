#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mpi.h>
#include <unistd.h>
#include <ctime>

#define REQUEST 0
#define ACK 1
#define RELEASE 2

int rank, size;
int lamport_clock = 0;
int ack_count = 0;

#define GATE_CAPACITY 1  // Default Y=2
typedef enum {
    NONE = 0,
    DIR_LEFT = 1,
    DIR_RIGHT = 2
} Direction;

// Logger
class Logger {
private:
    std::chrono::steady_clock::time_point start_time;
    
public:
    Logger() {
        start_time = std::chrono::steady_clock::now();
    }
    
    std::string getCurrentTimestamp() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
        
        int total_ms = elapsed.count();
        int minutes = (total_ms / 60000) % 60;
        int seconds = (total_ms / 1000) % 60;
        int milliseconds = total_ms % 1000;
        
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << minutes << ":"
            << std::setfill('0') << std::setw(2) << seconds << ":"
            << std::setfill('0') << std::setw(3) << milliseconds;
        return oss.str();
    }
    
    void log(const std::string& state, const std::string& message) {
        std::cout << "[" << getCurrentTimestamp() << "][Process " << rank << "][" << state << "] " << message << std::endl;
    }
    
    std::string directionToString(Direction dir) {
        switch(dir) {
            case DIR_LEFT: return "Left";
            case DIR_RIGHT: return "Right";
            case NONE: return "None";
            default: return "Unknown";
        }
    }
};

Logger logger;

typedef struct {
    int timestamp;
    int rank;
    Direction direction;

} Request;

Request my_request;
Request request_queue[100];
int queue_size = 0;

int current_direction = NONE; // -1 means no direction yet

// inside_flags[i] == 1 means process i is inside the gate :)
int inside_flags[100] = {0};

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

int count_inside() {
    int c = 0;
    for (int i = 0; i < size; i++) {
        if (inside_flags[i])
            c++;
    }
    return c;
}

int count_top_n_same_direction() {
    // CURRENTLY IN INITIAL STATE
    if (current_direction == NONE)
        return 0;

    int count = 0;
    for (int i = 0; i < queue_size && count < GATE_CAPACITY; i++) {
        if (request_queue[i].direction == current_direction)
            count++;
        else
            break;
    }
    return count;
}

int is_among_top_n(int rank_to_check) {
    // CURRENTLY IN INITIAL STATE
    if (current_direction == NONE)
        return 0;

    int count = 0;
    for (int i = 0; i < queue_size; i++) {
        if (request_queue[i].direction == current_direction) {
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
        current_direction = NONE;
        return;
    }

    // CURRENT DIRECTION
    Direction top_dir = request_queue[0].direction;

    // NOT SET YET
    if (current_direction == NONE) {
        current_direction = top_dir;
        logger.log("DIRECTION", "Gate direction set to " + logger.directionToString(static_cast<Direction>(current_direction)));
        return;
    }

    // 
    if (top_dir != current_direction && count_inside() == 0) {
        current_direction = top_dir;
        logger.log("DIRECTION", "Gate direction changed to " + logger.directionToString(static_cast<Direction>(current_direction)));
    }
}

int can_enter() {
    if (current_direction == NONE)
        return 0;

    if (my_request.direction != current_direction)
        return 0;

    if (!is_among_top_n(rank))
        return 0;

    if (count_inside() >= GATE_CAPACITY)
        return 0;

    if (ack_count < size - 1)
        return 0;

    return 1;
}

void send_request_to_all() {
    lamport_clock++;
    my_request.timestamp = lamport_clock;
    my_request.rank = rank;

    for (int i = 0; i < size; i++) {
        if (i == rank) continue;
        int data[3] = {my_request.timestamp, rank, my_request.direction};
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
        if (i == rank) continue;
        int data[1] = {lamport_clock};
        MPI_Send(&data, 1, MPI_INT, i, RELEASE, MPI_COMM_WORLD);
    }
}

void enter_critical_section() {
    inside_flags[rank] = 1;
    
    lamport_clock++;
    for (int i = 0; i < size; i++) {
        if (i == rank) continue;
        int data[2] = {lamport_clock, rank};
        MPI_Send(&data, 2, MPI_INT, i, 100, MPI_COMM_WORLD);
    }
    
    int inside_count = count_inside();
    logger.log("ENTERING", "Gate direction " + logger.directionToString(static_cast<Direction>(current_direction)) + " at Lamport " + std::to_string(lamport_clock) + ". Inside count: " + std::to_string(inside_count));

    sleep(2);  

    logger.log("LEAVING", "Gate at Lamport " + std::to_string(lamport_clock));
    inside_flags[rank] = 0;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Half go left (DIR_LEFT=1), half go right (DIR_RIGHT=2)
    my_request.direction = (rank % 2 == 0) ? DIR_LEFT : DIR_RIGHT;
    
    srand(rank + time(NULL));
    sleep(rand() % 3);

    for (int i = 0; i < size; i++) inside_flags[i] = 0;

    send_request_to_all();
    add_request(my_request);

    MPI_Status status;

    while (1) {
        int buffer[3];
        MPI_Recv(buffer, 3, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        update_clock(buffer[0]);

        // RECV
        if (status.MPI_TAG == REQUEST) {
            Request req = {buffer[0], buffer[1], static_cast<Direction>(buffer[2])};
            add_request(req);
            
            try_change_direction();
            send_ack(buffer[1]);

        } else if (status.MPI_TAG == ACK) {
            ack_count++;
            try_change_direction();
        } else if (status.MPI_TAG == RELEASE) {
            int releasing_rank = status.MPI_SOURCE;
            inside_flags[releasing_rank] = 0;
            remove_request(releasing_rank);
            
            try_change_direction();
        } else if (status.MPI_TAG == 100) {
            // ENTERING notification - update inside flag // FOR LOGGING ONLY
            int entering_rank = buffer[1];
            inside_flags[entering_rank] = 1;
        }

        // TRY
        if (can_enter()) {
            enter_critical_section();

            // After leaving CS
            remove_request(rank);
            send_release_to_all();
            
            // Check IF ANY ONE NEED TO MAKE THROU 
            // SO WE WILL SEND ALL 
            try_change_direction();

            break;
        }
    }

    MPI_Finalize();
    return 0;
}
