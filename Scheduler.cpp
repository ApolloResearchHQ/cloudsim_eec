//
//  Scheduler.cpp
//  CloudSim
//
//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
//

#include "Scheduler.hpp"

/**
 * Initialize the E-eco scheduler
 * 
 * This method implements the initialization phase of the E-eco algorithm:
 * 1. Discovers all available machines in the cluster
 * 2. Groups machines by CPU type for compatibility matching
 * 3. Divides machines into three tiers (Running, Intermediate, Switched Off)
 * 4. Creates VMs for machines in the Running tier
 * 5. Sets appropriate power states for machines in each tier
 */
void Scheduler::Init() {
    // Find the parameters of the clusters
    SimOutput("Scheduler::Init(): Initializing E-eco scheduler", 1);
    
    unsigned totalMachines = Machine_GetTotal();
    
    unsigned runningMachines = max((unsigned)(totalMachines * 0.8), (unsigned)4);
    unsigned intermediateMachines = max((unsigned)(totalMachines * 0.15), (unsigned)2);
    
    if (runningMachines + intermediateMachines > totalMachines) {
        intermediateMachines = max(totalMachines - runningMachines, (unsigned)1);
    }
    
    for (unsigned i = 0; i < totalMachines; i++) {
        MachineInfo_t info = Machine_GetInfo(MachineId_t(i));
        cpuTypeMachines[info.cpu].push_back(MachineId_t(i));
    }
    
    unsigned machineCount = 0;
    unsigned cpuTypeCount = cpuTypeMachines.size();
    
    if (cpuTypeCount > 0) {
        unsigned perCpuType = max(runningMachines / cpuTypeCount, (unsigned)1);
        
        for (auto& cpuGroup : cpuTypeMachines) {
            unsigned allocated = 0;
            for (auto& machineId : cpuGroup.second) {
                if (allocated < perCpuType && machineCount < runningMachines) {
                    machines.push_back(machineId);
                    MachineInfo_t info = Machine_GetInfo(machineId);
                    vms.push_back(VM_Create(LINUX, info.cpu));
                    machineTiers[machineId] = RUNNING;
                    machineLoads[machineId] = 0;
                    machineCount++;
                    allocated++;
                }
            }
        }
    }
    
    for (auto& cpuGroup : cpuTypeMachines) {
        for (auto& machineId : cpuGroup.second) {
            if (machineTiers.find(machineId) == machineTiers.end()) {
                if (machineCount < runningMachines) {
                    machines.push_back(machineId);
                    MachineInfo_t info = Machine_GetInfo(machineId);
                    vms.push_back(VM_Create(LINUX, info.cpu));
                    machineTiers[machineId] = RUNNING;
                    machineLoads[machineId] = 0;
                    machineCount++;
                } else if (machineCount < runningMachines + intermediateMachines) {
                    machineTiers[machineId] = INTERMEDIATE;
                    Machine_SetState(machineId, S3);
                    machineLoads[machineId] = 0;
                    machineCount++;
                } else {
                    machineTiers[machineId] = SWITCHED_OFF;
                    Machine_SetState(machineId, S5);
                    machineLoads[machineId] = 0;
                }
            }
        }
    }
    
    for (unsigned i = 0; i < machines.size(); i++) {
        VM_Attach(vms[i], machines[i]);
    }
    
    SimOutput("Scheduler::Init(): E-eco scheduler initialized with " + 
             to_string(machineCount) + " active machines", 1);
}

/**
 * Handle VM migration completion
 * 
 * Updates tracking data structures after a VM has been migrated.
 * E-eco minimizes migrations, but this handles any necessary migrations.
 * 
 * @param time Current simulation time
 * @param vm_id ID of the VM that completed migration
 */
void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    SimOutput("Scheduler::MigrationComplete(): VM " + to_string(vm_id) + " migration complete", 3);
}

