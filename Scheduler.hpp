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
    vector<VMId_t> vms;
    vector<MachineId_t> machines;
    vector<pair<MachineId_t, uint64_t>> energySortedMachines; // Machines sorted by energy consumption
    map<MachineId_t, unsigned> machineUtilization;           // Track machine utilization
    bool initialized;                                        // Flag to check if sorted once
    
    void BuildEnergySortedMachineList();
    VMId_t FindVMForMachine(MachineId_t machineId);
    void CheckAndTurnOffUnusedMachines();
    MachineId_t FindMachineForTask(TaskId_t taskId);
    TaskId_t FindSmallestTaskOnMachine(MachineId_t machineId);
};



#endif /* Scheduler_hpp */
