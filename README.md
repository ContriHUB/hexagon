# High-Performance In-Memory System with Event-Driven I/O

## Overview
This project demonstrates an optimized data retrieval system for **low-latency access** and **high throughput** in high-concurrency environments.  

It combines:
- In-memory architecture for fast data retrieval  
- Event-driven I/O with `epoll()` for scalable, non-blocking operations  
- I/O multiplexing to handle thousands of concurrent connections efficiently  
- Pipelined request-response protocols to reduce I/O bottlenecks in single-threaded applications  

---

## Features
- Low-latency in-memory data retrieval  
- High concurrency support using non-blocking event-driven I/O  
- Request pipelining for efficient request-response handling  
- Single-threaded design with high throughput  

---

## Architecture
1. **In-Memory Data Store** – Minimizes disk I/O by keeping data in memory.  
2. **Event-Driven I/O** – Uses `epoll()` to manage concurrent client connections.  
3. **Request Pipelining** – Processes multiple requests without waiting for sequential responses.  

---

## Getting Started

### Prerequisites
- Linux system with `epoll` support  
- `g++` compiler (C++11 or later)  

### Build

Compile the server:
```bash
g++ -std=c++11 -O2 server.cpp -o server
```

Compile the client:
```bash
g++ -std=c++11 -O2 client.cpp -o client
```

### Run

Start the server:
```bash
./server
```

In another terminal, run the client:
```bash
./client
```

The client will send requests, and the server will respond using the in-memory, event-driven architecture.

---

## Future Work
- Create a custom **hashmap** for optimal in-memory retrieval and support for additional data structures  
- Implement **distributed in-memory caching** for scalability across multiple nodes  
- Add **load balancing and sharding** to support large-scale deployments  