/**
 * Handle new task arrival
 * 
 * Implements the task allocation strategy of E-eco:
 * 1. Try to allocate the task to a machine in the Running tier with compatible CPU
 * 2. If no suitable machine is found, activate a machine from the Intermediate tier
 * 3. Report SLA violation if no suitable machine is found
 * 4. Adjust tiers based on system load after allocation
 * 
 * @param now Current simulation time
 * @param task_id ID of the newly arrived task
 */
void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
    unsigned taskMemory = GetTaskMemory(task_id);
    CPUType_t requiredCPU = RequiredCPUType(task_id);
    
    Priority_t priority = HIGH_PRIORITY;
    
    bool allocated = false;
    
    vector<pair<MachineId_t, unsigned>> suitableMachines;
    for (auto& machineId : machines) {
        if (machineTiers[machineId] == RUNNING && IsMachineSuitable(machineId, task_id)) {
            suitableMachines.push_back({machineId, machineLoads[machineId]});
        }
    }
    
    sort(suitableMachines.begin(), suitableMachines.end(), 
         [](const pair<MachineId_t, unsigned>& a, const pair<MachineId_t, unsigned>& b) {
             return a.second < b.second;
         });
    
    for (auto& pair : suitableMachines) {
        MachineId_t machineId = pair.first;
        VMId_t vmId = vms[find(machines.begin(), machines.end(), machineId) - machines.begin()];
        
        try {
            VM_AddTask(vmId, task_id, priority);
            machineLoads[machineId] += taskMemory;
            allocated = true;
            break;
        } catch (const exception& e) {
        }
    }
    
    if (!allocated) {
        MachineId_t machineId = FindCompatibleMachine(requiredCPU, true);
        if (machineId != (MachineId_t)-1) {
            ActivateMachine(machineId, now);
            
            VMId_t vmId = VM_Create(LINUX, requiredCPU);
            vms.push_back(vmId);
            machines.push_back(machineId);
            VM_Attach(vmId, machineId);
            
            try {
                VM_AddTask(vmId, task_id, priority);
                machineLoads[machineId] += taskMemory;
                allocated = true;
            } catch (const exception& e) {
                SimOutput("Scheduler::NewTask(): Error allocating task: " + string(e.what()), 1);
            }
        }
    }
    
    if (!allocated) {
        for (auto& pair : machineTiers) {
            if (pair.second == SWITCHED_OFF) {
                MachineId_t machineId = pair.first;
                MachineInfo_t info = Machine_GetInfo(machineId);
                
                if (info.cpu == requiredCPU) {
                    Machine_SetState(machineId, S0);
                    machineTiers[machineId] = RUNNING;
                    machineLoads[machineId] = 0;
                    
                    VMId_t vmId = VM_Create(LINUX, requiredCPU);
                    vms.push_back(vmId);
                    machines.push_back(machineId);
                    VM_Attach(vmId, machineId);
                    
                    try {
                        VM_AddTask(vmId, task_id, priority);
                        machineLoads[machineId] += taskMemory;
                        allocated = true;
                        SimOutput("Scheduler::NewTask(): Activated machine " + to_string(machineId), 1);
                        break;
                    } catch (const exception& e) {
                        SimOutput("Scheduler::NewTask(): Error allocating task: " + string(e.what()), 1);
                    }
                }
            }
        }
    }
    
    if (!allocated) {
        for (auto& cpuPair : cpuTypeMachines) {
            if (cpuPair.first == requiredCPU) {
                for (auto& machineId : cpuPair.second) {
                    if (machineTiers[machineId] != RUNNING) {
                        Machine_SetState(machineId, S0);
                        machineTiers[machineId] = RUNNING;
                        machineLoads[machineId] = 0;
                        
                        VMId_t vmId = VM_Create(LINUX, requiredCPU);
                        vms.push_back(vmId);
                        machines.push_back(machineId);
                        VM_Attach(vmId, machineId);
                        
                        try {
                            VM_AddTask(vmId, task_id, priority);
                            machineLoads[machineId] += taskMemory;
                            allocated = true;
                            SimOutput("Scheduler::NewTask(): Emergency activation of machine " + 
                                    to_string(machineId), 1);
                            break;
                        } catch (const exception& e) {
                            SimOutput("Scheduler::NewTask(): Error in emergency allocation: " + 
                                    string(e.what()), 1);
                        }
                    }
                }
                if (allocated) break;
            }
        }
    }
    
    if (!allocated) {
        SimOutput("Scheduler::NewTask(): SLA violation - Could not allocate task " + 
                 to_string(task_id), 1);
    }
    
    if (allocated && now % 10000000 == 0) {
        AdjustTiers(now);
    }
}

