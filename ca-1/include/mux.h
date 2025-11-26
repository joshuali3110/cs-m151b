#include <iostream>
#include <bitset>
#include <stdio.h>
#include<stdlib.h>
#include <string>
#include <cstdint>
using namespace std;

class Mux {
public:
    Mux();
    uint32_t execute(uint32_t input1, uint32_t input2, bool select);
};