//
//  Scheduler.hpp
//  CloudSim
//
//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
//

#ifndef Scheduler_hpp
#define Scheduler_hpp

#include <vector>
#include <map>
#include <algorithm>

#include "Interfaces.h"

/**
 * Class that implements the greedy allocation algorithm for cloud scheduling
 * 
 * This scheduler optimizes cloud resource allocation with the following strategies:
 * 1. Allocates tasks to machines based on current utilization and CPU compatibility
 * 2. Migrates tasks from less utilized to more utilized machines to consolidate workloads
 * 3. Powers off unused machines to save energy
 * 4. Handles SLA violations by migrating tasks to less loaded machines
 * 5. Responds to thermal events by adjusting machine workloads
 * 
 * The greedy allocation algorithm prioritizes energy efficiency while maintaining
 * performance by keeping fewer machines active with higher utilization.
 */
class Scheduler {
public:
    /**
     * Default constructor
     */
    Scheduler()                 {}
    
    /**
     * Initialize the scheduler
     * 
     * Discovers all available machines, creates VMs with appropriate CPU types,
     * and initializes tracking data structures.
     */
    void Init();
    
    /**
     * Handle VM migration completion
     * 
     * Updates tracking data structures after a VM has been migrated.
     * 
     * @param time Current simulation time
     * @param vm_id ID of the VM that completed migration
     */
    void MigrationComplete(Time_t time, VMId_t vm_id);
    
    /**
     * Handle new task arrival
     * 
     * Allocates a new task to an appropriate machine based on
     * utilization and CPU compatibility.
     * 
     * @param now Current simulation time
     * @param task_id ID of the newly arrived task
     */
    void NewTask(Time_t now, TaskId_t task_id);
    
    /**
     * Perform periodic maintenance
     * 
     * Updates utilization tracking, checks for potential optimizations,
     * and turns off unused machines.
     * 
     * @param now Current simulation time
     */
    void PeriodicCheck(Time_t now);
    
    /**
     * Perform final cleanup and reporting
     * 
     * Shuts down all VMs and reports final statistics.
     * 
     * @param now Final simulation time
     */
    void Shutdown(Time_t now);
    
    /**
     * Handle task completion
     * 
     * Updates utilization tracking and potentially migrates tasks
     * for workload consolidation.
     * 
     * @param now Current simulation time
     * @param task_id ID of the completed task
     */
    void TaskComplete(Time_t now, TaskId_t task_id);
    
    /**
     * Handle SLA violation
     * 
     * Attempts to migrate a task to a less loaded machine to
     * improve performance and meet SLA requirements.
     * 
     * @param taskId ID of the task violating its SLA
     * @param machineId ID of the machine running the task, or -1 if unknown
     */
    void HandleSLAViolation(TaskId_t taskId, MachineId_t machineId);
    
    /**
     * Handle thermal event
     * 
     * Responds to a machine overheating by migrating tasks away.
     * 
     * @param machineId ID of the machine with thermal issues
     */
    void HandleThermalEvent(MachineId_t machineId);
    
    /**
     * Calculate machine utilization
     * 
     * Computes the utilization of a machine based on memory usage.
     * 
     * @param machineId ID of the machine to calculate utilization for
     * @return Utilization as a float between 0.0 and 1.0
     */
    float CalculateMachineUtilization(MachineId_t machineId);
private:
    /**
     * List of all virtual machines created by the scheduler
     */
    vector<VMId_t> vms;
    
    /**
     * List of all physical machines managed by the scheduler
     */
    vector<MachineId_t> machines;
    
    /**
     * Maps machines to their current utilization level
     * Utilization is calculated as memory_used/memory_size
     */
    map<MachineId_t, float> machineUtilization;
    
    /**
     * Maps machines to their associated VMs
     * Each machine has one primary VM in this implementation
     */
    map<MachineId_t, VMId_t> machineToVMMap;
    
    /**
     * Maps tasks to the machines they're running on
     * Used for quick lookups during migration and SLA handling
     */
    map<TaskId_t, MachineId_t> taskToMachineMap;
    
    // Helper methods
    
    /**
     * Calculate the load factor of a task
     * 
     * Computes the relative load a task places on a machine
     * based on its memory requirements.
     * 
     * @param taskId ID of the task to calculate load for
     * @return Load factor as a float between 0.0 and 1.0
     */
    float CalculateTaskLoadFactor(TaskId_t taskId);
    
    /**
     * Find or create a VM for a specific machine
     * 
     * Ensures CPU compatibility between the VM and tasks.
     * 
     * @param machineId ID of the machine to find/create VM for
     * @param vmType Type of VM to create if needed
     * @param cpuType CPU architecture required
     * @return ID of the VM
     */
    VMId_t FindVMForMachine(MachineId_t machineId, VMType_t vmType = LINUX, CPUType_t cpuType = X86);
    
    /**
     * Power off machines with no active tasks
     * 
     * Key energy efficiency feature that reduces power consumption
     * by turning off idle machines.
     */
    void CheckAndTurnOffUnusedMachines();
    
    /**
     * Get machines sorted by their utilization
     * 
     * Used for workload consolidation and task migration decisions.
     * 
     * @return Vector of machine IDs paired with their utilization values,
     *         sorted from lowest to highest utilization
     */
    vector<pair<MachineId_t, float>> GetSortedMachinesByUtilization();
    
    /**
     * Migrate a task from its current machine to a target machine
     * 
     * Core mechanism for workload consolidation and SLA management.
     * 
     * @param taskId ID of the task to migrate
     * @param targetMachineId ID of the destination machine
     * @return True if migration was successful, false otherwise
     */
    bool MigrateTask(TaskId_t taskId, MachineId_t targetMachineId);
};



#endif /* Scheduler_hpp */
