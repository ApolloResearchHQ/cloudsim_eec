//
//  Scheduler.cpp
//  CloudSim
//
//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
//

#include "Scheduler.hpp"

static bool migrating = false;
static unsigned task_count = 0;
static map<TaskId_t, Time_t> taskStartTimes;

/**
 * Initialize the Predictive scheduler
 * 
 * This method implements the initialization phase of the Predictive algorithm:
 * 1. Discovers all available machines in the cluster
 * 2. Creates VMs and attaches them to machines
 * 3. Initializes data structures for response time tracking
 * 4. Groups machines by CPU type for compatibility
 */
void Scheduler::Init() {
    // Find the parameters of the clusters
    SimOutput("Scheduler::Init(): Total number of machines is " + to_string(Machine_GetTotal()), 3);
    SimOutput("Scheduler::Init(): Initializing Predictive scheduler", 1);
    
    unsigned total_machines = Machine_GetTotal();
    
    for (unsigned i = 0; i < total_machines; i++) {
        MachineId_t machineId = MachineId_t(i);
        MachineInfo_t info = Machine_GetInfo(machineId);
        
        cpuTypeMachines[info.cpu].push_back(machineId);
        
        if (i < min(total_machines, (unsigned)16)) { // Start with a reasonable number of VMs
            machines.push_back(machineId);
            VMId_t vmId = VM_Create(LINUX, info.cpu);
            vms.push_back(vmId);
            
            vmToMachine[vmId] = machineId;
            machineToVMs[machineId].push_back(vmId);
            vmSizes[vmId] = 1; // Initial VM size (corresponds to P0 state)
            machineActive[machineId] = true;
            
            vmAverageResponseTime[vmId] = 0;
            vmResponseTimeSlope[vmId] = 0.0;
        } else {
            Machine_SetState(machineId, S5);
            machineActive[machineId] = false;
        }
    }
    
    for (unsigned i = 0; i < vms.size(); i++) {
        VM_Attach(vms[i], machines[i]);
    }
    
    SimOutput("Scheduler::Init(): Successfully initialized the Predictive scheduler", 3);
}

/**
 * Handle VM migration completion
 * 
 * Updates tracking data structures after a VM has been migrated.
 * 
 * @param time Current simulation time
 * @param vm_id ID of the VM that completed migration
 */
void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    MachineId_t oldMachine = vmToMachine[vm_id];
    MachineId_t newMachine = VM_GetInfo(vm_id).machine_id;
    
    auto& vmList = machineToVMs[oldMachine];
    vmList.erase(remove(vmList.begin(), vmList.end(), vm_id), vmList.end());
    machineToVMs[newMachine].push_back(vm_id);
    
    vmToMachine[vm_id] = newMachine;
    
    SimOutput("Scheduler::MigrationComplete(): VM " + to_string(vm_id) + " migration complete", 3);
}

/**
 * Handle new task arrival
 * 
 * Allocates a new task to an appropriate VM based on CPU compatibility
 * and current response times. Tracks task start time for response time calculation.
 * 
 * @param now Current simulation time
 * @param task_id ID of the newly arrived task
 */
