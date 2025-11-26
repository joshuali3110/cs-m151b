#include "memory.h"
#include <cstdint>

DataMemory::DataMemory() {
    // Initialize with 128KB to comfortably fit addresses around 0x20000 used by tests; grows dynamically as needed
    memory.resize(131072, 0);
}

void DataMemory::execute(uint32_t address, uint32_t writeData, bool memWrite, bool memRead, uint32_t& readData, bool fullWord) {
    // Safeguard against out-of-range access by growing memory when writing,
    // and treating out-of-range reads as zero (like reading unmapped memory does here).
    size_t addr = static_cast<size_t>(address);
    if (memWrite) {
        // Always write at least 2 bytes; 4 if fullWord is true
        ensureCapacity(addr, fullWord ? 4 : 2);
        memory[addr] = writeData & 0xFF;
        memory[addr + 1] = (writeData >> 8) & 0xFF;

        if (fullWord) {
            memory[addr + 2] = (writeData >> 16) & 0xFF;
            memory[addr + 3] = (writeData >> 24) & 0xFF;
        }
    }
    if (memRead) {
        if (fullWord) {
            if (addr + 3 < memory.size()) {
                readData = memory[addr] |
                          (memory[addr + 1] << 8) |
                          (memory[addr + 2] << 16) |
                          (memory[addr + 3] << 24);
            } else {
                readData = 0;
            }
        } else {
            readData = (addr < memory.size()) ? (memory[addr] & 0xFF) : 0;
        }
    }
}

InstructionMemory::InstructionMemory(vector<uint8_t>& instMem) {
    this->memory = instMem;
    // Safely pack bytes into 32-bit instructions (little-endian)
    for (size_t i = 0; i + 3 < instMem.size(); i += 4) {
        uint32_t word = (static_cast<uint32_t>(instMem[i + 3]) << 24) |
                        (static_cast<uint32_t>(instMem[i + 2]) << 16) |
                        (static_cast<uint32_t>(instMem[i + 1]) << 8)  |
                        static_cast<uint32_t>(instMem[i]);
        instructions.push_back(word);
    }
}

uint32_t InstructionMemory::fetchInstruction(uint32_t address) {
    uint32_t index = address / 4;
    if (index >= instructions.size()) {
        return 0; // Return zero instruction if out of bounds
    }
    return instructions[index];
}

void DataMemory::update() {
    // DataMemory doesn't need to update anything in this implementation
    // This method is here to match the interface expected by CPU::update()
}