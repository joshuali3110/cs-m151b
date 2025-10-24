#include <iostream>
#include <bitset>
#include <stdio.h>
#include<stdlib.h>
#include <string>
using namespace std;

class Controller {
public:
    Controller();
    void execute(
        uint32_t instruction, 
        bool& regWrite, 
        bool& memWrite, 
        bool& memRead, 
        bool& fullWord, 
        bool& MemToReg, 
        bool& loadImm, 
        bool& aluSrc, 
        bool& jump, 
        bool& branch,
        bool& offset,
        uint8_t& funct7, 
        uint8_t& funct3,
        uint8_t& opcode);
};