void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
    taskStartTimes[task_id] = now;
    
    CPUType_t requiredCPU = RequiredCPUType(task_id);
    Priority_t priority = (task_id == 0 || task_id == 64) ? HIGH_PRIORITY : MID_PRIORITY;
    
    VMId_t bestVmId = (VMId_t)-1;
    Time_t lowestResponseTime = (Time_t)-1;
    
    for (auto& vmId : vms) {
        MachineId_t machineId = vmToMachine[vmId];
        MachineInfo_t machineInfo = Machine_GetInfo(machineId);
        
        if (machineInfo.cpu != requiredCPU) {
            continue;
        }
        
        if (!machineActive[machineId]) {
            continue;
        }
        
        Time_t responseTime = vmAverageResponseTime[vmId];
        if (bestVmId == (VMId_t)-1 || responseTime < lowestResponseTime) {
            bestVmId = vmId;
            lowestResponseTime = responseTime;
        }
    }
    
    if (bestVmId == (VMId_t)-1) {
        MachineId_t compatibleMachine = FindCompatibleMachine(requiredCPU);
        if (compatibleMachine != (MachineId_t)-1) {
            ActivateMachine(compatibleMachine);
            
            VMId_t newVmId = VM_Create(LINUX, requiredCPU);
            VM_Attach(newVmId, compatibleMachine);
            
            vms.push_back(newVmId);
            vmToMachine[newVmId] = compatibleMachine;
            machineToVMs[compatibleMachine].push_back(newVmId);
            vmSizes[newVmId] = 1;
            vmAverageResponseTime[newVmId] = 0;
            vmResponseTimeSlope[newVmId] = 0.0;
            
            bestVmId = newVmId;
        }
    }
    
    if (bestVmId != (VMId_t)-1) {
        VM_AddTask(bestVmId, task_id, priority);
        task_count++;
        
        if (task_count % 10 == 0) {
            SimOutput("Scheduler::NewTask(): Allocated task " + to_string(task_id) + 
                      " to VM " + to_string(bestVmId), 3);
        }
    } else {
        SimOutput("Scheduler::NewTask(): Could not allocate task " + to_string(task_id) + 
                  " due to no compatible resources", 1);
    }
}

/**
 * Perform periodic maintenance
 * 
 * Analyzes VM performance and looks for opportunities to turn off machines.
 * This is called periodically by the simulator.
 * 
 * @param now Current simulation time
 */
void Scheduler::PeriodicCheck(Time_t now) {
    bool shouldLog = (now % 1000000 == 0);
    
    if (shouldLog) {
        SimOutput("Scheduler::PeriodicCheck(): Checking system at time " + to_string(now), 3);
    }
    
    if (task_count % MACHINE_CONSOLIDATION_INTERVAL == 0) {
        if (ConsolidateVMs(now) && shouldLog) {
            SimOutput("Scheduler::PeriodicCheck(): Consolidated VMs and turned off machines", 3);
        }
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
    for (auto& vmId : vms) {
        VM_Shutdown(vmId);
    }
    
    SimOutput("Scheduler::Shutdown(): Predictive scheduler shutdown complete", 1);
}

/**
 * Handle task completion
 * 
 * Updates response time measurements and adjusts VM sizes based on trends.
 * 
 * @param now Current simulation time
 * @param task_id ID of the completed task
 */
void Scheduler::TaskComplete(Time_t now, TaskId_t task_id) {
    auto startTimeIter = taskStartTimes.find(task_id);
    if (startTimeIter == taskStartTimes.end()) {
        return; // Task start time not found
    }
    
    Time_t startTime = startTimeIter->second;
    Time_t responseTime = now - startTime;
    
    VMId_t executingVmId = (VMId_t)-1;
    for (auto& vmId : vms) {
        VMInfo_t vmInfo = VM_GetInfo(vmId);
        for (auto& activeTask : vmInfo.active_tasks) {
            if (activeTask == task_id) {
                executingVmId = vmId;
                break;
            }
        }
        if (executingVmId != (VMId_t)-1) {
            break;
        }
    }
    
    if (executingVmId == (VMId_t)-1) {
        return; // VM not found for this task
    }
    
    UpdateResponseTimeData(executingVmId, task_id, startTime, now);
    
    if (task_count % VM_CHECK_INTERVAL == 0) {
        AdjustVMSize(executingVmId, now);
    }
    
    taskStartTimes.erase(task_id);
    
    if (task_count % 20 == 0) {
        SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) + 
                  " completed with response time " + to_string(responseTime), 3);
    }
}

/**
 * Update response time data for a VM
 * 
 * Adds new response time data and updates average and slope calculations.
 * 
 * @param vmId ID of the VM
 * @param taskId ID of the completed task
 * @param startTime Start time of the task
 * @param endTime End time of the task
 */
