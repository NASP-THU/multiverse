#pragma once

#include <madrona/components.hpp>
#include <madrona/math.hpp>
#include <madrona/physics.hpp>
#include <madrona/rand.hpp>
#include <madrona/render/ecs.hpp>

//-----------------FOR All SYSTEM------------------------------
// 0 SYS 1 NET 2 MIX
#define DEV_MODE 2

// simple-log-mode，should set SYS_LOG 0
#define SIMPLE_LOG_MODE 1
// SYS LOG，should set SIMPLE_LOG_MODE 0
#define SYS_LOG 0


// detail log
// // simple-log-mode，should set SYS_LOG 0
// #define SIMPLE_LOG_MODE 0
// // SYS LOG，should set SIMPLE_LOG_MODE 0
// #define SYS_LOG 1

#define SYS_LOG_SPECIAL 0

#define SYS_LOG_TARGET_NODE 0
#define SYS_LOG_ENABLED_LIMIT_COMM_SIZE_DURATION 1
// 3M/3K
#define SYS_LOG_LIMIT_COMM_SIZE 1000//3000000

#define SCALE_RATIO 1//3000000

#define MAX_SKIP_NODE_ID 1

// v1 has bugs. src or recv is not correct.
#define CHAKARA_VERSION 1

// NET LOG
// #define PRINT_PKT_LOG 1
// #define PRINT_SYS_LOG 1
// #define PRINT_CC_LOG 1
// #define PRINT_SEND_RECV_PKT_LOG 1
// #define PRINT_PKT_PATH_LOG 1
// #define ENABLE_PKT_PATH_RECORD 1



//TEST
#define TEST_RING_TOPO_LOG_FOR_CPU_ONLY 0
// WORK WITH SYS_LOG_TARGET_NODE 
#define TEST_RING_COLLECTIVE_COMM_LOG_FOR_CPU_ONLY 0

#define TEST_CHAKRA_NODE_LOG_FOR_CPU_ONLY 1

#define TEST_ALL_RING_COLLECTIVE_COMM_LOG_FOR_CPU_ONLY 1
