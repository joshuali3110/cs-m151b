#include "procsim.hpp"
#include <algorithm>
#include <deque>
#include <set>
#include <tuple>
#include <vector>
#include <cstdio>

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

// Track tags that have broadcast their results (for WAW hazard handling)
std::set<uint64_t> tags_broadcast;  // Set of tags that have successfully broadcast

// Global register file state - track ready bits and producer tags for each register (0-127)
bool reg_ready[128];              // true if register value is ready
uint64_t reg_producer[128];       // Tag of instruction that will produce the value (0 if no pending producer)

// Global processor state
uint64_t current_cycle;          // Current simulation cycle
uint64_t next_tag;               // Next instruction tag to assign (starts at 1)
bool trace_done;                 // Flag indicating all instructions fetched
uint64_t instructions_fetched;   // Total instructions fetched
uint64_t instructions_retired;   // Total instructions retired
uint64_t rs_slots_available_this_cycle;  // RS slots available at start of cycle (before state_update frees slots)

// Store retired instructions for output
std::vector<proc_inst_t> retired_instructions;

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
    
    // Initialize broadcast tracking (empty)
    tags_broadcast.clear();
    
    // Initialize register file (all registers start as ready, no pending producers)
    for (int i = 0; i < 128; i++) {
        reg_ready[i] = true;
        reg_producer[i] = 0;  // 0 means no pending producer
    }
    
    // Initialize global state
    current_cycle = 0;
    next_tag = 1;
    trace_done = false;
    instructions_fetched = 0;
    instructions_retired = 0;
    rs_slots_available_this_cycle = RS_SIZE;  // Initially all slots available
    
    // Initialize retired instructions storage
    retired_instructions.clear();
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
        
        // Set dispatch cycle = fetch + 1
        // This is when the dispatch stage first sees the instruction, regardless of RS availability
        inst.dispatch_cycle = current_cycle + 1;
        
        // Initialize tracking fields
        inst.schedule_cycle = 0;  // Will be set when instruction enters RS
        inst.execute_cycle = 0;
        inst.state_update_cycle = 0;
        inst.completed_cycle = 0;
        inst.fu_id = -1;
        inst.ready_to_fire = false;
        inst.fired = false;
        inst.completed = false;
        inst.result_broadcast = false;
        inst.retired = false;
        inst.src_producer[0] = 0;  // Will be set in dispatch_stage
        inst.src_producer[1] = 0;  // Will be set in dispatch_stage
        
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
    // Use rs_slots_available_this_cycle which was captured at start of cycle (before state_update)
    // This implements the half-cycle behavior: dispatch reserves slots in first half,
    // state_update frees slots in second half
    uint64_t slots_remaining = rs_slots_available_this_cycle;
    
    while (!dispatch_queue.empty() && slots_remaining > 0) {
        // Get instruction from front of dispatch queue (head)
        proc_inst_t inst = dispatch_queue.front();
        dispatch_queue.pop_front();
        
        // Set schedule cycle (instruction enters RS now, so schedule stage sees it next cycle)
        inst.schedule_cycle = current_cycle + 1;
        
        // Track source producers at dispatch time
        // This captures the current producer for each source register
        // If no producer (reg_ready=true or no register), src_producer=0
        for (int s = 0; s < 2; s++) {
            if (inst.src_reg[s] >= 0 && inst.src_reg[s] < 128) {
                // Always check reg_producer, not reg_ready
                // reg_producer tracks the latest instruction that will write to this register
                // Even if reg_ready is true (due to an earlier broadcast), we should wait
                // for the latest producer if there is one
                if (reg_producer[inst.src_reg[s]] != 0) {
                    inst.src_producer[s] = reg_producer[inst.src_reg[s]];  // Wait for this producer
                } else {
                    inst.src_producer[s] = 0;  // No pending producer
                }
            } else {
                inst.src_producer[s] = 0;  // No register
            }
        }
        
        // Mark destination register as not ready and track this instruction as the producer
        // This handles WAW: subsequent instructions reading this register will wait for THIS instruction
        if (inst.dest_reg >= 0 && inst.dest_reg < 128) {
            reg_ready[inst.dest_reg] = false;
            reg_producer[inst.dest_reg] = inst.tag;  // Track latest producer
        }
        
        // Move instruction to reservation station
        reservation_station.push_back(inst);
        slots_remaining--;  // Used one slot
        
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
        // Using src_producer tracking: an operand is ready when:
        // 1. No source register (-1)
        // 2. src_producer[0] == 0 (no dependency at dispatch time, value was ready)
        // 3. src_producer[0] has broadcast (the specific instruction we're waiting for has broadcast)
        bool src0_ready = (inst.src_reg[0] == -1) ||
                         (inst.src_producer[0] == 0) ||
                         (tags_broadcast.find(inst.src_producer[0]) != tags_broadcast.end());
        
        // Check if src_reg[1] is ready
        // Same logic with src_producer tracking
        bool src1_ready = (inst.src_reg[1] == -1) ||
                         (inst.src_producer[1] == 0) ||
                         (tags_broadcast.find(inst.src_producer[1]) != tags_broadcast.end());
        
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
        
        // Mark result as broadcast for instruction in RS (if still present)
        // Note: Instruction may have been retired before result was broadcast
        for (size_t i = 0; i < reservation_station.size(); i++) {
            proc_inst_t& inst = reservation_station[i];
            if (inst.tag == tag && inst.completed && !inst.result_broadcast) {
                inst.result_broadcast = true;
                break;
            }
        }
        
        // Free the FU now that result is written to result bus
        // (per spec: "The function unit is freed only when the result is put onto a result bus")
        // Search all FUs to find the one executing this tag (instruction may have been retired)
        bool fu_freed = false;
        
        // Check type 0 FUs
        for (size_t i = 0; i < fu_type0.size() && !fu_freed; i++) {
            if (fu_type0[i].executing_tag == tag && fu_type0[i].busy) {
                fu_type0[i].busy = false;
                fu_type0[i].executing_tag = 0;
                fu_type0[i].cycles_remaining = 0;
                fu_freed = true;
            }
        }
        
        // Check type 1 FUs
        for (size_t i = 0; i < fu_type1.size() && !fu_freed; i++) {
            if (fu_type1[i].executing_tag == tag && fu_type1[i].busy) {
                fu_type1[i].busy = false;
                fu_type1[i].executing_tag = 0;
                fu_type1[i].cycles_remaining = 0;
                fu_freed = true;
            }
        }
        
        // Check type 2 FUs
        for (size_t i = 0; i < fu_type2.size() && !fu_freed; i++) {
            if (fu_type2[i].executing_tag == tag && fu_type2[i].busy) {
                fu_type2[i].busy = false;
                fu_type2[i].executing_tag = 0;
                fu_type2[i].cycles_remaining = 0;
                fu_freed = true;
            }
        }
        
        // Update register file ready bits ONLY if this instruction is the current producer
        // This handles proper RAW dependencies: a later instruction reading from this register
        // should wait for the LATEST write, not any earlier write
        if (dest_reg >= 0 && dest_reg < 128) {
            // Only set ready if this instruction is the current producer
            // If another instruction is the producer (dispatched later with same dest_reg),
            // we still set ready=true because that instruction's dependents might already be waiting
            // and they depend on THIS broadcast (they were dispatched before the later write)
            // CORRECTION: Always set ready=true on broadcast, but track with reg_producer
            // The reg_producer only affects NEW dispatches, not existing dependents
            reg_ready[dest_reg] = true;
            // If this instruction was the producer, clear it
            if (reg_producer[dest_reg] == tag) {
                reg_producer[dest_reg] = 0;
            }
        }
        
        // Track that this tag has broadcast (for WAW hazard handling)
        tags_broadcast.insert(tag);
        
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
    // Collect completed instruction entries (tag and dest_reg) to add in tag order for next cycle's broadcast
    std::vector<ResultBusEntry> completed_entries;
    
    // For each instruction in RS that has fired = true
    // Also check instructions that are completed but waiting for result buses (FU still busy)
    for (size_t i = 0; i < reservation_station.size(); i++) {
        proc_inst_t& inst = reservation_station[i];
        
        if (!inst.fired) {
            continue;  // Skip if not fired
        }
        
        // Skip if already completed (result should already be on result_buses or broadcast)
        if (inst.completed) {
            continue;
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
        
        // Verify that this FU is still executing this instruction
        // (It might have been freed and reused by another instruction)
        if (fu->executing_tag != inst.tag) {
            // FU is executing a different instruction (or was freed)
            // If the FU is not busy, it means the instruction already completed
            // Mark it as completed if it hasn't been already
            if (!fu->busy && !inst.completed) {
                inst.completed = true;
                inst.completed_cycle = current_cycle;
                
                // Capture dest_reg NOW before instruction might be retired
                ResultBusEntry entry;
                entry.tag = inst.tag;
                entry.dest_reg = inst.dest_reg;
                completed_entries.push_back(entry);
            }
            continue;
        }
        
        // With latency=1, instruction completes in the SAME cycle it fires
        // Mark as completed immediately
        inst.completed = true;
        inst.completed_cycle = current_cycle;
        
        // Capture dest_reg NOW before instruction might be retired
        ResultBusEntry entry;
        entry.tag = inst.tag;
        entry.dest_reg = inst.dest_reg;
        completed_entries.push_back(entry);
        
        // DO NOT free the FU here - it must remain busy until result is written to result bus
        // (per spec: "The function unit is freed only when the result is put onto a result bus")
        // The FU will be freed in the broadcast section when the result is actually written to the bus
    }
    
    // Add completed instructions to result bus queue in tag order
    // These will be broadcast at the beginning of the next cycle
    // Note: result_buses can grow beyond R - we broadcast up to R per cycle
    std::sort(completed_entries.begin(), completed_entries.end(), 
              [](const ResultBusEntry& a, const ResultBusEntry& b) { return a.tag < b.tag; });
    for (const ResultBusEntry& entry : completed_entries) {
        result_buses.push_back(entry);
    }
}

/**
 * State Update stage: Retire completed instructions
 */
void state_update_stage()
{
    
    // Find instructions ready to retire
    // Per spec: state update happens in second half of cycle, after result broadcast in first half
    // Since we execute in reverse order (state update before execute), state update runs first.
    // However, the half-cycle behavior means that results broadcast in execute_stage (first half)
    // make instructions eligible for state update (second half) in the SAME cycle.
    // To achieve this with reverse order, we check for instructions whose results are in result_buses
    // (about to be broadcast in this cycle's execute_stage), OR whose results were already broadcast.
    std::vector<std::tuple<uint64_t, uint64_t, size_t>> ready_to_retire;  
    // (completed_cycle, tag, index in RS)
    
    // Build set of tags that will ACTUALLY be broadcast this cycle
    // IMPORTANT: Only the first R entries (sorted by tag) will be broadcast!
    // The result_buses deque is already in tag order (lowest tags at front)
    std::set<uint64_t> tags_about_to_broadcast;
    uint64_t count = 0;
    for (const auto& entry : result_buses) {
        if (count >= R) break;  // Only first R will be broadcast
        tags_about_to_broadcast.insert(entry.tag);
        count++;
    }
    
    for (size_t i = 0; i < reservation_station.size(); i++) {
        proc_inst_t& inst = reservation_station[i];
        // Instruction is eligible for state update if:
        // 1. Result was already broadcast (result_broadcast = true), OR
        // 2. Result is about to be broadcast this cycle (in result_buses)
        // This allows state update (second half) to retire instructions whose results
        // were broadcast in execute_stage (first half) of the same cycle
        if (inst.completed && !inst.retired && 
            (inst.result_broadcast || tags_about_to_broadcast.count(inst.tag) > 0)) {
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
        
        // Store instruction for output
        retired_instructions.push_back(inst);
        
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
            
            // Debug: Check RS state
            uint64_t fired_count = 0, completed_count = 0, ready_count = 0;
            for (size_t i = 0; i < reservation_station.size(); i++) {
                if (reservation_station[i].fired) fired_count++;
                if (reservation_station[i].completed) completed_count++;
                if (reservation_station[i].ready_to_fire) ready_count++;
            }
            fprintf(stderr, "  RS: fired=%lu, completed=%lu, ready=%lu\n", fired_count, completed_count, ready_count);
            
            // Debug: Check FU state
            uint64_t busy_fu0 = 0, busy_fu1 = 0, busy_fu2 = 0;
            for (size_t i = 0; i < fu_type0.size(); i++) if (fu_type0[i].busy) busy_fu0++;
            for (size_t i = 0; i < fu_type1.size(); i++) if (fu_type1[i].busy) busy_fu1++;
            for (size_t i = 0; i < fu_type2.size(); i++) if (fu_type2[i].busy) busy_fu2++;
            fprintf(stderr, "  FUs busy: k0=%lu/%lu, k1=%lu/%lu, k2=%lu/%lu\n", 
                    busy_fu0, fu_type0.size(), busy_fu1, fu_type1.size(), busy_fu2, fu_type2.size());
            
            // Debug: Check first few instructions in RS
            fprintf(stderr, "  First 5 instructions in RS:\n");
            for (size_t i = 0; i < reservation_station.size() && i < 5; i++) {
                proc_inst_t& inst = reservation_station[i];
                fprintf(stderr, "    tag=%lu: fired=%d, completed=%d, ready=%d, src_reg=[%d,%d], dest_reg=%d\n",
                        inst.tag, inst.fired, inst.completed, inst.ready_to_fire,
                        inst.src_reg[0], inst.src_reg[1], inst.dest_reg);
                if (inst.src_reg[0] >= 0 && inst.src_reg[0] < 128) {
                    fprintf(stderr, "      src_reg[0]=%d ready=%d\n", inst.src_reg[0], reg_ready[inst.src_reg[0]]);
                }
                if (inst.src_reg[1] >= 0 && inst.src_reg[1] < 128) {
                    fprintf(stderr, "      src_reg[1]=%d ready=%d\n", inst.src_reg[1], reg_ready[inst.src_reg[1]]);
                }
            }
            
            // Debug: Check what should have written to these registers
            fprintf(stderr, "  Checking registers 17, 18, 19:\n");
            fprintf(stderr, "    reg_ready[17]=%d, reg_ready[18]=%d, reg_ready[19]=%d\n", 
                    reg_ready[17], reg_ready[18], reg_ready[19]);
            
            // Check if there are any completed instructions in RS that should have written to these
            for (size_t i = 0; i < reservation_station.size(); i++) {
                proc_inst_t& inst = reservation_station[i];
                if (inst.completed && (inst.dest_reg == 17 || inst.dest_reg == 18 || inst.dest_reg == 19)) {
                    fprintf(stderr, "    Found completed instruction tag=%lu writing to reg=%d\n", 
                            inst.tag, inst.dest_reg);
                }
            }
            
            // Debug: Check what the first few instructions that retired wrote to
            fprintf(stderr, "  First 15 instructions that should have retired (tags 1-15):\n");
            fprintf(stderr, "    (Note: Only 11 retired, so some may still be in RS or dispatch)\n");
            exit(1);
        }
        
        // Capture RS slots available at START of cycle (before state_update frees slots)
        // Per spec: "reservation station is freed in the second half cycle, so if RS is currently 
        // full and two instructions are in the state update, you can't put new instructions in the RS"
        rs_slots_available_this_cycle = (RS_SIZE > reservation_station.size()) ? 
                                        (RS_SIZE - reservation_station.size()) : 0;
        
        // Execute stages in REVERSE ORDER (as per spec)
        // Note: Even though we call stages in reverse order, the half-cycle behavior means
        // that within a cycle, first half events (broadcast in execute) happen before
        // second half events (retire in state update). We achieve this by having execute_stage
        // broadcast results from the previous cycle, which state_update can then retire.
        // 1. State Update (second half: retire instructions whose results were broadcast)
        state_update_stage();
        
        // 2. Execute (first half: completion, firing, and result broadcast)
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
 * Print debug output in the format matching .output files
 * Format: INST FETCH DISP SCHED EXEC STATE (tab-separated)
 */
void print_debug_output()
{
    // Print header
    printf("INST\tFETCH\tDISP\tSCHED\tEXEC\tSTATE\n");
    
    // Sort retired instructions by tag
    std::sort(retired_instructions.begin(), retired_instructions.end(),
              [](const proc_inst_t& a, const proc_inst_t& b) { return a.tag < b.tag; });
    
    // Print each instruction's stage entry cycles
    for (const proc_inst_t& inst : retired_instructions) {
        printf("%lu\t%lu\t%lu\t%lu\t%lu\t%lu\n",
               inst.tag,
               inst.fetch_cycle,
               inst.dispatch_cycle,
               inst.schedule_cycle,
               inst.execute_cycle,
               inst.state_update_cycle);
    }
    printf("\n");
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
    
    // Print debug output (instruction stage timing)
    // print_debug_output();
    
    // cycle_count was already set in run_proc()
}