void Scheduler::UpdateResponseTimeData(VMId_t vmId, TaskId_t taskId, Time_t startTime, Time_t endTime) {
    Time_t responseTime = endTime - startTime;
    
    ResponseTimeData data = {startTime, endTime, responseTime};
    auto& responseTimeHistory = vmResponseTimes[vmId];
    
    if (responseTimeHistory.size() >= RESPONSE_TIME_HISTORY_SIZE) {
        responseTimeHistory.pop_front();
    }
    responseTimeHistory.push_back(data);
    
    Time_t totalResponseTime = 0;
    for (auto& data : responseTimeHistory) {
        totalResponseTime += data.responseTime;
    }
    vmAverageResponseTime[vmId] = totalResponseTime / responseTimeHistory.size();
    
    vmResponseTimeSlope[vmId] = CalculateResponseTimeSlope(vmId);
}

/**
 * Calculate the slope of response time change
 * 
 * A positive slope indicates increasing response times (degrading performance),
 * A negative slope indicates decreasing response times (improving performance).
 * 
 * @param vmId ID of the VM
 * @return Slope as a percentage change (-1.0 to 1.0)
 */
double Scheduler::CalculateResponseTimeSlope(VMId_t vmId) {
    auto& history = vmResponseTimes[vmId];
    if (history.size() < 2) {
        return 0.0; // Not enough data points
    }
    
    unsigned midpoint = history.size() / 2;
    Time_t firstHalfTotal = 0;
    Time_t secondHalfTotal = 0;
    
    for (unsigned i = 0; i < midpoint; i++) {
        firstHalfTotal += history[i].responseTime;
    }
    
    for (unsigned i = midpoint; i < history.size(); i++) {
        secondHalfTotal += history[i].responseTime;
    }
    
    Time_t firstHalfAvg = firstHalfTotal / midpoint;
    Time_t secondHalfAvg = secondHalfTotal / (history.size() - midpoint);
    
    if (firstHalfAvg == 0) return 0.0;
    return (double)(secondHalfAvg - firstHalfAvg) / firstHalfAvg;
}

/**
 * Adjust VM size based on response time trend
 * 
 * If response times are increasing, increase VM size.
 * If response times are decreasing, decrease VM size.
 * 
 * @param vmId ID of the VM to adjust
 * @param now Current simulation time
 */
void Scheduler::AdjustVMSize(VMId_t vmId, Time_t now) {
    double slope = vmResponseTimeSlope[vmId];
    unsigned currentSize = vmSizes[vmId];
    MachineId_t machineId = vmToMachine[vmId];
    
    if (slope > RESPONSE_TIME_INCREASE_THRESHOLD) {
        if (currentSize < 3) {
            unsigned newSize = currentSize + 1;
            vmSizes[vmId] = newSize;
            
            CPUPerformance_t newPState = CPUPerformance_t(4 - newSize);
            MachineInfo_t info = Machine_GetInfo(machineId);
            for (unsigned i = 0; i < info.num_cpus; i++) {
                Machine_SetCorePerformance(machineId, i, newPState);
            }
            
            SimOutput("Scheduler::AdjustVMSize(): Increased size of VM " + to_string(vmId) + 
                      " due to rising response times", 2);
        }
    } else if (slope < RESPONSE_TIME_DECREASE_THRESHOLD) {
        if (currentSize > 1) {
            unsigned newSize = currentSize - 1;
            vmSizes[vmId] = newSize;
            
            CPUPerformance_t newPState = CPUPerformance_t(4 - newSize);
            MachineInfo_t info = Machine_GetInfo(machineId);
            for (unsigned i = 0; i < info.num_cpus; i++) {
                Machine_SetCorePerformance(machineId, i, newPState);
            }
            
            SimOutput("Scheduler::AdjustVMSize(): Decreased size of VM " + to_string(vmId) + 
                      " due to falling response times", 2);
            
            ConsolidateVMs(now);
        }
    }
}

