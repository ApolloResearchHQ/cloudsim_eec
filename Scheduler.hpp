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
 * Class that implements the pmapper algorithm for cloud scheduling
 * 
 * This scheduler optimizes cloud resource allocation with the following strategies:
 * 1. Sorts machines by energy consumption (lowest to highest)
 * 2. Allocates tasks to machines in order of the sorted list
 * 3. Migrates tasks from less utilized to more utilized machines
 * 4. Powers off unused machines to save energy
 * 
 * The pmapper algorithm prioritizes energy efficiency while maintaining
 * performance through workload consolidation.
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
     * and initializes tracking data structures including the energy-sorted machine list.
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
     * energy consumption and CPU compatibility.
     * 
     * @param now Current simulation time
     * @param task_id ID of the newly arrived task
     */
    void NewTask(Time_t now, TaskId_t task_id);
    
    /**
     * Perform periodic maintenance
     * 
     * Updates energy-sorted machine list, checks for potential optimizations,
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
     * Implements the workload consolidation aspect of pmapper:
     * 1. Divides servers into two halves based on utilization
     * 2. Finds smallest workload in least utilized server
     * 3. Migrates it to a highly utilized server
     * 
     * @param now Current simulation time
     * @param task_id ID of the completed task
     */
    void TaskComplete(Time_t now, TaskId_t task_id);
    
    /**
     * Handle SLA violation for a task
     * 
     * Attempts to find a better machine for a task that is experiencing
     * SLA violations, particularly due to CPU compatibility issues.
     * 
     * @param taskId ID of the task experiencing SLA violation
     * @param currentMachine Current machine running the task, or -1 if unknown
     * @return True if successfully handled, false otherwise
     */
    bool HandleSLAViolation(TaskId_t taskId, MachineId_t currentMachine);
    
    // Make these public so they can be accessed by interface methods
    /**
     * List of all virtual machines created by the scheduler
     */
    vector<VMId_t> vms;
    
    /**
     * List of all physical machines managed by the scheduler
     */
    vector<MachineId_t> machines;
    
    /**
     * Machines sorted by energy consumption (lowest to highest)
     * Key component of the pmapper algorithm for energy-efficient task allocation
     */
    vector<pair<MachineId_t, uint64_t>> energySortedMachines;
    
    /**
     * Find or create a VM for a specific machine
     * 
     * Ensures CPU compatibility between the VM and tasks.
     * 
     * @param machineId ID of the machine to find/create VM for
     * @param vmType Type of VM to create if needed
     * @return ID of the VM
     */
    VMId_t FindVMForMachine(MachineId_t machineId, VMType_t vmType = LINUX);
    
private:
    /**
     * Maps machines to their current utilization level
     * Used for workload consolidation decisions
     */
    map<MachineId_t, unsigned> machineUtilization;
    
    /**
     * Flag to check if the machine list has been sorted
     */
    bool initialized;
    
    /**
     * Build list of machines sorted by energy consumption
     * 
     * Core component of the pmapper algorithm that enables
     * energy-efficient task allocation.
     */
    void BuildEnergySortedMachineList();
    
    /**
     * Power off machines with no active tasks
     * 
     * Key energy efficiency feature that reduces power consumption
     * by turning off idle machines.
     */
    void CheckAndTurnOffUnusedMachines();
    
    /**
     * Find the machine that a task is running on
     * 
     * Used for task migration and workload consolidation.
     * 
     * @param taskId ID of the task to locate
     * @return ID of the machine running the task, or -1 if not found
     */
    MachineId_t FindMachineForTask(TaskId_t taskId);
    
    /**
     * Find the smallest task running on a machine
     * 
     * Used in the workload consolidation strategy to determine
     * which task to migrate from a lightly loaded machine.
     * 
     * @param machineId ID of the machine to search
     * @return ID of the smallest task, or -1 if no tasks found
     */
    TaskId_t FindSmallestTaskOnMachine(MachineId_t machineId);
};



#endif /* Scheduler_hpp */
