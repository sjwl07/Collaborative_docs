#include <iostream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <algorithm>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include "utils.hpp"

using namespace std;

const int PORT = 8080;
const int BUFFER_SIZE = 1024;
const int MAX_EVENTS = 64;

// Global document path and version
string out_path = "server_document.txt"; // Added document persistence path
int serverVersion = 0;

typedef struct client_data{
    sockaddr_in addr;
    size_t cursor;
} client_data;

typedef struct OP_Node{
    Operation op;
    OP_Node* next;
} OP_Node;

OP_Node* head = NULL; // Head of the linked list (should be the oldest operation)
string readDocumentContent(const string& path) {
    ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}
void send_to_new_client(int client_socket, const string& document_content){
    Operation* s_pkt = new Operation("INSERT", 0, document_content, "1", serverVersion);
    string s_str = s_pkt->toString();
    send(client_socket, s_str.c_str(), s_str.length(), 0);
}
void insertNode(const Operation &op) {
    OP_Node* new_node = new OP_Node();
    new_node->op = op;
    new_node->next = nullptr;
    if (!head || op.localVersion < head->op.localVersion) { // Changed to '<'
        new_node->next = head;
        head = new_node;
        return;
    }
    OP_Node* curr = head;
    while (curr->next &&
           curr->next->op.localVersion < op.localVersion) { // Changed to '<'
        curr = curr->next;
    }
    new_node->next = curr->next;
    curr->next = new_node;
}

unordered_map<int, client_data> client_map;
// Sends the transformed operation (s_str) to all clients except the sender,
// and sends an acknowledgment (e_str) to the sender.
void sendToClients(int sender_sd, string& s_str, string& e_str){
    for (const auto& client : client_map) {
        int client_sd = client.first;

        if (client_sd == sender_sd){
            send(client_sd, e_str.c_str(), e_str.length(), 0);
        } else {
            send(client_sd, s_str.c_str(), s_str.length(), 0);
        }
    }
}

void handle_client_pkt(int sd, string& r_str){
    Operation r_pkt = Operation::fromString(r_str);
    if(r_pkt.type=="MOVE_CURSOR"){
        client_map[sd].cursor = r_pkt.position;
        return;
    }
    long r_pkt_len_or_count = 0;
    if (r_pkt.type == "INSERT") {
        r_pkt_len_or_count = r_pkt.content.length();
    } else if (r_pkt.type == "DELETE") {
        r_pkt_len_or_count = stol(r_pkt.content);
    }
    // Case 1: Packet is up-to-date (r_pkt.localVersion == serverVersion)
    if (r_pkt.localVersion == serverVersion){
        // Apply to document and history
        if (r_pkt.type == "INSERT") {
            insertIntoFile(out_path, r_pkt.content, r_pkt.position); 
        } else if (r_pkt.type == "DELETE") {
            deleteFromFile(out_path, r_pkt.position, r_pkt_len_or_count);
        }
        // insertIntoFile(out_path, r_pkt.content, r_pkt.position); 
        insertNode(r_pkt); 
        serverVersion++;
        r_pkt.localVersion = serverVersion;
        string s_str = r_pkt.toString();
        r_pkt.content = "";
        string e_str = r_pkt.toString();

        sendToClients(sd, s_str, e_str);
        cout << "[Server] Applied V" << serverVersion << ". Broadcast complete." << endl;
        return;
    }
    
    else if (r_pkt.localVersion < serverVersion){
        
        OP_Node* curr = head;
        while (curr) {
            if (curr->op.localVersion > r_pkt.localVersion) {
                Operation& transform_op = curr->op;
                if (transform_op.type == "INSERT") {
                    long ins_len = transform_op.content.length();
                    if (transform_op.position <= r_pkt.position) {
                        r_pkt.position += ins_len;
                    }
                }
                else if (transform_op.type == "DELETE") {
                    long del_len = stol(transform_op.content);
                    if (transform_op.position < r_pkt.position) {
                        r_pkt.position -= min((long)r_pkt.position - transform_op.position, del_len);
                    }
                }
                if (transform_op.position <= r_pkt.position) {
                    r_pkt.position += transform_op.content.length();
                }

            } else if (curr->op.localVersion == r_pkt.localVersion) {
                // We reached the base version, stop the traversal.
                break;
            }
            curr = curr->next;
        }
        if (r_pkt.type == "INSERT") {
            insertIntoFile(out_path, r_pkt.content, r_pkt.position);
        } else if (r_pkt.type == "DELETE") {
            deleteFromFile(out_path, r_pkt.position, r_pkt_len_or_count);
        }
        serverVersion++;
        r_pkt.localVersion = serverVersion;
        string s_str = r_pkt.toString();
        r_pkt.content = "";
        string e_str = r_pkt.toString();

        sendToClients(sd, s_str, e_str);
        cout << "[Server] Transformed and Applied V" << serverVersion << ". Broadcast complete." << endl;
        return;
    }
    else {
        cout << "[Server] Warning: Received future packet V" << r_pkt.localVersion << ", current V" << serverVersion << ". Ignoring." << endl;
    }
}


int main() {
    int master_socket, new_socket, valread;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];

    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listener on port %d \n", PORT);

    if (listen(master_socket, SOMAXCONN) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    puts("Waiting for connections ...");

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = master_socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, master_socket, &event) == -1) {
        perror("epoll_ctl");
        exit(EXIT_FAILURE);
    }

    struct epoll_event events[MAX_EVENTS];

    while (true) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == master_socket) {
                if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }
                client_map[new_socket].addr = address;
                printf("New connection, socket fd is %d, ip is : %s, port : %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

                string document_content = readDocumentContent(out_path);
                send_to_new_client(new_socket,document_content);
                
                event.events = EPOLLIN | EPOLLET; 
                event.data.fd = new_socket;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_socket, &event) == -1) {
                    perror("epoll_ctl");
                    exit(EXIT_FAILURE);
                }
            }
            else {
                int sd = events[i].data.fd;
                valread = read(sd, buffer, BUFFER_SIZE - 1); // Read up to size-1
                if (valread == 0) {
                    sockaddr_in addr = client_map[sd].addr;
                    printf("Host disconnected, ip %s, port %d \n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
                    close(sd);
                    client_map.erase(sd); // Remove from map
                }
                else if (valread > 0) {
                    buffer[valread] = '\0';
                    string r_str(buffer);
                    sockaddr_in addr = client_map[sd].addr;

                    cout << "ip " << inet_ntoa(addr.sin_addr) << " said: " << r_str << endl;

                    handle_client_pkt(sd, r_str);
                }
            }
        }
    }
    close(master_socket);
    return 0;
}
