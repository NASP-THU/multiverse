#pragma once

#include <madrona/components.hpp>
#include <madrona/math.hpp>
#include <madrona/physics.hpp>
#include <madrona/rand.hpp>
#include <madrona/render/ecs.hpp>


// // ----------- for llm sys -------------------------

// // System configuration macros
// #define ENABLE_TEST true

// Each ChakraNode occupies 51 ints with new 3-int attribute format
#define INTS_PER_NODE 51

#define NPU_NUM 128


// #define MAX_CHAKRA_NODES_PER_NPU 9 * 9999
#define MAX_CHAKRA_NODES_PER_NPU 1000
#define MAX_CONCURRENT_CHAKRA_NODES_PER_NPU 3
#define MAX_CHAKRA_NODP_NODES 5
#define CHAKRA_NODES_DATA_LENGTH 50000 // aligned with tensor export size in mgr.cpp
// Define invalid dependency value
#define INVALID_DEPENDENCY 2147483646
#define NODE_NAME_LENGTH 20
#define NODE_DATA_DEPS_LENGTH 10
#define NODE_ATTR_PER_LENGTH 3
#define NODE_DURATION_MICROS_LENGTH 1


// Define invalid flow ID value
#define INVALID_FLOW_ID 2147483646

// Data type enumeration for attribute storage
enum class AttributeDataType : uint32_t
{
    BOOL_VAL = 0,   // boolVal
    UINT32_VAL = 1, // uint32Val
    INT64_VAL = 2,  // int64Val
    UINT64_VAL = 3, // uint64Val
    BOOL_LIST = 4   // boolList
};

// Maximum number of current dependency-free nodes
#define CURRENT_EXEC_NODES_MAX 10

// Check skip time every x frames
#define CHECK_SKIPTIME_INTERVAL_PER_FRAME 100

// Maximum number of flows per NPU
#define MAX_COMM_TASK_PER_NPU 5

// Maximum communication volume per comm node
#define MAX_FLOW_NUM_PER_COMM_NODE 100

// Flow ID range interval per node
#define FLOW_ID_MAX_LENGTH 10000

// #define MAX_FLOW_NUM_ALL_COMM_NODE 9999

namespace madEscape
{

    // ------------ Topology Ring-----------------
    enum class Direction
    {
        Clockwise,
        Anticlockwise
    };
    enum class Dimension
    {
        Local,
        Horizontal,
        Vertical,
        NA
    };

    // -----------------------------

    enum TaskState : int32_t
    {
        INIT = 0,
        START = 1,
        FINISH = 2
    };

    struct ProcessingCommTask
    {
        int64_t time_start_ns;
        int64_t time_end_ns;
        // means process times
        int64_t time_finish_ns;
        TaskState state;
        int32_t node_id;

        // Default constructor
        ProcessingCommTask()
            : time_start_ns(0),      // Initialize to 0
              time_end_ns(0),      // Initialize to 0
              time_finish_ns(0),      // Initialize to 0
              state(TaskState::INIT), // Initialize to INIT
              node_id(-1)
        { // Initialize to -1
        }

        void init()
        {
            time_start_ns = 0;
            time_end_ns = 0;
            time_finish_ns = 0;
            state = TaskState::INIT;
            node_id = -1;
        }
    };


    struct SysFlow
    {
        uint32_t id;
        uint64_t comm_size;
        uint64_t comm_src;
        uint64_t comm_dst;
        uint32_t durationMicros;

        // Flow execution order ID
        int exec_index;
        // Whether execution is completed
        TaskState state;
        // Send completion or receive completion
        bool is_send;

        SysFlow()
            : id(INVALID_FLOW_ID), // Initialize to -1
              comm_size(0),
              comm_src(0),
              comm_dst(0),
              durationMicros(0),
              exec_index(0),
              state(TaskState::INIT),
              is_send(true)
        { // Initialize to true
        }

        void init()
        {
            id = INVALID_FLOW_ID;
            comm_size = 0;
            comm_src = 0;
            comm_dst = 0;
            durationMicros = 0;
            exec_index = 0;
            state = TaskState::INIT;
            is_send = true;
        }
    };

    struct NpuID
    {
        uint32_t value;
        bool test_dim;
        bool test_collective_comm_ring;

        SysFlow flows_exec[MAX_FLOW_NUM_PER_COMM_NODE];
        SysFlow flows_finish[MAX_FLOW_NUM_PER_COMM_NODE];
        SysFlow recv_node_flows_finish[MAX_FLOW_NUM_PER_COMM_NODE];
        ProcessingCommTask result[MAX_COMM_TASK_PER_NPU];

