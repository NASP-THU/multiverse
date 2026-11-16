#include "mgr.hpp"
#include "sim.hpp"
#include <madrona/utils.hpp>
#include <madrona/importer.hpp>
#include <madrona/physics_loader.hpp>
#include <madrona/tracing.hpp>
#include <madrona/mw_cpu.hpp>
#include <madrona/render/api.hpp>
#include <array>
#include <charconv>
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <string>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <vector>
#include <thread>
#include <future>
#include <atomic>
#include <mutex>
#include <chrono>
#include <queue>
#include <functional>
#include <limits>
#include <regex>
#include "../external/madrona/external/tinyusdz/src/external/jsonhpp/nlohmann/json.hpp"
#include "types/common_types.hpp"
#include "types/llm_sys_types.hpp"

#ifdef MADRONA_CUDA_SUPPORT
#include <madrona/mw_gpu.hpp>
#include <madrona/cuda_utils.hpp>
#endif

using namespace madrona;
using namespace madrona::math;
using namespace madrona::phys;
using namespace madrona::py;
using namespace std;

namespace madEscape {

    struct RenderGPUState {
        render::APILibHandle apiLib;
        render::APIManager apiMgr;
        render::GPUHandle gpu;
    };


    static inline Optional<RenderGPUState> initRenderGPUState(
            const Manager::Config &mgr_cfg)
    {
        if (mgr_cfg.extRenderDev || !mgr_cfg.enableBatchRenderer) {
            return Optional<RenderGPUState>::none();
        }

        auto render_api_lib = render::APIManager::loadDefaultLib();
        render::APIManager render_api_mgr(render_api_lib.lib());
        render::GPUHandle gpu = render_api_mgr.initGPU(mgr_cfg.gpuID);

        return RenderGPUState {
            .apiLib = std::move(render_api_lib),
                .apiMgr = std::move(render_api_mgr),
                .gpu = std::move(gpu),
        };
    }

    static inline Optional<render::RenderManager> initRenderManager(
            const Manager::Config &mgr_cfg,
            const Optional<RenderGPUState> &render_gpu_state)
    {
        if (!mgr_cfg.extRenderDev && !mgr_cfg.enableBatchRenderer) {
            return Optional<render::RenderManager>::none();
        }

        render::APIBackend *render_api;
        render::GPUDevice *render_dev;

        if (render_gpu_state.has_value()) {
            render_api = render_gpu_state->apiMgr.backend();
            render_dev = render_gpu_state->gpu.device();
        } else {
            render_api = mgr_cfg.extRenderAPI;
            render_dev = mgr_cfg.extRenderDev;
        }

        return render::RenderManager(render_api, render_dev, {
                .enableBatchRenderer = mgr_cfg.enableBatchRenderer,
                .renderMode = render::RenderManager::Config::RenderMode::RGBD,
                .agentViewWidth = mgr_cfg.batchRenderViewWidth,
                .agentViewHeight = mgr_cfg.batchRenderViewHeight,
                .numWorlds = mgr_cfg.numWorlds,
                .maxViewsPerWorld = consts::numAgents,
                .maxInstancesPerWorld = 1000,
                .execMode = mgr_cfg.execMode,
                .voxelCfg = {},
                });
    }

    struct Manager::Impl {
        Config cfg;
        PhysicsLoader physicsLoader;
        WorldReset *worldResetBuffer;
        Action *agentActionsBuffer;
        Optional<RenderGPUState> renderGPUState;
        Optional<render::RenderManager> renderMgr;


        TopoParseResult topoData;  // Store parsed topology data
        //Tensor topoTensor;
        //Tensor fibTensor;
        std::vector<int32_t> topoDataBuffer;  // Buffer to store flat topo data
        std::vector<uint8_t> fibDataBuffer;   // Buffer to store flat fib data (uint8_t)
        std::vector<std::vector<uint32_t>> chakraNodesDataBuffer;  // Buffer to store chakra nodes data [npu_num][variable_size]
        
        // FIB build data structure to avoid large object copying
        struct FibBuildData {
            // Routing entry: (destination node, list of next hop ports)
            using RoutingEntry = std::pair<int, std::vector<int>>;
            using RoutingTable = std::vector<RoutingEntry>;
            using GlobalRoutingTable = std::vector<RoutingTable>;
            
            GlobalRoutingTable global_routing_table;
            std::vector<std::vector<std::vector<uint8_t>>> routing_array;
            std::vector<std::vector<uint8_t>> next_hop_num;
        } fibBuildData;

        inline Impl(const Manager::Config &mgr_cfg,
                PhysicsLoader &&phys_loader,
                WorldReset *reset_buffer,
                Action *action_buffer,
                Optional<RenderGPUState> &&render_gpu_state,
                Optional<render::RenderManager> &&render_mgr)
            : cfg(mgr_cfg),
            physicsLoader(std::move(phys_loader)),
            worldResetBuffer(reset_buffer),
            agentActionsBuffer(action_buffer),
            renderGPUState(std::move(render_gpu_state)),
            renderMgr(std::move(render_mgr))
        {}

        inline virtual ~Impl() {}

        virtual void run() = 0;

        virtual Tensor exportTensor(ExportID slot,
                TensorElementType type,
                madrona::Span<const int64_t> dimensions) const = 0;

        static inline Impl * init(const Config &cfg);
    };

    struct Manager::CPUImpl final : Manager::Impl {
        using TaskGraphT =
            TaskGraphExecutor<Engine, Sim, Sim::Config, Sim::WorldInit>;

        TaskGraphT cpuExec;

        inline CPUImpl(const Manager::Config &mgr_cfg,
                PhysicsLoader &&phys_loader,
                WorldReset *reset_buffer,
                Action *action_buffer,
                Optional<RenderGPUState> &&render_gpu_state,
                Optional<render::RenderManager> &&render_mgr,
                TaskGraphT &&cpu_exec)
            : Impl(mgr_cfg, std::move(phys_loader),
                    reset_buffer, action_buffer,
                    std::move(render_gpu_state), std::move(render_mgr)),
            cpuExec(std::move(cpu_exec))
        {}

        inline virtual ~CPUImpl() final {}

        inline virtual void run()
        {
            cpuExec.run();
        }

        virtual inline Tensor exportTensor(ExportID slot,
                TensorElementType type,
                madrona::Span<const int64_t> dims) const final
        {
            void *dev_ptr = cpuExec.getExported((uint32_t)slot);
            return Tensor(dev_ptr, type, dims, Optional<int>::none());
        }
    };

#ifdef MADRONA_CUDA_SUPPORT
    struct Manager::CUDAImpl final : Manager::Impl {
        MWCudaExecutor gpuExec;
        MWCudaLaunchGraph stepGraph;

        inline CUDAImpl(const Manager::Config &mgr_cfg,
                PhysicsLoader &&phys_loader,
                WorldReset *reset_buffer,
                Action *action_buffer,
                Optional<RenderGPUState> &&render_gpu_state,
                Optional<render::RenderManager> &&render_mgr,
                MWCudaExecutor &&gpu_exec)
            : Impl(mgr_cfg, std::move(phys_loader),
                    reset_buffer, action_buffer,
                    std::move(render_gpu_state), std::move(render_mgr)),
            gpuExec(std::move(gpu_exec)),
            stepGraph(gpuExec.buildLaunchGraphAllTaskGraphs())
        {}

        inline virtual ~CUDAImpl() final {}

        inline virtual void run()
        {
            gpuExec.run(stepGraph);
        }

        virtual inline Tensor exportTensor(ExportID slot,
                TensorElementType type,
                madrona::Span<const int64_t> dims) const final
        {
            void *dev_ptr = gpuExec.getExported((uint32_t)slot);
            return Tensor(dev_ptr, type, dims, cfg.gpuID);
        }
    };
#endif

