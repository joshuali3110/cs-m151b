#include "alu.h"

ALU::ALU() {}

void ALU::execute(uint32_t op1, uint32_t op2, uint8_t opcode, uint32_t& result) {
    switch (opcode) {
        case 0b111: // ADD: addi, lui, lbu, lw, sh, sw, jalr
            result = op1 + op2;
            break;
        case 0b110: // SUB
            result = op1 - op2;
            break;
        case 0b101: // AND
            result = op1 & op2;
            break;
        case 0b100: // OR
            result = op1 | op2;
            break;
        case 0b011: // SLTU
            result = (uint32_t)op1 < (uint32_t)op2;
            break;
        case 0b010: // SRA
            result = (int32_t)op1 >> op2;
            break;
        case 0b000:
            cerr << "No-op" << endl;
            break;
        default:
            cerr << "Invalid opcode" << endl;
            break;
    }
}

ALUControl::ALUControl() {}

uint8_t ALUControl::getALUControl(uint8_t funct7, uint8_t funct3, bool offset, bool bne) {
    if(offset) {
        return 0b111;
    }
    if(bne) {
        return 0b110;
    }
    
    switch (funct3) {
        case 0b000: // addi
            return 0b111;
        case 0b110: // ori
            return 0b100;
        case 0b011: // sltiu
            return 0b011;
        case 0b101: // sra
            return 0b010;
        case 0b111: // and
            return 0b101;
        default:
            cerr << "Invalid funct3" << endl;
            break;
    }

    return 0b000;
}