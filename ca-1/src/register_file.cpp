#include "register_file.h"

// ------------------------------------------------------------
// RegisterFile class implementation
// ------------------------------------------------------------

RegisterFile::RegisterFile()
{
	for (int i = 0; i < 32; i++)
	{
		registers[i] = 0;
		nextRegisters[i] = 0;
	}
}

void RegisterFile::execute(uint8_t rs1, uint8_t rs2, uint32_t& rs1Data, uint32_t& rs2Data, uint8_t rd, uint32_t writeData, bool regWrite) {
	if (regWrite && rd != 0) {
		nextRegisters[rd] = writeData;
	}
	rs1Data = registers[rs1];
	rs2Data = registers[rs2];
}

void RegisterFile::update() {
	for (int i = 0; i < 32; i++) {
		registers[i] = nextRegisters[i];
	}
}