    static void loadRenderObjects(render::RenderManager &render_mgr)
    {
        StackAlloc tmp_alloc;

        std::array<std::string, (size_t)SimObject::NumObjects> render_asset_paths;
        render_asset_paths[(size_t)SimObject::Cube] =
            (std::filesystem::path(DATA_DIR) / "cube_render.obj").string();
        render_asset_paths[(size_t)SimObject::Wall] =
            (std::filesystem::path(DATA_DIR) / "wall_render.obj").string();
        render_asset_paths[(size_t)SimObject::Door] =
            (std::filesystem::path(DATA_DIR) / "wall_render.obj").string();
        render_asset_paths[(size_t)SimObject::Agent] =
            (std::filesystem::path(DATA_DIR) / "agent_render.obj").string();
        render_asset_paths[(size_t)SimObject::Button] =
            (std::filesystem::path(DATA_DIR) / "cube_render.obj").string();
        render_asset_paths[(size_t)SimObject::Plane] =
            (std::filesystem::path(DATA_DIR) / "plane.obj").string();

        std::array<const char *, (size_t)SimObject::NumObjects> render_asset_cstrs;
        for (size_t i = 0; i < render_asset_paths.size(); i++) {
            render_asset_cstrs[i] = render_asset_paths[i].c_str();
        }

        imp::AssetImporter importer;

        std::array<char, 1024> import_err;
        auto render_assets = importer.importFromDisk(
                render_asset_cstrs, Span<char>(import_err.data(), import_err.size()));

        if (!render_assets.has_value()) {
            FATAL("Failed to load render assets: %s", import_err);
        }

        auto materials = std::to_array<imp::SourceMaterial>({
                { render::rgb8ToFloat(191, 108, 10), -1, 0.8f, 0.2f },
                { math::Vector4{0.4f, 0.4f, 0.4f, 0.0f}, -1, 0.8f, 0.2f,},
                { math::Vector4{1.f, 1.f, 1.f, 0.0f}, 1, 0.5f, 1.0f,},
                { render::rgb8ToFloat(230, 230, 230),   -1, 0.8f, 1.0f },
                { math::Vector4{0.5f, 0.3f, 0.3f, 0.0f},  0, 0.8f, 0.2f,},
                { render::rgb8ToFloat(230, 20, 20),   -1, 0.8f, 1.0f },
                { render::rgb8ToFloat(230, 230, 20),   -1, 0.8f, 1.0f },
                });

        // Override materials
        render_assets->objects[(CountT)SimObject::Cube].meshes[0].materialIDX = 0;
        render_assets->objects[(CountT)SimObject::Wall].meshes[0].materialIDX = 1;
        render_assets->objects[(CountT)SimObject::Door].meshes[0].materialIDX = 5;
        render_assets->objects[(CountT)SimObject::Agent].meshes[0].materialIDX = 2;
        render_assets->objects[(CountT)SimObject::Agent].meshes[1].materialIDX = 3;
        render_assets->objects[(CountT)SimObject::Agent].meshes[2].materialIDX = 3;
        render_assets->objects[(CountT)SimObject::Button].meshes[0].materialIDX = 6;
        render_assets->objects[(CountT)SimObject::Plane].meshes[0].materialIDX = 4;

        imp::ImageImporter img_importer;
        Span<imp::SourceTexture> imported_textures = img_importer.importImages(
                tmp_alloc, {
                (std::filesystem::path(DATA_DIR) /
                 "green_grid.png").string().c_str(),
                (std::filesystem::path(DATA_DIR) /
                 "smile.png").string().c_str(),
                });

        render_mgr.loadObjects(
                render_assets->objects, materials, imported_textures);

        render_mgr.configureLighting({
                { true, math::Vector3{1.0f, 1.0f, -2.0f}, math::Vector3{1.0f, 1.0f, 1.0f} }
                });
    }

    static void loadPhysicsObjects(PhysicsLoader &loader)
    {
        std::array<std::string, (size_t)SimObject::NumObjects - 1> asset_paths;
        asset_paths[(size_t)SimObject::Cube] =
            (std::filesystem::path(DATA_DIR) / "cube_collision.obj").string();
        asset_paths[(size_t)SimObject::Wall] =
            (std::filesystem::path(DATA_DIR) / "wall_collision.obj").string();
        asset_paths[(size_t)SimObject::Door] =
            (std::filesystem::path(DATA_DIR) / "wall_collision.obj").string();
        asset_paths[(size_t)SimObject::Agent] =
            (std::filesystem::path(DATA_DIR) / "agent_collision_simplified.obj").string();
        asset_paths[(size_t)SimObject::Button] =
            (std::filesystem::path(DATA_DIR) / "cube_collision.obj").string();

        std::array<const char *, (size_t)SimObject::NumObjects - 1> asset_cstrs;
        for (size_t i = 0; i < asset_paths.size(); i++) {
            asset_cstrs[i] = asset_paths[i].c_str();
        }

        imp::AssetImporter importer;

        char import_err_buffer[4096];
        auto imported_src_hulls = importer.importFromDisk(
                asset_cstrs, import_err_buffer, true);

        if (!imported_src_hulls.has_value()) {
            FATAL("%s", import_err_buffer);
        }

        DynArray<imp::SourceMesh> src_convex_hulls(
                imported_src_hulls->objects.size());

        DynArray<DynArray<SourceCollisionPrimitive>> prim_arrays(0);
        HeapArray<SourceCollisionObject> src_objs(
                (CountT)SimObject::NumObjects);

        auto setupHull = [&](SimObject obj_id,
                float inv_mass,
                RigidBodyFrictionData friction) {
            auto meshes = imported_src_hulls->objects[(CountT)obj_id].meshes;
            DynArray<SourceCollisionPrimitive> prims(meshes.size());

            for (const imp::SourceMesh &mesh : meshes) {
                src_convex_hulls.push_back(mesh);
                prims.push_back({
                        .type = CollisionPrimitive::Type::Hull,
                        .hullInput = {
                        .hullIDX = uint32_t(src_convex_hulls.size() - 1),
                        },
                        });
            }

            prim_arrays.emplace_back(std::move(prims));

            src_objs[(CountT)obj_id] = SourceCollisionObject {
                .prims = Span<const SourceCollisionPrimitive>(prim_arrays.back()),
                    .invMass = inv_mass,
                    .friction = friction,
            };
        };

        setupHull(SimObject::Cube, 0.075f, {
                .muS = 0.5f,
                .muD = 0.75f,
                });

        setupHull(SimObject::Wall, 0.f, {
                .muS = 0.5f,
                .muD = 0.5f,
                });

        setupHull(SimObject::Door, 0.f, {
                .muS = 0.5f,
                .muD = 0.5f,
                });

        setupHull(SimObject::Agent, 1.f, {
                .muS = 0.5f,
                .muD = 0.5f,
                });

        setupHull(SimObject::Button, 1.f, {
                .muS = 0.5f,
                .muD = 0.5f,
                });

        SourceCollisionPrimitive plane_prim {
            .type = CollisionPrimitive::Type::Plane,
                .plane = {},
        };

        src_objs[(CountT)SimObject::Plane] = {
            .prims = Span<const SourceCollisionPrimitive>(&plane_prim, 1),
            .invMass = 0.f,
            .friction = {
                .muS = 0.5f,
                .muD = 0.5f,
            },
        };

        StackAlloc tmp_alloc;
        RigidBodyAssets rigid_body_assets;
        CountT num_rigid_body_data_bytes;
        void *rigid_body_data = RigidBodyAssets::processRigidBodyAssets(
                src_convex_hulls,
                src_objs,
                false,
                tmp_alloc,
                &rigid_body_assets,
                &num_rigid_body_data_bytes);

        if (rigid_body_data == nullptr) {
            FATAL("Invalid collision hull input");
        }

        // This is a bit hacky, but in order to make sure the agents
        // remain controllable by the policy, they are only allowed to
        // rotate around the Z axis (infinite inertia in x & y axes)
        rigid_body_assets.metadatas[
            (CountT)SimObject::Agent].mass.invInertiaTensor.x = 0.f;
        rigid_body_assets.metadatas[
            (CountT)SimObject::Agent].mass.invInertiaTensor.y = 0.f;

        loader.loadRigidBodies(rigid_body_assets);
        free(rigid_body_data);
    }

