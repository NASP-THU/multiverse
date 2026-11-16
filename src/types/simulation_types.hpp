#pragma once

#include "common_types.hpp"

namespace madEscape {

    struct SimTime {
        int64_t sim_time;
    };

    struct SimTimePerUpdate {
        int64_t sim_time_per_update;
    };

    struct CurStep {
        uint32_t step;
    };

    struct SimulationTime {
        int64_t time;
    };

}
