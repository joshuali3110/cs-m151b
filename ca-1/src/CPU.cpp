#include "CPU.h"

// ------------------------------------------------------------
// CPU class implementation
// ------------------------------------------------------------

CPU::CPU(uint32_tmaxPC, vector<uint8_t>& instMem)
{
	PC = 0; //set PC to 0
	nextPC = 0;
	this->maxPC = maxPC;
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
	registerFile = RegisterFile();
	alu = ALU();
	aluControl = ALUControl();
	mux = Mux();
	dataMemory = DataMemory();
	controller = Controller();
	instructionMemory = InstructionMemory(instMem);
}


uint32_t CPU::readPC()
{
	return PC;
}

void CPU::incPC()
{
	nextPC = PC + 4;
}

void CPU::update()
{
	PC = nextPC;
	registerFile.update();
	dataMemory.update();
}

void CPU::setPC(uint32_t pc) { nextPC = pc; }

Instruction::Instruction(uint32_t instruction) {
	this->instruction = instruction;
	
	// Extract register fields from instruction
	// rd: bits 7-11 (5 bits)
	this->rd = (instruction >> 7) & 0x1F;
	
	// rs1: bits 15-19 (5 bits)
	this->rs1 = (instruction >> 15) & 0x1F;
	
	// rs2: bits 20-24 (5 bits)
	this->rs2 = (instruction >> 20) & 0x1F;

	generateImmediate();
}

void Instruction::generateImmediate() {
	// Extract opcode to determine instruction format
	uint8_t opcode = instruction & 0x7F;
	
	// Initialize immediate to 0
	immediate = 0;
	
	switch (opcode) {
		case 0x13: // I-type arithmetic/logical (addi, ori, sltiu)
		case 0x03: // Load instructions (lbu, lw)
		case 0x67: // JALR
			// I-type: immediate is in bits [31:20] (12 bits)
			immediate = (instruction >> 20) & 0xFFF;
			// Sign extend the 12-bit immediate to 32 bits
			if (immediate & 0x800) { // Check if bit 11 is set (negative)
				immediate |= 0xFFFFF000; // Sign extend with 1s
			}
			break;
			
		case 0x37: // U-type (lui)
			// U-type: immediate is in bits [31:12] (20 bits)
			immediate = (instruction >> 12) & 0xFFFFF;
			// Shift left by 12 bits to get the upper 20 bits
			immediate <<= 12;
			break;
			
		case 0x23: // S-type (sw, sh)
			// S-type: immediate is split across bits [31:25] and [11:7]
			// imm[11:5] in bits [31:25], imm[4:0] in bits [11:7]
			uint32_t imm_high = (instruction >> 25) & 0x7F;  // bits [31:25]
			uint32_t imm_low = (instruction >> 7) & 0x1F;   // bits [11:7]
			immediate = (imm_high << 5) | imm_low;
			// Sign extend the 12-bit immediate to 32 bits
			if (immediate & 0x800) { // Check if bit 11 is set (negative)
				immediate |= 0xFFFFF000; // Sign extend with 1s
			}
			break;
			
		case 0x63: // B-type (bne)
			// B-type: immediate is split across bits [31], [30:25], [11:8], [7]
			// imm[12] in bit [31], imm[10:5] in bits [30:25], imm[4:1] in bits [11:8], imm[11] in bit [7]
			uint32_t imm_12 = (instruction >> 31) & 0x1;     // bit [31]
			uint32_t imm_10_5 = (instruction >> 25) & 0x3F;  // bits [30:25]
			uint32_t imm_4_1 = (instruction >> 8) & 0xF;    // bits [11:8]
			uint32_t imm_11 = (instruction >> 7) & 0x1;      // bit [7]
			
			// Reconstruct the 13-bit immediate (bit 0 is always 0 for branches)
			immediate = (imm_12 << 12) | (imm_11 << 11) | (imm_10_5 << 5) | (imm_4_1 << 1);
			// Sign extend the 13-bit immediate to 32 bits
			if (immediate & 0x1000) { // Check if bit 12 is set (negative)
				immediate |= 0xFFFFE000; // Sign extend with 1s
			}
			break;
			
			
		default:
			// R-type instructions (sub, and, sra) have no immediate
			immediate = 0;
			break;
	}
}