    Manager::Impl * Manager::Impl::init(
            const Manager::Config &mgr_cfg)
    {
        Sim::Config sim_cfg;
        sim_cfg.autoReset = mgr_cfg.autoReset;
        sim_cfg.initRandKey = rand::initKey(mgr_cfg.randSeed);

        sim_cfg.kAray = mgr_cfg.kAray;
        sim_cfg.maxEquivalentPathNum = mgr_cfg.maxEquivalentPathNum;
        sim_cfg.ccMethod = mgr_cfg.ccMethod;

        switch (mgr_cfg.execMode) {
            case ExecMode::CUDA: {
#ifdef MADRONA_CUDA_SUPPORT
                                     CUcontext cu_ctx = MWCudaExecutor::initCUDA(mgr_cfg.gpuID);

                                     PhysicsLoader phys_loader(ExecMode::CUDA, 10);
                                     loadPhysicsObjects(phys_loader);

                                     ObjectManager *phys_obj_mgr = &phys_loader.getObjectManager();
                                     sim_cfg.rigidBodyObjMgr = phys_obj_mgr;

                                     Optional<RenderGPUState> render_gpu_state =
                                         initRenderGPUState(mgr_cfg);

                                     Optional<render::RenderManager> render_mgr =
                                         initRenderManager(mgr_cfg, render_gpu_state);

                                     if (render_mgr.has_value()) {
                                         loadRenderObjects(*render_mgr);
                                         sim_cfg.renderBridge = render_mgr->bridge();
                                     } else {
                                         sim_cfg.renderBridge = nullptr;
                                     }

                                     HeapArray<Sim::WorldInit> world_inits(mgr_cfg.numWorlds);

                                     printf("sizeof(Sim::WorldInit): %zu\n", sizeof(Sim::WorldInit));
                                     printf("sizeof(Sim::Config): %zu\n", sizeof(Sim::Config));
                                     printf("sizeof(Sim): %zu\n", sizeof(Sim));

                                     MWCudaExecutor gpu_exec({
                                             .worldInitPtr = world_inits.data(),
                                             .numWorldInitBytes = sizeof(Sim::WorldInit),
                                             .userConfigPtr = (void *)&sim_cfg,
                                             .numUserConfigBytes = sizeof(Sim::Config),
                                             // .numUserConfigBytes = (uint64_t)sizeof(Sim::Config),
                                             .numWorldDataBytes = sizeof(Sim),
                                             .worldDataAlignment = alignof(Sim),
                                             .numWorlds = mgr_cfg.numWorlds,
                                             .numTaskGraphs = 1,
                                             .numExportedBuffers = (uint32_t)ExportID::NumExports,
                                             }, {
                                             { GPU_HIDESEEK_SRC_LIST },
                                             { GPU_HIDESEEK_COMPILE_FLAGS },
                                             CompileConfig::OptMode::LTO,
                                             }, cu_ctx);

                                     WorldReset *world_reset_buffer =
                                         (WorldReset *)gpu_exec.getExported((uint32_t)ExportID::Reset);

                                     Action *agent_actions_buffer =
                                         (Action *)gpu_exec.getExported((uint32_t)ExportID::Action);

                                     return new CUDAImpl {
                                         mgr_cfg,
                                             std::move(phys_loader),
                                             world_reset_buffer,
                                             agent_actions_buffer,
                                             std::move(render_gpu_state),
                                             std::move(render_mgr),
                                             std::move(gpu_exec),
                                     };
#else
                                     FATAL("Madrona was not compiled with CUDA support");
#endif
                                 } break;
            case ExecMode::CPU: {
                                    PhysicsLoader phys_loader(ExecMode::CPU, 10);
                                    loadPhysicsObjects(phys_loader);

                                    ObjectManager *phys_obj_mgr = &phys_loader.getObjectManager();
                                    sim_cfg.rigidBodyObjMgr = phys_obj_mgr;

                                    Optional<RenderGPUState> render_gpu_state =
                                        initRenderGPUState(mgr_cfg);

                                    Optional<render::RenderManager> render_mgr =
                                        initRenderManager(mgr_cfg, render_gpu_state);

                                    if (render_mgr.has_value()) {
                                        loadRenderObjects(*render_mgr);
                                        sim_cfg.renderBridge = render_mgr->bridge();
                                    } else {
                                        sim_cfg.renderBridge = nullptr;
                                    }

                                    HeapArray<Sim::WorldInit> world_inits(mgr_cfg.numWorlds);

                                    CPUImpl::TaskGraphT cpu_exec {
                                        ThreadPoolExecutor::Config {
                                            .numWorlds = mgr_cfg.numWorlds,
                                                .numExportedBuffers = (uint32_t)ExportID::NumExports,
                                        },
                                            sim_cfg,
                                            world_inits.data(),
                                            (uint32_t)TaskGraphID::NumTaskGraphs,
                                    };

                                    WorldReset *world_reset_buffer =
                                        (WorldReset *)cpu_exec.getExported((uint32_t)ExportID::Reset);

                                    Action *agent_actions_buffer =
                                        (Action *)cpu_exec.getExported((uint32_t)ExportID::Action);

                                    auto cpu_impl = new CPUImpl {
                                        mgr_cfg,
                                            std::move(phys_loader),
                                            world_reset_buffer,
                                            agent_actions_buffer,
                                            std::move(render_gpu_state),
                                            std::move(render_mgr),
                                            std::move(cpu_exec),
                                    };

                                    return cpu_impl;
                                } break;
            default: MADRONA_UNREACHABLE();
        }
    }

    Manager::Manager(const Config &cfg)
        : impl_(Impl::init(cfg))
    {
        // Currently, there is no way to populate the initial set of observations
        // without stepping the simulations in order to execute the taskgraph.
        // Therefore, after setup, we step all the simulations with a forced reset
        // that ensures the first real step will have valid observations at the
        // start of a fresh episode in order to compute actions.
        //
        // This will be improved in the future with support for multiple task
        // graphs, allowing a small task graph to be executed after initialization.

        for (int32_t i = 0; i < (int32_t)cfg.numWorlds; i++) {
            triggerReset(i);
        }

        step();
    }

    Manager::~Manager() {}

    void Manager::step()
    {
        impl_->run();

        if (impl_->renderMgr.has_value()) {
            impl_->renderMgr->readECS();
        }

        if (impl_->cfg.enableBatchRenderer) {
            impl_->renderMgr->batchRender();
        }
    }

    Tensor Manager::resetTensor() const
    {
        return impl_->exportTensor(ExportID::Reset,
                TensorElementType::Int32,
                {
                impl_->cfg.numWorlds,
                1,
                });
    }

    Tensor Manager::actionTensor() const
    {
        return impl_->exportTensor(ExportID::Action, TensorElementType::Int32,
                {
                impl_->cfg.numWorlds,
                consts::numAgents,
                4,
                });
    }

    Tensor Manager::rewardTensor() const
    {
        return impl_->exportTensor(ExportID::Reward, TensorElementType::Float32,
                {
                impl_->cfg.numWorlds,
                consts::numAgents,
                1,
                });
    }

    Tensor Manager::doneTensor() const
    {
        return impl_->exportTensor(ExportID::Done, TensorElementType::Int32,
                {
                impl_->cfg.numWorlds,
                consts::numAgents,
                1,
                });
    }

    Tensor Manager::selfObservationTensor() const
    {
        return impl_->exportTensor(ExportID::SelfObservation,
                TensorElementType::Float32,
                {
                impl_->cfg.numWorlds,
                consts::numAgents,
                8,
                });
    }

    Tensor Manager::partnerObservationsTensor() const
    {
        return impl_->exportTensor(ExportID::PartnerObservations,
                TensorElementType::Float32,
                {
                impl_->cfg.numWorlds,
                consts::numAgents,
                consts::numAgents - 1,
                3,
                });
    }

