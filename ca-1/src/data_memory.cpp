#include "data_memory.h"

DataMemory::DataMemory() {
    for (int i = 0; i < 16384; i++) {
        memory[i] = 0;
    }
}

void DataMemory::execute(uint32_t address, uint32_t writeData, bool memWrite, bool memRead, uint32_t& readData, bool fullWord) {
    if (memWrite) {
        memory[address] = writeData & 0xFF;
        memory[address + 1] = (writeData >> 8) & 0xFF;

        if(fullWord) {
            memory[address + 2] = (writeData >> 16) & 0xFF;
            memory[address + 3] = (writeData >> 24) & 0xFF;
        }
    }
    if (memRead) {
        if (fullWord) {
            readData = memory[address] | 
                      (memory[address + 1] << 8) | 
                      (memory[address + 2] << 16) | 
                      (memory[address + 3] << 24);
        } else {
            readData = memory[address] & 0xFF;
        }
    }
}