/**
 * Perform periodic maintenance
 * 
 * Adjusts tier sizes based on current system load and
 * activates/deactivates machines as needed.
 * 
 * @param now Current simulation time
 */
void Scheduler::PeriodicCheck(Time_t now) {
    if (now % 50000000 == 0) { // 50 million cycles between adjustments
        AdjustTiers(now);
    }
}

/**
 * Perform final cleanup and reporting
 * 
 * Shuts down all VMs and reports final statistics.
 * 
 * @param now Final simulation time
 */
void Scheduler::Shutdown(Time_t now) {
    for (auto& vm : vms) {
        VM_Shutdown(vm);
    }
    
    SimOutput("Scheduler::Shutdown(): E-eco scheduler shutdown complete", 3);
}

/**
 * Handle task completion
 * 
 * Updates machine loads and adjusts tiers based on
 * the new system state after task completion.
 * 
 * @param now Current simulation time
 * @param task_id ID of the completed task
 */
void Scheduler::TaskComplete(Time_t now, TaskId_t task_id) {
    if (task_id % 100 == 0) {
        SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) + " completed", 1);
    }
    
    for (unsigned i = 0; i < machines.size(); i++) {
        VMInfo_t vmInfo = VM_GetInfo(vms[i]);
        auto taskIter = find(vmInfo.active_tasks.begin(), vmInfo.active_tasks.end(), task_id);
        
        if (taskIter != vmInfo.active_tasks.end()) {
            MachineId_t machineId = machines[i];
            machineLoads[machineId] -= GetTaskMemory(task_id);
            
            if (now % 50000000 == 0) {
                AdjustTiers(now);
            }
            break;
        }
    }
    
    static unsigned completedTasks = 0;
    completedTasks++;
    
    if (completedTasks % 500 == 0) {
        unsigned totalMachines = Machine_GetTotal();
        unsigned runningMachines = 0;
        
        for (auto& pair : machineTiers) {
            if (pair.second == RUNNING) runningMachines++;
        }
        
        if (runningMachines < totalMachines * 0.7) {
            unsigned toActivate = min((unsigned)(totalMachines * 0.1), (unsigned)4);
            unsigned activated = 0;
            
            for (auto& pair : machineTiers) {
                if (pair.second != RUNNING && activated < toActivate) {
                    if (pair.second == INTERMEDIATE) {
                        ActivateMachine(pair.first, now);
                    } else {
                        Machine_SetState(pair.first, S0);
                        machineTiers[pair.first] = RUNNING;
                        machineLoads[pair.first] = 0;
                        
                        MachineInfo_t info = Machine_GetInfo(pair.first);
                        VMId_t vmId = VM_Create(LINUX, info.cpu);
                        vms.push_back(vmId);
                        machines.push_back(pair.first);
                        VM_Attach(vmId, pair.first);
                    }
                    activated++;
                }
            }
            
            if (activated > 0) {
                SimOutput("Scheduler::TaskComplete(): Proactively activated " + 
                         to_string(activated) + " machines to prevent SLA violations", 1);
            }
        }
    }
}

/**
 * Calculate running and intermediate tier sizes based on current workload
 * 
 * Simplified tier sizing for better performance with large input files.
 * Uses a more aggressive approach to ensure enough machines are available
 * to prevent SLA violations.
 * 
 * @param totalMachines Total number of machines in the cluster
 * @param activeWorkload Number of active tasks in the system
 * @param runningSize Output parameter for calculated running tier size
 * @param intermediateSize Output parameter for calculated intermediate tier size
 */
