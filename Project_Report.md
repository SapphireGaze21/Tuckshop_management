# Project Report: Concurrent Tuckshop Order Management System

## 1. Problem Statement
The objective of this project is to design and implement a concurrent, file-based tuckshop (canteen) order management system. In a real-world canteen, multiple users place orders simultaneously, and the kitchen must process these orders while managing limited resources (stoves, chefs) and tracking stock. 

This project simulates that environment by providing a client-server architecture where multiple users can connect concurrently, authenticate based on roles, and place orders. The system handles concurrent access to shared resources (like the menu stock and order history files) and ensures data consistency across all operations.

## 2. Implementation of OS Concepts

### 2.1 Role-Based Authorization
The system implements three distinct roles: `ADMIN`, `USER`, and `GUEST`. 
- **Implementation:** During authentication (`login()` and `signup()` in `utils.c`), the user's role is determined. The `handle_client()` function in `server.c` strictly enforces access control. For example, `ADMIN` can add items to the menu, `USER` can place orders, and `GUEST` is restricted to only viewing the menu and checking order status.

### 2.2 File Locking
To ensure data consistency when multiple threads access the same files simultaneously, file locking is used.
- **Implementation:** POSIX `fcntl()` locks are heavily utilized in `utils.c`. 
  - Write locks (`F_WRLCK`) are acquired before modifying `users.txt` during signup.
  - File locking is used in `add_order()` and `update_order_status()` for `orders.txt` to prevent lost updates or race conditions when assigning order IDs.
  - `check_and_update_stock()` locks `menu.txt` while decrementing item quantities.

### 2.3 Concurrency Control
The system handles multiple processes and threads executing simultaneously.
- **Implementation:**
  - **Threads:** The server uses `pthread_create()` to spawn a new thread for every incoming client connection, allowing the server to handle multiple clients concurrently without blocking. The kitchen process also spawns worker threads for different food categories.
  - **Semaphores:** The kitchen worker threads use POSIX semaphores (`sem_t`) to limit cooking capacity. For instance, `maggi_sem` is initialized to 2, ensuring that at most two Maggi orders can be processed simultaneously.

### 2.4 Data Consistency
Data consistency is maintained across shared resources to prevent dirty reads, race conditions, and lost updates.
- **Implementation:** The combination of `fcntl()` file locks and the structured `check_and_update_stock()` function ensures that if two users attempt to order the last remaining packaged item at the exact same millisecond, only one user will succeed, and the other will correctly receive an "Out of stock" message.

### 2.5 Socket Programming
A robust client-server model was built using TCP sockets.
- **Implementation:** The server (`server.c`) creates an IPv4 TCP socket, binds to port 8080, and calls `listen()`. The client (`client.c`) uses `connect()` to establish a connection. All communication (commands and responses) flows seamlessly over this socket using `read()` and `write()`.

### 2.6 Inter-Process Communication (IPC)
The system separates the client-facing server from the backend kitchen operations into two distinct OS processes.
- **Implementation:** A System V Message Queue (`msgget`, `msgsnd`, `msgrcv`) is used to pass orders from the Server process to the Kitchen process. When an order is placed, the server constructs an `OrderMessage` struct and sends it to the queue. The independent Kitchen process receives these messages based on `msg_type` to route them to the correct worker threads.

## 3. Screenshots and Outputs
*(Note: Please replace the placeholders below with actual screenshots of your terminal windows prior to submission)*

- **Screenshot 1: Server and Kitchen Startup**
  *(Insert screenshot showing `server.c` and `kitchen.c` running in split terminals)*
- **Screenshot 2: Multiple Clients Connecting**
  *(Insert screenshot showing two `client.c` instances logging in and placing orders simultaneously)*
- **Screenshot 3: Order Processing & Status Updates**
  *(Insert screenshot showing the kitchen terminal outputting "Processing order X" and the client terminal successfully executing `VIEW_STATUS`)*

## 4. Challenges Faced and Solutions

**Challenge 1: Safe File Updates and Truncation**
- *Problem:* When updating the stock in `menu.txt` or the status in `orders.txt`, simply writing over the file caused garbage data if the new data was shorter than the old data.
- *Solution:* For `orders.txt`, the system safely reads from the original file, writes updates to a temporary file (`temp.txt`), and then uses atomic `rename()` to replace the original file. For `menu.txt`, `ftruncate()` was successfully implemented to discard leftover characters after an in-place update.

**Challenge 2: Isolating Kitchen Logic from Client Handling**
- *Problem:* Handling long-running tasks like "cooking" (which involves `sleep()`) inside the main server process would block network threads and clutter the server architecture.
- *Solution:* The kitchen was decoupled into an entirely separate OS process (`kitchen.c`). Inter-Process Communication (IPC) via Message Queues was introduced to safely bridge the gap, allowing the server to instantly acknowledge order placement while the kitchen processes it asynchronously.

**Challenge 3: Managing Kitchen Capacity**
- *Problem:* The kitchen cannot process an infinite number of orders instantly; resources are limited.
- *Solution:* Counting Semaphores were introduced in the kitchen process. By initializing semaphores with specific values (e.g., 2 for Maggi), the worker threads naturally wait if the "stoves" are full, elegantly simulating real-world physical constraints without busy-waiting.
