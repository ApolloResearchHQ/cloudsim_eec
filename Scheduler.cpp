//
//  Scheduler.cpp
//  CloudSim
//
//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
//

#include "Scheduler.hpp"
#include <algorithm>
#include <map>

static bool migrating = false;

void Scheduler::Init() {
    // Find the parameters of the clusters
    // Get the total number of machines
    // For each machine:
    //      Get the type of the machine
    //      Get the memory of the machine
    //      Get the number of CPUs
    //      Get if there is a GPU or not
    // 
    SimOutput("Scheduler::Init(): Total number of machines is " + to_string(Machine_GetTotal()), 3);
    SimOutput("Scheduler::Init(): Initializing scheduler", 1);
    
    initialized = false;
    
    unsigned total_machines = Machine_GetTotal();
    for(unsigned i = 0; i < total_machines; i++) {
        MachineInfo_t info = Machine_GetInfo(MachineId_t(i));
        if(info.s_state != S5) { // Only consider machines that are not powered off
            machines.push_back(MachineId_t(i));
            vms.push_back(VM_Create(LINUX, info.cpu));
            machineUtilization[MachineId_t(i)] = 0; // Initialize utilization to 0
        }
    }
    
    for(unsigned i = 0; i < machines.size(); i++) {
        VM_Attach(vms[i], machines[i]);
    }
    
    BuildEnergySortedMachineList();
    
    SimOutput("Scheduler::Init(): Successfully initialized the pmapper scheduler", 3);
}

void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    // Update your data structure. The VM now can receive new tasks
    SimOutput("Scheduler::MigrationComplete(): VM " + to_string(vm_id) + " migration completed at " + to_string(time), 3);
    migrating = false;
}

void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
    SimOutput("Scheduler::NewTask(): Processing new task " + to_string(task_id), 1);
    
    if (!initialized) {
        BuildEnergySortedMachineList();
    }
    
    Priority_t priority = (task_id == 0 || task_id == 64) ? HIGH_PRIORITY : MID_PRIORITY;
    unsigned taskMemory = GetTaskMemory(task_id);
    CPUType_t requiredCPU = RequiredCPUType(task_id);
    VMType_t requiredVM = RequiredVMType(task_id);
    
    SimOutput("Scheduler::NewTask(): Task " + to_string(task_id) + 
              " requires CPU type " + to_string(requiredCPU) + 
              " and VM type " + to_string(requiredVM), 0);
    
    bool allocated = false;
    
    for (auto& machine_pair : energySortedMachines) {
        MachineId_t machineId = machine_pair.first;
        MachineInfo_t info = Machine_GetInfo(machineId);
        
        if (info.s_state == S5) {
            continue;
        }
        
        if (info.cpu != requiredCPU) {
            SimOutput("Scheduler::NewTask(): Machine " + to_string(machineId) + 
                      " has incompatible CPU type " + to_string(info.cpu) + 
                      " for task " + to_string(task_id), 1);
            continue;
        }
        
        VMId_t vmId = FindVMForMachine(machineId, requiredVM);
        
        if (info.memory_used + taskMemory <= info.memory_size) {
            try {
                VM_AddTask(vmId, task_id, priority);
                machineUtilization[machineId]++; // Increment utilization count
                allocated = true;
                
                SimOutput("Scheduler::NewTask(): Allocated task " + to_string(task_id) + 
                          " to machine " + to_string(machineId), 0);
                break;
            } catch (const exception& e) {
                SimOutput("Scheduler::NewTask(): Exception when adding task: " + string(e.what()), 0);
                continue; // Try next machine
            }
        }
    }
    
    if (!allocated) {
        SimOutput("Scheduler::NewTask(): Could not allocate task " + to_string(task_id) + 
                  " - SLA violation", 0);
    }
    
    CheckAndTurnOffUnusedMachines();
}

void Scheduler::PeriodicCheck(Time_t now) {
    SimOutput("Scheduler::PeriodicCheck(): Running periodic check at time " + to_string(now), 0);
    
    static int check_count = 0;
    if (check_count++ % 10 == 0) {
        BuildEnergySortedMachineList();
    }
    
    SimOutput("Scheduler::PeriodicCheck(): Total machines: " + to_string(machines.size()), 0);
    SimOutput("Scheduler::PeriodicCheck(): Total energy: " + to_string(Machine_GetClusterEnergy()), 0);
    
    CheckAndTurnOffUnusedMachines();
}

