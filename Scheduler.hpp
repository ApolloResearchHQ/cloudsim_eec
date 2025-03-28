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
 * Class that implements the e-eco algorithm for cloud scheduling
 * 
 * This scheduler optimizes cloud resource allocation with the following strategies:
 * 1. Divides hosts into three tiers (Running, Intermediate, Switched Off)
 * 2. Dynamically adjusts tier sizes based on workload intensity
 * 3. Activates/deactivates machines based on system load
 * 4. Ensures CPU compatibility for task allocation
 * 
 * The e-eco algorithm balances energy efficiency and performance
 * through its three-tier host management system.
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
     * Discovers all available machines, creates VMs for running tier machines,
     * and initializes the three-tier system.
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
     * CPU compatibility and current tier status.
     * 
     * @param now Current simulation time
     * @param task_id ID of the newly arrived task
     */
    void NewTask(Time_t now, TaskId_t task_id);
    
    /**
     * Perform periodic maintenance
     * 
     * Adjusts tier sizes based on current system load and
     * activates/deactivates machines as needed.
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
     * Updates machine loads and adjusts tiers based on
     * the new system state after task completion.
     * 
     * @param now Current simulation time
     * @param task_id ID of the completed task
     */
    void TaskComplete(Time_t now, TaskId_t task_id);
    
private:
    // Host tier management
    enum HostTier {
        RUNNING,        // Active hosts running applications
        INTERMEDIATE,   // Standby hosts ready to be activated
        SWITCHED_OFF    // Powered off hosts for energy saving
    };
    
    // Machine management
    vector<VMId_t> vms;
    vector<MachineId_t> machines;
    
    // E-eco tier management
    map<MachineId_t, HostTier> machineTiers;    // Track which tier each machine belongs to
    map<MachineId_t, unsigned> machineLoads;     // Track current load for each machine
    map<CPUType_t, vector<MachineId_t>> cpuTypeMachines; // Group machines by CPU type
    
    // Tier calculation and management
    void CalculateTierSizes(unsigned totalMachines, unsigned activeWorkload, 
                           unsigned& runningSize, unsigned& intermediateSize);
    void AdjustTiers(Time_t now);
    void ActivateMachine(MachineId_t machineId, Time_t now);
    void DeactivateMachine(MachineId_t machineId, Time_t now);
    MachineId_t FindCompatibleMachine(CPUType_t cpuType, bool includeIntermediate = false);
    
    // Load thresholds for tier transitions
    const double HIGH_LOAD_THRESHOLD = 0.7;  // 70% utilization threshold for high load
    const double LOW_LOAD_THRESHOLD = 0.3;   // 30% utilization threshold for low load
    
    // Helper methods
    double GetSystemLoad();
    double GetMachineLoad(MachineId_t machineId);
    bool IsMachineSuitable(MachineId_t machineId, TaskId_t taskId);
};

#endif /* Scheduler_hpp */
