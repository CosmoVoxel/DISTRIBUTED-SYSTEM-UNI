#include <mpi.h>
#include <stdio.h>

#include <memory>
#include <iostream>
#include <vector>
#include <queue>

#define TAG_REQ 1 // REQUEST: CAN I ENTER?
#define TAG_GRANT 2  // RESPONSE: YES, YOU CAN ENTER
#define TAG_ACK 3 // ACKNOWLEDGEMENT: I HAVE RECEIVED YOUR RESPONSE
#define TAG_REL 4 // RELEASE: I AM DONE GOING THROUGH
#define TAG_EXIT 5 // EXIT: I AM LEAVING (THREAD EXIT)

#define TAG_CAPACITY 6 // CAPACITY


enum Direction {
    LEFT = 0,
    RIGHT = 1
};

struct Request {
    int id; // ID/RANK/NAME of the human/researcher
    Direction direction;
}; // Human/Researcher will request to enter gates

struct Gate {
    uint current_capacity = 0; // TODO: Current number of humans/researchers make it through
    uint total_capacity = 1; // Total number of humans/researchers before the gate changes direction
    Direction direction = LEFT;
}; // Gate will have a capacity and direction


std::queue<Request> left_side_queue;
std::queue<Request> right_side_queue;

int main(int argc, char** argv) {
    MPI_Init(NULL, NULL);

    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    printf("Hello from process %d of %d\n", world_rank, world_size);

    MPI_Finalize();
return 0;
}