    Tensor Manager::roomEntityObservationsTensor() const
    {
        return impl_->exportTensor(ExportID::RoomEntityObservations,
                TensorElementType::Float32,
                {
                impl_->cfg.numWorlds,
                consts::numAgents,
                consts::maxEntitiesPerRoom,
                3,
                });
    }

    Tensor Manager::doorObservationTensor() const
    {
        return impl_->exportTensor(ExportID::DoorObservation,
                TensorElementType::Float32,
                {
                impl_->cfg.numWorlds,
                consts::numAgents,
                3,
                });
    }

    Tensor Manager::lidarTensor() const
    {
        return impl_->exportTensor(ExportID::Lidar, TensorElementType::Float32,
                {
                impl_->cfg.numWorlds,
                consts::numAgents,
                consts::numLidarSamples,
                2,
                });
    }

    Tensor Manager::stepsRemainingTensor() const
    {
        return impl_->exportTensor(ExportID::StepsRemaining,
                TensorElementType::Int32,
                {
                impl_->cfg.numWorlds,
                consts::numAgents,
                1,
                });
    }

    Tensor Manager::rgbTensor() const
    {
        const uint8_t *rgb_ptr = impl_->renderMgr->batchRendererRGBOut();

        return Tensor((void*)rgb_ptr, TensorElementType::UInt8, {
                impl_->cfg.numWorlds,
                consts::numAgents,
                impl_->cfg.batchRenderViewHeight,
                impl_->cfg.batchRenderViewWidth,
                4,
                }, impl_->cfg.gpuID);
    }

    Tensor Manager::depthTensor() const
    {
        const float *depth_ptr = impl_->renderMgr->batchRendererDepthOut();

        return Tensor((void *)depth_ptr, TensorElementType::Float32, {
                impl_->cfg.numWorlds,
                consts::numAgents,
                impl_->cfg.batchRenderViewHeight,
                impl_->cfg.batchRenderViewWidth,
                1,
                }, impl_->cfg.gpuID);
    }

    void Manager::triggerReset(int32_t world_idx)
    {
        WorldReset reset {
            1,
        };

        auto *reset_ptr = impl_->worldResetBuffer + world_idx;

        if (impl_->cfg.execMode == ExecMode::CUDA) {
#ifdef MADRONA_CUDA_SUPPORT
            cudaMemcpy(reset_ptr, &reset, sizeof(WorldReset),
                    cudaMemcpyHostToDevice);
#endif
        }  else {
            *reset_ptr = reset;
        }
    }

    void Manager::setAction(int32_t world_idx,
            int32_t agent_idx,
            int32_t move_amount,
            int32_t move_angle,
            int32_t rotate,
            int32_t grab)
    {
        Action action {
            .moveAmount = move_amount,
            .moveAngle = move_angle,
            .rotate = rotate,
            .grab = grab,
        };

        auto *action_ptr = impl_->agentActionsBuffer +
            world_idx * consts::numAgents + agent_idx;

        if (impl_->cfg.execMode == ExecMode::CUDA) {
#ifdef MADRONA_CUDA_SUPPORT
            cudaMemcpy(action_ptr, &action, sizeof(Action),
                    cudaMemcpyHostToDevice);
#endif
        } else {
            *action_ptr = action;
        }
    }

    render::RenderManager & Manager::getRenderManager()
    {
        return *impl_->renderMgr;
    }


        // ============================================================================
    // Topology Parsing Helper Functions (C++ implementation of Python parse_topo)
    // ============================================================================

    namespace {
        
        // ============================================================================
        // Topology Parsing Helper Functions
        // ============================================================================
        
        // Helper function to parse topology file with port mappings
        static bool parse_topology_with_ports(
            const std::string& file_path,
            NetworkGraph& graph,
            PortMapping& port_mapping,
            PortMapping& separate_port_mapping,
            std::vector<int>& switch_ids,
            std::vector<std::array<int, 5>>& aj_link,
            int& num_nodes,
            int& sw_num
        ) {
            std::ifstream file(file_path);
            if (!file.is_open()) {
                std::cerr << "Failed to open topology file: " << file_path << std::endl;
                return false;
            }

            std::string line;
            std::vector<std::string> lines;
            
            // Read all lines
            while (std::getline(file, line)) {
                lines.push_back(line);
            }
            file.close();

            if (lines.empty()) {
                std::cerr << "Empty topology file" << std::endl;
                return false;
            }

            // Parse first line: num_nodes sw_id1 sw_id2 sw_num num_links device_type num_gpus
            std::istringstream first_line(lines[0]);
            int sw_id1, sw_id2, num_links;
            std::string device_type;
            int num_gpus;
            first_line >> num_nodes >> sw_id1 >> sw_id2 >> sw_num >> num_links >> device_type >> num_gpus;

            // Parse second line: switch IDs
            std::istringstream second_line(lines[1]);
            int switch_id;
            while (second_line >> switch_id) {
                switch_ids.push_back(switch_id);
            }

            // Initialize graph
            int num_npus = num_nodes - sw_num;
            graph = NetworkGraph(num_nodes, num_npus);

            // Use set to avoid duplicate links
            std::set<std::pair<int, int>> link_set;

            // Parse link lines
            for (size_t i = 2; i < lines.size(); ++i) {
                std::istringstream line_stream(lines[i]);
                int u, v, queue_size;
                std::string bw_str, lat_str, loss_str, link_type;
                
                line_stream >> u >> v >> bw_str >> lat_str >> loss_str >> queue_size >> link_type;

                // Only process ETH or IB links
                // if (link_type != "ETH" && link_type != "IB") {
                //     continue;
                // }

                // Parse bandwidth (remove "Gbps")
                int bw = std::stoi(bw_str.substr(0, bw_str.find("Gbps")));
                
                // Parse latency (remove "ns")
                int lat = std::stoi(lat_str.substr(0, lat_str.find("ns")));
                
                // Weight is set to queue_size (matching Python logic which uses w in line)
                int w = queue_size;

                // Add link to graph
                graph.add_link(u, v, w, bw, lat);

                // Add to aj_link array (bidirectional)
                if (link_set.find({u, v}) == link_set.end()) {
                    aj_link.push_back({u, v, w, bw, lat});
                    link_set.insert({u, v});
                }
                if (link_set.find({v, u}) == link_set.end()) {
                    aj_link.push_back({v, u, w, bw, lat});
                    link_set.insert({v, u});
                }
            }

            // Sort aj_link by (u, v)
            std::sort(aj_link.begin(), aj_link.end(), 
                [](const std::array<int, 5>& a, const std::array<int, 5>& b) {
                    if (a[0] != b[0]) return a[0] < b[0];
                    return a[1] < b[1];
                });

            // Generate port_mapping
            int global_port_counter = 0;
            for (int node = 0; node < num_nodes; ++node) {
                const auto& neighbors = graph.get_neighbors(node);
                
                // Sort neighbors by node ID
                std::vector<std::tuple<int, int, int>> sorted_neighbors(neighbors.begin(), neighbors.end());
                std::sort(sorted_neighbors.begin(), sorted_neighbors.end(),
                    [](const auto& a, const auto& b) {
                        return std::get<0>(a) < std::get<0>(b);
                    });

                // Generate local and global port mappings
                for (int idx = 0; idx < static_cast<int>(sorted_neighbors.size()); ++idx) {
                    int neighbor = std::get<0>(sorted_neighbors[idx]);
                    port_mapping.local_ports[node][neighbor] = idx;
                    port_mapping.global_ports[node][neighbor] = global_port_counter;
                    global_port_counter++;
                }

                // Reset global_port_counter after last net_npu node
                if (node == (num_nodes - sw_num - 1)) {
                    global_port_counter = 0;
                }
            }

            // Generate separate_port_mapping (deep copy first)
            separate_port_mapping = port_mapping;

            // Find max global port among net_npu nodes
            int max_net_npu_port = -1;
            for (int node = 0; node < num_nodes; ++node) {
                if (std::find(switch_ids.begin(), switch_ids.end(), node) == switch_ids.end()) {
                    // This is a net_npu node
                    for (const auto& entry : port_mapping.global_ports[node]) {
                        max_net_npu_port = std::max(max_net_npu_port, entry.second);
                    }
                }
            }

            // Reassign global ports for switches in separate_port_mapping
            int switch_global_port_counter = max_net_npu_port + 1;
            for (int node = 0; node < num_nodes; ++node) {
                if (std::find(switch_ids.begin(), switch_ids.end(), node) != switch_ids.end()) {
                    // This is a switch node
                    const auto& neighbors = graph.get_neighbors(node);
                    std::vector<std::tuple<int, int, int>> sorted_neighbors(neighbors.begin(), neighbors.end());
                    std::sort(sorted_neighbors.begin(), sorted_neighbors.end(),
                        [](const auto& a, const auto& b) {
                            return std::get<0>(a) < std::get<0>(b);
                        });
                    
                    for (const auto& neighbor_tuple : sorted_neighbors) {
                        int neighbor = std::get<0>(neighbor_tuple);
                        separate_port_mapping.global_ports[node][neighbor] = switch_global_port_counter;
                        switch_global_port_counter++;
                    }
                }
            }
            
            return true;

        }

