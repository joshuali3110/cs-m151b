#include "CPU.h"
#include "register_file.h"
#include "alu.h"

// ------------------------------------------------------------
// CPU class implementation
// ------------------------------------------------------------

CPU::CPU()
{
	PC = 0; //set PC to 0
	nextPC = 0;
	for (int i = 0; i < 4096; i++) //copy instrMEM
	{
		dmemory[i] = (0);
	}

	registerFile = RegisterFile();
	alu = ALU();
	aluControl = ALUControl();
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

