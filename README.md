# C++ Level 2 Order Book — Single-Writer Matching Engine with Multi-Producer I/O

This project implements a Level 2 order book with a single-writer matching engine fed by multiple producer threads (self-test workers and per-client connection threads). It demonstrates an actor-style design suitable for systems/interview discussions and performance-oriented C++ on Windows.

## Features
- **Level 2 Order Book**: Price-level aggregation on buy (descending) and sell (ascending) sides
- **Single-Writer Matching Engine**: One dedicated thread owns and mutates the book (no global lock contention)
- **Multi-Producer Ingress**: Self-test workers and each TCP client connection enqueue orders concurrently
- **Throughput Benchmark**: Built-in self-test generates 100,000 orders and reports throughput
- **TCP Server**: Accepts multiple client connections (thread-per-connection) on `127.0.0.1:54000`


## Performance
- The self-test generates 100,000 random orders across multiple producer threads and enqueues them.
- The single-writer engine thread drains the queue and updates the book.
- The server prints overall throughput (orders/sec) for your machine.

```
Processed 100000 orders in 0.0330099 seconds
Throughput: 3.02939e+06 orders/sec
Top of Book:
Best Bid: 99.3486 x 96
Best Ask: 99.3922 x 12
```

## Why this design
- **Correctness**: Only one thread updates the book → no data races, no complex locking
- **Performance**: I/O and parsing scale across cores; the engine thread keeps the book hot in cache
- **Burst Handling**: A thread-safe queue decouples producers from the consumer, smoothing spikes

## Project Structure
- `server/` — matching engine, TCP listener, self-test
- `client/` — simple TCP client that sends one order

## Build (Windows, g++)

Prerequisites: MinGW-w64 (or other g++ with WinSock2), PowerShell or CMD.

- Build server
```
g++ -O2 -std=c++17 server/main.cpp -lws2_32 -o server.exe
```

- Build client
```
g++ -O2 -std=c++17 client/main.cpp -lws2_32 -o client.exe
```

(Optional) MSVC `cl` builds are also supported with equivalent flags.

## Run
1. Start the server (it runs a self-test, then listens):
```
./server.exe
```
2. In other terminals, run one or more clients:
```
./client.exe
```
You can run multiple clients concurrently; each connection is handled by its own producer thread.

## Protocol
- Each message: `side price quantity`
  - Examples: `buy 100.5 10`, `sell 99.8 25`
- The server replies with `Order received` or `Invalid order format`.
- The server’s per-connection thread reads in a loop, so you can send multiple messages on one connection.


Note: We focus on throughput; average-latency instrumentation was removed to simplify the critical path.

## Technical Details
- **Order book**: `std::map<double,int,greater<double>>` for bids; `std::map<double,int>` for asks
- **Engine**: One thread loops on a `std::condition_variable`, pops from a `std::deque<Order>`, and applies updates
- **Ingress**:
  - Self-test workers: generate and enqueue orders
  - TCP accept loop: thread-per-connection, parse and enqueue orders continuously
- **Threading model**: Many producers → one consumer (actor model)
- **Networking**: WinSock2 on Windows

## Notes
- Educational/demo project; extend with persistence, richer order types, error handling, snapshots, and sharding for production use. 