        // Generate node port counts
        static std::vector<int> generate_node_port_counts(const PortMapping& port_mapping) {
            std::vector<int> port_counts;
            
            // Iterate through sorted node IDs
            std::vector<int> node_ids;
            for (const auto& entry : port_mapping.local_ports) {
                node_ids.push_back(entry.first);
            }
            std::sort(node_ids.begin(), node_ids.end());
            
            for (int node : node_ids) {
                port_counts.push_back(port_mapping.local_ports.at(node).size());
            }
            
            return port_counts;
        }

        // Generate port connection lists
        static void generate_port_connection_lists(
            const PortMapping& port_mapping,
            const NetworkGraph& /*graph*/,
            const std::vector<int>& switch_ids,
            bool use_separate,
            const PortMapping* separate_port_mapping,
            std::vector<int>& switch_next_hop_port,
            std::vector<int>& net_npu_next_hop_port,
            int& switch_port_num,
            int& net_npu_port_num
        ) {
            // Calculate max_global_port
            int max_global_port = 0;
            
            if (use_separate && separate_port_mapping != nullptr) {
                for (const auto& node_entry : port_mapping.global_ports) {
                    int node = node_entry.first;
                    bool is_switch = std::find(switch_ids.begin(), switch_ids.end(), node) != switch_ids.end();
                    
                    if (is_switch) {
                        for (const auto& port_entry : separate_port_mapping->global_ports.at(node)) {
                            max_global_port = std::max(max_global_port, port_entry.second);
                        }
                    } else {
                        for (const auto& port_entry : node_entry.second) {
                            max_global_port = std::max(max_global_port, port_entry.second);
                        }
                    }
                }
            } else {
                for (const auto& node_entry : port_mapping.global_ports) {
                    for (const auto& port_entry : node_entry.second) {
                        max_global_port = std::max(max_global_port, port_entry.second);
                    }
                }
            }

            // Initialize arrays with -1
            switch_next_hop_port.resize(max_global_port + 1, -1);
            net_npu_next_hop_port.resize(max_global_port + 1, -1);

            // Fill port connection information
            for (const auto& node_entry : port_mapping.local_ports) {
                int node = node_entry.first;
                bool is_switch = std::find(switch_ids.begin(), switch_ids.end(), node) != switch_ids.end();
                
                for (const auto& neighbor_entry : node_entry.second) {
                    int neighbor = neighbor_entry.first;
                    
                    // Get global port for current node
                    int global_port;
                    int neighbor_global_port;
                    bool neighbor_is_switch = std::find(switch_ids.begin(), switch_ids.end(), neighbor) != switch_ids.end();

                    if (use_separate && separate_port_mapping != nullptr && is_switch) {
                        global_port = separate_port_mapping->global_ports.at(node).at(neighbor);
                        if (neighbor_is_switch) {
                            neighbor_global_port = separate_port_mapping->global_ports.at(neighbor).at(node);
                        } else {
                            neighbor_global_port = port_mapping.global_ports.at(neighbor).at(node);
                        }
                    } else {
                        global_port = port_mapping.global_ports.at(node).at(neighbor);
                        neighbor_global_port = port_mapping.global_ports.at(neighbor).at(node);
                    }
                    
                    // Fill appropriate array
                    if (is_switch) {
                        switch_next_hop_port[global_port] = neighbor_global_port;
                    } else {
                        net_npu_next_hop_port[global_port] = neighbor_global_port;
                    }
                }
            }

            // Count valid ports
            net_npu_port_num = 0;
            for (int port : net_npu_next_hop_port) {
                if (port >= 0) net_npu_port_num++;
            }
            
            switch_port_num = 0;
            for (int port : switch_next_hop_port) {
                if (port >= 0) switch_port_num++;
            }
        }
    }  // anonymous namespace

    void Manager::dijkstraECMP(const NetworkGraph& graph, int src, NextHopsMap& out_next_hops)
    {
        const int INF = std::numeric_limits<int>::max();
        int num_nodes = graph.num_nodes;
        
        std::vector<int> dist(num_nodes, INF);
        std::vector<std::set<int>> prev_hops(num_nodes);
        dist[src] = 0;
        
        // Priority queue: (distance, node)
        using PII = std::pair<int, int>;
        std::priority_queue<PII, std::vector<PII>, std::greater<PII>> heap;
        heap.push({0, src});
        
        while (!heap.empty()) {
            auto [current_dist, u] = heap.top();
            heap.pop();
            
            if (current_dist > dist[u]) continue;
            
            // Iterate through neighbors
            const auto& neighbors = graph.get_neighbors(u);
            for (const auto& neighbor_tuple : neighbors) {
                int v = std::get<0>(neighbor_tuple);
                int weight = std::get<1>(neighbor_tuple);
                // int bw = std::get<2>(neighbor_tuple); // Unused
                
                int path_cost = dist[u] + weight;
                if (path_cost < dist[v]) {
                    dist[v] = path_cost;
                    prev_hops[v] = {u};
                    heap.push({dist[v], v});
                } else if (path_cost == dist[v]) {
                    prev_hops[v].insert(u);
                }
            }
        }
        
        // Recursively find next hops (with memoization)
        out_next_hops.clear();
        std::function<std::set<int>(int)> find_next_hop = [&](int dst) -> std::set<int> {
            if (out_next_hops.count(dst)) return out_next_hops[dst];
            
            if (prev_hops[dst].count(src)) {
                out_next_hops[dst] = {dst};
            } else {
                out_next_hops[dst] = {};
                for (int prev : prev_hops[dst]) {
                    auto prev_next = find_next_hop(prev);
                    out_next_hops[dst].insert(prev_next.begin(), prev_next.end());
                }
            }
            return out_next_hops[dst];
        };
        
        // Build next hops for all reachable destinations
        for (int dst = 0; dst < num_nodes; ++dst) {
            if (dst != src && dist[dst] != INF) {
                find_next_hop(dst);
            }
        }
    }

    void Manager::computeRoutingTableForSource(
        int src, const NetworkGraph& graph, const PortMapping& port_mapping,
        RoutingTable& out_routing_table)
    {
        out_routing_table.clear();
        out_routing_table.reserve(graph.num_nodes);
        
        // Check if source node has port mapping
        auto it = port_mapping.local_ports.find(src);
        if (it == port_mapping.local_ports.end()) {
            // Return empty routing table
            for (int dst = 0; dst < graph.num_nodes; ++dst) {
                out_routing_table.push_back({dst, {}});
            }
            return;
        }
        
        // Compute next hops
        NextHopsMap next_hops;
        dijkstraECMP(graph, src, next_hops);
        
        // Convert to port numbers
        for (int dst = 0; dst < graph.num_nodes; ++dst) {
            std::vector<int> hops;
            if (next_hops.count(dst)) {
                for (int nhop : next_hops[dst]) {
                    auto port_it = it->second.find(nhop);
                    if (port_it != it->second.end()) {
                        hops.push_back(port_it->second);
                    }
                }
                std::sort(hops.begin(), hops.end());
            }
            out_routing_table.push_back({dst, hops});
        }
    }

