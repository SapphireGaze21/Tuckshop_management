# Project Report: Concurrent Tuckshop Order Management System

## 1. Problem Statement
The objective of this project is to design and implement a highly concurrent, file-and-memory-based tuckshop (canteen) order management system. In a real-world canteen, multiple users place orders simultaneously, and the kitchen must process these orders while managing limited resources (stoves, chefs) and tracking stock in real-time. 

This project simulates that environment by providing a client-server architecture where multiple users can connect concurrently, authenticate based on roles, and place orders. The system handles concurrent access to shared resources (like the in-RAM menu stock and disk-based order history files) and ensures data consistency across all operations while preventing memory leaks and buffer overflows.

## 2. Implementation of OS Concepts

### 2.1 Role-Based Authorization
The system implements three distinct roles: `ADMIN`, `USER`, and `GUEST`. 
- **Implementation:** During authentication (`login()` and `signup()` in `utils.c`), the user's role is determined. The `handle_client()` function in `server.c` strictly enforces access control. For example, `ADMIN` can add items to the menu, `USER` can place orders, and `GUEST` is restricted to only viewing the menu and checking order status.

### 2.2 File Locking
To ensure data consistency when multiple threads access the same persistent files simultaneously, file locking is used.
- **Implementation:** POSIX `fcntl()` locks are heavily utilized in `utils.c`. 
  - Write locks (`F_WRLCK`) are acquired before modifying `users.txt` during signup.
  - File locking is used in `add_order()` and `update_order_status()` for `orders.txt` to prevent lost updates or race conditions when assigning order IDs.

### 2.3 Concurrency Control (Threads, Mutexes, Semaphores)
The system handles multiple processes and threads executing simultaneously.
- **Implementation:**
  - **Threads:** The server uses `pthread_create()` to spawn a new thread for every incoming client connection.
  - **Mutexes:** A `pthread_mutex_t` named `client_mutex` perfectly protects the global `active_clients` counter from race conditions when multiple clients connect/disconnect at the exact same millisecond. Additionally, a `PTHREAD_PROCESS_SHARED` mutex is used to lock the shared memory segment.
  - **Semaphores:** The kitchen worker threads use POSIX counting semaphores (`sem_t`) to limit cooking capacity. For instance, `maggi_sem` is initialized to 2, ensuring that at most two "stoves" can be used for Maggi simultaneously.

### 2.4 Data Consistency & System V Shared Memory
Data consistency is maintained across shared resources. To massively optimize performance, the live menu stock was migrated from slow disk I/O (`menu.txt`) to lightning-fast System V Shared Memory.
- **Implementation:** Upon booting, `server.c` uses `shmget()` and `shmat()` to create a block of shared RAM. It reads `menu.txt` once to load the stock into this block. From then on, all stock reductions and menu views access the RAM instantly. To prevent race conditions, the RAM block contains a `pthread_mutex_t` that is locked before updating the `quantity` of any `MenuItem`. When the server gracefully shuts down via `Ctrl+C`, it writes the final RAM state back to `menu.txt` permanently and destroys the shared memory using `shmctl(IPC_RMID)`.

### 2.5 Socket Programming
A robust client-server model was built using TCP sockets.
- **Implementation:** The server (`server.c`) creates an IPv4 TCP socket, binds to port 8080, and calls `listen()`. The client (`client.c`) uses `connect()` to establish a connection. All communication flows seamlessly over this socket using `read()` and `write()`.

### 2.6 Inter-Process Communication (IPC)
The system separates operations into three distinct OS processes: the Server, the Kitchen, and the Logger, communicating via three different IPC mechanisms.
- **Implementation:**
  - **Message Queues:** A System V Message Queue (`msgsnd`, `msgrcv`) is used to pass order structs from the Server process to the Kitchen process asynchronously.
  - **Pipes & `fork()`:** In `server.c`, `fork()` is used to spawn a child Logger Process. The main server passes strings down an unnamed `pipe()`. The child infinitely reads this pipe and writes to `server_logs.txt`, ensuring that slow disk logging doesn't stall the main network server.
  - **Shared Memory:** The Kitchen process attaches to the Server's Shared Memory block to print real-time live stock updates to the terminal after finishing a batch of food.

## 3. Demonstration & Screenshots

### Demonstration 1: Initialization & IPC Setup (Pipes, Shared Memory, Message Queues)
*(Instructions for you to take the screenshot: Run `./server` in Terminal 1, `tail -f server_logs.txt` in Terminal 2, and `./kitchen` in Terminal 3. Take a screenshot of all 3 running. Then delete these italic instructions before submitting.)*

**Screenshot 1:** *(Insert screenshot here)*

**Technical Analysis & Implementation Details:**
The screenshot above demonstrates the initialization of the system's Inter-Process Communication (IPC) architecture. When `./server` is executed, it uses `shmget` to allocate a System V Shared Memory block for the live menu and `msgget` to create a Message Queue. It then calls `fork()` to spawn a child Logger process. The parent server and child logger communicate asynchronously via an unnamed `pipe()`, as verified by the `tail -f server_logs.txt` command capturing the output. Simultaneously, the `./kitchen` process successfully attaches to the exact same shared memory segment and message queue using `shmat`, establishing a perfectly linked multi-process environment.

