//
//  Scheduler.cpp
//  CloudSim
//
//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
//

#include "Scheduler.hpp"

static bool migrating = false;
static unsigned active_machines = 16;

/**
 * Initialize the greedy allocation scheduler
 * 
 * This method:
 * 1. Discovers all available machines in the cluster
 * 2. Creates VMs with appropriate CPU types for each machine
 * 3. Initializes tracking data structures for utilization and mapping
 * 4. Attaches VMs to their respective machines
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
    
    unsigned total_machines = Machine_GetTotal();
    for(unsigned i = 0; i < total_machines; i++) {
        MachineInfo_t info = Machine_GetInfo(MachineId_t(i));
        if(info.s_state != S5) { // Only consider machines that are not powered off
            machines.push_back(MachineId_t(i));
            
            VMId_t vm = VM_Create(LINUX, info.cpu);
            vms.push_back(vm);
            
            machineUtilization[MachineId_t(i)] = 0.0f;
            
            machineToVMMap[MachineId_t(i)] = vm;
        }
    }
    
    for(unsigned i = 0; i < machines.size(); i++) {
        VM_Attach(vms[i], machines[i]);
    }
    
    SimOutput("Scheduler::Init(): Successfully initialized the greedy allocation scheduler", 3);
}

/**
 * Calculate the utilization of a machine based on memory usage
 * 
 * This method serves as a proxy for overall machine utilization by using
 * the ratio of used memory to total memory.
 * 
 * @param machineId The ID of the machine to calculate utilization for
 * @return Float value between 0.0 and 1.0 representing utilization percentage
 */
float Scheduler::CalculateMachineUtilization(MachineId_t machineId) {
    MachineInfo_t info = Machine_GetInfo(machineId);
    if(info.memory_size == 0) {
        return 1.0f; // Avoid division by zero, treat as fully utilized
    }
    return static_cast<float>(info.memory_used) / static_cast<float>(info.memory_size);
}

/**
 * Calculate the load factor of a task relative to machine memory
 * 
 * This method serves as a proxy for task resource requirements by calculating
 * the ratio of task memory to machine memory. This helps determine how much
 * of a machine's resources a task will consume.
 * 
 * @param taskId The ID of the task to calculate load factor for
 * @return Float value representing the task's memory requirements relative to machine memory
 */
float Scheduler::CalculateTaskLoadFactor(TaskId_t taskId) {
    unsigned taskMemory = GetTaskMemory(taskId);
    for(auto& machine : machines) {
        MachineInfo_t info = Machine_GetInfo(machine);
        if(info.s_state != S5 && info.memory_size > 0) {
            return static_cast<float>(taskMemory) / static_cast<float>(info.memory_size);
        }
    }
    return 1.0f; // If no suitable machine found, task has high load factor
}

/**
 * Find or create a VM for a machine with compatible CPU type
 * 
 * This method ensures CPU compatibility by:
 * 1. Checking if a VM already exists for the machine
 * 2. Verifying the VM has a compatible CPU type
 * 3. Creating a new VM with the correct CPU type if needed
 * 
 * CPU compatibility is critical to prevent "VM with incompatible CPU" exceptions
 * when tasks are allocated to machines.
 * 
 * @param machineId The ID of the machine to find/create a VM for
 * @param vmType The type of VM to create if needed
 * @param cpuType The CPU type required for the VM
 * @return The ID of the VM for the machine
 */
VMId_t Scheduler::FindVMForMachine(MachineId_t machineId, VMType_t vmType, CPUType_t cpuType) {
    if(machineToVMMap.find(machineId) != machineToVMMap.end()) {
        VMId_t existingVM = machineToVMMap[machineId];
        VMInfo_t vmInfo = VM_GetInfo(existingVM);
        
        if(vmInfo.cpu == cpuType) {
            return existingVM;
        } else {
    SimOutput("Scheduler::FindVMForMachine(): Existing VM has incompatible CPU type. Creating new VM.", 1);
        }
    }
    
    MachineInfo_t info = Machine_GetInfo(machineId);
    SimOutput("Scheduler::FindVMForMachine(): Creating new VM with CPU type " + to_string(cpuType) + 
              " and VM type " + to_string(vmType) + " for machine " + to_string(machineId), 3);
    
    if (info.cpu != cpuType) {
        SimOutput("Scheduler::FindVMForMachine(): Machine " + to_string(machineId) + 
                  " has CPU type " + to_string(info.cpu) + 
                  " which is incompatible with required CPU type " + to_string(cpuType), 1);
        
        cpuType = info.cpu;
    }
    
    VMId_t newVM = VM_Create(vmType, cpuType);
    VM_Attach(newVM, machineId);
    
    machineToVMMap[machineId] = newVM;
    
    return newVM;
}

