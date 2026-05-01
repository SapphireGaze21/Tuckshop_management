# Concurrent Tuckshop Order Management System

A robust, multi-process OS-level application built in C that simulates a high-concurrency college tuck shop environment. This project uses System V IPC, POSIX Threads, and synchronization primitives to safely manage concurrent network clients, batch processing, and live stock tracking.

## Features
- **Client-Server Architecture:** Handles multiple concurrent users via socket programming and POSIX Threads (`pthread`).
- **Inter-Process Communication (IPC):** Uses unnamed pipes (`pipe()`) for a dedicated logger process, Message Queues (`msgget`) for asynchronous order passing, and Shared Memory (`shmget`) for live stock tracking.
- **Synchronization:** Prevents memory and file-based race conditions using POSIX Mutexes (`pthread_mutex_lock`), Counting Semaphores (`sem_t`), and File Locks (`fcntl`).
- **Graceful Shutdown:** Implements signal handlers (`SIGINT`) to perform safe Garbage Collection and RAM wipes (`IPC_RMID`) to prevent memory leaks.
- **Batch Processing:** Uses `IPC_NOWAIT` to dynamically scoop up and batch identical orders into the Kitchen for maximum throughput.

## Prerequisites
- GCC Compiler
- MacOS/Linux (POSIX-compliant system)

## How to Run the System

### 1. Compilation
First, compile the three separate components (Server, Kitchen, and Client):
```bash
gcc server.c utils.c -o server
gcc kitchen.c utils.c -o kitchen
gcc client.c -o client
```

### 2. Execution
You will need at least 3 separate terminal windows to run the full simulation.

**Terminal 1 (The Server):**
Start the main server process. This will initialize the Shared Memory, Message Queues, and the Logger pipe.
```bash
./server
```

**Terminal 2 (The Kitchen):**
Start the kitchen worker process. It will attach to the server's memory and wait for orders.
```bash
./kitchen
```

**Terminal 3+ (The Clients):**
Run the client application to connect to the server. You can run this in as many terminals as you want to test concurrency.
```bash
./client
```

### 3. Usage & Commands
When you open `./client`, you must first `SIGNUP` or `LOGIN`.
- **Admin Accounts:** `SIGNUP username password ADMIN`
- **User Accounts:** `SIGNUP username password USER`

Once logged in, the terminal will dynamically display all the commands available for your specific role (e.g., `PLACE_ORDER`, `ADD_ITEM`, `VIEW_MENU`, etc.).

### 4. Graceful Shutdown
To safely shut down the system without leaving zombie processes or memory leaks, simply press `Ctrl+C` in the Server terminal. It will flush the RAM stock to `menu.txt` and securely wipe the System V IPC resources.