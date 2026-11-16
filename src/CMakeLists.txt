set(SIMULATOR_SRCS
    collective/topo_logic.hpp
    sys/llm_system.hpp sys/llm_system.cpp
    sys/system_init.hpp sys/system_init.cpp
    sys/chakra_node_processing.hpp sys/chakra_node_processing.cpp
    sys/communication_system.hpp sys/communication_system.cpp
    sys/time_management.hpp sys/time_management.cpp
    sys/net_sys_interface.hpp sys/net_sys_interface.cpp
    types.hpp types/llm_sys_types.hpp types/common_types.hpp types/simulation_types.hpp types/packet_types.hpp types/flow_types.hpp types/network_device_types.hpp types/switch_types.hpp types/topology_types.hpp types/agent_types.hpp types/event_types.hpp types/global_types.hpp
    network/queue_system.hpp network/queue_system.cpp
    network/congestion_control_system.hpp network/congestion_control_system.cpp
    network/packet_system.hpp network/packet_system.cpp
    network/flow_system.hpp network/flow_system.cpp
    network/nic_system.hpp network/nic_system.cpp
    network/forward_system.hpp network/forward_system.cpp
    network/transmit_system.hpp network/transmit_system.cpp
    network/network_config_system.hpp network/network_config_system.cpp
    sim.hpp sim.inl sim.cpp
    level_gen.hpp level_gen.cpp
)

add_library(mad_escape_cpu_impl STATIC
    ${SIMULATOR_SRCS}
)

target_link_libraries(mad_escape_cpu_impl
    PUBLIC
        madrona_mw_core
    PRIVATE
        madrona_common
        madrona_mw_physics
        madrona_rendering_system
)

add_library(mad_escape_mgr STATIC
    mgr.hpp mgr.cpp
)

target_link_libraries(mad_escape_mgr 
    PUBLIC
        madrona_python_utils
    PRIVATE
        mad_escape_cpu_impl
        madrona_mw_cpu
        madrona_common
        madrona_importer
        madrona_physics_loader
        madrona_render
)

if (TARGET madrona_mw_gpu)
    madrona_build_compile_defns(
        OUT_TARGET
            mad_escape_gpu_srcs
        SOURCES_DEFN
            GPU_HIDESEEK_SRC_LIST
        FLAGS_DEFN
            GPU_HIDESEEK_COMPILE_FLAGS 
        SRCS
            ${SIMULATOR_SRCS}
    )

    target_link_libraries(mad_escape_mgr PRIVATE
        mad_escape_gpu_srcs
        madrona_mw_gpu
    )
endif ()

target_compile_definitions(mad_escape_mgr PRIVATE
    -DDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../data/"
)

madrona_python_module(madrona_escape_room
    bindings.cpp
)

target_link_libraries(madrona_escape_room PRIVATE
    mad_escape_mgr
)

# if (TARGET madrona_viz)
#     add_executable(viewer viewer.cpp)
#     target_link_libraries(viewer PRIVATE
#         madrona_mw_core mad_escape_mgr madrona_viz)

#     target_compile_definitions(viewer PRIVATE
#         -DDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../data/"
#     )
# endif()

add_executable(headless headless.cpp)
target_link_libraries(headless madrona_mw_core mad_escape_mgr)