        void result_init()
        {
            for (int i = 0; i < MAX_COMM_TASK_PER_NPU; i++)
            {
                result[i].init();
            }
        }

        void flows_exec_init()
        {
            for (int i = 0; i < MAX_FLOW_NUM_PER_COMM_NODE; i++)
            {
                flows_exec[i].init();
            }
        }

        void flows_finish_init()
        {
            for (int i = 0; i < MAX_FLOW_NUM_PER_COMM_NODE; i++)
            {
                flows_finish[i].init();
            }
        }

        void recv_node_flows_finish_init()
        {
            for (int i = 0; i < MAX_FLOW_NUM_PER_COMM_NODE; i++)
            {
                recv_node_flows_finish[i].init();
            }
        }


        NpuID()
            : value(0),
              test_dim(false),
              test_collective_comm_ring(false)
        {
        }
    };

    struct NodeID
    {
        uint32_t value;
    };

    enum class NodeType : int32_t
    {
        None = 0,
        COMP_NODE = 1,
        COMM_SEND_NODE = 2,
        COMM_RECV_NODE = 3,
        COMM_COLL_NODE = 4
    };

    enum CollectiveCommType : uint64_t
    {
        ALL_REDUCE = 0,
        REDUCE = 1,
        ALL_GATHER = 2,
        GATHER = 3,
        SCATTER = 4,
        BROADCAST = 5,
        ALL_TO_ALL = 6,
        REDUCE_SCATTER = 7,
        REDUCE_SCATTER_BLOCK = 8,
        BARRIER = 9
    };

    enum CommImplementationType : int32_t
    {
        Ring = 0,
        Tree = 1
    };

    enum class AttributeKey : int32_t
    {
        comm_para = 1,
        comm_size = 2,
        comm_src = 3,
        comm_dst = 4,
        involved_dim = 5
    };

    struct CommModel
    {
        CommImplementationType all_reduce_implementation;
        CommImplementationType reduce_implementation;
        CommImplementationType all_gather_implementation;
        CommImplementationType gather_implementation;
        CommImplementationType scatter_implementation;
        CommImplementationType broadcast_implementation;
        CommImplementationType all_to_all_implementation;
        CommImplementationType reduce_scatter_implementation;
        CommImplementationType reduce_scatter_block_implementation;
        CommImplementationType barrier_implementation;
    };

    struct SysConfig : public madrona::Archetype<CommModel>
    {
    };




    struct TaskFlows
    {
        bool is_exec;
        int current_index;
        SysFlow flows[MAX_FLOW_NUM_PER_COMM_NODE];

        TaskFlows()
        {
            is_exec = false;
            current_index = -1;
            for (int i = 0; i < MAX_FLOW_NUM_PER_COMM_NODE; ++i)
            {
                flows[i] = SysFlow(); // Initialize each task
            }
        }

        void setCurrentIndexFlowsFinish()
        {
            for (int i = 0; i < MAX_FLOW_NUM_PER_COMM_NODE; ++i)
            {
                if (flows[i].id == INVALID_FLOW_ID || flows[i].state == TaskState::FINISH)
                    continue; // Skip uninitialized flows
                if ((int)flows[i].exec_index == current_index)
                {
                    flows[i].state = TaskState::FINISH;
                }
            }
        }

        int getNextExecFlows(SysFlow flows_out[])
        {
            int min_exec_index = -1;
            int count = 0;

            // First pass: find the smallest exec_index greater than current_index
            for (int i = 0; i < MAX_FLOW_NUM_PER_COMM_NODE; ++i)
            {
                const SysFlow &flow = flows[i];
                if (flow.id == INVALID_FLOW_ID || flow.state == TaskState::FINISH)
                    continue;
                if ((int)flow.exec_index > current_index)
                {
                    if (min_exec_index == -1 || flow.exec_index < min_exec_index)
                    {
                        min_exec_index = flow.exec_index;
                    }
                }
            }

            // If no exec_index greater than current_index is found, return 0
            if (min_exec_index == -1)
            {
                return 0;
            }
            else
            {
                current_index = min_exec_index;
            }

            // Second pass: collect all flows with exec_index equal to min_exec_index
            for (int i = 0; i < MAX_FLOW_NUM_PER_COMM_NODE; ++i)
            {
                const SysFlow &flow = flows[i];
                if (flow.id == INVALID_FLOW_ID)
                    continue;
                if (flow.exec_index == min_exec_index)
                {
                    flows_out[count++] = flow;
                }
            }
            return count;
        }