    void Manager::buildGlobalRoutingTableParallel(
        const NetworkGraph& graph, const PortMapping& port_mapping, int num_threads)
    {
        int num_nodes = graph.num_nodes;
        
        // Auto-determine thread count
        if (num_threads == 0) {
            num_threads = std::thread::hardware_concurrency();
            if (num_threads == 0) num_threads = 4; // Default value
        }
        
       
        
        // Initialize global routing table (operate directly on member variable)
        impl_->fibBuildData.global_routing_table.clear();
        impl_->fibBuildData.global_routing_table.resize(num_nodes);


        // Allocate and initialize fibDataBuffer before parallel computation
        impl_->fibDataBuffer.clear();
        impl_->fibDataBuffer.resize(FIB_TENSOR_SIZE);  // 不初始化
        std::memset(impl_->fibDataBuffer.data(), 255, FIB_TENSOR_SIZE); 
        
        {
            // Parallel version
            std::vector<std::thread> threads;
            std::atomic<int> completed(0);
            std::mutex progress_mutex;
            
            auto worker = [&](int start, int end) {
                for (int src = start; src < end; ++src) {
                    computeRoutingTableForSource(src, graph, port_mapping,
                                                 impl_->fibBuildData.global_routing_table[src]);

                    int max_nodes = MAX_SW_NUM + MAX_NET_NPU_NUM;
                    // Step 2: Fill fibDataBuffer (routing_array part)
                    size_t routing_base_offset =
                        (size_t)src * max_nodes * MAX_ONE_SW_PORT_NUM;
                    for (int dst = 0; dst < num_nodes && dst < max_nodes;
                         ++dst) {
                      const auto &hops =
                          impl_->fibBuildData.global_routing_table[src][dst]
                              .second;
                      int copy_len =
                          std::min((int)hops.size(), MAX_ONE_SW_PORT_NUM);
                      size_t dst_offset =
                          routing_base_offset + dst * MAX_ONE_SW_PORT_NUM;
                      for (int k = 0; k < copy_len; ++k) {
                        impl_->fibDataBuffer[dst_offset + k] =
                            static_cast<uint8_t>(hops[k]);
                      }
                    }

                    // Step 3: Calculate and fill next_hop_num in fibDataBuffer
                    size_t fib_size =
                        (size_t)max_nodes * max_nodes * MAX_ONE_SW_PORT_NUM;
                    size_t next_hop_base_offset =
                        fib_size + (size_t)src * max_nodes;
                    for (int dst = 0; dst < max_nodes; ++dst) {
                      uint8_t count = 0;
                      size_t dst_offset =
                          routing_base_offset + dst * MAX_ONE_SW_PORT_NUM;
                      for (int k = 0; k < MAX_ONE_SW_PORT_NUM; ++k) {
                        uint8_t hop = impl_->fibDataBuffer[dst_offset + k];
                        if (hop >= 0 && hop < 200) {
                          count++;
                        }
                      }
                      impl_->fibDataBuffer[next_hop_base_offset + dst] = count;
                    }

                    int current = ++completed;
                    if (current % std::max(1, num_nodes / 10) == 0) {
                        std::lock_guard<std::mutex> lock(progress_mutex);
                        std::cout << "Progress: " << current << "/" << num_nodes 
                                  << " (" << (100 * current / num_nodes) << "%)" << std::endl;
                    }
                }
            };
            
            // Assign tasks to threads
            int chunk_size = (num_nodes + num_threads - 1) / num_threads;
            for (int i = 0; i < num_threads; ++i) {
                int start = i * chunk_size;
                int end = std::min(start + chunk_size, num_nodes);
                if (start < num_nodes) {
                    threads.emplace_back(worker, start, end);
                }
            }
            
            // Wait for all threads to complete
            for (auto& t : threads) {
                if (t.joinable()) t.join();
            }
        }

        
        // Debug: Print next_hop_num array
        // {
        //     int max_nodes = MAX_SW_NUM + MAX_NET_NPU_NUM;
        //     size_t fib_size = (size_t)max_nodes * max_nodes * MAX_ONE_SW_PORT_NUM;
            
        //     std::cout << "\n=== next_hop_num Debug Info ===" << std::endl;
        //     std::cout << "num_nodes=" << num_nodes << ", max_nodes=" << max_nodes << std::endl;
            
        //     // Print a sample of next_hop_num values
        //     int print_limit = std::min(num_nodes, 20);  // Print first 20 rows
        //     for (int src = 0; src < num_nodes; ++src) {
        //         std::cout << "next_hop_num[" << src << "][0.." << std::min(print_limit, num_nodes) - 1 << "]: ";
        //         for (int dst = 0; dst < num_nodes; ++dst) {
        //             size_t next_hop_offset = fib_size + (size_t)src * max_nodes + dst;
        //             std::cout << (int)impl_->fibDataBuffer[next_hop_offset] << " ";
        //         }
        //         std::cout << std::endl;
        //     }
            
        //     // Check for any 0 or 255 values that might cause issues
        //     int zero_count = 0, invalid_count = 0;
        //     for (int src = 0; src < num_nodes; ++src) {
        //         for (int dst = 0; dst < num_nodes; ++dst) {
        //             size_t next_hop_offset = fib_size + (size_t)src * max_nodes + dst;
        //             uint8_t val = impl_->fibDataBuffer[next_hop_offset];
        //             if (val == 0) zero_count++;
        //             if (val == 255) invalid_count++;
        //         }
        //     }
        //     std::cout << "Total entries checked: " << num_nodes * num_nodes << std::endl;
        //     std::cout << "Entries with value 0: " << zero_count << std::endl;
        //     std::cout << "Entries with value 255 (invalid): " << invalid_count << std::endl;
        //     std::cout << "=== End next_hop_num Debug ===" << std::endl << std::endl;
        // }

    }

    void Manager::buildGlobalRoutingTableWithArray(
        const NetworkGraph& graph, const PortMapping& port_mapping)
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        // 1. Build global routing table (parallel), result stored in impl_->fibBuildData.global_routing_table
        buildGlobalRoutingTableParallel(graph, port_mapping);
        
        auto mid1 = std::chrono::high_resolution_clock::now();
        std::cout << "Time for calculating routing table: " 
                  << std::chrono::duration<double>(mid1 - start).count() << " s" << std::endl;
        
        /*
        // 2. Calculate max next hops (for debugging)
        int max_next_hops = 0;
        for (const auto& table : impl_->fibBuildData.global_routing_table) {
            for (const auto& [dst, hops] : table) {
                max_next_hops = std::max(max_next_hops, (int)hops.size());
            }
        }
        
        int num_nodes = graph.num_nodes;
        int max_nodes = MAX_SW_NUM + MAX_NET_NPU_NUM;
        
        // 3. Initialize routing array (255 means invalid), operate directly on member variable
        impl_->fibBuildData.routing_array.clear();
        impl_->fibBuildData.routing_array.resize(max_nodes, 
            std::vector<std::vector<uint8_t>>(max_nodes, 
                std::vector<uint8_t>(MAX_ONE_SW_PORT_NUM, 255)));
        
        // 4. Fill routing data
        for (int src = 0; src < num_nodes && src < max_nodes; ++src) {
            for (int dst = 0; dst < num_nodes && dst < max_nodes; ++dst) {
                const auto& hops = impl_->fibBuildData.global_routing_table[src][dst].second;
                int copy_len = std::min((int)hops.size(), MAX_ONE_SW_PORT_NUM);
                for (int k = 0; k < copy_len; ++k) {
                    impl_->fibBuildData.routing_array[src][dst][k] = static_cast<uint8_t>(hops[k]);
                }
            }
        }
        
        auto mid2 = std::chrono::high_resolution_clock::now();
        std::cout << "Time for converting to routing table array: " 
                  << std::chrono::duration<double>(mid2 - mid1).count() << " s" << std::endl;
        
        // 5. Calculate next_hop_num, operate directly on member variable
        impl_->fibBuildData.next_hop_num.clear();
        impl_->fibBuildData.next_hop_num.resize(max_nodes, std::vector<uint8_t>(max_nodes, 0));
        
        for (int src = 0; src < max_nodes; ++src) {
            for (int dst = 0; dst < max_nodes; ++dst) {
                uint8_t count = 0;
                for (int k = 0; k < MAX_ONE_SW_PORT_NUM; ++k) {
                    uint8_t hop = impl_->fibBuildData.routing_array[src][dst][k];
                    if (hop >= 0 && hop < 200) {
                        count++;
                    }
                }
                impl_->fibBuildData.next_hop_num[src][dst] = count;
            }
        }
        
        auto mid3 = std::chrono::high_resolution_clock::now();
        std::cout << "Time for calculating next_hop_num: " 
                  << std::chrono::duration<double>(mid3 - mid2).count() << " s" << std::endl;
        */
        
