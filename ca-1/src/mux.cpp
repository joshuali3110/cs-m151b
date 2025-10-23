#include "mux.h"

Mux::Mux() {}

uint32_t Mux::execute(uint32_t input1, uint32_t input2, bool select) {
    return select ? input1 : input2;
}
