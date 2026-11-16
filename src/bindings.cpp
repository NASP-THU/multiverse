#include "mgr.hpp"

#include <madrona/macros.hpp>
#include <madrona/py/bindings.hpp>
#include <nanobind/ndarray.h> // Include for nanobind::ndarray
#include <cstring> // For std::memcpy

namespace nb = nanobind;

namespace madEscape {

// This file creates the python bindings used by the learning code.
// Refer to the nanobind documentation for more details on these functions.
NB_MODULE(madrona_escape_room, m) {
    // Each simulator has a madrona submodule that includes base types
    // like madrona::py::Tensor and madrona::py::PyExecMode.
    madrona::py::setupMadronaSubmodule(m);

    // Expose Topo directly under the madrona_escape_room module
    nb::class_<Topo>(m, "Topo")
        .def(nb::init<>())
        .def_prop_rw("aj_link",
            [](Topo &self) {
                return nb::ndarray<nb::numpy, uint16_t>(
                    self.aj_link, {MAX_LINKS_NUM, 5}, nb::handle()); // Changed to uint16_t
            },
            [](Topo &self, nb::ndarray<> arr) {
                if (arr.ndim() != 2 || arr.shape(0) != MAX_LINKS_NUM || arr.shape(1) != 5)
                    throw nb::value_error("aj_link must have shape (MAX_LINKS_NUM, 5)");
                std::memcpy(self.aj_link, arr.data(), sizeof(uint16_t) * MAX_LINKS_NUM * 5); // Changed to uint16_t
            }
        )
        .def_rw("link_num", &Topo::link_num)
        .def_rw("net_npu_num", &Topo::net_npu_num)
        .def_rw("sw_num", &Topo::sw_num) // Bind sw_num
        .def_prop_rw("port_num",
            [](Topo &self) {
                return nb::ndarray<nb::numpy, int16_t>(
                    self.port_num, {MAX_SW_NUM + MAX_NET_NPU_NUM}, nb::handle()); // Changed to int16_t
            },
            [](Topo &self, nb::ndarray<> arr) {
                if (arr.ndim() != 1 || arr.shape(0) != (MAX_SW_NUM + MAX_NET_NPU_NUM))
                    throw nb::value_error("port_num must have shape (MAX_SW_NUM + MAX_NET_NPU_NUM)");
                std::memcpy(self.port_num, arr.data(), sizeof(int16_t) * (MAX_SW_NUM + MAX_NET_NPU_NUM)); // Changed to int16_t
            }
        )

        .def_prop_rw("next_hop_port",
            [](Topo &self) {
                return nb::ndarray<nb::numpy, int16_t>(
                    self.next_hop_port, {MAX_ALL_PORT_NUM}, nb::handle()); // Changed to int16_t
            },
            [](Topo &self, nb::ndarray<> arr) {
                if (arr.ndim() != 1 || arr.shape(0) != MAX_ALL_PORT_NUM)
                    throw nb::value_error("next_hop_port must have shape (MAX_ONE_SW_PORT_NUM)");
                std::memcpy(self.next_hop_port, arr.data(), sizeof(int16_t) * MAX_ALL_PORT_NUM); // Changed to int16_t
            }
        )

        .def_rw("sw_port_num", &Topo::sw_port_num)
        .def_rw("net_npu_port_num", &Topo::net_npu_port_num)
        ;

    // Note: Fib class is too large for nanobind (90MB > 16MB limit)
    // We create a minimal FibProxy class instead and pass data via tensors
    struct FibProxy {
        // Empty proxy class for Fib - actual data passed via tensors
    };
    
    nb::class_<FibProxy>(m, "Fib")
        .def(nb::init<>());

    nb::class_<Manager> (m, "SimManager")
        .def("__init__", [](Manager *self,
                            madrona::py::PyExecMode exec_mode,
                            int64_t gpu_id,
                            int64_t num_worlds,
                            int64_t rand_seed,
                            bool auto_reset,
                            bool enable_batch_renderer,
                            uint32_t k_aray,
                            uint32_t cc_method
                        ) 
        {
            // Validate basic parameters
            if (num_worlds <= 0 || num_worlds > 1000) {
                throw nb::value_error("num_worlds must be between 1 and 1000");
            }
            
            if (gpu_id < 0 || gpu_id >= 8) {
                throw nb::value_error("gpu_id must be between 0 and 15");
            }
            
            // Create Manager with basic config only
            new (self) Manager(Manager::Config {
                .execMode = exec_mode,
                .gpuID = (int)gpu_id,
                .numWorlds = (uint32_t)num_worlds,
                .randSeed = (uint32_t)rand_seed,
                .autoReset = auto_reset,
                .enableBatchRenderer = enable_batch_renderer,
                .kAray = (uint32_t)k_aray,
                .ccMethod = (uint32_t)cc_method
            });
        }, nb::arg("exec_mode"),
           nb::arg("gpu_id"),
           nb::arg("num_worlds"),
           nb::arg("rand_seed"),
           nb::arg("auto_reset"),
           nb::arg("enable_batch_renderer") = false,
           nb::arg("k_aray") = 4,
           nb::arg("cc_method") = 0
        )
        .def("step", &Manager::step)
        .def("reset_tensor", &Manager::resetTensor)
        .def("action_tensor", &Manager::actionTensor)
        .def("reward_tensor", &Manager::rewardTensor)
        .def("done_tensor", &Manager::doneTensor)
        .def("self_observation_tensor", &Manager::selfObservationTensor)
        .def("partner_observations_tensor", &Manager::partnerObservationsTensor)
        .def("room_entity_observations_tensor",
             &Manager::roomEntityObservationsTensor)
        .def("door_observation_tensor",
             &Manager::doorObservationTensor)
        .def("lidar_tensor", &Manager::lidarTensor)
        .def("steps_remaining_tensor", &Manager::stepsRemainingTensor)
        .def("rgb_tensor", &Manager::rgbTensor)
        .def("depth_tensor", &Manager::depthTensor)
        .def("results_tensor", &Manager::resultsTensor)  // fei add in 202412015
        .def("results2_tensor", &Manager::results2Tensor)
        .def("madronaEvents_tensor", &Manager::madronaEventsTensor)
        .def("madronaEventsResult_tensor", &Manager::madronaEventsResultTensor)
        .def("simulation_time_tensor", &Manager::simulationTimeTensor)
        .def("processParams_tensor",&Manager::processParamsTensor)
        .def("topo_tensor", &Manager::topoTensor)
        .def("fib_tensor", &Manager::fibTensor)
    ;
}

}
