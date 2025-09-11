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

## Workflow
1. Listening socket is created.  
2. In every loop iteration, a **new epoll fd** is created.  
3. The listening socket and all active connections are added to this epoll instance.  
4. `epoll_wait` waits for events from either the listening socket or the active connections.  
5. If the event is on the listening socket → accept new connections and add them to the connections vector.  
6. If the event is on a connection → read request data.  
7. Requests follow a custom protocol:  
   - Number of strings  
   - Length of first string, first string  
   - Length of second string, second string  
   - … and so on.  
8. The server parses the request according to this protocol.  
9. The request is processed (`get`, `set`, `del`) and a response is generated with a response code and optional payload.  
10. Response is written back to the client.  

### Visual Workflow (Mermaid Diagram)

```mermaid
flowchart TD
    A[Create socket] --> B[Event Loop (iteration)]
    B --> C[Create new epoll fd<br/>Clear epoll events list]
    C --> D[Add listening socket +<br/>all current connections to epoll]
    D --> E[Wait for events (epoll_wait)]

    E -->|Event on listening socket| F[Accept new connection]
    E -->|Event on connection (incoming data)| G[Read request buffer<br/>(custom protocol)]

    F --> H[Track connection in vector]
    G --> I[Parse request → Process request]
    I --> J[Generate Response]
    J --> K[Send back to client]

    K --> B

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