void Scheduler::CalculateTierSizes(unsigned totalMachines, unsigned activeWorkload, 
                                  unsigned& runningSize, unsigned& intermediateSize) {
    runningSize = max((unsigned)(totalMachines * 0.8), (unsigned)4);
    intermediateSize = max((unsigned)(totalMachines * 0.15), (unsigned)2);
    
    unsigned minimumRunning = max(activeWorkload / 2 + 1, (unsigned)3);
    runningSize = max(runningSize, minimumRunning);
    
    if (runningSize + intermediateSize > totalMachines) {
        intermediateSize = max(totalMachines - runningSize, (unsigned)1);
    }
}

/**
 * Adjust machine tiers based on current system load
 * 
 * Core function of the E-eco algorithm that moves machines between tiers
 * based on workload intensity and calculated tier sizes.
 * Optimized for performance with large input files.
 * 
 * @param now Current simulation time
 */
void Scheduler::AdjustTiers(Time_t now) {
    static Time_t lastFullAdjustment = 0;
    bool fullAdjustment = (now - lastFullAdjustment > 200000000); // Increased interval
    
    if (fullAdjustment) {
        lastFullAdjustment = now;
    } else {
        return;
    }
    
    unsigned totalMachines = Machine_GetTotal();
    unsigned activeWorkload = 0;
    
    unsigned taskCount = GetNumTasks();
    unsigned sampleStep = (taskCount > 200) ? 10 : 1; // More efficient sampling
    
    unsigned maxTasksToCheck = min(taskCount, (unsigned)1000);
    for (unsigned i = 0; i < maxTasksToCheck; i += sampleStep) {
        if (!IsTaskCompleted(TaskId_t(i))) {
            activeWorkload++;
        }
    }
    
    if (sampleStep > 1) {
        activeWorkload = (activeWorkload * taskCount) / (maxTasksToCheck / sampleStep);
    }
    
    activeWorkload = (unsigned)(activeWorkload * 1.2); // 20% safety margin
    
    unsigned desiredRunning, desiredIntermediate;
    CalculateTierSizes(totalMachines, activeWorkload, desiredRunning, desiredIntermediate);
    
    unsigned currentRunning = 0, currentIntermediate = 0;
    for (auto& pair : machineTiers) {
        if (pair.second == RUNNING) currentRunning++;
        else if (pair.second == INTERMEDIATE) currentIntermediate++;
    }
    
    if (currentRunning < desiredRunning) {
        unsigned toActivate = min(desiredRunning - currentRunning, (unsigned)8);
        
        for (auto& pair : machineTiers) {
            if (pair.second == INTERMEDIATE && toActivate > 0) {
                ActivateMachine(pair.first, now);
                toActivate--;
            }
        }
        
        if (toActivate > 0) {
            for (auto& pair : machineTiers) {
                if (pair.second == SWITCHED_OFF && toActivate > 0) {
                    Machine_SetState(pair.first, S0);
                    machineTiers[pair.first] = RUNNING;
                    machineLoads[pair.first] = 0;
                    
                    MachineInfo_t info = Machine_GetInfo(pair.first);
                    VMId_t vmId = VM_Create(LINUX, info.cpu);
                    vms.push_back(vmId);
                    machines.push_back(pair.first);
                    VM_Attach(vmId, pair.first);
                    
                    toActivate--;
                }
            }
        }
    } 
    else if (currentRunning > desiredRunning + 4 && GetSystemLoad() < LOW_LOAD_THRESHOLD) {
        vector<pair<MachineId_t, unsigned>> loadPairs;
        
        for (auto& machineId : machines) {
            if (machineLoads[machineId] == 0) {
                loadPairs.push_back({machineId, 0});
            }
        }
        
        unsigned toDeactivate = min(currentRunning - desiredRunning - 2, (unsigned)2);
        for (auto& pair : loadPairs) {
            if (toDeactivate > 0 && pair.second == 0) {
                DeactivateMachine(pair.first, now);
                toDeactivate--;
            }
        }
    }
}

