#include <iostream>
#include <bitset>
#include <stdio.h>
#include<stdlib.h>
#include <string>
using namespace std;

class RegisterFile {
private:
	uint32_t registers[32];
	uint32_t next_registers[32];
public:
	RegisterFile();
	void execute(uint8_t rs1, uint8_t rs2, uint32_t& rs1Data, uint32_t& rs2Data, uint8_t rd, uint32_t writeData, bool regWrite);
	void update();
}