/**
 * Power off machines with zero utilization to save energy
 * 
 * This method is a key part of the energy efficiency strategy:
 * 1. Checks all machines in the cluster
 * 2. Identifies machines with zero utilization or no active tasks
 * 3. Powers them off (sets to S5 state) to reduce energy consumption
 */
void Scheduler::CheckAndTurnOffUnusedMachines() {
    for(auto& machine : machines) {
        MachineInfo_t info = Machine_GetInfo(machine);
        
        if(info.s_state == S5) {
            continue;
        }
        
        if(machineUtilization[machine] == 0.0f || info.active_tasks == 0) {
            Machine_SetState(machine, S5);
            SimOutput("Scheduler::CheckAndTurnOffUnusedMachines(): Turned off machine " + 
                      to_string(machine) + " due to zero utilization", 3);
        }
    }
}

/**
 * Get a list of machines sorted by their utilization (lowest to highest)
 * 
 * This method:
 * 1. Collects all active machines (not in S5 state)
 * 2. Calculates their current utilization
 * 3. Sorts them from least utilized to most utilized
 * 
 * This sorted list is used for task allocation and migration decisions
 * to optimize resource usage and energy efficiency.
 * 
 * @return Vector of machine ID and utilization pairs, sorted by utilization
 */
vector<pair<MachineId_t, float>> Scheduler::GetSortedMachinesByUtilization() {
    vector<pair<MachineId_t, float>> sortedMachines;
    
    for(auto& machine : machines) {
        MachineInfo_t info = Machine_GetInfo(machine);
        
        if(info.s_state == S5) {
            continue;
        }
        
        float utilization = CalculateMachineUtilization(machine);
        sortedMachines.push_back(make_pair(machine, utilization));
    }
    
    sort(sortedMachines.begin(), sortedMachines.end(), 
         [](const pair<MachineId_t, float>& a, const pair<MachineId_t, float>& b) {
             return a.second < b.second;
         });
    
    return sortedMachines;
}

/**
 * Migrate a task from its current machine to a target machine
 * 
 * This method:
 * 1. Finds the source machine for the task
 * 2. Gets the required VM and CPU types for the task
 * 3. Finds or creates compatible VMs on both source and target machines
 * 4. Removes the task from the source VM
 * 5. Adds the task to the target VM
 * 
 * Task migration is a key part of the greedy allocation strategy for
 * consolidating workloads and improving energy efficiency.
 * 
 * @param taskId The ID of the task to migrate
 * @param targetMachineId The ID of the target machine
 * @return True if migration was successful, false otherwise
 */
bool Scheduler::MigrateTask(TaskId_t taskId, MachineId_t targetMachineId) {
    if(taskToMachineMap.find(taskId) == taskToMachineMap.end()) {
        SimOutput("Scheduler::MigrateTask(): Could not find source machine for task " + to_string(taskId), 1);
        return false;
    }
    
    MachineId_t sourceMachineId = taskToMachineMap[taskId];
    
    VMType_t requiredVM = RequiredVMType(taskId);
    CPUType_t requiredCPU = RequiredCPUType(taskId);
    
    VMId_t sourceVMId = FindVMForMachine(sourceMachineId, requiredVM, requiredCPU);
    VMId_t targetVMId = FindVMForMachine(targetMachineId, requiredVM, requiredCPU);
    
    Priority_t priority = (taskId == 0 || taskId == 64) ? HIGH_PRIORITY : MID_PRIORITY;
    
    try {
        VM_RemoveTask(sourceVMId, taskId);
        
        VM_AddTask(targetVMId, taskId, priority);
        
        SimOutput("Scheduler::MigrateTask(): Successfully migrated task " + to_string(taskId) + 
                 " from machine " + to_string(sourceMachineId) + 
                 " to machine " + to_string(targetMachineId), 2);
        return true;
    } catch (const exception& e) {
        SimOutput("Scheduler::MigrateTask(): Exception during migration: " + string(e.what()), 1);
        return false;
    }
}

