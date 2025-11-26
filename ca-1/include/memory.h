#include <iostream>
#include <bitset>
#include <stdio.h>
#include<stdlib.h>
#include <string>
#include <vector>
#include <cstdint>
using namespace std;

class DataMemory {
private:
    // Use a dynamically sizable memory to avoid out-of-bounds when testcases use high addresses (e.g., 0x10000+)
    std::vector<uint8_t> memory;
    inline void ensureCapacity(size_t address, size_t bytes) {
        size_t required = address + bytes;
        if (required > memory.size()) {
            // Grow to the next power-of-two-ish to avoid frequent reallocations
            size_t newSize = memory.size() ? memory.size() : 65536; // start at 64KB
            while (newSize < required) newSize *= 2;
            memory.resize(newSize, 0);
        }
    }
public:
    DataMemory();
    void execute(uint32_t address, uint32_t writeData, bool memWrite, bool memRead, uint32_t& readData, bool fullWord);
    void update();
};

class InstructionMemory {
private:
    vector<uint8_t> memory;
    vector<uint32_t> instructions;
public:
    InstructionMemory(vector<uint8_t>& instMem);
    uint32_t fetchInstruction(uint32_t address);
};