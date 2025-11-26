#include "procsim.hpp"
#include <algorithm>
#include <deque>
#include <tuple>
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
struct ResultBusEntry {
    uint64_t tag;
    int32_t dest_reg;
};
std::deque<ResultBusEntry> result_buses;  // Stores instruction tags and dest_regs waiting to broadcast (tag order)

// Global register file state - track ready bits for each register (0-127)
bool reg_ready[128];  // true if register value is ready

// Global processor state
uint64_t current_cycle;          // Current simulation cycle
uint64_t next_tag;               // Next instruction tag to assign (starts at 1)
bool trace_done;                 // Flag indicating all instructions fetched
uint64_t instructions_fetched;   // Total instructions fetched
uint64_t instructions_retired;   // Total instructions retired

// Statistics tracking per cycle
uint64_t inst_fired_this_cycle;      // Number of instructions fired this cycle
uint64_t inst_retired_this_cycle;    // Number of instructions retired this cycle
uint64_t total_inst_fired;           // Total instructions fired across all cycles
uint64_t total_disp_size_sum;        // Sum of dispatch queue sizes for averaging

// Forward declarations for stage functions
void fetch_stage();
void dispatch_stage();
void schedule_stage();
void execute_stage();
void state_update_stage();
void update_stats(proc_stats_t* p_stats);
bool all_instructions_retired();

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
 * Helper function to check if all instructions have been retired
 * @return true if all instructions are retired, false otherwise
 */
bool all_instructions_retired()
{
    // All instructions are retired when:
    // 1. Trace is done (all instructions fetched)
    // 2. Dispatch queue is empty
    // 3. Reservation station is empty
    // 4. All function units are free
    // 5. Result buses are empty
    
    if (!trace_done || !dispatch_queue.empty() || !reservation_station.empty()) {
        return false;
    }
    
    // Check if all FUs are free
    for (size_t i = 0; i < fu_type0.size(); i++) {
        if (fu_type0[i].busy) return false;
    }
    for (size_t i = 0; i < fu_type1.size(); i++) {
        if (fu_type1[i].busy) return false;
    }
    for (size_t i = 0; i < fu_type2.size(); i++) {
        if (fu_type2[i].busy) return false;
    }
    
    // Check if result buses are empty
    if (!result_buses.empty()) {
        return false;
    }
    
    return true;
}

/**
 * Update statistics for the current cycle
 * @p_stats Pointer to the statistics structure
 */
void update_stats(proc_stats_t* p_stats)
{
    // Track per-cycle statistics
    total_inst_fired += inst_fired_this_cycle;
    total_disp_size_sum += dispatch_queue.size();
    
    // Update max dispatch queue size
    if (dispatch_queue.size() > p_stats->max_disp_size) {
        p_stats->max_disp_size = dispatch_queue.size();
    }
    
    // Reset per-cycle counters for next cycle
    inst_fired_this_cycle = 0;
    inst_retired_this_cycle = 0;
}

/**
 * Fetch stage: Read new instructions from trace
 */
void fetch_stage()
{
    // If trace is done, no more instructions to fetch
    if (trace_done) {
        return;
    }
    
    // Fetch up to F instructions
    for (uint64_t i = 0; i < F; i++) {
        proc_inst_t inst;
        
        // Read instruction from trace
        if (!read_instruction(&inst)) {
            // No more instructions available
            trace_done = true;
            break;
        }
        
        // Assign sequential tag
        inst.tag = next_tag++;
        
        // Set fetch cycle
        inst.fetch_cycle = current_cycle;
        
        // Set dispatch cycle (instruction enters dispatch when added to dispatch queue)
        inst.dispatch_cycle = current_cycle;
        
        // Initialize tracking fields
        inst.schedule_cycle = 0;
        inst.execute_cycle = 0;
        inst.state_update_cycle = 0;
        inst.completed_cycle = 0;
        inst.fu_id = -1;
        inst.ready_to_fire = false;
        inst.fired = false;
        inst.completed = false;
        inst.retired = false;
        
        // Handle op_code == -1 → set fu_type = 1
        if (inst.op_code == -1) {
            inst.fu_type = 1;
        } else {
            // fu_type is determined by op_code (0, 1, or 2)
            inst.fu_type = inst.op_code;
        }
        
        // Add to dispatch queue
        dispatch_queue.push_back(inst);
        instructions_fetched++;
    }
}

