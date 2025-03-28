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
    SimOutput("Scheduler::Init(): Total number of machines is " + to_string(Machine_GetTotal()), 3);
    SimOutput("Scheduler::Init(): Initializing E-eco scheduler", 1);
    
    unsigned totalMachines = Machine_GetTotal();
    unsigned runningMachines = max(totalMachines / 3, (unsigned)4); // Start with 1/3 of machines in running tier
    unsigned intermediateMachines = max(totalMachines / 6, (unsigned)2); // Start with 1/6 in intermediate tier
    
    for (unsigned i = 0; i < totalMachines; i++) {
        MachineInfo_t info = Machine_GetInfo(MachineId_t(i));
        cpuTypeMachines[info.cpu].push_back(MachineId_t(i));
    }
    
    unsigned machineCount = 0;
    
    for (auto& cpuGroup : cpuTypeMachines) {
        for (auto& machineId : cpuGroup.second) {
            if (machineCount < runningMachines) {
                machines.push_back(machineId);
                MachineInfo_t info = Machine_GetInfo(machineId);
                vms.push_back(VM_Create(LINUX, info.cpu)); // Create VMs with matching CPU type
                machineTiers[machineId] = RUNNING;
                machineLoads[machineId] = 0;
                machineCount++;
            } else if (machineCount < runningMachines + intermediateMachines) {
                machineTiers[machineId] = INTERMEDIATE;
                Machine_SetState(machineId, S3); // Put in standby mode
                machineLoads[machineId] = 0;
                machineCount++;
            } else {
                machineTiers[machineId] = SWITCHED_OFF;
                Machine_SetState(machineId, S5); // Power off
                machineLoads[machineId] = 0;
            }
        }
    }
    
    for (unsigned i = 0; i < machines.size(); i++) {
        VM_Attach(vms[i], machines[i]);
    }
    
    SimOutput("Scheduler::Init(): Successfully initialized the E-eco scheduler with " + 
              to_string(runningMachines) + " running machines and " + 
              to_string(intermediateMachines) + " intermediate machines", 3);
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
    SimOutput("Scheduler::NewTask(): Processing task " + to_string(task_id), 3);
    
    unsigned taskMemory = GetTaskMemory(task_id);
    CPUType_t requiredCPU = RequiredCPUType(task_id);
    Priority_t priority = (task_id == 0 || task_id == 64) ? HIGH_PRIORITY : MID_PRIORITY;
    
    bool allocated = false;
    for (auto& machineId : machines) {
        if (machineTiers[machineId] == RUNNING && IsMachineSuitable(machineId, task_id)) {
            VMId_t vmId = vms[find(machines.begin(), machines.end(), machineId) - machines.begin()];
            
            try {
                VM_AddTask(vmId, task_id, priority);
                machineLoads[machineId] += taskMemory;
                allocated = true;
                SimOutput("Scheduler::NewTask(): Allocated task " + to_string(task_id) + 
                         " to machine " + to_string(machineId), 3);
                break;
            } catch (const exception& e) {
                SimOutput("Scheduler::NewTask(): Error allocating task: " + string(e.what()), 1);
                continue;
            }
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
                SimOutput("Scheduler::NewTask(): Activated machine " + to_string(machineId) + 
                         " for task " + to_string(task_id), 1);
            } catch (const exception& e) {
                SimOutput("Scheduler::NewTask(): Error allocating task: " + string(e.what()), 1);
            }
        }
    }
    
    if (!allocated) {
        SimOutput("Scheduler::NewTask(): SLA violation - Could not allocate task " + 
                 to_string(task_id), 1);
    }
    
    if (allocated) {
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
    AdjustTiers(now);
    
    if (now % 1000000 == 0) { // Reduced frequency to improve performance
        unsigned running = 0, intermediate = 0, off = 0;
        for (auto& pair : machineTiers) {
            if (pair.second == RUNNING) running++;
            else if (pair.second == INTERMEDIATE) intermediate++;
            else off++;
        }
        
        SimOutput("Scheduler::PeriodicCheck(): System load: " + to_string(GetSystemLoad()) + 
                 ", Running: " + to_string(running) + 
                 ", Intermediate: " + to_string(intermediate) + 
                 ", Off: " + to_string(off), 3);
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
    SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) + " completed", 3);
    
    for (unsigned i = 0; i < machines.size(); i++) {
        VMInfo_t vmInfo = VM_GetInfo(vms[i]);
        auto taskIter = find(vmInfo.active_tasks.begin(), vmInfo.active_tasks.end(), task_id);
        
        if (taskIter != vmInfo.active_tasks.end()) {
            MachineId_t machineId = machines[i];
            machineLoads[machineId] -= GetTaskMemory(task_id);
            
            AdjustTiers(now);
            break;
        }
    }
}

/**
 * Calculate running and intermediate tier sizes based on current workload
 * 
 * Implements the tier sizing formulas from the E-eco algorithm.
 * 
 * @param totalMachines Total number of machines in the cluster
 * @param activeWorkload Number of active tasks in the system
 * @param runningSize Output parameter for calculated running tier size
 * @param intermediateSize Output parameter for calculated intermediate tier size
 */