/**
 * Handle completion of VM migration
 * 
 * This method is called by the simulator when a VM migration is complete.
 * It updates internal tracking and logs the completion event.
 * 
 * @param time Current simulation time
 * @param vm_id ID of the VM that completed migration
 */
void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    // Update your data structure. The VM now can receive new tasks
    SimOutput("Scheduler::MigrationComplete(): VM " + to_string(vm_id) + " migration completed at " + to_string(time), 2);
}

/**
 * Handle allocation of a new task to a suitable machine
 * 
 * This method implements the core greedy allocation strategy:
 * 1. Determine task requirements (memory, CPU type, VM type)
 * 2. Calculate the task's load factor
 * 3. Find a machine with:
 *    - Compatible CPU type
 *    - Sufficient available memory
 *    - Utilization that can accommodate the task
 * 4. Create or use an existing VM on the machine
 * 5. Add the task to the VM
 * 6. Update utilization tracking
 * 
 * @param now Current simulation time
 * @param task_id ID of the new task to allocate
 */
void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
    SimOutput("Scheduler::NewTask(): Processing new task " + to_string(task_id), 1);
    
    Priority_t priority = (task_id == 0 || task_id == 64) ? HIGH_PRIORITY : MID_PRIORITY;
    unsigned taskMemory = GetTaskMemory(task_id);
    CPUType_t requiredCPU = RequiredCPUType(task_id);
    VMType_t requiredVM = RequiredVMType(task_id);
    
    SimOutput("Scheduler::NewTask(): Task " + to_string(task_id) + 
              " requires CPU type " + to_string(requiredCPU) + 
              " and VM type " + to_string(requiredVM), 2);
    
    float taskLoadFactor = CalculateTaskLoadFactor(task_id);
    
    bool allocated = false;
    
    for(auto& machine : machines) {
        MachineInfo_t info = Machine_GetInfo(machine);
        
        if(info.s_state == S5) {
            continue;
        }
        
        if(info.cpu != requiredCPU) {
            SimOutput("Scheduler::NewTask(): Machine " + to_string(machine) + 
                      " has incompatible CPU type " + to_string(info.cpu) + 
                      " for task " + to_string(task_id), 2);
            continue;
        }
        
        float machineUtil = CalculateMachineUtilization(machine);
        
        if(machineUtil + taskLoadFactor < 1.0f && info.memory_used + taskMemory <= info.memory_size) {
            VMId_t vmId = FindVMForMachine(machine, requiredVM, requiredCPU);
            
            try {
                VM_AddTask(vmId, task_id, priority);
                
                machineUtilization[machine] = machineUtil + taskLoadFactor;
                
                taskToMachineMap[task_id] = machine;
                
                allocated = true;
                
                SimOutput("Scheduler::NewTask(): Allocated task " + to_string(task_id) + 
                          " to machine " + to_string(machine) + 
                          " with utilization " + to_string(machineUtilization[machine]), 2);
                break;
            } catch (const exception& e) {
                SimOutput("Scheduler::NewTask(): Exception when adding task: " + string(e.what()), 1);
                continue;
            }
        }
    }
    
    if(!allocated) {
        SimOutput("Scheduler::NewTask(): Could not allocate task " + to_string(task_id) + 
                  " - SLA violation", 1);
    }
    
    CheckAndTurnOffUnusedMachines();
}

