#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>     // ftruncate, fileno
#include <cstdio> 
#include <string>
#include <sstream>
#include <vector>
#include <bits/stdc++.h>

using namespace std;

bool insertIntoFile(const string& path,const string& text,size_t pos);

class Operation {
public:
    string type;
    int position;
    string content;
    string clientId;
    int localVersion;

    Operation();

    Operation(const string &type,
              int position,
              const string &content,
              const string &clientId,
              int localVersion);

    string toString() const;
    static Operation fromString(const string &s);
};