/**
 * Activate a machine (move from intermediate to running tier)
 * 
 * @param machineId ID of the machine to activate
 * @param now Current simulation time
 */
void Scheduler::ActivateMachine(MachineId_t machineId, Time_t now) {
    if (machineTiers[machineId] == INTERMEDIATE) {
        Machine_SetState(machineId, S0);
        machineTiers[machineId] = RUNNING;
        
        if (find(machines.begin(), machines.end(), machineId) == machines.end()) {
            machines.push_back(machineId);
            
            MachineInfo_t info = Machine_GetInfo(machineId);
            VMId_t vmId = VM_Create(LINUX, info.cpu);
            vms.push_back(vmId);
            VM_Attach(vmId, machineId);
        }
        
        SimOutput("Scheduler::ActivateMachine(): Activated machine " + to_string(machineId), 1);
    }
}

/**
 * Deactivate a machine (move from running to intermediate tier)
 * 
 * @param machineId ID of the machine to deactivate
 * @param now Current simulation time
 */
void Scheduler::DeactivateMachine(MachineId_t machineId, Time_t now) {
    if (machineTiers[machineId] == RUNNING && machineLoads[machineId] == 0) {
        Machine_SetState(machineId, S3);
        machineTiers[machineId] = INTERMEDIATE;
        
        auto machineIt = find(machines.begin(), machines.end(), machineId);
        if (machineIt != machines.end()) {
            size_t index = machineIt - machines.begin();
            VM_Shutdown(vms[index]);
            
            vms.erase(vms.begin() + index);
            machines.erase(machineIt);
        }
        
        SimOutput("Scheduler::DeactivateMachine(): Deactivated machine " + to_string(machineId), 1);
    }
}

/**
 * Find a machine compatible with the specified CPU type
 * 
 * @param cpuType CPU type to match
 * @param includeIntermediate Whether to include machines in the intermediate tier
 * @return ID of a compatible machine, or -1 if none found
 */
MachineId_t Scheduler::FindCompatibleMachine(CPUType_t cpuType, bool includeIntermediate) {
    if (cpuTypeMachines.find(cpuType) != cpuTypeMachines.end()) {
        for (auto& machineId : cpuTypeMachines[cpuType]) {
            if (machineTiers[machineId] == RUNNING) {
                return machineId;
            } else if (includeIntermediate && machineTiers[machineId] == INTERMEDIATE) {
                return machineId;
            }
        }
    }
    return (MachineId_t)-1;
}

/**
 * Get overall system load (0.0 to 1.0)
 * 
 * Uses memory usage as a proxy for system load.
 * 
 * @return System load as a value between 0.0 and 1.0
 */
double Scheduler::GetSystemLoad() {
    unsigned totalMemory = 0;
    unsigned usedMemory = 0;
    
    for (auto& machineId : machines) {
        MachineInfo_t info = Machine_GetInfo(machineId);
        totalMemory += info.memory_size;
        usedMemory += info.memory_used;
    }
    
    return totalMemory > 0 ? (double)usedMemory / totalMemory : 0.0;
}

/**
 * Get load for a specific machine (0.0 to 1.0)
 * 
 * Uses memory usage as a proxy for machine load.
 * 
 * @param machineId ID of the machine to check
 * @return Machine load as a value between 0.0 and 1.0
 */
double Scheduler::GetMachineLoad(MachineId_t machineId) {
    MachineInfo_t info = Machine_GetInfo(machineId);
    return info.memory_size > 0 ? (double)info.memory_used / info.memory_size : 0.0;
}

/**
 * Check if a machine is suitable for a task
 * 
 * Verifies CPU compatibility and memory availability.
 * 
 * @param machineId ID of the machine to check
 * @param taskId ID of the task to check
 * @return True if the machine is suitable for the task, false otherwise
 */
bool Scheduler::IsMachineSuitable(MachineId_t machineId, TaskId_t taskId) {
    MachineInfo_t info = Machine_GetInfo(machineId);
    
    if (info.cpu != RequiredCPUType(taskId)) {
        return false;
    }
    
    unsigned taskMemory = GetTaskMemory(taskId);
    if (info.memory_used + taskMemory > info.memory_size) {
        return false;
    }
    
    return true;
}