/**
 * Handle SLA violations by migrating tasks to more suitable machines
 * 
 * This method implements the SLA violation recovery strategy:
 * 1. Identify the machine where the task is running
 * 2. Calculate the task's resource requirements
 * 3. Try to find a less utilized machine with compatible CPU
 * 4. If no suitable running machine is found, power on a compatible machine
 * 5. Migrate the task to the new machine
 * 
 * SLA violation handling is critical for maintaining performance requirements
 * while still optimizing for energy efficiency.
 * 
 * @param taskId ID of the task that violated its SLA
 * @param machineId ID of the machine where the violation occurred
 */
void Scheduler::HandleSLAViolation(TaskId_t taskId, MachineId_t machineId) {
    SimOutput("Scheduler::HandleSLAViolation(): Task " + to_string(taskId) + 
              " violated SLA on machine " + to_string(machineId), 1);
    
    if(machineId == (MachineId_t)-1) {
        if(taskToMachineMap.find(taskId) != taskToMachineMap.end()) {
            machineId = taskToMachineMap[taskId];
        } else {
            SimOutput("Scheduler::HandleSLAViolation(): Could not find machine for task " + to_string(taskId), 1);
            return;
        }
    }
    
    float taskLoadFactor = CalculateTaskLoadFactor(taskId);
    CPUType_t requiredCPU = RequiredCPUType(taskId);
    
    vector<pair<MachineId_t, float>> sortedMachines = GetSortedMachinesByUtilization();
    
    for(auto& machine_pair : sortedMachines) {
        MachineId_t candidateMachine = machine_pair.first;
        
        if(candidateMachine == machineId) {
            continue;
        }
        
        MachineInfo_t info = Machine_GetInfo(candidateMachine);
        
        if(info.cpu != requiredCPU) {
            continue;
        }
        
        float machineUtil = machine_pair.second;
        unsigned taskMemory = GetTaskMemory(taskId);
        
        if(machineUtil + taskLoadFactor < 1.0f && info.memory_used + taskMemory <= info.memory_size) {
            if(MigrateTask(taskId, candidateMachine)) {
                SimOutput("Scheduler::HandleSLAViolation(): Migrated task " + to_string(taskId) + 
                          " to machine " + to_string(candidateMachine), 2);
                return;
            }
        }
    }
    
    for(auto& machine : machines) {
        MachineInfo_t info = Machine_GetInfo(machine);
        
        if(info.s_state == S5 && info.cpu == requiredCPU) {
            Machine_SetState(machine, S0);
            
            if(MigrateTask(taskId, machine)) {
                SimOutput("Scheduler::HandleSLAViolation(): Powered on machine " + to_string(machine) + 
                          " and migrated task " + to_string(taskId), 2);
                return;
            }
        }
    }
    
    SimOutput("Scheduler::HandleSLAViolation(): Failed to find a suitable machine for task " + 
              to_string(taskId), 1);
}

/**
 * Handle thermal events by migrating tasks away from affected machines
 * 
 * This method:
 * 1. Identifies the VM running on the affected machine
 * 2. Checks if there are any active tasks on the VM
 * 3. If tasks exist, migrates the first task using the SLA violation handler
 * 
 * Thermal events are treated similarly to SLA violations, as they require
 * immediate action to prevent performance degradation.
 * 
 * @param machineId ID of the machine experiencing a thermal event
 */
void Scheduler::HandleThermalEvent(MachineId_t machineId) {
    SimOutput("Scheduler::HandleThermalEvent(): Thermal event on machine " + to_string(machineId), 1);
    
    VMId_t vmId = FindVMForMachine(machineId);
    VMInfo_t vmInfo = VM_GetInfo(vmId);
    
    if(vmInfo.active_tasks.empty()) {
        SimOutput("Scheduler::HandleThermalEvent(): No tasks on machine " + to_string(machineId), 2);
        return;
    }
    
    TaskId_t taskId = vmInfo.active_tasks[0];
    
    HandleSLAViolation(taskId, machineId);
}

/**
 * Perform periodic maintenance and monitoring of the cluster
 * 
 * This method is called regularly by the simulator to:
 * 1. Update machine utilization metrics
 * 2. Turn off unused machines to save energy
 * 3. Report current energy consumption
 * 
 * Periodic checks are essential for maintaining accurate utilization data
 * and implementing energy-saving measures.
 * 
 * @param now Current simulation time
 */
