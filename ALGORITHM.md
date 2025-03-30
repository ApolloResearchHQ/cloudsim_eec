# Bare-Bones Scheduler Implementation

## Overview
This document describes the implementation of a bare-bones scheduler for the CloudSim EEC project. The primary goal of this implementation is to achieve 0% SLA violations by correctly matching tasks with compatible VMs based on OS type and CPU architecture.

## Approach

### 1. VM Organization
The scheduler organizes VMs into separate vectors based on their OS type:
- `linux`: VMs running Linux OS
- `linux_rt`: VMs running Linux Real-Time OS
- `win`: VMs running Windows OS
- `aix`: VMs running AIX OS

This organization allows for efficient matching of tasks to compatible VMs based on OS requirements.

### 2. Initialization
During initialization, the scheduler:
- Retrieves information about all available machines
- Creates VMs of appropriate types for each machine based on CPU architecture
- Attaches VMs to machines
- Organizes VMs into OS-specific vectors for efficient task assignment

### 3. Task Assignment Strategy
When a new task arrives, the scheduler:
1. Retrieves task information (OS type, CPU type, priority, memory requirements, GPU requirements)
2. Selects the appropriate VM vector based on the task's OS requirements
3. Iterates through the selected VM vector to find a compatible VM based on:
   - CPU architecture compatibility
   - Available memory
   - GPU requirements (if needed)
4. Assigns the task to the first compatible VM found
5. If no compatible VM is found, the task is not assigned, resulting in an SLA violation

### 4. Priority Handling
Tasks are prioritized based on their SLA level:
- SLA0 tasks: Assigned HIGH_PRIORITY
- SLA1 tasks: Assigned MID_PRIORITY
- SLA2 tasks: Assigned LOW_PRIORITY

This ensures that higher priority tasks are processed first, minimizing SLA violations for critical tasks.

### 5. Performance Results
The implementation achieves:
- 0% SLA violations for some workloads (NiceAndSmooth, Day)
- Varying levels of SLA violations for other workloads, which would require further optimization

## Future Improvements
Future versions of the scheduler could implement:
1. More sophisticated load balancing across VMs
2. Dynamic VM creation and destruction based on workload
3. Task migration to optimize resource utilization
4. Energy-aware scheduling to reduce power consumption
5. Predictive scheduling based on historical workload patterns
