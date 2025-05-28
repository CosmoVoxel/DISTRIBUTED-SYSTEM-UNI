//
// Created by marce on 27/05/2025.
//
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <unistd.h>

#define REQUEST 0
#define ACK 1
#define RELEASE 2

int rank, size;
int lamport_clock = 0;
int ack_count = 0;

// Request structure
typedef struct {
    int timestamp;
    int rank;
} Request;

Request my_request;
Request request_queue[100];  // queue buffer
int queue_size = 0;

void update_clock(int other) {
    if (other > lamport_clock)
        lamport_clock = other;
    lamport_clock++;
}

int compare_requests(Request a, Request b) {
    // If not the same time sort by it
    if (a.timestamp != b.timestamp)
        return a.timestamp - b.timestamp;
    // Else sort by id
    return a.rank - b.rank;
}

void sort_queue() {
    // Bubble sort simple
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
    // Add + sort queue
    request_queue[queue_size++] = req;
    sort_queue();
}

void remove_request(int from_rank) {
    // Delete request with rank from_rank
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

int can_enter() {
    // If you are first and all ack
    return (queue_size > 0 && request_queue[0].rank == rank && ack_count == size - 1);
}

void send_request_to_all() {
    lamport_clock++;
    my_request.timestamp = lamport_clock;
    my_request.rank = rank;

    for (int i = 0; i < size; i++) {
        if (i == rank) continue;
        int data[2] = {my_request.timestamp, rank};
        MPI_Send(&data, 2, MPI_INT, i, REQUEST, MPI_COMM_WORLD);
    }
}

void send_ack(int dest) {
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

#include <chrono>
#include <ctime>

void enter_critical_section() {
    using namespace std::chrono;
    auto now = system_clock::now();
    std::time_t now_c = system_clock::to_time_t(now);
    std::tm *parts = std::localtime(&now_c);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    printf("[LAMPORT:%d] [%02d:%02d.%03ld] [Proces %d] ENTER\n",
           lamport_clock, parts->tm_min, parts->tm_sec, ms.count(), rank);
    sleep(3);  // symulacja przejścia przez bramę

    now = system_clock::now();
    now_c = system_clock::to_time_t(now);
    parts = std::localtime(&now_c);
    ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    printf("[LAMPORT:%d] [%02d:%02d.%03ld] [Proces %d] LEAVES\n",
           lamport_clock, parts->tm_min, parts->tm_sec, ms.count(), rank);
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    srand(rank + time(NULL));
    sleep(rand() % 3);  // każdy proces losowo startuje

    send_request_to_all();
    add_request(my_request);

    MPI_Status status;

    while (1) {
        int buffer[2];
        MPI_Recv(buffer, 2, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        update_clock(buffer[0]);

        if (status.MPI_TAG == REQUEST) {
            Request req = {buffer[0], buffer[1]};
            add_request(req);
            send_ack(buffer[1]);

        } else if (status.MPI_TAG == ACK) {
            ack_count++;
        } else if (status.MPI_TAG == RELEASE) {
            remove_request(status.MPI_SOURCE);
        }

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