#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread> // Added for concurrency
#include <errno.h> // Added for error checking

#include "utils.hpp"

using namespace std;
const int PORT = 8080;
const int BUFFER_SIZE = 1024;
int cursor_index = 0;
string out_path = "client_out.txt";
int local_version = 0;

void handle_remote_pkt(string& message){
    Operation pkt = Operation::fromString(message);
    
    // Only apply content if it's not an empty acknowledgment (ACK)
    if (!pkt.content.empty()) {
        insertIntoFile(out_path, pkt.content, pkt.position);
    }
    
    // Use .length() instead of sizeof() - FIX
    if (pkt.position < cursor_index){
        cursor_index += pkt.content.length(); 
    }
    local_version = pkt.localVersion;
    cout<<"cursor_index after op:"<<cursor_index<<endl;
    // For debugging/logging the current state
    // cout << "[Remote] Applied Op V" << pkt.localVersion << ", New cursor: " << cursor_index << endl;
}

// handling packet sent by client itself
void handle_local_packet(string& message){
    insertIntoFile(out_path, message, cursor_index);
    
    // Use .length() instead of sizeof() - FIX
    cursor_index += message.length(); 
    cout<<"cursor_index after op:"<<cursor_index<<endl;
    
    // For debugging/logging the current state
    // cout << "[Local] Applied Op, New cursor: " << cursor_index << endl;
}

// New function to handle network reception concurrently
void receive_thread(int sock) {
    char buffer[BUFFER_SIZE];
    while (true) {
        int valread = read(sock, buffer, BUFFER_SIZE - 1); // Read up to size-1
        if (valread > 0) {
            buffer[valread] = '\0';
            std::string r_str(buffer);
            cout << "\nServer response: " << r_str << endl; // Display the response
            handle_remote_pkt(r_str);
        } else if (valread == 0) {
            cout << "\nServer disconnected." << endl;
            break;
        } else if (valread == -1) {
            // Check for a real error (not EINTR)
            if (errno != EINTR) {
                perror("recv error");
                break;
            }
        }
    }
}


int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    
    // Removed unused char buffer[BUFFER_SIZE] from main

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // NOTE: Replace "192.168.44.204" with your actual server IP if needed
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    cout << "Connected to server. Type messages to send. (Type 'exit' to quit)" << endl;
    
    // Start the receiving thread
    std::thread receiver(receive_thread, sock);

    while (true) {
        string message;
        cout << "Enter text: ";
        getline(cin, message);

        if (message == "exit") {
            break;
        }

        // The operation's version must be the version it is based on (local_version)
        Operation* s_pkt = new Operation("INSERT", cursor_index, message, "1", local_version);
        string s_str = s_pkt->toString();
        
        cout << "Sending V" << local_version << " op: " << s_str << endl;
        
        // 1. Send the packet
        send(sock, s_str.c_str(), s_str.length(), 0);
        
        // 2. Apply the change locally (before server confirms)
        handle_local_packet(message);

        // REMOVED the blocking read() call. It's now handled by the separate thread.
        delete s_pkt; // Clean up memory
    }

    // Attempt to close the socket and stop the thread gracefully
    close(sock);
    if (receiver.joinable()) {
        receiver.join();
    }
    return 0;
}