#pragma once

#include <madrona/custom_context.hpp>
#include <madrona/rand.hpp>
#include <madrona/sync.hpp>  // Include lock support
#include <madrona/taskgraph_builder.hpp>
#include "consts.hpp"
#include "types.hpp"

namespace madEscape {

    using madrona::CountT;
    using madrona::Entity;

    template <typename T1, typename T2>
        class Map {
            private:
                // Pair<T1, T2> Entry = new Pair(T1 key, T2 value)
                class Entry {
                    public:
                        T1 key;
                        T2 value;
                        Entry() : key(T1()), value(T2()) {}  // Use default constructors of T1 and T2
                        Entry(const T1 &key, const T2 &value) : key(key), value(value) {}
                };
                Entry entries[MAP_SIZE];
                int length;

            public:
                // Custom classes for key and value need to have overloaded constructors
                Map() : length(0) {
                    for (int i = 0; i < MAP_SIZE; ++i) {
                        entries[i] = Entry();  // Initialize each Entry
                    }
                }
                // Read-only search, no insertion. Returns nullptr or special value if not found.
                Entry* find(const T1 &key) {
                    for (int i = 0; i < length; ++i) {
                        if (entries[i].key == key)
                            return &entries[i];
                    }
                    return nullptr;  // Return nullptr if not found
                }

                // Writable/insertable: insert if not found and return reference
                T2 &operator[](const T1 &key) {
                    for (int i = 0; i < length; ++i) {
                        if (entries[i].key == key)
                            return entries[i].value;
                    }
                    assert(length < MAP_SIZE - 1);
                    entries[length] = Entry(key, T2{});
                    return entries[length++].value;
                }

                T2 &at(const T1 &key) {
                    return get(key);
                }

                Entry* endEntry() {
                    return &entries[length];  // Return pointer to end of array, indicating not found
                }

                void clear() {
                    for (int i = 0; i < length; i++) {
                        entries[i] = Entry();
                    }
                }

                // Search first then index: assert(map.find(key)); map[key]
                T2 &get(const T1 &key) {
                    for (int i = 0; i < length; i++) {
                        if (entries[i].key == key) {
                            return entries[i].value;
                        }
                    }
                    // If key doesn't exist, add new entry
                    assert(length < MAP_SIZE - 1);
                    entries[length] = Entry(key, T2());
                    return entries[length++].value;
                }

                void set(const T1 &key, const T2 &value) {
                    for (int i = 0; i < length; i++) {
                        if (entries[i].key == key) {
                            entries[i].value = value;
                            return;
                        }
                    }
                    // If key doesn't exist, add new entry
                    assert(length < MAP_SIZE - 1);
                    entries[length] = Entry(key, value);
                    length++;
                }

                int size() const {
                    return length;
                }

                bool remove(const T1& key) {
                    for (int i = 0; i < length; i++) {
                        if (entries[i].key == key) {
                            // Found the key, now remove it
                            // Overwrite this element by moving subsequent elements forward
                            for (int j = i; j < length - 1; j++) {
                                entries[j] = entries[j + 1];
                            }
                            // Decrease length
                            length--;
                            // Since our array is now one element smaller than actual length,
                            // we can choose to clean up the last element, but this is not necessary,
                            // as it will be overwritten on the next addition or when reaching that position.
                            // But to keep data tidy, we can choose to reset the last element.
                            entries[length] = Entry(); // Optional
                            return true; // Indicates successful deletion
                        }
                    }
                    return false; // If key not found, return false
                }

                // Iterator class supporting key-value pair traversal
                class Iterator {
                    private:
                        Entry* current;
                    public:
                        // Constructor, initialize iterator
                        Iterator(Entry* start) : current(start) {}

                        // Prefix++, move to next element
                        Iterator& operator++() {
                            ++current;
                            return *this;
                        }

                        // Dereference, return current key-value pair
                        Entry& operator*() const {
                            return *current;
                        }

                        // Comparison operator for end condition checking
                        bool operator!=(const Iterator& other) const {
                            return current != other.current;
                        }
                };

                Iterator begin() {
                    return Iterator(entries);
                }

                Iterator end() {
                    return Iterator(entries + length);
                }

        };

    class Engine;
    // This enum is used to uniquely identify each ECS system task graph
    // that can be separately executed
    enum class TaskGraphID : uint32_t {
        Step,
        NumTaskGraphs,
    };

    // This enum is used by the Sim and Manager classes to track the export slots
    // for each component exported to the training code.
    enum class ExportID : uint32_t {
        Reset,
        Action,
        Reward,
        Done,
        SelfObservation,
        PartnerObservations,
        RoomEntityObservations,
        DoorObservation,
        Lidar,
        StepsRemaining,
        Results,
        Results2,
        SimulationTime,
        MadronaEvent,
        MadronaEvents,
        MadronaEventsResult,
        ProcessParams,
        TopoTensor,
        FibTensor,
        //for llm sys
        ChakraNodesData,
        NumExports,

    };

    // Stores values for the ObjectID component that links entities to
    // render / physics assets.
    enum class SimObject : uint32_t {
        Cube,
        Wall,
        Door,
        Agent,
        Button,
        Plane,
        NumObjects,
    };

