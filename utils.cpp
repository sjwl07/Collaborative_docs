#include "utils.hpp"
#include <fstream>
#include <sstream>
#include <vector>
#include <iostream> 
#include <cstdio> 

using namespace std;

bool insertIntoFile(const string& path, const string& text, size_t pos)
{
    // cout << "Writing " << text << " to " << path << endl;
    string data;
    // Try to open for reading
    FILE* f = fopen(path.c_str(), "rb");
    if (f) {
        // cout << "File exists, loading content" << endl;
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);

        data.resize(size);

        if (size > 0) {
            if (fread(&data[0], 1, size, f) != (size_t)size) {
                fclose(f);
                return false;
            }
        }

        fclose(f);
    } else {
        // cout << "File does not exist, creating new file" << endl;
        data.clear();
    }

    // Adjust position
    if (pos > data.size()) pos = data.size();

    data.insert(pos, text);
    f = fopen(path.c_str(), "wb");
    if (!f) return false;

    if (!data.empty()) {
        if (fwrite(data.data(), 1, data.size(), f) != data.size()) {
            fclose(f);
            return false;
        }
    } 
    fclose(f);
    return true;
}
long getFileSize(const string& path){
    FILE* f=fopen(path.c_str(), "rb"); 
    if (f==NULL) {
        return 0; 
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f); 
    fclose(f); 
    if (size < 0) {
        return 0;
    }
    return size;
}
bool deleteFromFile(const string& path, size_t pos, size_t length) {
    string data;
    FILE* f = fopen(path.c_str(), "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);

        data.resize(size);

        if (size > 0) {
            if (fread(&data[0], 1, size, f) != (size_t)size) {
                fclose(f);
                return false;
            }
        }
        fclose(f);
    } else {
        // File doesn't exist, nothing to delete
        return true; 
    }

    // Adjust position and length to be valid
    if (pos > data.size()) {
        // Position is past the end, nothing to delete
        return true;
    }
    if (length > data.size() - pos) {
        length = data.size() - pos;
    }

    if (length == 0) {
        return true; // Nothing to delete
    }

    data.erase(pos, length);

    // Write back
    f = fopen(path.c_str(), "wb");
    if (!f) return false;

    if (!data.empty()) {
        if (fwrite(data.data(), 1, data.size(), f) != data.size()) {
            fclose(f);
            return false;
        }
    }
    fclose(f);
    return true;
}

Operation::Operation() = default;

Operation::Operation(const string &type,
                     int position,
                     const string &content,
                     const string &clientId,
                     int localVersion)
    : type(type),
      position(position),
      content(content),
      clientId(clientId),
      localVersion(localVersion) {}

string Operation::toString() const {
    string pktStr;
    pktStr ="OP";
    pktStr +="|"+type + "|" + to_string(position) + "|" + content + "|" + clientId + "|" + to_string(localVersion);
    return pktStr;
}

Operation Operation::fromString(const string &s) {
    vector<string> parts;
    stringstream ss(s);
    string item;
    while (getline(ss, item, '|')) {
        parts.push_back(item);
    }
    if (parts.size() < 6) {
        cerr << "Error: Malformed Operation string received: " << s << ". Expected at least 6 parts, got " << parts.size() << "." << endl;
        return Operation("ERROR", 0, "PARSE_FAIL", "0", 0);
    }
    return Operation(
        parts[1],              // type (e.g., "INSERT")
        stoi(parts[2]),        // position
        parts[3],              // content
        parts[4],              // clientId
        stoi(parts[5])         // localVersion
    );
}
