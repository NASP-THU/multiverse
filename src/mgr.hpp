#pragma once

#include <memory>
#include <madrona/py/utils.hpp>
#include <madrona/exec_mode.hpp>
#include <madrona/render/render_mgr.hpp>
#include "types.hpp"
#include <vector>
#include <map>
#include <tuple>
#include <set>
#include <deque>

namespace madEscape {
    // The Manager class encapsulates the linkage between the outside training
    // code and the internal simulation state (src/sim.hpp / src/sim.cpp)
    // Manager is responsible for initializing the simulator, loading physics
    // and rendering assets off disk, and mapping ECS components to tensors
    // for learning


     // NetworkGraph class for topology parsing (C++ version of Python NetworkGraph)
     class NetworkGraph {
        public:
            int num_nodes;
            int num_npus;
            // graph: node -> list of (neighbor, weight, bandwidth)
            std::map<int, std::vector<std::tuple<int, int, int>>> graph;
    
            NetworkGraph() : num_nodes(0), num_npus(0) {}
    
            NetworkGraph(int nodes, int npus) : num_nodes(nodes), num_npus(npus) {}
    
            void add_link(int u, int v, int w, int bw, int /*lat*/) {
                graph[u].push_back(std::make_tuple(v, w, bw));
                graph[v].push_back(std::make_tuple(u, w, bw));
            }
    
            const std::vector<std::tuple<int, int, int>>& get_neighbors(int node) const {
                static const std::vector<std::tuple<int, int, int>> empty_vec;
                auto it = graph.find(node);
                if (it != graph.end()) {
                    return it->second;
                }
                return empty_vec;
            }
    
            bool is_connected() const {
                if (num_nodes == 0) return true;
                
                std::set<int> visited;
                std::deque<int> queue;
                queue.push_back(0);
                
                while (!queue.empty()) {
                    int node = queue.front();
                    queue.pop_front();
                    
                    if (visited.find(node) == visited.end()) {
                        visited.insert(node);
                        auto it = graph.find(node);
                        if (it != graph.end()) {
                            for (const auto& neighbor_tuple : it->second) {
                                queue.push_back(std::get<0>(neighbor_tuple));
                            }
                        }
                    }
                }
                
                return visited.size() == static_cast<size_t>(num_nodes);
            }
        };
    
        // PortMapping structure for storing local and global port mappings
        struct PortMapping {
            // local_ports: node -> {neighbor -> local_port_id}
            std::map<int, std::map<int, int>> local_ports;
            // global_ports: node -> {neighbor -> global_port_id}
            std::map<int, std::map<int, int>> global_ports;
    
            PortMapping() {}
        };
    
        // TopoParseResult: encapsulates all return values from parse_topo
        struct TopoParseResult {
            // Arrays
            std::vector<std::array<int, 5>> aj_link;
            std::vector<int> port_num;
            std::vector<int> switch_next_hop_port;
            std::vector<int> net_npu_next_hop_port;
            
            // Scalars
            int link_num;
            int net_npu_num;
            int sw_num;
            int switch_port_num;
            int net_npu_port_num;
            
            // Objects
            NetworkGraph graph;
            PortMapping port_mapping;
            PortMapping separate_port_mapping;
    
            TopoParseResult() 
                : link_num(0), net_npu_num(0), sw_num(0), 
                  switch_port_num(0), net_npu_port_num(0) {}
        };

    class Manager {
        public:
            struct Config {
                madrona::ExecMode execMode;  // CPU or CUDA
                int gpuID;                   // Which GPU for CUDA backend?
                uint32_t numWorlds;          // Simulation batch size
                uint32_t randSeed;           // Seed for random world gen
                bool autoReset;              // Immediately generate new world on episode end
                bool enableBatchRenderer;
                uint32_t batchRenderViewWidth = 64;
                uint32_t batchRenderViewHeight = 64;
                madrona::render::APIBackend *extRenderAPI = nullptr;
                madrona::render::GPUDevice *extRenderDev = nullptr;
                uint32_t kAray;              // fei add in 20241215
                uint32_t ccMethod;           // fei add in 20241215
                uint32_t maxEquivalentPathNum;
            };

            Manager(const Config &cfg);
            ~Manager();

            void step();

            // These functions export Tensor objects that link the ECS
            // simulation state to the python bindings / PyTorch tensors (src/bindings.cpp)
            madrona::py::Tensor resetTensor() const;
            madrona::py::Tensor actionTensor() const;
            madrona::py::Tensor rewardTensor() const;
            madrona::py::Tensor doneTensor() const;
            madrona::py::Tensor selfObservationTensor() const;
            madrona::py::Tensor partnerObservationsTensor() const;
            madrona::py::Tensor roomEntityObservationsTensor() const;
            madrona::py::Tensor doorObservationTensor() const;
            madrona::py::Tensor lidarTensor() const;
            madrona::py::Tensor stepsRemainingTensor() const;
            madrona::py::Tensor rgbTensor() const;
            madrona::py::Tensor depthTensor() const;
            madrona::py::Tensor resultsTensor() const;
            madrona::py::Tensor results2Tensor() const;
            madrona::py::Tensor madronaEventsTensor() const;
            madrona::py::Tensor simulationTimeTensor() const;
            madrona::py::Tensor madronaEventsResultTensor() const;
            madrona::py::Tensor processParamsTensor() const;
            madrona::py::Tensor topoTensor() const;
            madrona::py::Tensor fibTensor() const;
            // for llm-sys
            madrona::py::Tensor chakraNodesDataTensor() const;
            // These functions are used by the viewer to control the simulation
            // with keyboard inputs in place of DNN policy actions
            void triggerReset(int32_t world_idx);
            void setAction(int32_t world_idx,
                    int32_t agent_idx,
                    int32_t move_amount,
                    int32_t move_angle,
                    int32_t rotate,
                    int32_t grab);

            madrona::render::RenderManager & getRenderManager();
            void buildFIBTable();
            void parseTopologyFile(std::string topology_file);
            void convertTopoDataToTopoTensor();
            void convertFibDataToFibTensor();
            madrona::py::Tensor getCppTopoTensor() const;
            madrona::py::Tensor getCppFibTensor() const;

        private:
            struct Impl;
            struct CPUImpl;
            struct CUDAImpl;
            std::unique_ptr<Impl> impl_;

             // Type aliases for FIB building
             using NextHopsMap = std::map<int, std::set<int>>;
             using RoutingEntry = std::pair<int, std::vector<int>>;
             using RoutingTable = std::vector<RoutingEntry>;
             
             // FIB building helper functions (all return void, use member variables to avoid copying)
             void dijkstraECMP(const NetworkGraph& graph, int src, NextHopsMap& out_next_hops);
             void computeRoutingTableForSource(int src, const NetworkGraph& graph, 
                                               const PortMapping& port_mapping,
                                               RoutingTable& out_routing_table);
             void buildGlobalRoutingTableParallel(const NetworkGraph& graph, 
                                                  const PortMapping& port_mapping,
                                                  int num_threads = 0);
             void buildGlobalRoutingTableWithArray(const NetworkGraph& graph, 
                                                   const PortMapping& port_mapping);
    };
}
