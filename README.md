# Distributed Bridge Access Control (MPI + Lamport Clocks)

MPI-based distributed mutual exclusion system implementing a coordinator-based algorithm for bi-directional bridge access control with configurable capacity.

## Problem

Multiple processes need to cross a bridge that only allows traffic in one direction at a time, with configurable capacity (Y=2 concurrent processes). The system guarantees:
- **Mutual exclusion** — no opposing-direction conflicts on the bridge
- **FIFO ordering** — via Lamport logical clocks with total ordering
- **Deadlock freedom** — coordinator handles all direction switching
- **Fairness** — same-direction batching prevents starvation

## Architecture

```
┌─────────────┐     REQUEST/RELEASE     ┌─────────────────┐
│  Process 0  │ ◄─────────────────────► │                 │
│  (DIR_LEFT) │                         │   Coordinator   │
├─────────────┤     GRANT/DONE          │  (rank=size-1)  │
│  Process 1  │ ◄─────────────────────► │                 │
│ (DIR_RIGHT) │                         │  Priority Queue │
├─────────────┤                         │  Lamport Clock  │
│  Process N  │ ◄─────────────────────► │  Gate Direction │
└─────────────┘                         └─────────────────┘
```

### Message Protocol

| Tag | Direction | Payload | Purpose |
|-----|-----------|---------|---------|
| `REQUEST` | Process → Coordinator | timestamp, rank, direction | Request bridge access |
| `GRANT` | Coordinator → Process | clock, gate_dir, top_dir | Grant bridge access |
| `RELEASE` | Process → Coordinator | timestamp | Exit bridge |
| `DONE` | Coordinator → All | — | All processes finished |

### Algorithm

1. Processes send `REQUEST` with Lamport timestamp and desired direction
2. Coordinator maintains a priority queue sorted by `(timestamp, rank)`
3. Coordinator grants access to top-N processes going the same direction (`GATE_CAPACITY=2`)
4. When all same-direction processes finish, coordinator switches direction to serve waiting processes
5. Lamport clocks ensure consistent total ordering across all processes

## Technical Highlights

- **C++23** — `std::ranges`, `std::format`, `constexpr`, `std::string_view`
- **Lamport Logical Clocks** — total ordering with tie-breaking by rank
- **Microsecond-precision Logger** — structured `[timestamp][process][state] message` format
- **Configurable Gate Capacity** — `GATE_CAPACITY` macro controls concurrent bridge access
- **Comprehensive Makefile** — debug/release builds, Valgrind, GDB, multi-process testing

## Build & Run

```bash
cd mpi/

# Build (debug mode by default)
make

# Build optimized
make release

# Run with 6 processes (default)
make run

# Run with custom process count
make run-np N=8

# Build and run in one step
make buildrun

# Memory check with Valgrind
make memcheck

# Test with different process counts (2, 4, 6, 8)
make test

# Show all available targets
make help
```

### Prerequisites

```bash
# Install MPI (Ubuntu/Debian)
make install-mpi

# Or manually
sudo apt-get install mpich libmpich-dev
```

## Tech Stack

`C++23` · `MPI (MPICH)` · `Lamport Clocks` · `Distributed Mutual Exclusion` · `Make`
