#include "CPU.h"
#include "register_file.h"
#include "alu.h"

// ------------------------------------------------------------
// CPU class implementation
// ------------------------------------------------------------

CPU::CPU(unsigned long maxPC, vector<uint8_t>& instMem)
{
	PC = 0; //set PC to 0
	nextPC = 0;
	this->maxPC = maxPC;
	registerFile = RegisterFile();
	alu = ALU();
	aluControl = ALUControl();
	mux = Mux();
	dataMemory = DataMemory();
	controller = Controller();
	instructionMemory = InstructionMemory(instMem);
}


unsigned long CPU::readPC()
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
}

void CPU::getPC() { return PC; }

void CPU::setPC(unsigned long pc) { nextPC = pc; }

