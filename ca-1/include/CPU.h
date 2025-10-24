#include <iostream>
#include <bitset>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include "register_file.h"
#include "alu.h"
#include "mux.h"
#include "memory.h"
#include "controller.h"
using namespace std;

class CPU {
private:
	unsigned long PC; 
	unsigned long nextPC;
	unsigned long maxPC;
	RegisterFile registerFile;
	ALU alu;
	ALUControl aluControl;
	Mux mux;
	DataMemory dataMemory;
	Controller controller;
	InstructionMemory instructionMemory;
public:
	CPU(unsigned long maxPC, vector<uint8_t>& instMem);
	unsigned long readPC();
	void incPC();
	void update();
	void setPC(unsigned long pc);
};

class Instruction {
public:
	uint32_t instruction;
	uint8_t rd;
	uint8_t rs1;
	uint8_t rs2;
	uint32_t immediate;
	Instruction(uint32_t instruction);
	void generateImmediate();
};

