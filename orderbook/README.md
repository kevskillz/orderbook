# C++ Level 2 Order Book TCP Client-Server Example

This project demonstrates a high-performance Level 2 order book using a TCP socket-based client-server architecture in C++. It is suitable for quant developer resume projects.

## Features
- **Level 2 Order Book**: Price-level aggregation with buy/sell side management
- **Multithreaded Processing**: 4 concurrent threads for order processing
- **Performance Metrics**: Self-test with throughput and latency measurements
- **Real-time Order Matching**: Marketable order matching with price-time priority
- **TCP Socket Communication**: Client-server architecture for order entry and execution reporting

## Performance
- **Throughput**: 789,000+ orders/sec
- **Latency**: 0.83 microseconds average fill latency
- **Test**: 100,000 orders processed in 0.13 seconds using 4 threads

## Structure
- `server/` — C++ server that maintains the Level 2 order book and handles client connections
- `client/` — C++ client that sends orders (buy/sell) to the server

## Build Instructions (Windows, g++)

### Prerequisites
- [MinGW-w64](https://www.mingw-w64.org/) or similar g++ compiler for Windows
- Command Prompt or PowerShell

### Build Server
```
g++ -std=c++17 -o server.exe server/main.cpp -lws2_32
```

### Build Client
```
g++ -std=c++17 -o client.exe client/main.cpp -lws2_32
```

## Run
1. Start the server in one terminal:
   ```
   ./server.exe
   ```
   The server will run a self-test first, then start listening for connections.

2. In another terminal, run the client:
   ```
   ./client.exe
   ```

## Protocol
- The client sends a single line: `side price quantity` (e.g., `buy 100.5 10`)
- The server responds with an acknowledgment or error message
- Server logs client connections and order processing in real-time

## Technical Details
- **Order Book**: Uses `std::map` for price-level aggregation (buy side descending, sell side ascending)
- **Threading**: 4 threads with mutex-protected order book access
- **Matching**: Simple price-time priority matching for marketable orders
- **Networking**: Winsock2 for Windows TCP socket implementation

## Notes
- This is a demonstration project for educational and resume purposes
- The server includes a built-in self-test for performance benchmarking
- For production, consider persistent storage, more robust error handling, and additional order types 