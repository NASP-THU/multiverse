#pragma once

#include "../sim.hpp"

using namespace madrona;

namespace madEscape {

    // Network configuration and entity generation function
    // Processes tensor data from Python and creates network entities
    void pass_config_data_and_gen_network_entt(Engine &ctx, 
            TopoTensor &_topo_tensor, 
            FibTensor &_fib_tensor);

} // namespace madEscape 