        // Check if there exists a flow with exec_index equal to current_index
        bool hasFlowWithCurrentIndex() const
        {
            for (int i = 0; i < MAX_FLOW_NUM_PER_COMM_NODE; ++i)
            {
                const SysFlow &flow = flows[i];
                if (flow.id == INVALID_FLOW_ID || flow.state == TaskState::FINISH)
                    continue; // Skip uninitialized flows
                if (flow.exec_index == current_index && flow.state == TaskState::START)
                {
                    return true;
                }
            }
            return false;
        }

        void updateFlows(const SysFlow flows_finish[], int flows_finish_size)
        {
            for (int i = 0; i < flows_finish_size; ++i)
            {
                const SysFlow &finishedFlow = flows_finish[i];
                for (int j = 0; j < MAX_FLOW_NUM_PER_COMM_NODE; ++j)
                {
                    if (flows[j].id == finishedFlow.id)
                    { // Find element with same ID
                        flows[j].state = TaskState::FINISH;
                        flows[j].is_send = finishedFlow.is_send;               // Assign is_send
                        flows[j].durationMicros = finishedFlow.durationMicros; // Assign durationMicros
                        break;                                                 // Break inner loop after finding match
                    }
                }
            }
        }

        // Check if all tasks are completed
        bool areAllTasksDone() const
        {
            for (int i = 0; i < MAX_FLOW_NUM_PER_COMM_NODE; ++i)
            {
                if (flows[i].state == TaskState::INIT)
                {
                    continue;
                }

                if (flows[i].state == TaskState::START)
                {
                    return false;
                }
            }
            return true;
        }
    };

    struct ChakraNode
    {
        uint32_t name[20];
        NodeType type;
        uint32_t id;
        uint32_t data_deps[10];
        uint64_t comm_para;
        uint64_t comm_size;
        uint64_t comm_src;
        uint64_t comm_dst;
        CollectiveCommType comm_type;
        bool involved_dim_1;
        bool involved_dim_2;
        bool involved_dim_3;
        uint32_t durationMicros;

        // Copy constructor
        ChakraNode(const ChakraNode &other)
        {
            for (int i = 0; i < 20; ++i)
            {
                name[i] = other.name[i];
            }
            type = other.type;
            id = other.id;
            for (int i = 0; i < 10; ++i)
            {
                data_deps[i] = other.data_deps[i];
            }
            comm_para = other.comm_para;
            comm_size = other.comm_size;
            comm_src = other.comm_src;
            comm_dst = other.comm_dst;
            comm_type = other.comm_type;
            involved_dim_1 = other.involved_dim_1;
            involved_dim_2 = other.involved_dim_2;
            involved_dim_3 = other.involved_dim_3;
            durationMicros = other.durationMicros;
        }

        // Default constructor
        ChakraNode() = default;

        // Custom assignment operator
        ChakraNode &operator=(const ChakraNode &other)
        {
            if (this == &other)
                return *this; // Prevent self-assignment

            // Copy each member variable
            for (int i = 0; i < 20; ++i)
            {
                name[i] = other.name[i];
            }
            type = other.type;
            id = other.id;
            for (int i = 0; i < 10; ++i)
            {
                data_deps[i] = other.data_deps[i];
            }
            comm_para = other.comm_para;
            comm_size = other.comm_size;
            comm_src = other.comm_src;
            comm_dst = other.comm_dst;
            comm_type = other.comm_type;
            involved_dim_1 = other.involved_dim_1;
            involved_dim_2 = other.involved_dim_2;
            involved_dim_3 = other.involved_dim_3;
            durationMicros = other.durationMicros;

            return *this;
        }

        // Convert name from uint32_t array to readable string
        void getNameAsString(char output[64]) const
        {
            int output_len = 0;
            for (int i = 0; i < 20 && output_len < 63; ++i)
            {
                if (name[i] == 0 || name[i] == 2147483646 || name[i] > 127)
                    break;
                output[output_len] = static_cast<char>(name[i]);
                output_len++;
            }
            output[output_len] = '\0';
        }

        // Print name in readable format
        void printName() const
        {
            printf("name: ");
            for (int i = 0; i < 20; ++i)
            {
                if (name[i] == 0 || name[i] == 2147483646 || name[i] > 127)
                    break;
                printf("%c", static_cast<char>(name[i]));
            }
            printf("\n");
        }

        // Get name length
        int getNameLength() const
        {
            int length = 0;
            for (int i = 0; i < 20; ++i)
            {
                if (name[i] == 0 || name[i] == 2147483646 || name[i] > 127)
                    break;
                length++;
            }
            return length;
        }

