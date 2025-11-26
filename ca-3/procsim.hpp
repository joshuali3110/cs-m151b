#ifndef PROCSIM_HPP
#define PROCSIM_HPP

#include <cstdint>
#include <cstdio>

#define DEFAULT_K0 1
#define DEFAULT_K1 2
#define DEFAULT_K2 3
#define DEFAULT_R 8
#define DEFAULT_F 4

typedef struct _proc_inst_t
{
    uint32_t instruction_address;
    int32_t op_code;
    int32_t src_reg[2];
    int32_t dest_reg;
    
    // Instruction tracking fields
    uint64_t tag;                    // Sequential instruction tag (1, 2, 3, ...)
    uint64_t fetch_cycle;            // Cycle when instruction entered fetch
    uint64_t dispatch_cycle;         // Cycle when instruction entered dispatch
    uint64_t schedule_cycle;         // Cycle when instruction entered schedule/RS
    uint64_t execute_cycle;          // Cycle when instruction started execution
    uint64_t state_update_cycle;     // Cycle when instruction entered state update
    uint64_t completed_cycle;        // Cycle when instruction completed execution
    int32_t fu_type;                 // Function unit type (0, 1, 2, or -1 → use type 1)
    int32_t fu_id;                   // Which specific FU is executing this instruction
    bool ready_to_fire;              // Boolean indicating if dependencies are resolved
    bool fired;                       // Boolean indicating if instruction has been fired
    bool completed;                   // Boolean indicating if instruction has completed execution
    bool result_broadcast;            // Boolean indicating if result has been broadcast via result bus
    bool retired;                     // Boolean indicating if instruction has been retired
    
    // Source dependency tracking - tag of instruction that will produce each source value
    // 0 means the value is already available (no pending producer)
    uint64_t src_producer[2];
    
} proc_inst_t;

typedef struct _proc_stats_t
{
    float avg_inst_retired;
    float avg_inst_fired;
    float avg_disp_size;
    unsigned long max_disp_size;
    unsigned long retired_instruction;
    unsigned long cycle_count;
} proc_stats_t;

bool read_instruction(proc_inst_t* p_inst);

void setup_proc(uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f);
void run_proc(proc_stats_t* p_stats);
void complete_proc(proc_stats_t* p_stats);

#endif /* PROCSIM_HPP */