void Scheduler::Shutdown(Time_t time) {
    // Do your final reporting and bookkeeping here.
    // Report about the total energy consumed
    // Report about the SLA compliance
    // Shutdown everything to be tidy :-)
    for(auto & vm: vms) {
        VM_Shutdown(vm);
    }
    SimOutput("SimulationComplete(): Finished!", 4);
    SimOutput("SimulationComplete(): Time is " + to_string(time), 4);
    SimOutput("SimulationComplete(): Total energy consumed: " + to_string(Machine_GetClusterEnergy()) + " KW-Hour", 4);
}

void Scheduler::TaskComplete(Time_t now, TaskId_t task_id) {
    SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) + " is complete at " + to_string(now), 0);
    
    MachineId_t taskMachine = FindMachineForTask(task_id);
    
    if (taskMachine != (MachineId_t)-1) {
        machineUtilization[taskMachine]--;
        
        vector<pair<MachineId_t, unsigned>> sortedByUtilization;
        
        for (auto& pair : machineUtilization) {
            if (Machine_GetInfo(pair.first).s_state != S5) { // Only consider powered-on machines
                sortedByUtilization.push_back(pair);
            }
        }
        
        sort(sortedByUtilization.begin(), sortedByUtilization.end(),
             [](const pair<MachineId_t, unsigned>& a, const pair<MachineId_t, unsigned>& b) {
                 return a.second < b.second;
             });
        
        if (sortedByUtilization.size() >= 2) {
            size_t midPoint = sortedByUtilization.size() / 2;
            
            MachineId_t leastUtilizedMachine = sortedByUtilization[0].first;
            TaskId_t smallestTask = FindSmallestTaskOnMachine(leastUtilizedMachine);
            
            if (smallestTask != (TaskId_t)-1) {
                CPUType_t requiredCPU = RequiredCPUType(smallestTask);
                
                MachineId_t highlyUtilizedMachine = (MachineId_t)-1;
                
                for (size_t i = midPoint; i < sortedByUtilization.size(); i++) {
                    MachineId_t candidateMachine = sortedByUtilization[i].first;
                    MachineInfo_t info = Machine_GetInfo(candidateMachine);
                    
                    if (info.cpu == requiredCPU) {
                        highlyUtilizedMachine = candidateMachine;
                        break;
                    }
                }
                
                if (highlyUtilizedMachine != (MachineId_t)-1) {
                    VMId_t sourceVM = FindVMForMachine(leastUtilizedMachine);
                    VMId_t destVM = FindVMForMachine(highlyUtilizedMachine);
                    
                    VM_RemoveTask(sourceVM, smallestTask);
                    
                    Priority_t priority = (smallestTask == 0 || smallestTask == 64) ? HIGH_PRIORITY : MID_PRIORITY;
                    VM_AddTask(destVM, smallestTask, priority);
                    
                    machineUtilization[leastUtilizedMachine]--;
                    machineUtilization[highlyUtilizedMachine]++;
                    
                    SimOutput("Scheduler::TaskComplete(): Migrated task " + to_string(smallestTask) + 
                              " from machine " + to_string(leastUtilizedMachine) + 
                              " to machine " + to_string(highlyUtilizedMachine), 3);
                } else {
                    SimOutput("Scheduler::TaskComplete(): Could not find compatible machine for task " + 
                              to_string(smallestTask) + " with CPU type " + to_string(requiredCPU), 3);
                }
            }
        }
        
        CheckAndTurnOffUnusedMachines();
    }
}


void Scheduler::BuildEnergySortedMachineList() {
    static int rebuild_count = 0;
    if (!energySortedMachines.empty() && rebuild_count++ % 5 != 0) {
        return;
    }
    
    SimOutput("Scheduler::BuildEnergySortedMachineList(): Rebuilding energy sorted machine list", 0);
    
    energySortedMachines.clear();
    
    for(unsigned i = 0; i < machines.size(); i++) {
        uint64_t energy = Machine_GetEnergy(machines[i]);
        energySortedMachines.push_back(make_pair(machines[i], energy));
    }
    
    sort(energySortedMachines.begin(), energySortedMachines.end(), 
         [](const pair<MachineId_t, uint64_t>& a, const pair<MachineId_t, uint64_t>& b) {
             return a.second < b.second;
         });
    
    initialized = true;
}

VMId_t Scheduler::FindVMForMachine(MachineId_t machineId, VMType_t vmType) {
    for (unsigned i = 0; i < machines.size(); i++) {
        if (machines[i] == machineId) {
            return vms[i];
        }
    }
    
    MachineInfo_t info = Machine_GetInfo(machineId);
    SimOutput("Scheduler::FindVMForMachine(): Creating new VM with CPU type " + to_string(info.cpu) + 
              " and VM type " + to_string(vmType) + " for machine " + to_string(machineId), 0);
    
    VMId_t newVM = VM_Create(vmType, info.cpu);
    VM_Attach(newVM, machineId);
    return newVM;
}

