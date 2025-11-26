#include "controller.h"
#include <cstdint>

Controller::Controller() {}

void Controller::execute(
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
    uint8_t& opcode) {
        
    // Extract instruction fields
    opcode = (instruction >> 0) & 0x7F;        // bits [6:0]
    funct3 = (instruction >> 12) & 0x7;        // bits [14:12]
    funct7 = (instruction >> 25) & 0x7F;       // bits [31:25]
    
    // Initialize all control signals to 0
    regWrite = false;
    memWrite = false;
    memRead = false;
    fullWord = false;
    MemToReg = false;
    loadImm = false;
    aluSrc = false;
    jump = false;
    branch = false;
    offset = false;
    
    // Decode instruction based on opcode
    switch (opcode) {
        case 0x13: // I-type arithmetic/logical (0010011)
            regWrite = true;
            aluSrc = true;
            break;
            
        case 0x37: // U-type lui (0110111)
            regWrite = true;
            loadImm = true;
            break;
            
        case 0x33: // R-type (0110011)
            regWrite = true;
            break;
            
        case 0x03: // Load instructions (0000011)
            regWrite = true;
            memRead = true;
            MemToReg = true;
            aluSrc = true;
            offset = true; // Load instructions need address calculation
            // Set fullWord based on funct3
            if (funct3 == 0x2) { // lw
                fullWord = true;
            } else if (funct3 == 0x4) { // lbu
                fullWord = false;
            }
            break;
            
        case 0x23: // Store instructions (0100011)
            memWrite = true;
            aluSrc = true;
            offset = true; // Store instructions need address calculation
            // Set fullWord based on funct3
            if (funct3 == 0x2) { // sw
                fullWord = true;
            } else if (funct3 == 0x1) { // sh
                fullWord = false;
            }
            break;
            
        case 0x63: // Branch instructions (1100011)
            branch = true;
            break;
            
        case 0x67: // JALR (1100111)
            regWrite = true;
            aluSrc = true;
            jump = true;
            break;
    }
}