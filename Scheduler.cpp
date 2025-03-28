//
//  Scheduler.cpp
//  CloudSim
//
//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
//

#include "Scheduler.hpp"

static bool migrating = false;
static unsigned active_machines = 16;

void Scheduler::Init() {
    SimOutput("Scheduler::Init(): Initializing Predictive scheduler", 1);
    
    taskCount = 0;
    
    unsigned total_machines = Machine_GetTotal();
    
    for(unsigned i = 0; i < active_machines && i < total_machines; i++) {
        MachineId_t machineId = MachineId_t(i);
        MachineInfo_t info = Machine_GetInfo(machineId);
        
        cpuTypeMachines[info.cpu].push_back(machineId);
        
        machines.push_back(machineId);
        VMId_t vmId = VM_Create(LINUX, info.cpu);
        vms.push_back(vmId);
        
        vmToMachine[vmId] = machineId;
        machineToVMs[machineId].push_back(vmId);
        vmSizes[vmId] = 0; // Start with highest performance (P0)
        vmAverageResponseTime[vmId] = 0;
        machineActive[machineId] = true;
        
        VM_Attach(vmId, machineId);
    }
    
    for(unsigned i = active_machines; i < total_machines; i++) {
        MachineId_t machineId = MachineId_t(i);
        MachineInfo_t info = Machine_GetInfo(machineId);
        
        cpuTypeMachines[info.cpu].push_back(machineId);
        
        if(info.s_state != S5) {
            Machine_SetState(machineId, S5);
        }
        machineActive[machineId] = false;
    }
    
    SimOutput("Scheduler::Init(): Created " + to_string(vms.size()) + " VMs across " + to_string(machines.size()) + " machines", 1);
}

void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    // Update your data structure. The VM now can receive new tasks
}

void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
    taskStartTimes[task_id] = now;
    taskCount++;
    
    CPUType_t requiredCPU = RequiredCPUType(task_id);
    VMType_t requiredVM = RequiredVMType(task_id);
    SLAType_t slaType = RequiredSLA(task_id);
    
    Priority_t priority = (slaType == SLA0) ? HIGH_PRIORITY : 
                          (slaType == SLA1) ? MID_PRIORITY : LOW_PRIORITY;
    
    VMId_t bestVM = (VMId_t)-1;
    Time_t bestResponseTime = UINT64_MAX;
    
    for (auto& vmId : vms) {
        try {
            VMInfo_t vmInfo = VM_GetInfo(vmId);
            
            if (vmInfo.cpu == requiredCPU && vmInfo.vm_type == requiredVM) {
                if (priority == HIGH_PRIORITY) {
                    if (vmAverageResponseTime[vmId] < bestResponseTime) {
                        bestVM = vmId;
                        bestResponseTime = vmAverageResponseTime[vmId];
                    }
                } 
                else if (bestVM == (VMId_t)-1) {
                    bestVM = vmId;
                }
            }
        } catch (...) {
            continue;
        }
    }
    
    if (bestVM != (VMId_t)-1) {
        VM_AddTask(bestVM, task_id, priority);
        return;
    }
    
    for (auto& machineId : cpuTypeMachines[requiredCPU]) {
        if (!machineActive[machineId]) {
            MachineInfo_t info = Machine_GetInfo(machineId);
            if (info.s_state == S5) {
                Machine_SetState(machineId, S0);
            }
            
            VMId_t newVM = VM_Create(requiredVM, requiredCPU);
            VM_Attach(newVM, machineId);
            
            vms.push_back(newVM);
            vmToMachine[newVM] = machineId;
            machineToVMs[machineId].push_back(newVM);
            vmSizes[newVM] = 0; // Start with highest performance
            vmAverageResponseTime[newVM] = 0;
            machineActive[machineId] = true;
            
            VM_AddTask(newVM, task_id, priority);
            return;
        }
    }
    
    unsigned vmIndex = task_id % vms.size();
    VM_AddTask(vms[vmIndex], task_id, priority);
}

void Scheduler::PeriodicCheck(Time_t now) {
    static unsigned checkCount = 0;
    checkCount++;
    
    if (checkCount % 5 != 0) {
        return;
    }
    
    for (auto& machineId : machines) {
        try {
            if (!machineActive[machineId]) {
                continue;
            }
            
            auto vmsIter = machineToVMs.find(machineId);
            if (vmsIter == machineToVMs.end() || vmsIter->second.empty()) {
                if (machineId >= 16) {
                    Machine_SetState(machineId, S5);
                    machineActive[machineId] = false;
                }
            }
        } catch (...) {
            continue;
        }
    }
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
}

