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

/**
 * Initialize the pmapper scheduler
 * 
 * This method:
 * 1. Discovers all available machines in the cluster
 * 2. Creates VMs with appropriate CPU types for each machine
 * 3. Initializes tracking data structures for utilization
 * 4. Builds the initial list of machines sorted by energy consumption
 * 
 * The sorting of machines by energy consumption is a key component 
 * of the pmapper algorithm for energy-efficient task allocation.
 */
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

/**
 * Handle VM migration completion
 * 
 * Updates tracking data structures after a VM has been migrated.
 * This is part of the workload consolidation strategy in pmapper,
 * allowing tasks to be moved between machines for better energy efficiency.
 * 
 * @param time Current simulation time
 * @param vm_id ID of the VM that completed migration
 */
void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    // Update your data structure. The VM now can receive new tasks
    SimOutput("Scheduler::MigrationComplete(): VM " + to_string(vm_id) + " migration completed at " + to_string(time), 3);
    migrating = false;
}

/**
 * Handle new task arrival
 * 
 * Implements the task allocation strategy of pmapper:
 * 1. Ensure machines are sorted by energy consumption
 * 2. Try to allocate the task to the most energy-efficient machine with compatible CPU
 * 3. Report SLA violation if no suitable machine is found
 * 4. Turn off any unused machines after allocation
 * 
 * @param now Current simulation time
 * @param task_id ID of the newly arrived task
 */
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
              " and VM type " + to_string(requiredVM), 3);
    
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
                          " to machine " + to_string(machineId), 3);
                break;
            } catch (const exception& e) {
                SimOutput("Scheduler::NewTask(): Exception when adding task: " + string(e.what()), 1);
                continue; // Try next machine
            }
        }
    }
    
    if (!allocated) {
        SimOutput("Scheduler::NewTask(): Could not allocate task " + to_string(task_id) + 
                  " - SLA violation", 1);
    }
    
    CheckAndTurnOffUnusedMachines();
}

/**
 * Perform periodic maintenance
 * 
 * This method is called periodically to:
 * 1. Rebuild the energy-sorted machine list occasionally
 * 2. Report current system statistics
 * 3. Turn off any unused machines to save energy
 * 
 * Regular maintenance is important for the pmapper algorithm
 * to adapt to changing energy consumption patterns.
 * 
 * @param now Current simulation time
 */
void Scheduler::PeriodicCheck(Time_t now) {
    SimOutput("Scheduler::PeriodicCheck(): Running periodic check at time " + to_string(now), 4);
    
    static int check_count = 0;
    if (check_count++ % 10 == 0) {
        BuildEnergySortedMachineList();
    }
    
    SimOutput("Scheduler::PeriodicCheck(): Total machines: " + to_string(machines.size()), 4);
    SimOutput("Scheduler::PeriodicCheck(): Total energy: " + to_string(Machine_GetClusterEnergy()), 3);
    
    CheckAndTurnOffUnusedMachines();
}

/**
 * Perform final cleanup and reporting
 * 
 * This method is called at the end of the simulation to:
 * 1. Shutdown all VMs cleanly
 * 2. Report final statistics including energy consumption
 * 
 * @param time Final simulation time
 */
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

/**
 * Handle task completion
 * 
 * Implements the workload consolidation aspect of pmapper:
 * 1. Find the machine where the task was running
 * 2. Update the machine's utilization count
 * 3. Sort machines by utilization and divide into two halves
 * 4. Find the smallest task on the least utilized machine
 * 5. Migrate it to a highly utilized machine with compatible CPU
 * 6. Turn off any unused machines
 * 
 * This method is critical for the energy efficiency of pmapper,
 * as it continuously works to consolidate workloads onto fewer machines.
 * 
 * @param now Current simulation time
 * @param task_id ID of the completed task
 */
void Scheduler::TaskComplete(Time_t now, TaskId_t task_id) {
    SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) + " is complete at " + to_string(now), 3);
    
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


/**
 * Build list of machines sorted by energy consumption
 * 
 * Core component of the pmapper algorithm that:
 * 1. Gets energy consumption data for each machine
 * 2. Creates a list of machine ID and energy consumption pairs
 * 3. Sorts the list from lowest to highest energy consumption
 * 
 * This sorted list is used for energy-efficient task allocation.
 * The method includes an optimization to only rebuild the list
 * periodically rather than on every call.
 */
