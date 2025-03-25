#include "my_utils.h"

#include <string>
#include <cstring>
#include <vector>
#include <stdio.h>


using namespace std;

vector<string> split_string (string input, char *delim) {
    vector<string> output;
    char * pch;
    pch = strtok(const_cast<char *>(input.c_str()), delim);
    while (pch != NULL)
    {
        output.push_back(pch);
        pch = strtok(NULL, delim);
    }
    return output;
}