### Demonstration 2: Concurrency, Mutexes & File Locking
*(Instructions for you to take the screenshot: Open two `./client` terminals. Run `SIGNUP admin pass ADMIN` and `SIGNUP user pass USER` at the same time. Take a screenshot showing the logger updating active clients and the clients signing up. Delete these italic instructions later.)*

**Screenshot 2:** *(Insert screenshot here)*

**Technical Analysis & Implementation Details:**
This screenshot proves the successful implementation of concurrent thread execution and resource locking. When the two `./client` instances connect, the server spawns two isolated threads via `pthread_create`. As both threads attempt to increment the global `active_clients` counter simultaneously, a race condition is prevented using a POSIX Mutex (`pthread_mutex_lock`), correctly reflecting "Total live clients: 2" in the logs. Furthermore, both clients successfully register their accounts simultaneously because the `signup()` function utilizes `fcntl()` to place strict write-locks on the `users.txt` database, ensuring file consistency and preventing data corruption during concurrent writes.

### Demonstration 3: Batch Processing & Semaphores
*(Instructions for you to take the screenshot: In the client terminal, type `PLACE_ORDER Maggi` three times in a row very quickly. Take a screenshot of the Kitchen terminal showing it scoop up all 3. Delete these italic instructions later.)*

**Screenshot 3:** *(Insert screenshot here)*

**Technical Analysis & Implementation Details:**
This screenshot demonstrates advanced CPU scheduling and counting semaphores (`sem_t`) in action. When three identical orders are placed, they are pushed into the Message Queue. Instead of the Kitchen worker thread processing them inefficiently one by one, it leverages the `IPC_NOWAIT` flag with `msgrcv`. The worker thread instantly retrieves all three pending Maggi orders from the queue to form a single "batch". It then sleeps only once to simulate cooking for the entire batch. The counting semaphores ensure that the maximum cooking capacity per category (e.g., 2 stoves for Maggi) is never exceeded.

### Demonstration 4: Cross-Process Shared Memory & Graceful Shutdown
*(Instructions for you to take the screenshot: After the kitchen batch completes, it will print the live stock. Then, press `Ctrl+C` in the server terminal to shut it down. Take a screenshot of both. Delete these italic instructions later.)*

**Screenshot 4:** *(Insert screenshot here)*

**Technical Analysis & Implementation Details:**
The output `[STOCK] Maggi remaining in stock` printed by the Kitchen process confirms flawless cross-process memory synchronization. The Kitchen process directly read the stock integer from the System V Shared Memory (`shmat`) that was modified in RAM by the Server process, entirely bypassing slow disk I/O. Finally, when `Ctrl+C` is pressed in the server terminal, the `shutdown_handler` intercepts the OS `SIGINT` signal. It instantly flushes the shared RAM state permanently to the hard drive, and uses `shmctl` and `msgctl` with the `IPC_RMID` flag to gracefully wipe the memory segments and message queues, successfully preventing zombie processes and memory leaks.

## 4. Challenges Faced and Solutions

**Challenge 1: Slow Disk I/O and Stock Synchronization**
- *Problem:* Originally, every time a user placed an order, the server had to lock, read, update, and save the `menu.txt` file on the hard drive. This caused a massive bottleneck due to slow disk I/O, and syncing the live stock with the Kitchen process was incredibly inefficient.
- *Solution:* The entire live stock system was migrated to System V Shared Memory. `menu.txt` is now only read once on startup and saved once on shutdown. All live stock deductions happen instantly in RAM, completely eliminating the disk bottleneck and allowing both the Server and Kitchen to instantly read the stock at the speed of memory.

**Challenge 2: Safe File Updates and Truncation**
- *Problem:* When updating the status in `orders.txt`, simply writing over the file caused garbage data if the new data was shorter than the old data.
- *Solution:* The system safely reads from the original file, writes updates to a temporary file (`temp.txt`), and then uses atomic `rename()` to replace the original file securely.

**Challenge 3: Isolating Kitchen Logic from Client Handling**
- *Problem:* Handling long-running tasks like "cooking" (which involves `sleep()`) inside the main server process would block network threads and clutter the server architecture.
- *Solution:* The kitchen was decoupled into an entirely separate OS process (`kitchen.c`). Inter-Process Communication (IPC) via Message Queues was introduced to safely bridge the gap, allowing the server to instantly acknowledge order placement while the kitchen processes it asynchronously.

**Challenge 4: Efficient Kitchen Scheduling (Batch Processing)**
- *Problem:* If 3 users order Maggi at the same time, processing them one-by-one (`msgrcv` -> `sleep` -> complete) is inefficient and inaccurate to real-world cooking.
- *Solution:* The worker threads were upgraded to use the `IPC_NOWAIT` flag. The thread blocks on the first order, but then instantly scoops up any identical pending orders from the queue to form a "batch" (e.g., 3 orders). It then sleeps only *once* for the entire batch, maximizing throughput.

**Challenge 5: IPC Memory Leaks and Buffer Overflows**
- *Problem:* Force-closing the server (`Ctrl+C`) would leave zombie Message Queues and Shared Memory blocks taking up system RAM permanently. Furthermore, `sscanf()` on client input risked buffer overflow attacks.
- *Solution:* A `shutdown_handler` was added to catch `SIGINT` signals, gracefully calling `msgctl(IPC_RMID)` and `shmctl(IPC_RMID)` to wipe the RAM before exiting. Input buffers were secured by hardcoding limits into the format strings (e.g., `%49s`).