void Scheduler::BuildEnergySortedMachineList() {
    static int rebuild_count = 0;
    if (!energySortedMachines.empty() && rebuild_count++ % 5 != 0) {
        return;
    }
    
    SimOutput("Scheduler::BuildEnergySortedMachineList(): Rebuilding energy sorted machine list", 4);
    
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

/**
 * Find or create a VM for a specific machine
 * 
 * This method:
 * 1. Searches for an existing VM associated with the machine
 * 2. Creates a new VM with the machine's CPU type if none exists
 * 3. Ensures CPU compatibility between the VM and the machine
 * 
 * @param machineId ID of the machine to find/create VM for
 * @param vmType Type of VM to create if needed
 * @return ID of the VM
 */
VMId_t Scheduler::FindVMForMachine(MachineId_t machineId, VMType_t vmType) {
    for (unsigned i = 0; i < machines.size(); i++) {
        if (machines[i] == machineId) {
            return vms[i];
        }
    }
    
    MachineInfo_t info = Machine_GetInfo(machineId);
    SimOutput("Scheduler::FindVMForMachine(): Creating new VM with CPU type " + to_string(info.cpu) + 
              " and VM type " + to_string(vmType) + " for machine " + to_string(machineId), 3);
    
    VMId_t newVM = VM_Create(vmType, info.cpu);
    VM_Attach(newVM, machineId);
    return newVM;
}

/**
 * Power off machines with no active tasks
 * 
 * Key energy efficiency feature that reduces power consumption
 * by turning off idle machines. This method:
 * 1. Checks all machines in the energy-sorted list
 * 2. Identifies machines with zero utilization
 * 3. Powers them off (sets to S5 state) to save energy
 */
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

/**
 * Find the machine that a task is running on
 * 
 * Searches all machines and their associated VMs to find which
 * machine is currently running a specific task.
 * 
 * @param taskId ID of the task to locate
 * @return ID of the machine running the task, or -1 if not found
 */
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

/**
 * Find the smallest task running on a machine
 * 
 * Key part of the workload consolidation strategy that identifies
 * the smallest task (by memory usage) that can be migrated from
 * a lightly loaded machine to a more heavily loaded one.
 * 
 * @param machineId ID of the machine to search
 * @return ID of the smallest task, or -1 if no tasks found
 */
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

/**
 * Initialize the scheduler
 * 
 * Public interface method that creates and initializes the pmapper scheduler.
 */
void InitScheduler() {
    SimOutput("InitScheduler(): Initializing scheduler", 3);
    Scheduler.Init();
}

/**
 * Handle new task arrival events
 * 
 * Public interface method that delegates task allocation to the pmapper scheduler.
 * 
 * @param time Current simulation time
 * @param task_id ID of the newly arrived task
 */
void HandleNewTask(Time_t time, TaskId_t task_id) {
    SimOutput("HandleNewTask(): Received new task " + to_string(task_id) + " at time " + to_string(time), 4);
    Scheduler.NewTask(time, task_id);
}

/**
 * Handle task completion events
 * 
 * Public interface method that delegates to the scheduler's TaskComplete method.
 * 
 * @param time Current simulation time
 * @param task_id ID of the completed task
 */
void HandleTaskCompletion(Time_t time, TaskId_t task_id) {
    SimOutput("HandleTaskCompletion(): Task " + to_string(task_id) + " completed at time " + to_string(time), 4);
    Scheduler.TaskComplete(time, task_id);
}

/**
 * Handle memory warning events
 * 
 * Called when a machine's memory is overcommitted.
 * 
 * @param time Current simulation time
 * @param machine_id ID of the machine with memory overflow
 */
void MemoryWarning(Time_t time, MachineId_t machine_id) {
    // The simulator is alerting you that machine identified by machine_id is overcommitted
    SimOutput("MemoryWarning(): Overflow at " + to_string(machine_id) + " was detected at time " + to_string(time), 1);
}

/**
 * Handle VM migration completion events
 * 
 * Called when a VM migration completes, allowing the scheduler
 * to update its tracking data structures.
 * 
 * @param time Current simulation time
 * @param vm_id ID of the VM that completed migration
 */
void MigrationDone(Time_t time, VMId_t vm_id) {
    // The function is called on to alert you that migration is complete
    SimOutput("MigrationDone(): Migration of VM " + to_string(vm_id) + " was completed at time " + to_string(time), 3);
    Scheduler.MigrationComplete(time, vm_id);
    migrating = false;
}

/**
 * Handle periodic scheduler check events
 * 
 * Called periodically to allow the scheduler to perform maintenance
 * tasks and optimizations.
 * 
 * @param time Current simulation time
 */
void SchedulerCheck(Time_t time) {
    // This function is called periodically by the simulator, no specific event
    SimOutput("SchedulerCheck(): SchedulerCheck() called at " + to_string(time), 5);
    Scheduler.PeriodicCheck(time);
}

/**
 * Handle simulation completion event
 * 
 * Called at the end of the simulation to report final statistics
 * and perform cleanup.
 * 
 * @param time Final simulation time
 */
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

/**
 * Handle SLA violation warnings
 * 
 * Called when a task is at risk of violating its SLA.
 * 
 * @param time Current simulation time
 * @param task_id ID of the task at risk of violating its SLA
 */
/**
 * Handle SLA violation for a task
 * 
 * Attempts to find a better machine for a task that is experiencing
 * SLA violations, particularly due to CPU compatibility issues.
 * 
 * @param taskId ID of the task experiencing SLA violation
 * @param currentMachine Current machine running the task, or -1 if unknown
 * @return True if successfully handled, false otherwise
 */
bool Scheduler::HandleSLAViolation(TaskId_t taskId, MachineId_t currentMachine) {
    SimOutput("Scheduler::HandleSLAViolation(): Handling SLA violation for task " + to_string(taskId), 1);
    
    if (currentMachine == (MachineId_t)-1) {
        currentMachine = FindMachineForTask(taskId);
        if (currentMachine == (MachineId_t)-1) {
            SimOutput("Scheduler::HandleSLAViolation(): Could not find machine for task " + to_string(taskId), 1);
            return false;
        }
    }
    
    CPUType_t requiredCPU = RequiredCPUType(taskId);
    VMType_t requiredVM = RequiredVMType(taskId);
    unsigned taskMemory = GetTaskMemory(taskId);
    
    SimOutput("Scheduler::HandleSLAViolation(): Task " + to_string(taskId) + 
              " requires CPU type " + to_string(requiredCPU), 1);
    
    for (auto& machine_pair : energySortedMachines) {
        MachineId_t candidateMachine = machine_pair.first;
        
        if (candidateMachine == currentMachine) {
            continue;
        }
        
        MachineInfo_t info = Machine_GetInfo(candidateMachine);
        
        if (info.s_state == S5) {
            continue;
        }
        
        if (info.cpu == requiredCPU) {
            if (info.memory_used + taskMemory <= info.memory_size) {
                VMId_t sourceVM = FindVMForMachine(currentMachine);
                VMId_t destVM = FindVMForMachine(candidateMachine, requiredVM);
                
                try {
                    VM_RemoveTask(sourceVM, taskId);
                    
                    Priority_t priority = (taskId == 0 || taskId == 64) ? HIGH_PRIORITY : MID_PRIORITY;
                    VM_AddTask(destVM, taskId, priority);
                    
                    machineUtilization[currentMachine]--;
                    machineUtilization[candidateMachine]++;
                    
                    SimOutput("Scheduler::HandleSLAViolation(): Migrated task " + to_string(taskId) + 
                              " from machine " + to_string(currentMachine) + 
                              " to machine " + to_string(candidateMachine), 1);
                    return true;
                } catch (const exception& e) {
                    SimOutput("Scheduler::HandleSLAViolation(): Failed to migrate task: " + string(e.what()), 1);
                }
            }
        }
    }
    
    for (auto& machine_pair : energySortedMachines) {
        MachineId_t candidateMachine = machine_pair.first;
        
        if (candidateMachine == currentMachine) {
            continue;
        }
        
        MachineInfo_t info = Machine_GetInfo(candidateMachine);
        
        if (info.s_state != S5) {
            continue;
        }
        
        if (info.cpu == requiredCPU) {
            Machine_SetState(candidateMachine, S0);
            SimOutput("Scheduler::HandleSLAViolation(): Powered on machine " + to_string(candidateMachine) + 
                      " to handle SLA violation for task " + to_string(taskId), 1);
            
            
            info = Machine_GetInfo(candidateMachine); // Get updated info
            if (info.memory_used + taskMemory <= info.memory_size) {
                VMId_t sourceVM = FindVMForMachine(currentMachine);
                VMId_t destVM = FindVMForMachine(candidateMachine, requiredVM);
                
                try {
                    VM_RemoveTask(sourceVM, taskId);
                    
                    Priority_t priority = (taskId == 0 || taskId == 64) ? HIGH_PRIORITY : MID_PRIORITY;
                    VM_AddTask(destVM, taskId, priority);
                    
                    machineUtilization[currentMachine]--;
                    machineUtilization[candidateMachine]++;
                    
                    SimOutput("Scheduler::HandleSLAViolation(): Migrated task " + to_string(taskId) + 
                              " from machine " + to_string(currentMachine) + 
                              " to machine " + to_string(candidateMachine), 1);
                    return true;
                } catch (const exception& e) {
                    SimOutput("Scheduler::HandleSLAViolation(): Failed to migrate task: " + string(e.what()), 1);
                }
            }
        }
    }
    
    SimOutput("Scheduler::HandleSLAViolation(): Could not find suitable machine for task " + to_string(taskId), 1);
    return false;
}

void SLAWarning(Time_t time, TaskId_t task_id) {
    SimOutput("SLAWarning(): SLA violation for task " + to_string(task_id) + " at time " + to_string(time), 1);
    Scheduler.HandleSLAViolation(task_id, (MachineId_t)-1); // -1 means unknown machine, will be looked up
}

/**
 * Handle machine state change completion events
 * 
 * Called when a machine completes a state change (e.g., powering on or off).
 * 
 * @param time Current simulation time
 * @param machine_id ID of the machine that completed a state change
 */
void StateChangeComplete(Time_t time, MachineId_t machine_id) {
    // Called in response to an earlier request to change the state of a machine
    SimOutput("StateChangeComplete(): State change for machine " + to_string(machine_id) + " completed at time " + to_string(time), 3);
}