        // Set name from character array
        void setNameFromChars(const char input[64])
        {
            int i = 0;
            while (i < 20 && input[i] != '\0')
            {
                name[i] = static_cast<uint32_t>(input[i]);
                i++;
            }
            while (i < 20)
            {
                name[i] = 2147483646; // Use same placeholder as Python script
                i++;
            }
        }

        void print()
        {
            printf("id: %d\n", id);
            
            // Print name using the new GPU device function
            printName();
            
            printf("data_deps: ");
            for (int i = 0; i < 10; ++i)
            {
                printf("%d ", data_deps[i]);
            }
            printf("\n");
            printf("type: %d\n", static_cast<int32_t>(type));
            printf("comm_para: %lu\n", comm_para);
            printf("comm_size: %lu\n", comm_size);
            printf("comm_src: %lu\n", comm_src);
            printf("comm_dst: %lu\n", comm_dst);
            printf("comm_type: %lu\n", comm_type);
            printf("involved_dim_1: %d\n", involved_dim_1);
            printf("involved_dim_2: %d\n", involved_dim_2);
            printf("involved_dim_3: %d\n", involved_dim_3);
            printf("durationMicros: %d\n", durationMicros);
        }
    };

    struct ChakraNodesData
    {
        uint32_t data[NPU_NUM][50000];  // adjusted to 256 files × 400K elements, based on actual data size
    };

    struct ChakraNodes
    {
        ChakraNode nodes[MAX_CHAKRA_NODES_PER_NPU];
    };

    struct ChakraNodesForNoDP
    {
        ChakraNode current_exec_nodes[CURRENT_EXEC_NODES_MAX];
    };


    struct HardwareResource
    {
        bool comp_ocupy;
        // bool comm_ocupy;
        bool one_task_finish;

        HardwareResource()
            : comp_ocupy(false), // Initialize to false
              one_task_finish(true)
        { // Initialize to true
        }
    };

    struct NextProcessTimes
    {
        uint64_t times_abs[MAX_CHAKRA_NODES_PER_NPU];
    };

    struct NextProcessTimeE : public madrona::Archetype<NextProcessTimes>
    {
    };

    struct OneNPUFinishedFlag
    {
        bool is_finished;
        OneNPUFinishedFlag()
            : is_finished(false)
        { // Initialize to false
        }
    };

    //check finish
    struct CheckNpuFinishFlag
    {
        int32_t counter;
        CheckNpuFinishFlag()
            : counter(0)
        { // Initialize to 0
        }
    };

    struct CheckNpuFinishE : public madrona::Archetype<CheckNpuFinishFlag>
    {
    };

    struct ProcessingCompTask
    {
        // means process times
        int64_t time_finish_ns;
        TaskState state;
        int32_t node_id;

        // Default constructor
        ProcessingCompTask()
            : time_finish_ns(0),      // Initialize to 0
              state(TaskState::INIT), // Initialize to INIT
              node_id(-1)
        { // Initialize to -1 (invalid node)
        }
    };

   

    extern madrona::SpinLock flows_id_lock;

    struct ProcessingCommTasks
    {
        ProcessingCommTask tasks[MAX_COMM_TASK_PER_NPU];

        int64_t flow_id;

        // Default constructor
        ProcessingCommTasks()
        {
            for (int i = 0; i < MAX_COMM_TASK_PER_NPU; ++i)
            {
                tasks[i] = ProcessingCommTask(); // Initialize each task
            }
        }

        // Get total flow count of all tasks
        int getFlowId()
        {
            // int totalFlowCount = 0;
            // for (int i = 0; i < MAX_COMM_TASK_PER_NPU; ++i)
            // {
            //     if (tasks[i].state != TaskState::INIT)
            //     { // Only count valid tasks
            //         totalFlowCount += tasks[i].flow_count;
            //     }
            // }
            // return totalFlowCount;
            flows_id_lock.lock();
            flow_id += 1;
            flows_id_lock.unlock();
            return flow_id;
        }

        // Check if a node with specified ID exists
        bool containsNodeId(int32_t node_id) const
        {
            for (int i = 0; i < MAX_COMM_TASK_PER_NPU; ++i)
            {
                // Task state may be START or FINISH, not yet processed
                if (tasks[i].state != TaskState::INIT && tasks[i].node_id == node_id)
                {
                    return true; // Found matching task
                }
            }
            return false; // No matching task found
        }

