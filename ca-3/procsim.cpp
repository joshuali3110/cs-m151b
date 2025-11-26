#include "procsim.hpp"
#include <deque>
#include <vector>

// Function Unit structure
struct FU {
    bool busy;
    uint64_t executing_tag;      // Tag of instruction using this FU
    int cycles_remaining;        // For latency tracking
};

// Global processor configuration parameters
uint64_t R;              // Number of result buses
uint64_t k0;             // Number of k0 function units
uint64_t k1;             // Number of k1 function units
uint64_t k2;             // Number of k2 function units
uint64_t F;              // Fetch width (instructions per cycle)
uint64_t RS_SIZE;        // Reservation station size = 2 * (k0 + k1 + k2)

// Global dispatch queue
std::deque<proc_inst_t> dispatch_queue;

// Global reservation station (RS)
std::vector<proc_inst_t> reservation_station;

// Global function units
std::vector<FU> fu_type0;  // k0 function units
std::vector<FU> fu_type1;  // k1 function units
std::vector<FU> fu_type2;  // k2 function units

// Global result buses (CDBs) - track instructions waiting to broadcast results
std::deque<uint64_t> result_buses;  // Stores instruction tags waiting to broadcast (tag order)

// Global register file state - track ready bits for each register (0-127)
bool reg_ready[128];  // true if register value is ready

// Global processor state
uint64_t current_cycle;          // Current simulation cycle
uint64_t next_tag;               // Next instruction tag to assign (starts at 1)
bool trace_done;                 // Flag indicating all instructions fetched
uint64_t instructions_fetched;   // Total instructions fetched
uint64_t instructions_retired;   // Total instructions retired

/**
 * Subroutine for initializing the processor. You many add and initialize any global or heap
 * variables as needed.
 * XXX: You're responsible for completing this routine
 *
 * @r number of result busses
 * @k0 Number of k0 FUs
 * @k1 Number of k1 FUs
 * @k2 Number of k2 FUs
 * @f Number of instructions to fetch
 */
void setup_proc(uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f) 
{
    // Store configuration parameters
    ::R = r;
    ::k0 = k0;
    ::k1 = k1;
    ::k2 = k2;
    ::F = f;
    
    // Calculate reservation station size
    RS_SIZE = 2 * (::k0 + ::k1 + ::k2);
    
    // Initialize dispatch queue (empty, unlimited size)
    dispatch_queue.clear();
    
    // Initialize reservation station (empty, fixed size = RS_SIZE)
    reservation_station.clear();
    reservation_station.reserve(RS_SIZE);
    
    // Initialize function units (all busy = false)
    fu_type0.resize(::k0);
    for (size_t i = 0; i < ::k0; i++) {
        fu_type0[i].busy = false;
        fu_type0[i].executing_tag = 0;
        fu_type0[i].cycles_remaining = 0;
    }
    
    fu_type1.resize(::k1);
    for (size_t i = 0; i < ::k1; i++) {
        fu_type1[i].busy = false;
        fu_type1[i].executing_tag = 0;
        fu_type1[i].cycles_remaining = 0;
    }
    
    fu_type2.resize(::k2);
    for (size_t i = 0; i < ::k2; i++) {
        fu_type2[i].busy = false;
        fu_type2[i].executing_tag = 0;
        fu_type2[i].cycles_remaining = 0;
    }
    
    // Initialize result buses (empty, can hold up to R instructions per cycle)
    result_buses.clear();
    
    // Initialize register file (all registers start as ready)
    for (int i = 0; i < 128; i++) {
        reg_ready[i] = true;
    }
    
    // Initialize global state
    current_cycle = 0;
    next_tag = 1;
    trace_done = false;
    instructions_fetched = 0;
    instructions_retired = 0;
}

/**
 * Subroutine that simulates the processor.
 *   The processor should fetch instructions as appropriate, until all instructions have executed
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void run_proc(proc_stats_t* p_stats)
{

}

/**
 * Subroutine for cleaning up any outstanding instructions and calculating overall statistics
 * such as average IPC, average fire rate etc.
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void complete_proc(proc_stats_t *p_stats) 
{

}
