#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread> 
#include <errno.h> 

#include "utils.hpp"

using namespace std;
const int PORT = 8080;
const int BUFFER_SIZE = 1024;
int cursor_index = 0;
string out_path = "client_out.txt";
int local_version = 0;
void handle_remote_pkt(string& message){
    Operation pkt = Operation::fromString(message);
    if (pkt.type == "INSERT") {
        if (!pkt.content.empty()) {
            insertIntoFile(out_path, pkt.content, pkt.position);
        }
        if (pkt.position < cursor_index){
            cursor_index += pkt.content.length(); 
        }
    } 
    else if (pkt.type == "DELETE") {
        try {
            long length_to_delete = stol(pkt.content);
            if (length_to_delete > 0) {
                deleteFromFile(out_path, pkt.position, length_to_delete);
                if (pkt.position < cursor_index) {
                    cursor_index = max((long)pkt.position, cursor_index - length_to_delete);
                }
            }
        } catch (const exception& e) {
            cout << "Error parsing delete length: " << e.what() << endl;
        }
    }

    local_version = pkt.localVersion;
    cout<<"\ncursor_index after op:"<<cursor_index<<endl;
    // local_version = pkt.localVersion;
    // cout<<"\ncursor_index after op:"<<cursor_index<<endl;
    // if (!pkt.content.empty()) {
    //     insertIntoFile(out_path, pkt.content, pkt.position);
    // }
    // if (pkt.position < cursor_index){
    //     cursor_index += pkt.content.length(); 
    // }
    // local_version = pkt.localVersion;
    // cout<<"cursor_index after op:"<<cursor_index<<endl;
}
void handle_local_delete(long position, long length) {
    if (length <= 0) return;
    deleteFromFile(out_path, position, length);
    cursor_index = position; // Move cursor to the start of deletion
    cout << "cursor_index after op:" << cursor_index << endl;
}
void handle_local_packet(string& message){
    insertIntoFile(out_path, message, cursor_index);
    cursor_index += message.length(); 
    cout<<"cursor_index after op:"<<cursor_index<<endl;
}
void receive_thread(int sock) {
    char buffer[BUFFER_SIZE];
    while (true) {
        int valread = read(sock, buffer, BUFFER_SIZE - 1);
        if (valread > 0) {
            buffer[valread] = '\0';
            std::string r_str(buffer);
            cout << "\nServer response: " << r_str << endl;
            handle_remote_pkt(r_str);
        } else if (valread == 0) {
            cout << "\nServer disconnected." << endl;
            break;
        } else if (valread == -1) {
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
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    //Replace "192.168.44.204" with your actual server IP if needed
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    cout << "Connected to server. Type messages to send. (Type 'exit' to quit)" << endl;
    std::thread receiver(receive_thread, sock);

    while (true) {
        string message;
        cout << "Enter text: ";
        getline(cin, message);

        if(message.find("//",0)==0){
            message=message.substr(1);
        }
        else if (message == "/exit") {
            cout<<"EXITING\n";
            break;
        }
        else if(message.find("/cursor",0)==0){
            try{
                string pos_int=message.substr(8);
                long new_pos = stol(pos_int);
                new_pos=max(0L,min(getFileSize(out_path),new_pos));
                cursor_index=new_pos;
                Operation* s_pkt = new Operation("MOVE_CURSOR", cursor_index, "", "1", local_version);
                string s_str = s_pkt->toString();
                send(sock, s_str.c_str(), s_str.length(), 0);
            }
            catch(const exception& e){
                cout<<"INVALID COMMAND\n";
            }
            continue;
        }
        else if(message.find("/backspace",0)==0){
            try{
                string len_str = message.substr(11); // /backspace 
                long num_to_delete = stol(len_str);

                if (num_to_delete <= 0) continue;

                // Calculate deletion parameters
                // Deletes *before* the cursor
                long pos_to_delete = max(0L, (long)cursor_index - num_to_delete);
                long actual_num_deleted = cursor_index - pos_to_delete;

                if (actual_num_deleted == 0) continue; // Nothing to delete

                // Create the packet
                Operation* s_pkt = new Operation("DELETE", pos_to_delete, to_string(actual_num_deleted), "1", local_version);
                string s_str = s_pkt->toString();
                
                cout << "Sending V" << local_version << " op: " << s_str << endl;
                
                // 1. Send the packet
                send(sock, s_str.c_str(), s_str.length(), 0);
                
                // 2. Apply the change locally
                handle_local_delete(pos_to_delete, actual_num_deleted);

                delete s_pkt;
            }
            catch(const exception& e){
                cout<<"INVALID COMMAND\n";
            }
            continue;
        }
        // The operation's version must be the version it is based on (local_version)
        Operation* s_pkt = new Operation("INSERT", cursor_index, message, "1", local_version);
        string s_str = s_pkt->toString();
        cout << "Sending V" << local_version << " op: " << s_str << endl;
        send(sock, s_str.c_str(), s_str.length(), 0);
        handle_local_packet(message);
        delete s_pkt; 
    }
    shutdown(sock, SHUT_RDWR); 
    close(sock);
    if (receiver.joinable()) {
        receiver.join();
    }
    return 0;
}
