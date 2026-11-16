#pragma once
#include "../types.hpp"
#include <cstdio>

// Print logging macros
// #ifndef PRINT_CC_LOG
// #define PRINT_CC_LOG 1
// #endif

// DCQCN CC logic
namespace madEscape {
    void dcqcn_rate_increase(CC_Para &cc_para, uint32_t src, uint32_t dst, uint32_t flow_id, long curSimTime);
    void dcqcn_alpha_update(CC_Para &cc_para);
    // decrease stage of sending rate in DCQCN, when received ECE packet
    void dcqcn_rate_decrease(CC_Para &cc_para, uint32_t src, uint32_t dst, uint32_t flow_id, long curSimTime);
    // reset the alpha update and rate increase timer, when rate is decreased
    void dcqcn_timer_reset_if_decrease(CC_Para &cc_para, long curSimTime);
    void dcqcn_timer_rate_increase(CC_Para &cc_para, uint32_t src, uint32_t dst, uint32_t flow_id, long curSimTime);
    void dcqcn_timer_alpha_update(CC_Para &cc_para, long curSimTime); 
}
