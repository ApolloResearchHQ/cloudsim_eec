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
    Scheduler()                 {}
    void Init();
    void MigrationComplete(Time_t time, VMId_t vm_id);
    void NewTask(Time_t now, TaskId_t task_id);
    void PeriodicCheck(Time_t now);
    void Shutdown(Time_t now);
    void TaskComplete(Time_t now, TaskId_t task_id);
private:
    // VM and machine management
    vector<VMId_t> vms;
    vector<MachineId_t> machines;
    map<VMId_t, MachineId_t> vmToMachine;  // Maps VMs to their host machines
    map<MachineId_t, vector<VMId_t>> machineToVMs;  // Maps machines to their hosted VMs
    
    // Response time tracking
    map<TaskId_t, Time_t> taskStartTimes;  // When each task started
    map<VMId_t, deque<Time_t>> vmResponseTimes;  // Recent response times per VM
    map<VMId_t, Time_t> vmAverageResponseTime;  // Current average response time per VM
    
    // VM sizing and machine state
    map<VMId_t, unsigned> vmSizes;  // Current size (CPU allocation) of each VM
    map<MachineId_t, bool> machineActive;  // Whether a machine is currently active
    
    // CPU compatibility tracking
    map<CPUType_t, vector<MachineId_t>> cpuTypeMachines;  // Group machines by CPU type
    
    // Task counter for periodic operations
    unsigned taskCount;
};



#endif /* Scheduler_hpp */