/**
 * Dispatch stage: Move instructions from dispatch queue to reservation station
 */
void dispatch_stage()
{
    // Process instructions from head to tail (program order)
    // Continue until RS is full or dispatch queue is empty
    while (!dispatch_queue.empty() && reservation_station.size() < RS_SIZE) {
        // Get instruction from front of dispatch queue (head)
        proc_inst_t inst = dispatch_queue.front();
        dispatch_queue.pop_front();
        
        // dispatch_cycle was already set in fetch_stage when instruction entered dispatch queue
        // Set schedule cycle (instruction enters RS/schedule in this cycle)
        inst.schedule_cycle = current_cycle;
        
        // Mark destination register as not ready (until instruction completes and broadcasts result)
        if (inst.dest_reg >= 0 && inst.dest_reg < 128) {
            reg_ready[inst.dest_reg] = false;
        }
        
        // Move instruction to reservation station
        reservation_station.push_back(inst);
        
        // Instruction is now in RS (no explicit marking needed, it's in the vector)
    }
    
    // If RS is full, remaining instructions stay in dispatch queue
    // (handled by the while loop condition)
}

/**
 * Schedule stage: Update ready bits for instructions in reservation station
 */
void schedule_stage()
{
    // Update ready bits for all instructions in RS
    for (size_t i = 0; i < reservation_station.size(); i++) {
        proc_inst_t& inst = reservation_station[i];
        
        // Skip if instruction is already fired (no need to update ready bits)
        if (inst.fired) {
            continue;
        }
        
        // Check if src_reg[0] is ready
        bool src0_ready = (inst.src_reg[0] == -1) || 
                         (inst.src_reg[0] >= 0 && inst.src_reg[0] < 128 && reg_ready[inst.src_reg[0]]);
        
        // Check if src_reg[1] is ready
        bool src1_ready = (inst.src_reg[1] == -1) || 
                         (inst.src_reg[1] >= 0 && inst.src_reg[1] < 128 && reg_ready[inst.src_reg[1]]);
        
        // Instruction is ready to fire if both source registers are ready
        inst.ready_to_fire = src0_ready && src1_ready;
    }
}

/**
 * Execute stage: Fire ready instructions and complete executing instructions
 */