/**
 * Consolidate VMs and turn off empty machines
 * 
 * Identifies machines with no VMs and turns them off to save energy.
 * 
 * @param now Current simulation time
 * @return True if any machines were turned off, false otherwise
 */
bool Scheduler::ConsolidateVMs(Time_t now) {
    bool turnedOffMachines = false;
    
    for (auto& machineId : machines) {
        if (IsMachineEmpty(machineId) && machineActive[machineId]) {
            DeactivateMachine(machineId, now);
            turnedOffMachines = true;
        }
    }
    
    return turnedOffMachines;
}

/**
 * Check if a machine is empty (has no VMs or tasks)
 * 
 * @param machineId ID of the machine to check
 * @return True if the machine has no VMs or tasks, false otherwise
 */
bool Scheduler::IsMachineEmpty(MachineId_t machineId) {
    auto vmsIter = machineToVMs.find(machineId);
    if (vmsIter == machineToVMs.end() || vmsIter->second.empty()) {
        return true;
    }
    
    for (auto& vmId : vmsIter->second) {
        VMInfo_t vmInfo = VM_GetInfo(vmId);
        if (!vmInfo.active_tasks.empty()) {
            return false;
        }
    }
    
    return true;
}

/**
 * Activate a machine
 * 
 * @param machineId ID of the machine to activate
 */
void Scheduler::ActivateMachine(MachineId_t machineId) {
    Machine_SetState(machineId, S0);
    machineActive[machineId] = true;
    SimOutput("Scheduler::ActivateMachine(): Activated machine " + to_string(machineId), 2);
}

/**
 * Deactivate a machine
 * 
 * @param machineId ID of the machine to deactivate
 * @param now Current simulation time
 */
void Scheduler::DeactivateMachine(MachineId_t machineId, Time_t now) {
    Machine_SetState(machineId, S5);
    machineActive[machineId] = false;
    SimOutput("Scheduler::DeactivateMachine(): Deactivated machine " + to_string(machineId), 2);
}

/**
 * Find a compatible machine for a specific CPU type
 * 
 * @param cpuType Required CPU type
 * @return ID of a compatible machine, or -1 if none found
 */
MachineId_t Scheduler::FindCompatibleMachine(CPUType_t cpuType) {
    auto cpuGroupIter = cpuTypeMachines.find(cpuType);
    if (cpuGroupIter != cpuTypeMachines.end()) {
        for (auto& machineId : cpuGroupIter->second) {
            if (!machineActive[machineId]) {
                return machineId;
            }
        }
    }
    return (MachineId_t)-1;
}

// Public interface below

static Scheduler Scheduler;

void InitScheduler() {
    SimOutput("InitScheduler(): Initializing Predictive scheduler", 1);
    Scheduler.Init();
}

void HandleNewTask(Time_t time, TaskId_t task_id) {
    Scheduler.NewTask(time, task_id);
}

void HandleTaskCompletion(Time_t time, TaskId_t task_id) {
    Scheduler.TaskComplete(time, task_id);
}

void MemoryWarning(Time_t time, MachineId_t machine_id) {
    // The simulator is alerting you that machine identified by machine_id is overcommitted
    SimOutput("MemoryWarning(): Overflow at " + to_string(machine_id) + " was detected", 1);
}

void MigrationDone(Time_t time, VMId_t vm_id) {
    // The function is called on to alert you that migration is complete
    Scheduler.MigrationComplete(time, vm_id);
    migrating = false;
}

void SchedulerCheck(Time_t time) {
    // This function is called periodically by the simulator, no specific event
    Scheduler.PeriodicCheck(time);
}

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

void SLAWarning(Time_t time, TaskId_t task_id) {
    SimOutput("SLAWarning(): SLA violation risk for task " + to_string(task_id), 1);
}

void StateChangeComplete(Time_t time, MachineId_t machine_id) {
    // Called in response to an earlier request to change the state of a machine
}

