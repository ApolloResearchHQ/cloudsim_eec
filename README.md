This is the repository for the Cloud Simulator project for CS 378. To run this project, you can run `make all` to build the executable, which you can then run with `./simulator`.

For questions, please reach out to any of the course staff on via email (anish.palakurthi@utexas.edu, tarun.mohan@utexas.edu, mootaz@austin.utexas.edu) or Ed Discussion.

## Predictive Algorithm Implementation

This branch implements the Predictive Algorithm for cloud scheduling in the CloudSim EEC simulator, focusing on optimizing cloud resource allocation based on response time measurements rather than load factors.

### Algorithm Overview

The Predictive algorithm implemented here follows these key principles:

1. **Response Time Measurement**:
   - Instead of using load factor, the algorithm measures task response times
   - Response times provide a more direct indicator of system performance
   - Maintains a history of recent response times for trend analysis

2. **Adaptive VM Sizing**:
   - Response time rising: Increase the size of the VM (higher performance)
   - Response time dropping: Decrease the size of the VM (lower performance)
   - VM size adjustments are implemented through CPU P-states

3. **Physical Machine Optimization**:
   - Periodically checks for opportunities to turn off physical machines
   - Consolidates VMs after VM shrinkage to free up machines
   - Powers off idle machines to save energy

4. **Over-provisioning Prevention**:
   - Traditional SLA-focused approaches lead to over-provisioning
   - By measuring actual response times, resources are adjusted based on real needs
   - Adjustments are made based on the slope of change in response time

### Implementation Details

#### Proxies Used for Measuring System State

Since some metrics might not be directly available, the following proxies are used:

1. **Response Time Proxy**: 
   - Calculated as the time between task arrival and completion
   - Used to determine if VMs need to be resized

2. **VM Size Proxy**:
   - Implemented using CPU P-states (P0-P3)
   - P0 represents highest performance (largest VM size)
   - P3 represents lowest performance (smallest VM size)

3. **Response Time Trend Proxy**:
   - Uses the slope between first half and second half of the response time history
   - Positive slope indicates degrading performance
   - Negative slope indicates improving performance

#### Key Data Structures

1. `vmResponseTimes`: Tracks recent response times for each VM using a fixed-size queue
2. `vmAverageResponseTime`: Stores the current average response time for each VM
3. `vmSizes`: Tracks the current size (performance level) of each VM
4. `vmToMachine` and `machineToVMs`: Maps for efficient VM-to-machine and machine-to-VM lookups
5. `taskStartTimes`: Tracks when each task was started for response time calculation
6. `cpuTypeMachines`: Groups machines by CPU type for compatibility matching
7. `machineActive`: Tracks whether each machine is currently active

#### Performance Optimizations

1. **Reduced Logging Overhead**:
   - Minimized SimOutput calls to improve runtime performance
   - Critical messages remain at level 1 for visibility
   - Routine messages removed entirely
   - Periodic checks use modulo operations to limit processing frequency

2. **Efficient Data Structures**:
   - Used maps for O(1) lookups of VM and machine information
   - Limited response time history size to prevent memory growth
   - Pre-grouped machines by CPU type for efficient compatibility matching

3. **Conditional Processing**:
   - VM size adjustments only occur every N task completions
   - Machine consolidation checks only run periodically
   - Exception handling to prevent crashes from unexpected states

#### Algorithm Flow

1. **Initialization**:
   - Discover all available machines in the cluster
   - Create VMs for a subset of machines (to avoid over-provisioning)
   - Initialize data structures for response time tracking
   - Group machines by CPU type for compatibility matching
   - Turn off unused machines to save energy

2. **Task Allocation**:
   - Store task start time for response time calculation
   - Find VM with lowest average response time and compatible CPU
   - If no suitable VM found, activate a machine and create a new VM
   - Allocate task to selected VM

3. **Response Time Tracking**:
   - When a task completes, calculate its response time
   - Update the VM's response time history
   - Calculate new average response time and response time slope

4. **VM Size Adjustment**:
   - If response times are increasing (positive slope), increase VM size
   - If response times are decreasing (negative slope), decrease VM size
   - Implement size changes by adjusting CPU P-states

5. **Machine Consolidation**:
   - Periodically check for empty machines
   - Turn off machines with no VMs or tasks
   - After VM shrinkage, check if consolidation is possible

6. **Periodic Maintenance**:
   - Regularly analyze system state
   - Look for opportunities to turn off machines
   - Report system statistics for monitoring

### Testing Results

The Predictive algorithm has been tested with multiple input files:

1. **Input.md**: 100% SLA0 compliance (improved from 39.8438%)
2. **SmallTest**: 0% SLA violations across all SLA types
3. **SmallGentlerHour**: 0% SLA violations across all SLA types

The algorithm successfully balances energy efficiency with performance requirements, achieving optimal SLA compliance while minimizing energy consumption.
