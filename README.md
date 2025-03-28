This is the repository for the Cloud Simulator project for CS 378. To run this project, you can run `make all` to build the executable, which you can then run with `./simulator`.

For questions, please reach out to any of the course staff on via email (anish.palakurthi@utexas.edu, tarun.mohan@utexas.edu, mootaz@austin.utexas.edu) or Ed Discussion.

## Greedy Allocation Algorithm Implementation

This branch implements a greedy allocation algorithm for the CloudSim EEC simulator, focusing on optimizing cloud scheduling for energy efficiency while maintaining performance.

### Algorithm Overview

The greedy allocation algorithm implemented here follows these key principles:

1. **Task Allocation Strategy**:
   - Tasks are allocated to machines based on their current utilization
   - CPU compatibility is strictly enforced (tasks only run on machines with matching CPU types)
   - Memory requirements are checked to prevent overcommitment

2. **Utilization Metrics**:
   - Machine utilization is calculated as `memory_used / memory_size`
   - Task load factor is calculated as `task_memory / machine_memory`

3. **Energy Efficiency Measures**:
   - Unused machines are powered off to save energy
   - Tasks are migrated from less utilized to more utilized machines to consolidate workloads

4. **SLA Violation Handling**:
   - When SLA violations occur, tasks are migrated to less loaded machines
   - If no suitable running machine is found, powered-off machines are turned on

### Implementation Details

#### Proxies Used

Since direct measurements of some metrics might not be available, the following proxies are used:

1. **Machine Utilization**: 
   - Proxy: `memory_used / memory_size`
   - Rationale: Memory usage provides a good indicator of overall machine load

2. **Task Load Factor**:
   - Proxy: `task_memory / machine_memory`
   - Rationale: Memory requirements correlate with overall task resource needs

#### Key Data Structures

1. `machineUtilization`: Maps machines to their current utilization level
2. `machineToVMMap`: Maps machines to their associated VMs
3. `taskToMachineMap`: Maps tasks to the machines they're running on

#### Algorithm Flow

1. **Initialization**:
   - Create VMs with appropriate CPU types for each machine
   - Initialize utilization tracking

2. **New Task Allocation**:
   - Check CPU compatibility
   - Calculate task load factor
   - Find machine that can accommodate the task
   - Update utilization tracking

3. **Task Completion**:
   - Update utilization tracking
   - Sort machines by utilization
   - Migrate tasks from less utilized to more utilized machines
   - Turn off unused machines

4. **Periodic Checks**:
   - Update utilization values
   - Turn off unused machines
   - Report energy consumption

5. **SLA Violation Handling**:
   - Find a suitable machine with compatible CPU
   - Migrate the task
   - If necessary, power on a machine
