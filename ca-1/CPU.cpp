#include "CPU.h"

// ------------------------------------------------------------
// CPU class implementation
// ------------------------------------------------------------

CPU::CPU()
{
	PC = 0; //set PC to 0
	for (int i = 0; i < 4096; i++) //copy instrMEM
	{
		dmemory[i] = (0);
	}
}


unsigned long CPU::readPC()
{
	return PC;
}
void CPU::incPC()
{
	PC++;
}

// ------------------------------------------------------------
// RegisterFile class implementation
// ------------------------------------------------------------

RegisterFile::RegisterFile()
{
	for (int i = 0; i < 32; i++)
	{
		registers[i] = 0;
	}
}

void RegisterFile::execute(uint8_t rs1, uint8_t rs2, uint32_t& rs1Data, uint32_t& rs2Data, uint8_t rd, uint32_t writeData, bool regWrite) {
	if (regWrite && rd != 0) {
		registers[rd] = writeData;
	}
	rs1Data = registers[rs1];
	rs2Data = registers[rs2];
}