extern Scheduler scheduler;

/**
 * Initialize the scheduler
 * 
 * Called by the simulator to initialize the E-eco scheduler.
 */
// void InitScheduler() {
//     SimOutput("InitScheduler(): Initializing E-eco scheduler", 3);
//     scheduler.Init();
// }

/**
 * Handle new task arrival
 * 
 * Called by the simulator when a new task arrives.
 * 
 * @param time Current simulation time
 * @param task_id ID of the newly arrived task
 */
void HandleNewTask(Time_t time, TaskId_t task_id) {
    SimOutput("HandleNewTask(): Received task " + to_string(task_id), 3);
    scheduler.NewTask(time, task_id);
}

/**
 * Handle task completion
 * 
 * Called by the simulator when a task completes.
 * 
 * @param time Current simulation time
 * @param task_id ID of the completed task
 */
void HandleTaskCompletion(Time_t time, TaskId_t task_id) {
    SimOutput("HandleTaskCompletion(): Task " + to_string(task_id) + " completed", 3);
    scheduler.TaskComplete(time, task_id);
}

/**
 * Handle memory warning
 * 
 * Called by the simulator when a machine is overcommitted.
 * 
 * @param time Current simulation time
 * @param machine_id ID of the overcommitted machine
 */
void MemoryWarning(Time_t time, MachineId_t machine_id) {
    SimOutput("MemoryWarning(): Overflow at " + to_string(machine_id) + " detected", 1);
}

/**
 * Handle VM migration completion
 * 
 * Called by the simulator when a VM migration completes.
 * 
 * @param time Current simulation time
 * @param vm_id ID of the VM that completed migration
 */
void MigrationDone(Time_t time, VMId_t vm_id) {
    SimOutput("MigrationDone(): Migration of VM " + to_string(vm_id) + " completed", 3);
    scheduler.MigrationComplete(time, vm_id);
}

/**
 * Periodic scheduler check
 * 
 * Called periodically by the simulator to allow the scheduler
 * to monitor and adjust the system.
 * 
 * @param time Current simulation time
 */
void SchedulerCheck(Time_t time) {
    if (time % 20000000 == 0) {
        scheduler.PeriodicCheck(time);
    }
}

/**
 * Handle simulation completion
 * 
 * Called by the simulator when the simulation completes.
 * Reports final statistics and shuts down the scheduler.
 * 
 * @param time Final simulation time
 */
void SimulationComplete(Time_t time) {
    cout << "SLA violation report" << endl;
    cout << "SLA0: " << GetSLAReport(SLA0) << "%" << endl;
    cout << "SLA1: " << GetSLAReport(SLA1) << "%" << endl;
    cout << "SLA2: " << GetSLAReport(SLA2) << "%" << endl;
    cout << "Total Energy " << Machine_GetClusterEnergy() << "KW-Hour" << endl;
    cout << "Simulation run finished in " << double(time)/1000000 << " seconds" << endl;
    SimOutput("SimulationComplete(): Simulation finished at time " + to_string(time), 1);
    
    scheduler.Shutdown(time);
}

/**
 * Handle SLA warning
 * 
 * Called by the simulator when a task is at risk of violating its SLA.
 * 
 * @param time Current simulation time
 * @param task_id ID of the task at risk of violating its SLA
 */
void SLAWarning(Time_t time, TaskId_t task_id) {
    SimOutput("SLAWarning(): SLA violation risk for task " + to_string(task_id), 1);
}

/**
 * Handle machine state change completion
 * 
 * Called by the simulator when a machine completes a state change.
 * 
 * @param time Current simulation time
 * @param machine_id ID of the machine that completed a state change
 */
void StateChangeComplete(Time_t time, MachineId_t machine_id) {
    SimOutput("StateChangeComplete(): Machine " + to_string(machine_id) + " state change complete", 3);
}

