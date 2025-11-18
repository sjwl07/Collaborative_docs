# Collaborative Document Editor

A real-time collaborative documentation system built using a C++ server-client architecture with `epoll` for efficient handling of concurrent connections. This project focuses on implementing robust document synchronization and clash resolution for a seamless co-editing experience.

---

## 🚀 Getting Started (How to Run the Code)

Follow these steps to set up and run the collaborative document editor:

1.  **Clone the Repository**
    ```bash
    git clone [https://github.com/sjwl07/Collaborative_docs](https://github.com/sjwl07/Collaborative_docs)
    cd Collaborative_docs
    ```
2.  **Compile the Code**
    ```bash
    make
    ```
3.  **Run the Server**
    Start the server on one machine. **Note its IP address.**
    ```bash
    ./server
    ```
4.  **Configure the Client**
    Before running the client, you must modify the source code to point to the server:
    * Open **`client.cpp`**.
    * Change the hardcoded **IP address** to the IP address of the machine running the server.
5.  **Run the Client**
    Start the client on the same or a different machine.
    ```bash
    ./client
    ```
6.  **Start Collaborating**
    Type commands or text in the terminal. The document updates will be visible in the **`client_output.txt`** file and broadcast to other connected clients.

---

## ✨ Features

Our collaborative documentation system supports the following client interactions:

| Action | Command/Input | Description |
| :--- | :--- | :--- |
| **Text Insertion** | Typing any text | Inserts the text directly into the document at the current **cursor position**. |
| **Exit** | `/exit` | Causes the client to cleanly disconnect from the document session. |
| **Backspace** | `/backspace {length}` | Deletes characters backward from the current cursor position by the specified `length`. |
| **Cursor Movement** | `/cursor {index}` | Moves the client's cursor to the given `index` position in the document. |
| **New Client Sync** | (Logging in) | A new client logging in immediately receives the **full existing document state** to ensure they start in sync. |
| **Escaping Commands**| `//exit` | To type a command string (e.g., `/exit`) as text in the document, **prepend a second `/`**. |

---

## 🛠️ Implementation Details

### Architecture

The system is built on a basic **server-client model** utilizing the `epoll` mechanism for efficient, non-blocking I/O, allowing the server to handle multiple simultaneous client connections.

### Operation Class and Packet Format

All document updates are encapsulated and transmitted using an `Operation` class.

```cpp
class Operation {
    string type;          // e.g., "INSERT", "DELETE", "CURSOR"
    int position;         // The index where the change occurs
    string content;       // The text content (for INSERT/DELETE)
    string clientId;      // Unique identifier for the client that made the change
    int localVersion;     // The document version the client was looking at when the change was made
}
