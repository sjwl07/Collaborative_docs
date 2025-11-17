#include "utils.hpp"
#include <fstream>
#include <sstream>
#include <vector>
#include <iostream> // For cout used in file operations
#include <cstdio>   // For FILE* and file functions

using namespace std;

bool insertIntoFile(const string& path, const string& text, size_t pos)
{
    // cout << "Writing " << text << " to " << path << endl;
    string data;

    // Try to open for reading
    FILE* f = fopen(path.c_str(), "rb");
    if (f) {
        // cout << "File exists, loading content" << endl;
        // Find size
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
        // File does not exist → treat as empty
        // cout << "File does not exist, creating new file" << endl;
        data.clear();
    }

    // Adjust position
    if (pos > data.size()) pos = data.size();

    // Insert text
    data.insert(pos, text);

    // Write final content
    f = fopen(path.c_str(), "wb");
    if (!f) return false;

    if (!data.empty()) {
        if (fwrite(data.data(), 1, data.size(), f) != data.size()) {
            fclose(f);
            return false;
        }
    } else {
        // Ensure file is truncated/empty if data is empty
        // Since we opened with "wb", it overwrites, but writing 0 bytes is safe.
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
    // Format: OP|type|position|content|clientId|localVersion
    pktStr ="OP";
    pktStr +="|"+type + "|" + to_string(position) + "|" + content + "|" + clientId + "|" + to_string(localVersion);
    return pktStr;
}

Operation Operation::fromString(const string &s) {
    vector<string> parts;
    stringstream ss(s);
    string item;

    // Split the string by the '|' delimiter
    while (getline(ss, item, '|')) {
        parts.push_back(item);
    }

    // Check for the minimum required number of parts (OP + 5 fields = 6 parts)
    if (parts.size() < 6) {
        // Error handling for malformed packet.
        cerr << "Error: Malformed Operation string received: " << s << ". Expected at least 6 parts, got " << parts.size() << "." << endl;
        return Operation("ERROR", 0, "PARSE_FAIL", "0", 0);
    }

    // Parse the fields based on their expected index
    return Operation(
        parts[1],              // type (e.g., "INSERT")
        stoi(parts[2]),        // position
        parts[3],              // content
        parts[4],              // clientId
        stoi(parts[5])         // localVersion
    );
}