void Scheduler::CalculateTierSizes(unsigned totalMachines, unsigned activeWorkload, 
                                  unsigned& runningSize, unsigned& intermediateSize) {
    double systemLoad = GetSystemLoad();
    
    if (systemLoad > HIGH_LOAD_THRESHOLD) {
        runningSize = max((unsigned)(totalMachines * 0.6), (unsigned)4);
        intermediateSize = max((unsigned)(totalMachines * 0.2), (unsigned)2);
    } else if (systemLoad < LOW_LOAD_THRESHOLD) {
        runningSize = max((unsigned)(totalMachines * 0.3), (unsigned)2);
        intermediateSize = max((unsigned)(totalMachines * 0.2), (unsigned)2);
    } else {
        runningSize = max((unsigned)(totalMachines * 0.4), (unsigned)3);
        intermediateSize = max((unsigned)(totalMachines * 0.2), (unsigned)2);
    }
    
    unsigned minimumRunning = max(activeWorkload / 4, (unsigned)2); // Assuming 4 tasks per machine at max
    runningSize = max(runningSize, minimumRunning);
    
    if (runningSize + intermediateSize > totalMachines) {
        intermediateSize = totalMachines - runningSize;
    }
}

/**
 * Adjust machine tiers based on current system load
 * 
 * Core function of the E-eco algorithm that moves machines between tiers
 * based on workload intensity and calculated tier sizes.
 * 
 * @param now Current simulation time
 */
void Scheduler::AdjustTiers(Time_t now) {
    unsigned totalMachines = Machine_GetTotal();
    unsigned activeWorkload = 0;
    
    for (unsigned i = 0; i < GetNumTasks(); i++) {
        if (!IsTaskCompleted(TaskId_t(i))) {
            activeWorkload++;
        }
    }
    
    unsigned desiredRunning, desiredIntermediate;
    CalculateTierSizes(totalMachines, activeWorkload, desiredRunning, desiredIntermediate);
    
    unsigned currentRunning = 0, currentIntermediate = 0;
    for (auto& pair : machineTiers) {
        if (pair.second == RUNNING) currentRunning++;
        else if (pair.second == INTERMEDIATE) currentIntermediate++;
    }
    
    if (currentRunning < desiredRunning) {
        unsigned toActivate = desiredRunning - currentRunning;
        for (auto& pair : machineTiers) {
            if (pair.second == INTERMEDIATE && toActivate > 0) {
                ActivateMachine(pair.first, now);
                toActivate--;
            }
        }
    } else if (currentRunning > desiredRunning) {
        vector<pair<MachineId_t, unsigned>> loadPairs;
        
        for (auto& machineId : machines) {
            if (machineLoads[machineId] == 0) {
                loadPairs.push_back({machineId, 0});
            }
        }
        
        sort(loadPairs.begin(), loadPairs.end(), 
             [](const pair<MachineId_t, unsigned>& a, const pair<MachineId_t, unsigned>& b) {
                 return a.second < b.second;
             });
        
        unsigned toDeactivate = currentRunning - desiredRunning;
        for (auto& pair : loadPairs) {
            if (toDeactivate > 0 && pair.second == 0) {
                DeactivateMachine(pair.first, now);
                toDeactivate--;
            }
        }
    }
    
    unsigned currentTotal = currentRunning + currentIntermediate;
    unsigned desiredTotal = desiredRunning + desiredIntermediate;
    
    if (currentTotal < desiredTotal) {
        unsigned toWake = desiredTotal - currentTotal;
        for (auto& pair : machineTiers) {
            if (pair.second == SWITCHED_OFF && toWake > 0) {
                Machine_SetState(pair.first, S3);
                machineTiers[pair.first] = INTERMEDIATE;
                machineLoads[pair.first] = 0;
                toWake--;
                
                SimOutput("Scheduler::AdjustTiers(): Moving machine " + to_string(pair.first) + 
                         " from OFF to INTERMEDIATE", 3);
            }
        }
    } else if (currentIntermediate > desiredIntermediate) {
        unsigned toPowerOff = currentIntermediate - desiredIntermediate;
        for (auto& pair : machineTiers) {
            if (pair.second == INTERMEDIATE && toPowerOff > 0) {
                Machine_SetState(pair.first, S5);
                machineTiers[pair.first] = SWITCHED_OFF;
                machineLoads[pair.first] = 0;
                toPowerOff--;
                
                SimOutput("Scheduler::AdjustTiers(): Moving machine " + to_string(pair.first) + 
                         " from INTERMEDIATE to OFF", 3);
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

static Scheduler Scheduler;

/**
 * Initialize the scheduler
 * 
 * Called by the simulator to initialize the E-eco scheduler.
 */
void InitScheduler() {
    SimOutput("InitScheduler(): Initializing E-eco scheduler", 3);
    Scheduler.Init();
}

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
    Scheduler.NewTask(time, task_id);
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
    Scheduler.TaskComplete(time, task_id);
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
    Scheduler.MigrationComplete(time, vm_id);
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
    if (time % 1000000 == 0) {
        SimOutput("SchedulerCheck(): Called at " + to_string(time), 3);
    }
    Scheduler.PeriodicCheck(time);
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
    
    Scheduler.Shutdown(time);
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

