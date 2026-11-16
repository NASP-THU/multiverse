#pragma once

#include "../sim.hpp"
#include "../types.hpp"

// Include sub-module headers
#include "system_init.hpp"
#include "chakra_node_processing.hpp"
#include "communication_system.hpp"
#include "time_management.hpp"

// Use using declarations inside namespace, not global using
namespace madEscape::llm_system
{
        // Import madrona types into current namespace
        using madrona::ECSRegistry;

        // Main system functions (implementations are distributed across sub-modules)
        // Core system processing function - remains in main file
        void sys_processChakraNodes(Engine &ctx,
                                    NpuID &id,
                                    ChakraNodes &chakraNodes,
                                    HardwareResource &hardwareResource,
                                    ProcessingCompTask &processingCompTask,
                                    ProcessingCommTasks &processingCommTasks,
                                    OneNPUFinishedFlag & oneNPUFinishedFlag,
                                    ChakraNodesForNoDP & chakraNodesForNoDP);

        void sys_removeChakraNodes(Engine &ctx,
                                   NpuID &id,
                                   ChakraNodes &chakraNodes,
                                   HardwareResource &hardwareResource,
                                   ProcessingCompTask &processingCompTask,
                                   ProcessingCommTasks &processingCommTasks);

        void sys_checkNpuFinish(Engine &ctx, CheckNpuFinishFlag &checkNpuFinishFlag);

        // All other functions are declared in their respective sub-module headers:
        // - system_init.hpp: registerTypes, init, sys_init
        // - chakra_node_processing.hpp: parseChakraNodes, filterNoDependencyNodes, removeNode
        // - communication_system.hpp: sys_checkFlow, setNextExecFlows, communication helpers
        // - time_management.hpp: sys_checkSkipTime, time utilities

        void sys_test_ring_topo_dim(Engine & ctx,
                                    NpuID & id,
                                    ChakraNodes & chakraNodes,
                                    HardwareResource & hardwareResource,
                                    ProcessingCompTask & processingCompTask,
                                    ProcessingCommTasks & processingCommTasks,
                                    OneNPUFinishedFlag & oneNPUFinishedFlag,
                                    ChakraNodesForNoDP & chakraNodesForNoDP);
}