        std::cout << "FIB table built successfully" << std::endl;
        
        convertFibDataToFibTensor(); // convert to tensor
    }

    void Manager::buildFIBTable()
    {
        std::cout << "Building FIB table..." << std::endl;
        
        if (impl_->topoData.graph.num_nodes == 0) {
            std::cerr << "Error: Topology not parsed, cannot build FIB table" << std::endl;
            return;
        }
        
        buildGlobalRoutingTableWithArray(impl_->topoData.graph, impl_->topoData.port_mapping);
        
        std::cout << "FIB table construction completed" << std::endl;
    }

    void Manager::parseTopologyFile(std::string topology_file)
    {
        // 1. Call parse_topology_with_ports
        NetworkGraph graph;
        PortMapping port_mapping, separate_port_mapping;
        std::vector<int> switch_ids;
        std::vector<std::array<int, 5>> aj_link;
        int num_nodes = 0, sw_num = 0;
        
        bool success = parse_topology_with_ports(topology_file, graph, port_mapping, 
                                 separate_port_mapping, switch_ids, 
                                 aj_link, num_nodes, sw_num);
        
        if (!success) {
            std::cerr << "Error: Failed to parse topology file: " << topology_file << std::endl;
            return;
        }
        
        // 2. Call generate_node_port_counts
        std::vector<int> port_num = generate_node_port_counts(port_mapping);
        
        // 3. Call generate_port_connection_lists
        std::vector<int> switch_next_hop_port, net_npu_next_hop_port;
        int switch_port_num = 0, net_npu_port_num = 0;
        
        generate_port_connection_lists(port_mapping, graph, switch_ids, 
                                      true, &separate_port_mapping,
                                      switch_next_hop_port, net_npu_next_hop_port,
                                      switch_port_num, net_npu_port_num);
        
        // 4. Save results to impl_->topoData
        impl_->topoData.aj_link = std::move(aj_link);
        impl_->topoData.port_num = std::move(port_num);
        impl_->topoData.switch_next_hop_port = std::move(switch_next_hop_port);
        impl_->topoData.net_npu_next_hop_port = std::move(net_npu_next_hop_port);

        /*
        // print net_npu_next_hop_port
        std::cout << "net_npu_next_hop_port: ";
        for (int i = 0; i < impl_->topoData.net_npu_next_hop_port.size(); i++) {
            std::cout << impl_->topoData.net_npu_next_hop_port[i] << " ";
        }
        std::cout << std::endl;
        // print switch_next_hop_port
        std::cout << "switch_next_hop_port: ";
        for (int i = 0; i < impl_->topoData.switch_next_hop_port.size(); i++) {
            std::cout << impl_->topoData.switch_next_hop_port[i] << " ";
        }
        std::cout << std::endl;
        
        */

        impl_->topoData.link_num = impl_->topoData.aj_link.size();
        impl_->topoData.net_npu_num = num_nodes - sw_num;
        impl_->topoData.sw_num = sw_num;
        impl_->topoData.switch_port_num = switch_port_num;
        impl_->topoData.net_npu_port_num = net_npu_port_num;
        
        impl_->topoData.graph = std::move(graph);
        impl_->topoData.port_mapping = std::move(port_mapping);
        impl_->topoData.separate_port_mapping = std::move(separate_port_mapping);
        
        std::cout << "Topology file parsed successfully:" << std::endl;
        std::cout << "  - num_nodes: " << num_nodes << std::endl;
        std::cout << "  - net_npu_num: " << impl_->topoData.net_npu_num << std::endl;
        std::cout << "  - sw_num: " << impl_->topoData.sw_num << std::endl;
        std::cout << "  - link_num: " << impl_->topoData.link_num << std::endl;
        std::cout << "  - net_npu_port_num: " << impl_->topoData.net_npu_port_num << std::endl;
        std::cout << "  - switch_port_num: " << impl_->topoData.switch_port_num << std::endl;

        convertTopoDataToTopoTensor();
        
        // Build FIB table after topology is parsed
        buildFIBTable();
    }


    void Manager::convertTopoDataToTopoTensor()
    {
        // 1. Allocate buffer with TOPO_TENSOR_SIZE, initialize to 0
        size_t buffer_size = TOPO_TENSOR_SIZE;
        impl_->topoDataBuffer.resize(buffer_size, 0);
        
        size_t offset = 0;
        
        // 2. Fill scalar values (6 values total)
        impl_->topoDataBuffer[offset++] = impl_->topoData.link_num;
        impl_->topoDataBuffer[offset++] = impl_->topoData.net_npu_num;
        impl_->topoDataBuffer[offset++] = impl_->topoData.sw_num;
        impl_->topoDataBuffer[offset++] = impl_->topoData.switch_port_num;
        impl_->topoDataBuffer[offset++] = impl_->topoData.net_npu_port_num;
        impl_->topoDataBuffer[offset++] = 0;  // Reserved field for alignment
        
        // 3. Fill aj_link array (MAX_LINKS_NUM * 5 elements)
        // First copy actual data
        offset = 6;
        for (const auto& link : impl_->topoData.aj_link) {
            for (int field : link) {
                impl_->topoDataBuffer[offset++] = field;
            }
        }
        
        // 4. Fill port_num array (MAX_NODES elements)
        // First copy actual data
        offset = 6+(MAX_LINKS_NUM * 5);
        memcpy(impl_->topoDataBuffer.data() + offset, impl_->topoData.port_num.data(), impl_->topoData.port_num.size() * sizeof(int32_t));
        //for (size_t i = 0; i < impl_->topoData.port_num.size(); i++) {
        //    impl_->topoDataBuffer[offset++] = impl_->topoData.port_num[i];
        //}
        
        // 5. Fill next_hop_port array (MAX_ALL_PORT_NUM elements)
        // Combine net_npu_next_hop_port and switch_next_hop_port
        // According to Python code: net_npu_next_hop_port[0:net_npu_port_num] + switch_next_hop_port[net_npu_port_num:net_npu_port_num+sw_port_num]
        
        offset = 6+(MAX_LINKS_NUM * 5)+(MAX_SW_NUM + MAX_NET_NPU_NUM);
        memcpy(impl_->topoDataBuffer.data() + offset, impl_->topoData.net_npu_next_hop_port.data(), impl_->topoData.net_npu_port_num * sizeof(int32_t));
        offset += impl_->topoData.net_npu_port_num; 
        // Copy net_npu_next_hop_port
        //for (size_t i = 0; i < impl_->topoData.net_npu_port_num; i++) {
        //    impl_->topoDataBuffer[offset++] = impl_->topoData.net_npu_next_hop_port[i];
        //}
        
        // Copy switch_next_hop_port
        //int start_idx = impl_->topoData.net_npu_port_num;
        //int end_idx = start_idx + impl_->topoData.switch_port_num;
        memcpy(impl_->topoDataBuffer.data() + offset, impl_->topoData.switch_next_hop_port.data() + impl_->topoData.net_npu_port_num, impl_->topoData.switch_port_num * sizeof(int32_t));
        //for (int i = start_idx; i < end_idx; i++) {
        //    impl_->topoDataBuffer[offset++] = impl_->topoData.switch_next_hop_port[i];
        //}
        


        // 6. Create CPU Tensor and store to impl_->topoTensor
        //int64_t dims[1] = {static_cast<int64_t>(buffer_size)};
        //impl_->topoTensor = Tensor(
        //    impl_->topoDataBuffer.data(),
        //    TensorElementType::Int32,
        //    Span<const int64_t>(dims, 1),
        //    Optional<int>::none()  // CPU tensor
        //);

        std::cout << "TopoData converted to tensor successfully:" << std::endl;
        std::cout << "  - Buffer size: " << buffer_size << " elements" << std::endl;
        std::cout << "  - Data filled: " << offset << " elements" << std::endl;
        std::cout << "  - Tensor type: Int32, Device: CPU" << std::endl;

        Tensor ecs_topo_tensor = topoTensor();
        // note: we should copy based on the world index
        if (impl_->cfg.execMode == ExecMode::CUDA) {
            #ifdef MADRONA_CUDA_SUPPORT
                        cudaMemcpy(ecs_topo_tensor.devicePtr(), impl_->topoDataBuffer.data(), buffer_size * sizeof(int32_t),
                                cudaMemcpyHostToDevice);
            #endif
        }  else {
            /*
           // 方案1: 直接写入ECS组件，不通过导出缓冲区
          for (uint32_t world_idx = 0; world_idx < impl_->cfg.numWorlds; world_idx++) {
            // 获取该 world 的 Context
            auto &ctx = static_cast<CPUImpl*>(impl_.get())->cpuExec.getWorldContext(world_idx);
            
            // 获取 agents[0] entity（在 level_gen.cpp:181 中设置）
            Entity tensor_entity = ctx.data().agents[0];
            
            // 直接获取并修改 TopoTensor 组件
            TopoTensor &topo_tensor = ctx.get<TopoTensor>(tensor_entity);
            
            
            // 拷贝数据到 ECS 内部存储
            memcpy(topo_tensor.data, impl_->topoDataBuffer.data(),
                   buffer_size * sizeof(int32_t));
            
            std::cout << "TopoTensor data written directly to ECS for world " 
                      << world_idx << std::endl;
            */
            memcpy(ecs_topo_tensor.devicePtr(), impl_->topoDataBuffer.data(), buffer_size * sizeof(int32_t));
            
        }
        std::cout << "TopoTensor copied to ECS successfully" << std::endl;
    }

    void Manager::convertFibDataToFibTensor()
    {
         // 6. Convert FIB data to flat buffer (matching Python fib_to_tensor logic)
         std::cout << "Converting FIB data to flat buffer..." << std::endl;
         auto flatten_start = std::chrono::high_resolution_clock::now();
         
         
         auto flatten_end = std::chrono::high_resolution_clock::now();
         std::cout << "Time for flattening FIB data: " 
                   << std::chrono::duration<double>(flatten_end - flatten_start).count() << " s" << std::endl;
         

        auto start = std::chrono::high_resolution_clock::now();
        
        // 1. Check if fibDataBuffer is populated
        if (impl_->fibDataBuffer.empty()) {
            std::cerr << "Error: fibDataBuffer is empty. Please call buildFIBTable() first." << std::endl;
            return;
        }
        
        size_t buffer_size = impl_->fibDataBuffer.size();
        
        std::cout << "Converting FIB data buffer to FibTensor..." << std::endl;
        std::cout << "  - Buffer size: " << buffer_size << " bytes (" 
                  << (buffer_size / 1024.0 / 1024.0) << " MB)" << std::endl;
        
        Tensor ecs_fib_tensor = fibTensor();
        // 2. Copy data to GPU FibTensor (through ECS export system)
        if (impl_->cfg.execMode == ExecMode::CUDA) {
            
            #ifdef MADRONA_CUDA_SUPPORT
            // Copy to all worlds
            for (uint32_t world_idx = 0; world_idx < impl_->cfg.numWorlds; ++world_idx) {
                // Calculate offset for this world
                uint8_t* world_ptr = static_cast<uint8_t*>(ecs_fib_tensor.devicePtr()) + 
                                     world_idx * FIB_TENSOR_SIZE;
                
                cudaMemcpy(world_ptr, impl_->fibDataBuffer.data(), buffer_size,
                          cudaMemcpyHostToDevice);
            }
            
            // Synchronize to ensure copy is complete
            cudaDeviceSynchronize();
            #endif
            
            auto end = std::chrono::high_resolution_clock::now();
            std::cout << "  - FibTensor copied to GPU successfully for " 
                     << impl_->cfg.numWorlds << " worlds" << std::endl;
            std::cout << "  - Time cost: " 
                     << std::chrono::duration<double>(end - start).count() << " s" << std::endl;
            
            // Debug: Print first 10 elements to verify
            std::cout << "  - First 10 elements of FIB buffer (for verification): ";
            for (int i = 0; i < 10 && i < (int)impl_->fibDataBuffer.size(); ++i) {
                std::cout << (int)impl_->fibDataBuffer[i] << " ";
            }
            std::cout << std::endl;
            
        } else {
            memcpy(ecs_fib_tensor.devicePtr(), impl_->fibDataBuffer.data(), buffer_size);
            impl_->run();
        }
    }

    Tensor Manager::getCppTopoTensor() const
    {
        return Tensor(
            impl_->topoDataBuffer.data(),
            TensorElementType::Int32,
            Span<const int64_t>({TOPO_TENSOR_SIZE}),
            Optional<int>::none() // CPU tensor
        );
    }
    Tensor Manager::getCppFibTensor() const
    {
        return Tensor(
            impl_->fibDataBuffer.data(),
            TensorElementType::UInt8,
            Span<const int64_t>({FIB_TENSOR_SIZE}),
            Optional<int>::none() // CPU tensor
        );
    }

    //
    Tensor Manager::resultsTensor() const
    {
        return impl_->exportTensor(ExportID::Results, TensorElementType::Int32,
                {impl_->cfg.numWorlds, 1});
    }

    Tensor Manager::results2Tensor() const
    {
        return impl_->exportTensor(ExportID::Results2, TensorElementType::Int32,
                {impl_->cfg.numWorlds, 1000});
    }

    Tensor Manager::madronaEventsTensor() const
    {
        return impl_->exportTensor(ExportID::MadronaEvents, TensorElementType::Int32,
                {impl_->cfg.numWorlds, 100000});
    }

    Tensor Manager::madronaEventsResultTensor() const
    {
        return impl_->exportTensor(ExportID::MadronaEventsResult, TensorElementType::Int32,
                {impl_->cfg.numWorlds, 100000});
    }


    Tensor Manager::simulationTimeTensor() const
    {
        return impl_->exportTensor(ExportID::SimulationTime, TensorElementType::Int64,
                {impl_->cfg.numWorlds, 1});
    }

    Tensor Manager::processParamsTensor() const
    {
        return impl_->exportTensor(ExportID::ProcessParams, TensorElementType::Int32,
                {impl_->cfg.numWorlds, 1000});
    }

    Tensor Manager::topoTensor() const
    {
        return impl_->exportTensor(ExportID::TopoTensor, TensorElementType::Int32,
                {impl_->cfg.numWorlds, TOPO_TENSOR_SIZE});
    }

    Tensor Manager::fibTensor() const
    {
        return impl_->exportTensor(ExportID::FibTensor, TensorElementType::UInt8,
                {impl_->cfg.numWorlds, FIB_TENSOR_SIZE});
    }
    // //

    // for llm sys
    Tensor Manager::chakraNodesDataTensor() const
    {
        // Keep as Int32 (framework limitation), Python will reinterpret as uint32
        return impl_->exportTensor(ExportID::ChakraNodesData, TensorElementType::Int32,
                {impl_->cfg.numWorlds, NPU_NUM, CHAKRA_NODES_DATA_LENGTH});  
    }

}