void execute_stage()
{
    // According to half-cycle behavior and spec, operations should happen in this order:
    // 1. Broadcast results from previous cycle's completions (updates ready bits, frees FUs)
    // 2. Fire instructions (based on updated ready bits)
    // 3. Complete instructions (decrement cycles, add to result_buses for next cycle)
    
    // C. Broadcast Results (First Half Cycle) - MUST happen first
    // Process up to R instructions from result buses (instructions that completed in previous cycles)
    // These broadcasts happen "at the beginning of the next cycle" per spec
    uint64_t broadcasts_this_cycle = 0;
    
    // Process from front of deque (lowest tags first, since they're in tag order)
    while (!result_buses.empty() && broadcasts_this_cycle < R) {
        ResultBusEntry entry = result_buses.front();
        uint64_t tag = entry.tag;
        int32_t dest_reg = entry.dest_reg;
        
        // Broadcast result to RS (update ready bits for instructions waiting on dest_reg)
        // Note: We can update register ready bits even if instruction is no longer in RS
        // (instruction may have been retired before result was broadcast)
        if (dest_reg >= 0 && dest_reg < 128) {
            // Update register file ready bits
            // The schedule stage will check these bits to update ready_to_fire for waiting instructions
            reg_ready[dest_reg] = true;
        }
        
        // Note: FU was already freed when instruction completed
        // (FUs are freed immediately upon completion, not when result is broadcast)
        
        // Remove from result bus queue
        result_buses.pop_front();
        broadcasts_this_cycle++;
    }
    
    // B. Fire Instructions (First Half Cycle) - After broadcasts, so ready bits are updated
    // Collect ready instructions that haven't been fired yet
    std::vector<std::pair<uint64_t, size_t>> ready_instructions;  // (tag, index in RS)
    
    for (size_t i = 0; i < reservation_station.size(); i++) {
        proc_inst_t& inst = reservation_station[i];
        if (inst.ready_to_fire && !inst.fired) {
            ready_instructions.push_back({inst.tag, i});
        }
    }
    
    // Sort ready instructions by tag (lowest tag first)
    std::sort(ready_instructions.begin(), ready_instructions.end());
    
    // For each ready instruction (in tag order)
    for (auto& pair : ready_instructions) {
        size_t idx = pair.second;
        proc_inst_t& inst = reservation_station[idx];
        
        // Find an available FU of the appropriate type
        FU* allocated_fu = nullptr;
        int fu_id = -1;
        
        if (inst.fu_type == 0) {
            // Look for available type 0 FU
            for (size_t i = 0; i < fu_type0.size(); i++) {
                if (!fu_type0[i].busy) {
                    allocated_fu = &fu_type0[i];
                    fu_id = i;
                    break;
                }
            }
        } else if (inst.fu_type == 1) {
            // Look for available type 1 FU
            for (size_t i = 0; i < fu_type1.size(); i++) {
                if (!fu_type1[i].busy) {
                    allocated_fu = &fu_type1[i];
                    fu_id = i;
                    break;
                }
            }
        } else if (inst.fu_type == 2) {
            // Look for available type 2 FU
            for (size_t i = 0; i < fu_type2.size(); i++) {
                if (!fu_type2[i].busy) {
                    allocated_fu = &fu_type2[i];
                    fu_id = i;
                    break;
                }
            }
        }
        
        // If FU is available, allocate it and fire the instruction
        if (allocated_fu != nullptr) {
            // Allocate FU
            allocated_fu->busy = true;
            allocated_fu->executing_tag = inst.tag;
            allocated_fu->cycles_remaining = 1;  // Latency = 1 cycle
            
            // Update instruction
            inst.fired = true;
            inst.execute_cycle = current_cycle;
            inst.fu_id = fu_id;
            
            // Update statistics
            inst_fired_this_cycle++;
        }
    }
    
    // A. Complete Instructions (First Half Cycle) - After broadcasts and firing
    // Collect completed instruction tags to add in tag order for next cycle's broadcast
    std::vector<uint64_t> completed_tags;
    
    // For each instruction in RS that has fired = true
    for (size_t i = 0; i < reservation_station.size(); i++) {
        proc_inst_t& inst = reservation_station[i];
        
        if (!inst.fired || inst.completed) {
            continue;  // Skip if not fired or already completed
        }
        
        // CRITICAL: Skip instructions that were just fired in this cycle
        // Instructions fired in cycle N should complete in cycle N+1 (latency = 1)
        // This ensures they execute for at least 1 cycle before completing
        if (inst.execute_cycle == current_cycle) {
            continue;  // Instruction was just fired, wait until next cycle to decrement
        }
        
        // Get the FU this instruction is using
        FU* fu = nullptr;
        if (inst.fu_type == 0 && inst.fu_id >= 0 && inst.fu_id < (int)fu_type0.size()) {
            fu = &fu_type0[inst.fu_id];
        } else if (inst.fu_type == 1 && inst.fu_id >= 0 && inst.fu_id < (int)fu_type1.size()) {
            fu = &fu_type1[inst.fu_id];
        } else if (inst.fu_type == 2 && inst.fu_id >= 0 && inst.fu_id < (int)fu_type2.size()) {
            fu = &fu_type2[inst.fu_id];
        }
        
        if (fu == nullptr) {
            continue;  // Invalid FU reference
        }
        
        // Decrement execution cycles
        if (fu->cycles_remaining > 0) {
            fu->cycles_remaining--;
        }
        
        // If cycles reach 0, instruction is completed
        if (fu->cycles_remaining == 0) {
            inst.completed = true;
            inst.completed_cycle = current_cycle;
            completed_tags.push_back(inst.tag);
            
            // Free the FU immediately when instruction completes
            // (FU should be free even if result hasn't been broadcast yet due to result bus limitations)
            fu->busy = false;
            fu->executing_tag = 0;
            fu->cycles_remaining = 0;
        }
    }
    
    // Add completed instructions to result bus queue in tag order
    // These will be broadcast at the beginning of the next cycle
    std::sort(completed_tags.begin(), completed_tags.end());
    for (uint64_t tag : completed_tags) {
        // Find the instruction to get its dest_reg
        proc_inst_t* inst = nullptr;
        for (size_t i = 0; i < reservation_station.size(); i++) {
            if (reservation_station[i].tag == tag && reservation_station[i].completed) {
                inst = &reservation_station[i];
                break;
            }
        }
        
        // Store tag and dest_reg (even if instruction is later retired before broadcast)
        ResultBusEntry entry;
        entry.tag = tag;
        entry.dest_reg = (inst != nullptr) ? inst->dest_reg : -1;
        result_buses.push_back(entry);
    }
}

