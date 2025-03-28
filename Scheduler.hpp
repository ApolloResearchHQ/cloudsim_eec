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
    void HandleSLAViolation(TaskId_t taskId, MachineId_t machineId);
    void HandleThermalEvent(MachineId_t machineId);
    float CalculateMachineUtilization(MachineId_t machineId);
private:
    vector<VMId_t> vms;
    vector<MachineId_t> machines;
    map<MachineId_t, float> machineUtilization;     // Track machine utilization (memory_used/memory_size)
    map<MachineId_t, VMId_t> machineToVMMap;        // Map machines to their VMs
    map<TaskId_t, MachineId_t> taskToMachineMap;    // Map tasks to their machines
    
    // Helper methods
    float CalculateTaskLoadFactor(TaskId_t taskId);
    VMId_t FindVMForMachine(MachineId_t machineId, VMType_t vmType = LINUX, CPUType_t cpuType = X86);
    void CheckAndTurnOffUnusedMachines();
    vector<pair<MachineId_t, float>> GetSortedMachinesByUtilization();
    bool MigrateTask(TaskId_t taskId, MachineId_t targetMachineId);
};



#endif /* Scheduler_hpp */
