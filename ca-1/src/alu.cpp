#include "alu.h"

ALU::ALU() {}

void ALU::execute(uint32_t op1, uint32_t op2, uint8_t opcode, uint32_t& result, bool& zero) {
    switch (opcode) {
        case 0b111: // ADD: addi, lbu, lw, sh, sw, jalr
            result = op1 + op2;
            break;
        case 0b110: // SUB: sub, bne
            result = op1 - op2;
            break;
        case 0b101: // AND: and
            result = op1 & op2;
            break;
        case 0b100: // OR: ori
            result = op1 | op2;
            break;
        case 0b011: // SLTU: sltiu
            result = (uint32_t)op1 < (uint32_t)op2;
            break;
        case 0b010: // SRA: sra
            result = (int32_t)op1 >> op2;
            break;
        case 0b000: // No-Op: lui
            cerr << "No-op" << endl;
            break;
        default:
            cerr << "Invalid opcode" << endl;
            break;
    }

    zero = (result == 0);
}

ALUControl::ALUControl() {}

uint8_t ALUControl::execute(uint8_t funct7, uint8_t funct3, bool offset, bool bne, bool lui) {
    if(offset) {
        return 0b111;
    }
    if(bne) {
        return 0b110;
    }

    if(lui) {
        return 0b000;
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