This is the repository for the Cloud Simulator project for CS 378. To run this project, you can run `make all` to build the executable, which you can then run with `./simulator`.

For questions, please reach out to any of the course staff on via email (anish.palakurthi@utexas.edu, tarun.mohan@utexas.edu, mootaz@austin.utexas.edu) or Ed Discussion.

## pMapper Algorithm Implementation

This branch implements the pMapper algorithm for the CloudSim EEC simulator, focusing on optimizing cloud scheduling for energy efficiency while maintaining performance.

### Algorithm Overview

The pMapper algorithm implemented here follows these key principles:

1. **Initialization and Task Allocation**:
   - Sort machines by energy consumption (lowest to highest)
   - Assign tasks to machines in order of the sorted list
   - Use next machine when current becomes fully utilized
   - Report SLA violations if tasks can't be allocated
   - Turn off unused machines

2. **Workload Completion and Migration**:
   - When a task completes, divide servers into two halves based on utilization
   - Find smallest workload in least utilized server
   - Migrate it to a highly utilized server
   - Reassign tasks as needed

3. **CPU Compatibility Handling**:
   - Ensure tasks are only assigned to machines with compatible CPU types
   - When SLA violations occur, find machines with compatible CPUs
   - Power on machines if necessary to handle CPU compatibility issues

4. **Energy Efficiency Measures**:
   - Maintain a sorted list of machines by energy consumption
   - Consolidate workloads to minimize the number of active machines
   - Power off unused machines to save energy

### Implementation Details

#### Key Data Structures

1. `energySortedMachines`: Vector of machines sorted by energy consumption (lowest to highest)
2. `machineUtilization`: Maps machines to their current utilization level
3. `vms` and `machines`: Lists of all VMs and machines managed by the scheduler

#### Performance Optimizations

1. **Reduced Logging Overhead**:
   - Adjusted SimOutput verbosity levels to minimize logging overhead
   - Critical messages remain at level 1 for visibility
   - Routine messages moved to levels 3-5 for reduced output

2. **Efficient Machine Selection**:
   - Pre-sorted machine list for quick access to energy-efficient machines
   - Periodic rebuilding of the sorted list to adapt to changing conditions

3. **CPU Compatibility Handling**:
   - Dedicated SLA violation handler to address CPU compatibility issues
   - Proactive migration of tasks to machines with compatible CPUs

#### Algorithm Flow

1. **Initialization**:
   - Create VMs with appropriate CPU types for each machine
   - Build energy-sorted machine list
   - Initialize utilization tracking

2. **New Task Allocation**:
   - Check CPU compatibility
   - Allocate task to the most energy-efficient compatible machine
   - Update utilization tracking

3. **Task Completion**:
   - Update utilization tracking
   - Identify machines with low utilization
   - Migrate smallest tasks from low-utilization to high-utilization machines
   - Turn off unused machines

4. **Periodic Checks**:
   - Rebuild energy-sorted machine list
   - Turn off unused machines
   - Report energy consumption

5. **SLA Violation Handling**:
   - Find a suitable machine with compatible CPU
   - Migrate the task to the compatible machine
   - If necessary, power on a machine with compatible CPU