void Scheduler::TaskComplete(Time_t now, TaskId_t task_id) {
    auto startTimeIter = taskStartTimes.find(task_id);
    if (startTimeIter != taskStartTimes.end()) {
        Time_t startTime = startTimeIter->second;
        Time_t responseTime = now - startTime;
        
        for (auto& vmId : vms) {
            try {
                VMInfo_t vmInfo = VM_GetInfo(vmId);
                bool wasRunningTask = false;
                
                for (auto& runningTask : vmInfo.active_tasks) {
                    if (runningTask == task_id) {
                        wasRunningTask = true;
                        break;
                    }
                }
                
                if (wasRunningTask) {
                    if (vmResponseTimes[vmId].size() >= 10) {
                        vmResponseTimes[vmId].pop_front();
                    }
                    vmResponseTimes[vmId].push_back(responseTime);
                    
                    Time_t totalTime = 0;
                    for (auto& time : vmResponseTimes[vmId]) {
                        totalTime += time;
                    }
                    vmAverageResponseTime[vmId] = totalTime / vmResponseTimes[vmId].size();
                    
                    if (taskCount % 10 == 0 && vmResponseTimes[vmId].size() >= 5) {
                        Time_t oldAvg = 0, newAvg = 0;
                        size_t halfSize = vmResponseTimes[vmId].size() / 2;
                        
                        for (size_t i = 0; i < halfSize; i++) {
                            oldAvg += vmResponseTimes[vmId][i];
                        }
                        oldAvg /= halfSize;
                        
                        for (size_t i = halfSize; i < vmResponseTimes[vmId].size(); i++) {
                            newAvg += vmResponseTimes[vmId][i];
                        }
                        newAvg /= (vmResponseTimes[vmId].size() - halfSize);
                        
                        if (newAvg > oldAvg * 1.1) {
                            if (vmSizes[vmId] > 0) {
                                vmSizes[vmId]--;
                                
                                MachineId_t machineId = vmToMachine[vmId];
                                MachineInfo_t info = Machine_GetInfo(machineId);
                                for (unsigned j = 0; j < info.num_cpus; j++) {
                                    CPUPerformance_t perf = (CPUPerformance_t)vmSizes[vmId];
                                    Machine_SetCorePerformance(machineId, j, perf);
                                }
                            }
                        } 
                        else if (newAvg < oldAvg * 0.9) {
                            if (vmSizes[vmId] < 3) {
                                vmSizes[vmId]++;
                                
                                MachineId_t machineId = vmToMachine[vmId];
                                MachineInfo_t info = Machine_GetInfo(machineId);
                                for (unsigned j = 0; j < info.num_cpus; j++) {
                                    CPUPerformance_t perf = (CPUPerformance_t)vmSizes[vmId];
                                    Machine_SetCorePerformance(machineId, j, perf);
                                }
                            }
                        }
                    }
                    
                    break;
                }
            } catch (...) {
                continue;
            }
        }
        
        taskStartTimes.erase(startTimeIter);
    }
}

// Public interface below

static Scheduler Scheduler;

void InitScheduler() {
    SimOutput("InitScheduler(): Initializing scheduler", 4);
    Scheduler.Init();
}

void HandleNewTask(Time_t time, TaskId_t task_id) {
    SimOutput("HandleNewTask(): Received new task " + to_string(task_id) + " at time " + to_string(time), 4);
    Scheduler.NewTask(time, task_id);
}

void HandleTaskCompletion(Time_t time, TaskId_t task_id) {
    SimOutput("HandleTaskCompletion(): Task " + to_string(task_id) + " completed at time " + to_string(time), 4);
    Scheduler.TaskComplete(time, task_id);
}

void MemoryWarning(Time_t time, MachineId_t machine_id) {
    // The simulator is alerting you that machine identified by machine_id is overcommitted
    SimOutput("MemoryWarning(): Overflow at " + to_string(machine_id) + " was detected at time " + to_string(time), 0);
}

void MigrationDone(Time_t time, VMId_t vm_id) {
    // The function is called on to alert you that migration is complete
    SimOutput("MigrationDone(): Migration of VM " + to_string(vm_id) + " was completed at time " + to_string(time), 4);
    Scheduler.MigrationComplete(time, vm_id);
    migrating = false;
}

void SchedulerCheck(Time_t time) {
    // This function is called periodically by the simulator, no specific event
    SimOutput("SchedulerCheck(): SchedulerCheck() called at " + to_string(time), 4);
    Scheduler.PeriodicCheck(time);
    static unsigned counts = 0;
    counts++;
    if(counts == 10) {
        migrating = true;
        VM_Migrate(1, 9);
    }
}

void SimulationComplete(Time_t time) {
    // This function is called before the simulation terminates Add whatever you feel like.
    cout << "SLA violation report" << endl;
    cout << "SLA0: " << GetSLAReport(SLA0) << "%" << endl;
    cout << "SLA1: " << GetSLAReport(SLA1) << "%" << endl;
    cout << "SLA2: " << GetSLAReport(SLA2) << "%" << endl;     // SLA3 do not have SLA violation issues
    cout << "Total Energy " << Machine_GetClusterEnergy() << "KW-Hour" << endl;
    cout << "Simulation run finished in " << double(time)/1000000 << " seconds" << endl;
    SimOutput("SimulationComplete(): Simulation finished at time " + to_string(time), 4);
    
    Scheduler.Shutdown(time);
}

void SLAWarning(Time_t time, TaskId_t task_id) {
    
}

void StateChangeComplete(Time_t time, MachineId_t machine_id) {
    // Called in response to an earlier request to change the state of a machine
}