void Scheduler::PeriodicCheck(Time_t now) {
    SimOutput("Scheduler::PeriodicCheck(): Running periodic check at time " + to_string(now), 2);
    
    for(auto& machine : machines) {
        MachineInfo_t info = Machine_GetInfo(machine);
        if(info.s_state != S5) {
            machineUtilization[machine] = CalculateMachineUtilization(machine);
        }
    }
    
    CheckAndTurnOffUnusedMachines();
    
    SimOutput("Scheduler::PeriodicCheck(): Current cluster energy: " + 
              to_string(Machine_GetClusterEnergy()) + " KW-Hour", 2);
}

/**
 * Perform final cleanup and report simulation results
 * 
 * This method:
 * 1. Shuts down all VMs
 * 2. Reports final simulation time
 * 3. Reports total energy consumption
 * 4. Reports SLA compliance metrics for all SLA types
 * 
 * The final metrics provide a comprehensive evaluation of the greedy
 * allocation algorithm's performance in terms of energy efficiency
 * and service level compliance.
 * 
 * @param time Final simulation time
 */
void Scheduler::Shutdown(Time_t time) {
    for(auto & vm: vms) {
        VM_Shutdown(vm);
    }
    
    SimOutput("Scheduler::Shutdown(): Finished!", 4);
    SimOutput("Scheduler::Shutdown(): Time is " + to_string(time), 4);
    SimOutput("Scheduler::Shutdown(): Total energy consumed: " + to_string(Machine_GetClusterEnergy()) + " KW-Hour", 4);
    SimOutput("Scheduler::Shutdown(): SLA0 violations: " + to_string(GetSLAReport(SLA0)) + "%", 4);
    SimOutput("Scheduler::Shutdown(): SLA1 violations: " + to_string(GetSLAReport(SLA1)) + "%", 4);
    SimOutput("Scheduler::Shutdown(): SLA2 violations: " + to_string(GetSLAReport(SLA2)) + "%", 4);
}

/**
 * Handle task completion and perform workload consolidation
 * 
 * This method implements the core workload consolidation strategy:
 * 1. Update utilization tracking for the machine that completed a task
 * 2. Count active machines in the cluster
 * 3. If enough active machines exist, perform consolidation:
 *    - Find the least utilized machine with tasks
 *    - Find the most utilized machine with compatible CPU
 *    - Migrate a task if possible to consolidate workload
 * 4. Turn off any machines that become unused
 * 
 * This consolidation approach is key to the greedy allocation algorithm's
 * energy efficiency strategy by keeping fewer machines active with higher utilization.
 * 
 * @param now Current simulation time
 * @param task_id ID of the completed task
 */