        void setFinish(int32_t node_id, int64_t time_finish_ns,uint32_t npu_id)
        {
            for (int i = 0; i < MAX_COMM_TASK_PER_NPU; ++i)
            {
                if (tasks[i].state == TaskState::START && tasks[i].node_id == node_id)
                {
                    #if SYS_LOG
                    if (SYS_LOG_TARGET_NODE == npu_id) {
                    printf("node id %d -> setFinish\n", node_id);
                    }
                    #endif
                    #if SIMPLE_LOG_MODE
                    if (SYS_LOG_TARGET_NODE == npu_id) {
                    printf("node id %d -> setFinish\n", node_id);
                    }
                    #endif
                    tasks[i].state = TaskState::FINISH;
                    tasks[i].time_end_ns = time_finish_ns;
                    tasks[i].time_finish_ns = time_finish_ns-tasks[i].time_start_ns;
                    break;
                }
            }
        }

        // Method to add a task
        bool addTask(const ProcessingCommTask &new_task)
        {
            for (int i = 0; i < MAX_COMM_TASK_PER_NPU; ++i)
            {
                if (tasks[i].state == TaskState::INIT)
                {                                      // Find first position with node_id == -1
                    tasks[i] = new_task;               // Place new task
                    tasks[i].state = TaskState::START; // Mark task as valid
                    return true;                       // Add successful
                }
            }
            return false; // No free position, add failed
        }

        ProcessingCommTask& getNextFreeTask()
        {
            for (int i = 0; i < MAX_COMM_TASK_PER_NPU; ++i)
            {
                if (tasks[i].state == TaskState::INIT)
                {
                    return tasks[i];
                }
            }
            return tasks[0];
        }

        // Check if at least one node with is_none == false exists
        bool has_task() const
        {
            for (int i = 0; i < MAX_COMM_TASK_PER_NPU; ++i)
            {
                if (tasks[i].state == TaskState::START || tasks[i].state == TaskState::FINISH)
                {
                    // If found a node with is_none == false
                    return true;
                }
            }
            return false; // If all nodes have is_none == true
        }

        // Find all tasks with time_finish_ns <= t and set their is_none to true
        int dequeueTasksByTime(int64_t t, ProcessingCommTask result[], int max_result_size)
        {
            int count = 0;
            for (int i = 0; i < MAX_COMM_TASK_PER_NPU && count < max_result_size; ++i)
            {
                if (tasks[i].state == TaskState::FINISH && tasks[i].time_end_ns <= t)
                {
                    // #if SYS_LOG
                    // if (SYS_LOG_TARGET_NODE == id.value) {
                    //     printf(" tasks.node_id %d FINISH\n", tasks[i].node_id);
                    // }
                    // #endif
                    
                    // #if SIMPLE_LOG_MODE
                    // if (SYS_LOG_TARGET_NODE == id.value) {
                    //     printf(" tasks.node_id %d FINISH\n", tasks[i].node_id);
                    // }
                    // #endif
                    result[count++] = tasks[i];       // Place task in result array
                    tasks[i].state = TaskState::INIT; // Mark task as invalid (dequeue)
                                                      // tasks[i].node_id = -1;         // Reset node_id
                                                      // tasks[i].time_finish_ns = 0;   // Reset time_finish_ns
                }
            }
            return count; // Return number of tasks found
        }
    };

    struct NpuNode : public madrona::Archetype<
                         NpuID,
                         ChakraNodes,
                         HardwareResource,
                         ProcessingCompTask,
                         ProcessingCommTasks,
                         OneNPUFinishedFlag,
                         ChakraNodesForNoDP>
    {
    };

 

    // ID &id, CollectiveCommType &collective_comm_type, CommParams &comm_params,TaskFlows &taskFlows
    struct ProcessComm_E : public madrona::Archetype<
                               NpuID, NodeID,
                               TaskFlows>
    {
    };

    // Ring Parameters for collective communication
    struct RingParams
    {
        uint32_t ring_dim0;
        uint32_t ring_dim1;
        uint32_t ring_dim2;
        uint32_t chunks_num;
        uint32_t npu_num;
        // Default constructor
        RingParams() : ring_dim0(0), ring_dim1(0), ring_dim2(0), chunks_num(0), npu_num(0) {}
    };

    // Entity archetype to store ring configuration parameters
    struct RingConfigEntity : public madrona::Archetype<RingParams>
    {
    };

    struct RecvNodeFlag
    {
        uint64_t comm_src;
        uint64_t comm_dst;
    };



    struct ProcessRecvComm_E : public madrona::Archetype<
    NpuID, NodeID,
    RecvNodeFlag>
    {
    };
} // namespace madEscape
