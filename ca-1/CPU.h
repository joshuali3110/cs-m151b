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
	int dmemory[4096]; //data memory byte addressable in little endian fashion;
	unsigned long PC; //pc 

public:
	CPU();
	unsigned long readPC();
	void incPC();
	
};

class RegisterFile {
private:
	uint32_t registers[32];
public:
	RegisterFile();
	void execute(uint8_t rs1, uint8_t rs2, uint32_t& rs1Data, uint32_t& rs2Data, uint8_t rd, uint32_t writeData, bool regWrite);
}

// add other functions and objects here