    // The Sim class encapsulates the per-world state of the simulation.
    // Sim is always available by calling ctx.data() given a reference
    // to the Engine / Context object that is passed to each ECS system.
    //
    // Per-World state that is frequently accessed but only used by a few
    // ECS systems should be put in a singleton component rather than
    // in this class in order to ensure efficient access patterns.
    struct Sim : public madrona::WorldBase {
        struct Config {
            bool autoReset;
            RandKey initRandKey;
            madrona::phys::ObjectManager *rigidBodyObjMgr;
            const madrona::render::RenderECSBridge *renderBridge;
            uint32_t kAray; // fei add in 20241215
            uint32_t ccMethod; // fei add in 20241215
            uint32_t maxEquivalentPathNum;
        };

        // This class would allow per-world custom data to be passed into
        // simulator initialization, but that isn't necessary in this environment
        struct WorldInit {};

        // Sim::registerTypes is called during initialization
        // to register all components & archetypes with the ECS.
        static void registerTypes(madrona::ECSRegistry &registry,
                const Config &cfg);

        // Sim::setupTasks is called during initialization to build
        // the system task graph that will be invoked by the
        // Manager class (src/mgr.hpp) for each step.
        static void setupTasks(madrona::TaskGraphManager &mgr,
                const Config &cfg);

        // The constructor is called for each world during initialization.
        // Config is global across all worlds, while WorldInit (src/init.hpp)
        // can contain per-world initialization data, created in (src/mgr.cpp)
        Sim(Engine &ctx,
                const Config &cfg,
                const WorldInit &);
        // The base random key that episode random keys are split off of
        madrona::RandKey initRandKey;
        // Should the environment automatically reset (generate a new episode)
        // at the end of each episode?
        bool autoReset;

        // Are we enabling rendering? (whether with the viewer or not)
        bool enableRender;

        // Current episode within this world
        uint32_t curWorldEpisode;
        // Random number generator state
        madrona::RNG rng;

        /*
        // Floor plane entity, constant across all episodes.
        Entity floorPlane;

        // Border wall entities: 3 walls to the left, up and down that define
        // play area. These are constant across all episodes.
        Entity borders[3];

        // Agent entity references. This entities live across all episodes
        // and are just reset to the start of the level on reset.
        Entity agents[consts::numAgents];
        */

        uint32_t max_flow_num;
        uint32_t max_equivalent_path_num;

        Entity agents[1];
        bool entities_created;

        Entity inPorts[MAX_ALL_PORT_NUM];
        Entity ePorts[MAX_ALL_PORT_NUM];
        uint32_t numInPort;
        uint32_t numEPort;

        Entity _switches[MAX_SW_NUM];

        // indexed by the flow_id
        // each net_npu has a map of snd_flows and a map of rcv_flows
        Map<uint64_t, Entity> snd_flows[MAX_NET_NPU_NUM];
        Map<uint64_t, Entity> recv_flows[MAX_NET_NPU_NUM];
        Map<uint64_t, Entity> recv_snd_flows[MAX_NET_NPU_NUM];
        Map<uint64_t, Entity> snd_recv_flows[MAX_NET_NPU_NUM];
        uint32_t flow_cnt[MAX_NET_NPU_NUM]; // for flow_id

        Entity _net_npus[MAX_NET_NPU_NUM];

        Entity _nics[MAX_NET_NPU_NUM][MAX_NIC_NUM_PER_NET_NPU];
        uint32_t num_nic;
        // NIC idx
        uint8_t nic_idx[MAX_ALL_PORT_NUM];

        int32_t aj_link[MAX_LINKS_NUM][5];
        int32_t num_link;

        int32_t num_net_npu;
        int32_t num_switch;

        int32_t port_num[MAX_SW_NUM + MAX_NET_NPU_NUM]; // Changed to int32_t

        //next_hop
        int32_t next_hop_port[MAX_ALL_PORT_NUM]; // Changed to int32_t

        int32_t sw_port_num;
        int32_t net_npu_port_num;

        //FIB
        uint8_t (*fib) [MAX_SW_NUM + MAX_NET_NPU_NUM][MAX_ONE_SW_PORT_NUM];
        uint8_t (*next_hop_num) [MAX_SW_NUM + MAX_NET_NPU_NUM];


        Entity tmp_flow_entt_array[MAX_CONCURRENT_FLOW_NUM];

        uint64_t tmp_flow_ids[NPU_NUM][MAP_SIZE];

        //spine lock to address multiple-to-one write
        madrona::SpinLock flow_lock;
    
        // system
        Entity tmp_process_e_entt_array[6000];
        Entity tmp_process_recv_e_entt_array[6000];

        // LLM system related members
        Entity init_entity;
        Entity timer_entity;
        Entity npu_entities[NPU_NUM];
        Entity next_process_time_entity;
        Entity sys_config_entity;
        Entity ring_config_entity;  // Entity to store ring configuration parameters
        Entity npus_chakra_exec_entity[NPU_NUM][MAX_CHAKRA_NODES_PER_NPU];
        Entity check_npu_finish_entity;
        //true mean recv end is ready
        // 0：init 1：recv ready 2：ocupy - sending flow 
        int send_recv_map_recvend[NPU_NUM][NPU_NUM];

        bool sys_chakra_entities_created;
        Entity ring_topo_entity[3];
    };

    class Engine : public ::madrona::CustomContext<Engine, Sim> {
        public:
            using CustomContext::CustomContext;
            // These are convenience helpers for creating renderable
            // entities when rendering isn't necessarily enabled
            template <typename ArchetypeT>
                inline madrona::Entity makeRenderableEntity();
            inline void destroyRenderableEntity(Entity e);
    };

}

#include "sim.inl"
