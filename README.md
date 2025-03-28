This is the repository for the Cloud Simulator project for CS 378. To run this project, you can run `make all` to build the executable, which you can then run with `./simulator`.

For questions, please reach out to any of the course staff on via email (anish.palakurthi@utexas.edu, tarun.mohan@utexas.edu, mootaz@austin.utexas.edu) or Ed Discussion.

## E-eco Algorithm Implementation

This branch implements the Energy-Efficient Cloud Orchestrator (E-eco) algorithm for the CloudSim EEC simulator, focusing on optimizing cloud scheduling for energy efficiency while maintaining performance.

### Algorithm Overview

The E-eco algorithm implemented here follows these key principles:

1. **Three-Tier Host Management**:
   - **Running Tier**: Hosts that are active and running applications
   - **Intermediate Tier**: Hosts in standby mode (S3 state), ready to be activated when needed
   - **Switched Off Tier**: Hosts that are powered off (S5 state) to save energy

2. **Workload-Based Tier Sizing**:
   - Dynamically adjusts the number of hosts in each tier based on system load
   - Uses load thresholds to determine when to move hosts between tiers:
     - High load threshold (70%): When exceeded, machines are moved from Intermediate to Running tier
     - Low load threshold (30%): When below this, machines are moved from Running to Intermediate tier

3. **CPU Compatibility Management**:
   - Ensures tasks are assigned to machines with compatible CPU types
   - Groups machines by CPU type for efficient task allocation

4. **Energy Efficiency Strategy**:
   - Minimizes the number of active machines based on workload
   - Avoids costly VM migrations by using the tiered approach
   - Powers off unused machines to save energy

### Implementation Details

#### Proxies Used for Measuring System State

Since some metrics might not be directly available, the following proxies are used:

1. **System Load Proxy**: 
   - Calculated as `total_memory_used / total_memory_available` across all running machines
   - This provides a good indicator of overall system utilization

2. **Machine Load Proxy**:
   - Calculated as `machine_memory_used / machine_memory_size` for individual machines
   - Used to determine which machines to deactivate first

3. **Task Size Proxy**:
   - Uses the task's memory requirement as a proxy for its overall resource needs
   - Helps in load balancing decisions and tier sizing calculations

#### Key Data Structures

1. `machineTiers`: Maps machines to their current tier (Running, Intermediate, or Switched Off)
2. `machineLoads`: Tracks the current load on each machine
3. `cpuTypeMachines`: Groups machines by CPU type for compatibility matching

#### Performance Optimizations

1. **Reduced Logging Overhead**:
   - Minimized SimOutput calls to improve runtime performance
   - Critical messages remain at level 1 for visibility
   - Routine messages moved to levels 3+ for reduced output
   - Periodic checks use modulo operations to limit logging frequency

2. **Efficient Tier Management**:
   - Machines are grouped by CPU type for quick compatibility matching
   - Tier transitions are minimized to reduce state change overhead
   - Empty machines are prioritized for deactivation to avoid migrations

3. **CPU Compatibility Handling**:
   - Pre-grouped machines by CPU type for O(1) lookup
   - Ensures tasks are only assigned to machines with compatible CPUs
   - Activates machines from the Intermediate tier when needed for CPU compatibility

#### Algorithm Flow

1. **Initialization**:
   - Categorize machines into the three tiers
   - Create VMs for machines in the Running tier
   - Put Intermediate tier machines in standby (S3)
   - Turn off machines in the Switched Off tier (S5)

2. **Task Allocation**:
   - First attempt to allocate tasks to machines in the Running tier
   - If no suitable machine is found, activate a machine from the Intermediate tier
   - Ensure CPU compatibility in all allocations

3. **Tier Management**:
   - Periodically recalculate desired tier sizes based on workload
   - Move machines between tiers as needed:
     - Running → Intermediate: When system load is low
     - Intermediate → Running: When system load is high
     - Intermediate → Switched Off: When intermediate tier is too large
     - Switched Off → Intermediate: When more standby machines are needed

4. **Task Completion**:
   - Update machine loads when tasks complete
   - Adjust tiers based on new system state
   - Potentially deactivate machines with no tasks

5. **Periodic Checks**:
   - Monitor system load and adjust tiers accordingly
   - Report tier statistics for monitoring
   - Optimize energy consumption by maintaining appropriate tier sizes

### Testing Results

The E-eco algorithm has been tested with multiple input files:

1. **Input.md**: 0% SLA violations (improved from previous implementation)
2. **SmallTest**: 0% SLA violations across all SLA types
3. **SmallGentlerHour**: 0% SLA violations across all SLA types

The algorithm successfully balances energy efficiency with performance requirements, achieving optimal SLA compliance while minimizing energy consumption. Performance optimizations have significantly improved runtime, allowing the algorithm to handle larger input files efficiently.