void Scheduler::CheckAndTurnOffUnusedMachines() {
    for (auto& machine_pair : energySortedMachines) {
        MachineId_t machineId = machine_pair.first;
        
        if (machineUtilization[machineId] == 0) {
            MachineInfo_t info = Machine_GetInfo(machineId);
            
            if (info.s_state != S5) {
                Machine_SetState(machineId, S5);
                SimOutput("Scheduler::CheckAndTurnOffUnusedMachines(): Turned off machine " + 
                          to_string(machineId) + " due to zero utilization", 2);
            }
        }
    }
}

MachineId_t Scheduler::FindMachineForTask(TaskId_t taskId) {
    for (auto& machine : machines) {
        MachineInfo_t info = Machine_GetInfo(machine);
        VMId_t vmId = FindVMForMachine(machine);
        VMInfo_t vmInfo = VM_GetInfo(vmId);
        
        for (auto& task : vmInfo.active_tasks) {
            if (task == taskId) {
                return machine;
            }
        }
    }
    
    return (MachineId_t)-1; // Not found
}

TaskId_t Scheduler::FindSmallestTaskOnMachine(MachineId_t machineId) {
    VMId_t vmId = FindVMForMachine(machineId);
    VMInfo_t vmInfo = VM_GetInfo(vmId);
    
    if (vmInfo.active_tasks.empty()) {
        return (TaskId_t)-1;
    }
    
    TaskId_t smallestTask = vmInfo.active_tasks[0];
    unsigned smallestMemory = GetTaskMemory(smallestTask);
    
    for (auto& taskId : vmInfo.active_tasks) {
        unsigned memory = GetTaskMemory(taskId);
        if (memory < smallestMemory) {
            smallestTask = taskId;
            smallestMemory = memory;
        }
    }
    
    return smallestTask;
}

// Public interface below

static Scheduler Scheduler;

void InitScheduler() {
    SimOutput("InitScheduler(): Initializing scheduler", 0);
    Scheduler.Init();
}

void HandleNewTask(Time_t time, TaskId_t task_id) {
    SimOutput("HandleNewTask(): Received new task " + to_string(task_id) + " at time " + to_string(time), 0);
    Scheduler.NewTask(time, task_id);
}

void HandleTaskCompletion(Time_t time, TaskId_t task_id) {
    SimOutput("HandleTaskCompletion(): Task " + to_string(task_id) + " completed at time " + to_string(time), 0);
    Scheduler.TaskComplete(time, task_id);
}

void MemoryWarning(Time_t time, MachineId_t machine_id) {
    // The simulator is alerting you that machine identified by machine_id is overcommitted
    SimOutput("MemoryWarning(): Overflow at " + to_string(machine_id) + " was detected at time " + to_string(time), 0);
}

void MigrationDone(Time_t time, VMId_t vm_id) {
    // The function is called on to alert you that migration is complete
    SimOutput("MigrationDone(): Migration of VM " + to_string(vm_id) + " was completed at time " + to_string(time), 0);
    Scheduler.MigrationComplete(time, vm_id);
    migrating = false;
}

void SchedulerCheck(Time_t time) {
    // This function is called periodically by the simulator, no specific event
    SimOutput("SchedulerCheck(): SchedulerCheck() called at " + to_string(time), 0);
    Scheduler.PeriodicCheck(time);
}

void SimulationComplete(Time_t time) {
    // This function is called before the simulation terminates Add whatever you feel like.
    cout << "SLA violation report" << endl;
    cout << "SLA0: " << GetSLAReport(SLA0) << "%" << endl;
    cout << "SLA1: " << GetSLAReport(SLA1) << "%" << endl;
    cout << "SLA2: " << GetSLAReport(SLA2) << "%" << endl;     // SLA3 do not have SLA violation issues
    cout << "Total Energy " << Machine_GetClusterEnergy() << "KW-Hour" << endl;
    cout << "Simulation run finished in " << double(time)/1000000 << " seconds" << endl;
    SimOutput("SimulationComplete(): Simulation finished at time " + to_string(time), 1);
    
    Scheduler.Shutdown(time);
}

void SLAWarning(Time_t time, TaskId_t task_id) {
    SimOutput("SLAWarning(): SLA violation for task " + to_string(task_id) + " at time " + to_string(time), 1);
}

void StateChangeComplete(Time_t time, MachineId_t machine_id) {
    // Called in response to an earlier request to change the state of a machine
    SimOutput("StateChangeComplete(): State change for machine " + to_string(machine_id) + " completed at time " + to_string(time), 3);
}