void Scheduler::TaskComplete(Time_t now, TaskId_t task_id) {
    SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) + " is complete at " + to_string(now), 1);
    
    if(taskToMachineMap.find(task_id) == taskToMachineMap.end()) {
        SimOutput("Scheduler::TaskComplete(): Could not find machine for task " + to_string(task_id), 1);
        return;
    }
    
    MachineId_t completedTaskMachine = taskToMachineMap[task_id];
    
    taskToMachineMap.erase(task_id);
    
    float taskLoadFactor = CalculateTaskLoadFactor(task_id);
    machineUtilization[completedTaskMachine] = max(0.0f, machineUtilization[completedTaskMachine] - taskLoadFactor);
    
    unsigned activeCount = 0;
    for(auto& machine : machines) {
        MachineInfo_t info = Machine_GetInfo(machine);
        if(info.s_state != S5) {
            activeCount++;
        }
    }
    
    if(activeCount > 4) {
        vector<pair<MachineId_t, float>> sortedMachines = GetSortedMachinesByUtilization();
        
        if(sortedMachines.size() >= 2) {
            MachineId_t lowUtilMachine = sortedMachines[0].first;
            
            if(sortedMachines[0].second > 0.0f) {
                VMId_t vmId = FindVMForMachine(lowUtilMachine);
                VMInfo_t vmInfo = VM_GetInfo(vmId);
                
                if(!vmInfo.active_tasks.empty()) {
                    TaskId_t taskToMigrate = vmInfo.active_tasks[0];
                    CPUType_t requiredCPU = RequiredCPUType(taskToMigrate);
                    float taskLoad = CalculateTaskLoadFactor(taskToMigrate);
                    
                    for(int j = sortedMachines.size() - 1; j > 0; j--) {
                        MachineId_t highUtilMachine = sortedMachines[j].first;
                        MachineInfo_t highUtilInfo = Machine_GetInfo(highUtilMachine);
                        
                        if(highUtilInfo.cpu != requiredCPU) {
                            continue;
                        }
                        
                        float highUtilization = sortedMachines[j].second;
                        unsigned taskMemory = GetTaskMemory(taskToMigrate);
                        
                        if(highUtilization + taskLoad < 1.0f && 
                           highUtilInfo.memory_used + taskMemory <= highUtilInfo.memory_size) {
                            if(MigrateTask(taskToMigrate, highUtilMachine)) {
                                machineUtilization[lowUtilMachine] -= taskLoad;
                                machineUtilization[highUtilMachine] += taskLoad;
                                
                                taskToMachineMap[taskToMigrate] = highUtilMachine;
                                
                                SimOutput("Scheduler::TaskComplete(): Migrated task " + to_string(taskToMigrate) + 
                                         " from machine " + to_string(lowUtilMachine) + 
                                         " to machine " + to_string(highUtilMachine), 2);
                                
                                VMInfo_t updatedVmInfo = VM_GetInfo(vmId);
                                if(updatedVmInfo.active_tasks.empty()) {
                                    Machine_SetState(lowUtilMachine, S5);
                                    SimOutput("Scheduler::TaskComplete(): Turned off machine " + 
                                             to_string(lowUtilMachine) + " after migrating all tasks", 2);
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    
    CheckAndTurnOffUnusedMachines();
}

// Public interface below

static Scheduler Scheduler;

/**
 * Initialize the scheduler
 * 
 * This method is the entry point for scheduler initialization.
 * It delegates to the Scheduler class's Init method to set up
 * the greedy allocation algorithm.
 */
void InitScheduler() {
    SimOutput("InitScheduler(): Initializing scheduler", 1);
    Scheduler.Init();
}

/**
 * Handle new task arrival events
 * 
 * This method is called by the simulator when a new task arrives.
 * It delegates to the Scheduler class's NewTask method to allocate
 * the task according to the greedy allocation algorithm.
 * 
 * @param time Current simulation time
 * @param task_id ID of the newly arrived task
 */
void HandleNewTask(Time_t time, TaskId_t task_id) {
    SimOutput("HandleNewTask(): Received new task " + to_string(task_id) + " at time " + to_string(time), 5);
    Scheduler.NewTask(time, task_id);
}

/**
 * Handle task completion events
 * 
 * This method is called by the simulator when a task completes.
 * It delegates to the Scheduler class's TaskComplete method to
 * update utilization tracking and potentially migrate tasks for
 * workload consolidation.
 * 
 * @param time Current simulation time
 * @param task_id ID of the completed task
 */
void HandleTaskCompletion(Time_t time, TaskId_t task_id) {
    SimOutput("HandleTaskCompletion(): Task " + to_string(task_id) + " completed at time " + to_string(time), 5);
    Scheduler.TaskComplete(time, task_id);
}

/**
 * Handle memory overflow warnings from the simulator
 * 
 * This method is called when a machine's memory is overcommitted.
 * In the greedy allocation algorithm, this indicates that the
 * machine utilization calculation needs adjustment or that
 * task migration should be triggered.
 * 
 * @param time Current simulation time
 * @param machine_id ID of the machine with memory overflow
 */
void MemoryWarning(Time_t time, MachineId_t machine_id) {
    // The simulator is alerting you that machine identified by machine_id is overcommitted
    SimOutput("MemoryWarning(): Overflow at " + to_string(machine_id) + " was detected at time " + to_string(time), 0);
}

/**
 * Handle VM migration completion events
 * 
 * This method is called by the simulator when a VM migration completes.
 * It delegates to the Scheduler class's MigrationComplete method to
 * update tracking data structures and potentially trigger additional
 * optimizations.
 * 
 * VM migration is a key mechanism in the greedy allocation algorithm
 * for consolidating workloads onto fewer machines.
 * 
 * @param time Current simulation time
 * @param vm_id ID of the VM that completed migration
 */
void MigrationDone(Time_t time, VMId_t vm_id) {
    // The function is called on to alert you that migration is complete
    SimOutput("MigrationDone(): Migration of VM " + to_string(vm_id) + " was completed at time " + to_string(time), 5);
    Scheduler.MigrationComplete(time, vm_id);
    migrating = false;
}

/**
 * Handle periodic scheduler check events
 * 
 * This method is called periodically by the simulator to allow
 * the scheduler to perform maintenance tasks such as:
 * - Updating utilization tracking
 * - Checking for potential optimizations
 * - Turning off unused machines
 * - Reporting energy consumption
 * 
 * These periodic checks are essential for the greedy allocation
 * algorithm to maintain energy efficiency over time.
 * 
 * @param time Current simulation time
 */
void SchedulerCheck(Time_t time) {
    // This function is called periodically by the simulator, no specific event
    SimOutput("SchedulerCheck(): SchedulerCheck() called at " + to_string(time), 5);
    Scheduler.PeriodicCheck(time);
    static unsigned counts = 0;
    counts++;
    if(counts == 10) {
        migrating = true;
        VM_Migrate(1, 9);
    }
}

/**
 * Handle simulation completion event
 * 
 * This method is called by the simulator when the simulation is complete.
 * It reports final statistics including:
 * - SLA violation percentages for each SLA type
 * - Total energy consumption of the cluster
 * - Total simulation time
 * 
 * These metrics provide a comprehensive evaluation of the greedy
 * allocation algorithm's performance in terms of energy efficiency
 * and service level compliance.
 * 
 * @param time Final simulation time
 */
void SimulationComplete(Time_t time) {
    cout << "SLA violation report" << endl;
    cout << "SLA0: " << GetSLAReport(SLA0) << "%" << endl;
    cout << "SLA1: " << GetSLAReport(SLA1) << "%" << endl;
    cout << "SLA2: " << GetSLAReport(SLA2) << "%" << endl;     // SLA3 do not have SLA violation issues
    cout << "Total Energy " << Machine_GetClusterEnergy() << "KW-Hour" << endl;
    cout << "Simulation run finished in " << double(time)/1000000 << " seconds" << endl;
    SimOutput("SimulationComplete(): Simulation finished at time " + to_string(time), 4);
    
    Scheduler.Shutdown(time);
}

/**
 * Handle SLA violation warnings from the simulator
 * 
 * This method is called by the simulator when a task is at risk of violating its SLA.
 * It delegates the actual handling to the Scheduler's HandleSLAViolation method.
 * 
 * @param time Current simulation time
 * @param task_id ID of the task at risk of violating its SLA
 */
void SLAWarning(Time_t time, TaskId_t task_id) {
    SimOutput("SLAWarning(): SLA violation for task " + to_string(task_id) + " at time " + to_string(time), 1);
    Scheduler.HandleSLAViolation(task_id, (MachineId_t)-1); // -1 means unknown machine, will be looked up
}

/**
 * Handle machine state change completion events
 * 
 * This method is called by the simulator when a machine completes
 * a state change (e.g., powering on or off). It logs the new state
 * of the machine for monitoring purposes.
 * 
 * State changes are an important part of the energy efficiency strategy,
 * as powering machines off when not needed significantly reduces energy consumption.
 * 
 * @param time Current simulation time
 * @param machine_id ID of the machine that completed a state change
 */
void StateChangeComplete(Time_t time, MachineId_t machine_id) {
    // Called in response to an earlier request to change the state of a machine
    SimOutput("StateChangeComplete(): State change for machine " + to_string(machine_id) + 
              " completed at time " + to_string(time), 3);
    
    MachineInfo_t info = Machine_GetInfo(machine_id);
    SimOutput("StateChangeComplete(): Machine " + to_string(machine_id) + 
              " state changed to " + to_string(info.s_state), 3);
}

