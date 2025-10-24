#include <iostream>
#include <bitset>
#include <stdio.h>
#include<stdlib.h>
#include <string>
using namespace std;

// class instruction { // optional
// public:
// 	bitset<32> instr;//instruction
// 	instruction(bitset<32> fetch); // constructor

// };

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
	CPU();
	unsigned long readPC(maxPC);
	void incPC();
	void update();
	void getPC();
	void setPC(unsigned long pc);
};

