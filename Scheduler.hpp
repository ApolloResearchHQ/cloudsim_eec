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
#include <deque>
#include <algorithm>

#include "Interfaces.h"

/**
 * Class that implements the Predictive algorithm for cloud scheduling
 * 
 * This scheduler optimizes cloud resource allocation with the following strategies:
 * 1. Measures response time instead of load factor
 * 2. Adjusts VM sizes based on response time trends
 * 3. Turns off physical machines when possible
 * 4. Avoids over-provisioning by using actual response time measurements
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
     * Discovers all available machines, creates VMs, and initializes tracking structures.
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
     * Allocates a new task to an appropriate VM based on
     * CPU compatibility and current response times.
     * 
     * @param now Current simulation time
     * @param task_id ID of the newly arrived task
     */
    void NewTask(Time_t now, TaskId_t task_id);
    
    /**
     * Perform periodic maintenance
     * 
     * Analyzes response time trends and adjusts VM sizes accordingly.
     * Also looks for opportunities to turn off physical machines.
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
     * Updates response time measurements and potentially
     * adjusts VM sizes based on the new data.
     * 
     * @param now Current simulation time
     * @param task_id ID of the completed task
     */
    void TaskComplete(Time_t now, TaskId_t task_id);
    
private:
    // VM and machine management
    vector<VMId_t> vms;
    vector<MachineId_t> machines;
    map<VMId_t, MachineId_t> vmToMachine;  // Maps VMs to their host machines
    map<MachineId_t, vector<VMId_t>> machineToVMs;  // Maps machines to their hosted VMs
    
    // Response time tracking
    struct ResponseTimeData {
        Time_t startTime;
        Time_t endTime;
        Time_t responseTime;
    };
    
    map<VMId_t, deque<ResponseTimeData>> vmResponseTimes;  // Tracks recent response times per VM
    map<VMId_t, Time_t> vmAverageResponseTime;  // Current average response time per VM
    map<VMId_t, double> vmResponseTimeSlope;  // Trend of response time changes (rising/falling)
    
    // VM sizing and machine state
    map<VMId_t, unsigned> vmSizes;  // Current size (CPU allocation) of each VM
    map<MachineId_t, bool> machineActive;  // Whether a machine is currently active
    
    // CPU compatibility tracking
    map<CPUType_t, vector<MachineId_t>> cpuTypeMachines;  // Group machines by CPU type
    
    // Constants for response time analysis
    const unsigned RESPONSE_TIME_HISTORY_SIZE = 10;  // Number of recent tasks to track
    const double RESPONSE_TIME_INCREASE_THRESHOLD = 0.1;  // 10% increase triggers VM expansion
    const double RESPONSE_TIME_DECREASE_THRESHOLD = -0.1;  // 10% decrease triggers VM shrinkage
    const unsigned VM_CHECK_INTERVAL = 10;  // Check VM sizing every 10 task completions
    const unsigned MACHINE_CONSOLIDATION_INTERVAL = 50;  // Check for machine consolidation every 50 task completions
    
    // Helper methods
    void UpdateResponseTimeData(VMId_t vmId, TaskId_t taskId, Time_t startTime, Time_t endTime);
    double CalculateResponseTimeSlope(VMId_t vmId);
    void AdjustVMSize(VMId_t vmId, Time_t now);
    bool ConsolidateVMs(Time_t now);
    MachineId_t FindCompatibleMachine(CPUType_t cpuType);
    bool IsMachineEmpty(MachineId_t machineId);
    void ActivateMachine(MachineId_t machineId);
    void DeactivateMachine(MachineId_t machineId, Time_t now);
};

#endif /* Scheduler_hpp */
