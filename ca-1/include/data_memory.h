#include <iostream>
#include <bitset>
#include <stdio.h>
#include<stdlib.h>
#include <string>
using namespace std;

class DataMemory {
private:
    uint8_t memory[16384];
public:
    DataMemory();
    void execute(uint32_t address, uint32_t writeData, bool memWrite, bool memRead, uint32_t& readData, bool fullWord);
};