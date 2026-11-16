#pragma once

// Include all type modules
#include "types/common_types.hpp"
#include "types/simulation_types.hpp"
#include "types/packet_types.hpp"
#include "types/flow_types.hpp"
#include "types/network_device_types.hpp"
#include "types/switch_types.hpp"
#include "types/topology_types.hpp"
#include "types/agent_types.hpp"
#include "types/event_types.hpp"
#include "types/llm_sys_types.hpp"
#include "types/global_types.hpp"

namespace madEscape {

    /* ECS Archetypes for the game */

    // There are 2 Agents in the environment trying to get to the destination
    struct Agent : public madrona::Archetype<

                   // Network simulation components
                   CurStep,
                   Results,
                   Results2,
                   SimulationTime,
                   MadronaEventsQueue,
                   MadronaEventsResult,
                   MadronaEvents,
                   ProcessParams,
                   TopoTensor,
                   FibTensor,

                   ChakraNodesData
                       > {};
}
