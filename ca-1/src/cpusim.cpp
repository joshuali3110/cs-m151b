#include "CPU.h"

#include <iostream>
#include <bitset>
#include <stdio.h>
#include<stdlib.h>
#include <string>
#include<fstream>
#include <sstream>
using namespace std;

/*
Add all the required standard and developed libraries here
*/

/*
Put/Define any helper function/definitions you need here
*/
int main(int argc, char* argv[])
{
	/* This is the front end of your project.
	You need to first read the instructions that are stored in a file and load them into an instruction memory.
	*/

	/* Each cell should store 1 byte. You can define the memory either dynamically, or define it as a fixed size with size 4KB (i.e., 4096 lines). Each instruction is 32 bits (i.e., 4 lines, saved in little-endian mode).
	Each line in the input file is stored as an hex and is 1 byte (each four lines are one instruction). You need to read the file line by line and store it into the memory. You may need a mechanism to convert these values to bits so that you can read opcodes, operands, etc.
	*/

	vector<uint8_t> instMem;
	vector<uint32_t> instructions;


	if (argc < 2) {
		//cout << "No file name entered. Exiting...";
		return -1;
	}

	ifstream infile(argv[1]); //open the file
	if (!(infile.is_open() && infile.good())) {
		cout<<"error opening file\n";
		return 0; 
	}

	
	string line;

	while (infile >> line) {
		instMem.push_back((uint8_t)stoi(line, nullptr, 16));
	}

	uint32_t maxPC= instMem.size();

	/* Instantiate your CPU object here.  CPU class is the main class in this project that defines different components of the processor.
	CPU class also has different functions for each stage (e.g., fetching an instruction, decoding, etc.).
	*/

	CPU cpu = CPU(maxPC, instMem);  // call the approriate constructor here to initialize the processor...  
	// make sure to create a variable for PC and resets it to zero (e.g., unsigned int PC = 0); 

	/* OPTIONAL: Instantiate your Instruction object here. */
	//Instruction myInst; 
	
	while(true) {
		uint32_t dontCare = 0;
		bool dontCareBool = false;
		
		// fetch + decode part 1
		Instruction currentInstruction = Instruction(cpu.instructionMemory.fetchInstruction(cpu.readPC()));
		cpu.controller.execute(currentInstruction.instruction, cpu.regWrite, cpu.memWrite, cpu.memRead, cpu.fullWord, cpu.MemToReg, cpu.loadImm, cpu.aluSrc, cpu.jump, cpu.branch, cpu.offset, cpu.funct7, cpu.funct3, cpu.opcode);
		
		// Check for termination condition (zero opcode)
		if (cpu.opcode == 0) {
			break;
		}
		
		// decode part 2
		uint32_t rs1Data = 0;
		uint32_t rs2Data = 0;
		cpu.registerFile.execute(currentInstruction.rs1, currentInstruction.rs2, rs1Data, rs2Data, 0, 0, false);
		
		// execute
		uint32_t alu_result = 0;
		bool zero = false;
		cpu.alu.execute(rs1Data, cpu.mux.execute(currentInstruction.immediate, rs2Data, cpu.aluSrc), cpu.aluControl.execute(cpu.funct7, cpu.funct3, cpu.offset, cpu.branch, cpu.loadImm), alu_result, zero);

		// memory
		uint32_t memReadData = 0;
		cpu.dataMemory.execute(alu_result, rs2Data, cpu.memWrite, cpu.memRead, memReadData, cpu.fullWord);
		
		// write back
		uint32_t pcPlus4 = cpu.readPC() + 4;

		uint32_t bne_target = cpu.readPC() + (currentInstruction.immediate << 1);
		uint32_t jal_target = alu_result & ~1;

		uint32_t memToRegData = cpu.mux.execute(memReadData, alu_result, cpu.MemToReg);

		uint32_t rfWriteData = cpu.mux.execute(currentInstruction.immediate, cpu.mux.execute(pcPlus4, memToRegData, cpu.jump), cpu.loadImm);
		cpu.registerFile.execute(0, 0, 0, 0, currentInstruction.rd, rfWriteData, cpu.regWrite);

		bool branchAndZero = cpu.branch && zero;
		uint32_t nextPC = cpu.mux.execute(jal_target, cpu.mux.execute(bne_target, pcPlus4, branchAndZero), cpu.jump);

		cpu.setPC(nextPC);
		cpu.update();
	}
		
	// while (done == true) // processor's main loop. Each iteration is equal to one clock cycle.  
	// {
	// 	//fetch
		

	// 	// decode
		
	// 	// ... 
	// 	myCPU.incPC();
	// 	if (myCPU.readPC() > maxPC)
	// 		break;
	// }
	int a0 = cpu.registerFile.registers[10];
	int a1 = cpu.registerFile.registers[11];
	// print the results (you should replace a0 and a1 with your own variables that point to a0 and a1)
	cout << "(" << a0 << "," << a1 << ")" << endl;
	return 0;
}