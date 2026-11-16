#pragma once

#include "../sim.hpp"
#include "../types.hpp"

namespace madEscape::llm_system
{
    // Import madrona types into current namespace
    using madrona::ECSRegistry;

    // System initialization functions
    void registerTypes(ECSRegistry &registry);
    void init(Engine &ctx);
    void sys_init(Engine &ctx, ChakraNodesData &chakra_nodes_data, ProcessParams &processParams);
}