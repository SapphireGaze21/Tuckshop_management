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

## 3. Demonstration Script & Screenshots
*(Note: This section outlines the exact steps taken to demonstrate the core OS concepts. Replace the placeholders with actual screenshots of your terminal windows).*

### Demonstration 1: Initialization & IPC (Pipes, Shared Memory, Message Queues)
**How to Demonstrate:**
1. Open 3 Terminal windows side-by-side.
2. **Terminal 1:** Run `./server`. (This initializes the Shared Memory, creates the Message Queue, and uses `fork()` and `pipe()` to spawn the Logger process).
3. **Terminal 2:** Run `tail -f server_logs.txt` to watch the live pipe outputs.
4. **Terminal 3:** Run `./kitchen`. (This attaches to the Server's Shared Memory and connects to the Message Queue).
**Screenshot 1:** *(Insert screenshot showing all 3 terminals running perfectly without crashing)*
**Concepts Proven:** `shmget`, `msgget`, `fork`, `pipe`.

### Demonstration 2: Concurrency, Mutexes & File Locking
**How to Demonstrate:**
1. Open two new terminals (Terminal 4 and 5). Run `./client` in both.
2. Watch Terminal 2 (`server_logs.txt`). It will instantly show: `"New client connected. Total live clients: 2"`. This proves the `pthread_mutex_t client_mutex` successfully prevented a race condition when counting active clients.
3. In Terminal 4, type: `SIGNUP admin pass ADMIN`
4. In Terminal 5, type: `SIGNUP user pass USER`
5. Both will succeed simultaneously. This proves the `fcntl()` write-locks on `users.txt` prevented file corruption.
**Screenshot 2:** *(Insert screenshot showing both clients signing up, and the logger terminal showing the exact events)*
**Concepts Proven:** `pthread_create`, `pthread_mutex_lock`, `fcntl` (File Locks).

### Demonstration 3: Batch Processing & Semaphores
**How to Demonstrate:**
1. In Terminal 4 (Admin), add stock: `ADD_ITEM Maggi PACKAGED 10`.
2. In Terminal 5 (User), quickly type `PLACE_ORDER Maggi` three times in a row. 
3. Watch the Kitchen terminal (Terminal 3). Because you implemented `IPC_NOWAIT`, it will completely bypass the standard 1-by-1 processing and instantly print: `[MAGGI] Cooking batch of 3 orders!`.
4. It will wait exactly 10 seconds, and then mark all 3 orders as completed simultaneously.
**Screenshot 3:** *(Insert screenshot of the Kitchen terminal showing the Batch Processing output)*
**Concepts Proven:** `msgrcv` (`IPC_NOWAIT`), Advanced CPU/Thread Scheduling, Semaphores (`sem_wait` limit).

### Demonstration 4: Cross-Process Shared Memory & Graceful Shutdown
**How to Demonstrate:**
1. Immediately after the Maggi batch finishes cooking, look at the Kitchen terminal (Terminal 3). It will print: `[STOCK] Maggi remaining in stock: 7`. This proves that the Kitchen process instantly read the RAM that the Server process modified!
2. Finally, go to the Server terminal (Terminal 1) and press `Ctrl+C`.
3. It will print `"Server shutting down and cleaning up IPC resources..."`. This proves that it intercepted the OS shutdown signal, wrote the RAM stock back to the hard drive, and used `IPC_RMID` to prevent memory leaks.
**Screenshot 4:** *(Insert screenshot of the Kitchen printing the stock, and the Server cleanly shutting down)*
**Concepts Proven:** `shmat` (Shared Memory), `shmctl` & `msgctl` (Garbage Collection/Cleanup), `signal(SIGINT)`.

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
