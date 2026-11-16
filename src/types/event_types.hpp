#pragma once

#include "common_types.hpp"

namespace madEscape {

    //
    struct Results {
        uint32_t results;
    };

    struct Results2 {
        int32_t encoded_string[1000];
    };

    struct MadronaEvent {
        int32_t type;
        int32_t eventId;
        int32_t time;
        int32_t src;
        int32_t dst;
        int32_t size;
        int32_t port;
    };

    struct MadronaEventsQueue {
        int32_t events[MESSAGE_BUFFER_LENGTH];
    };

    struct MadronaEvents {
        int32_t events[MESSAGE_BUFFER_LENGTH];
    };

    struct MadronaEventsResult {
        int32_t events[MESSAGE_BUFFER_LENGTH];
    };

    struct Configuration {
        int32_t events[1000];
    };

    // ring form 0
    // npu_num from 100
    // coll-imp from 200
    // finish from 1000
    struct ProcessParams {
        int32_t params[1000];
    };

}