/**
 * State Update stage: Retire completed instructions
 */
void state_update_stage()
{
    // Find instructions ready to retire (completed = true and not yet retired)
    std::vector<std::tuple<uint64_t, uint64_t, size_t>> ready_to_retire;  
    // (completed_cycle, tag, index in RS)
    
    for (size_t i = 0; i < reservation_station.size(); i++) {
        proc_inst_t& inst = reservation_station[i];
        if (inst.completed && !inst.retired) {
            ready_to_retire.push_back({inst.completed_cycle, inst.tag, i});
        }
    }
    
    // Sort by: oldest first (by completed_cycle), then by tag
    std::sort(ready_to_retire.begin(), ready_to_retire.end());
    
    // Collect indices to remove (in reverse order to avoid index shifting issues)
    std::vector<size_t> indices_to_remove;
    
    // For each instruction (in order)
    for (auto& tuple : ready_to_retire) {
        size_t idx = std::get<2>(tuple);
        proc_inst_t& inst = reservation_station[idx];
        
        // Set retired = true
        inst.retired = true;
        
        // Set state_update_cycle = current_cycle
        inst.state_update_cycle = current_cycle;
        
        // Mark for removal from RS
        indices_to_remove.push_back(idx);
        
        // Increment instructions_retired
        instructions_retired++;
        inst_retired_this_cycle++;
    }
    
    // Remove from RS (in second half cycle)
    // Remove in reverse order to avoid index shifting issues
    std::sort(indices_to_remove.rbegin(), indices_to_remove.rend());
    for (size_t idx : indices_to_remove) {
        reservation_station.erase(reservation_station.begin() + idx);
    }
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
    // Initialize per-cycle statistics
    inst_fired_this_cycle = 0;
    inst_retired_this_cycle = 0;
    total_inst_fired = 0;
    total_disp_size_sum = 0;
    
    // Main simulation loop
    while (!all_instructions_retired()) {
        current_cycle++;
        
        // Safety check: prevent infinite loops
        if (current_cycle > 1000000) {
            fprintf(stderr, "ERROR: Simulation exceeded 1M cycles. Possible infinite loop!\n");
            fprintf(stderr, "  trace_done: %d\n", trace_done);
            fprintf(stderr, "  dispatch_queue.size(): %zu\n", dispatch_queue.size());
            fprintf(stderr, "  reservation_station.size(): %zu\n", reservation_station.size());
            fprintf(stderr, "  result_buses.size(): %zu\n", result_buses.size());
            fprintf(stderr, "  instructions_fetched: %lu\n", instructions_fetched);
            fprintf(stderr, "  instructions_retired: %lu\n", instructions_retired);
            exit(1);
        }
        
        // Execute stages in REVERSE ORDER (as per spec)
        // 1. State Update
        state_update_stage();
        
        // 2. Execute (completion and firing)
        execute_stage();
        
        // 3. Schedule (move from dispatch to RS)
        schedule_stage();
        
        // 4. Dispatch (move from fetch to dispatch)
        dispatch_stage();
        
        // 5. Fetch (read new instructions)
        fetch_stage();
        
        // Update statistics
        update_stats(p_stats);
    }
    
    // Set final cycle count
    p_stats->cycle_count = current_cycle;
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
    // Calculate final statistics
    if (p_stats->cycle_count > 0) {
        // Average instructions fired per cycle
        p_stats->avg_inst_fired = (float)total_inst_fired / (float)p_stats->cycle_count;
        
        // Average instructions retired per cycle (IPC)
        p_stats->avg_inst_retired = (float)instructions_retired / (float)p_stats->cycle_count;
        
        // Average dispatch queue size
        p_stats->avg_disp_size = (float)total_disp_size_sum / (float)p_stats->cycle_count;
    } else {
        // Handle edge case where cycle_count is 0
        p_stats->avg_inst_fired = 0.0f;
        p_stats->avg_inst_retired = 0.0f;
        p_stats->avg_disp_size = 0.0f;
    }
    
    // Set total instructions retired
    p_stats->retired_instruction = instructions_retired;
    
    // cycle_count was already set in run_proc()
}
