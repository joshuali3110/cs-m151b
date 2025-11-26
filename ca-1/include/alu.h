#include <iostream>
#include <bitset>
#include <stdio.h>
#include<stdlib.h>
#include <string>
#include <cstdint>
using namespace std;

class ALU {
public:
    ALU();
    void execute(uint32_t op1, uint32_t op2, uint8_t opcode, uint32_t& result, bool& zero);
};

class ALUControl {
public:
    ALUControl();
    uint8_t execute(uint8_t funct7, uint8_t funct3, bool offset, bool bne, bool lui);
};