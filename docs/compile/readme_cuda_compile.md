### README: GPU 编译与启动阶段可观测性与用法说明

#### 目标
- 提供逐文件 NVRTC 编译耗时与 NVJITLINK 链接耗时日志
- 提供每个初始化 kernel 的启动与同步耗时日志
- 运行方法与日志解读
- 常见告警与（非 OOM）问题定位

### 新增能力
- 编译期日志（JIT/NVRTC、NVJITLINK）
  - 输出每个设备端源码的编译时长与最终链接耗时
  - 示例：
    ```
    [NVRTC] compiled /path/to/file.cpp in 4321.32 ms
    [NVJITLINK] linked 80432992 bytes in 380.94 ms
    ```
- 启动期日志（初始化内核）
  - 输出每个内核的 grid/block 配置与执行耗时（含同步）
  - 示例：
    ```
    [KERNEL] launch 0x... blocks=1 threads=1
    [KERNEL] done 0x... in 910.10 ms
    [KERNEL] launch 0x... blocks=1 threads=256
    ```
  - 一般对应关系（TaskGraph 模式）：
    - 第 1 个：computeGPUImplConsts（1×1）
    - 第 2 个：initECS（1×1）
    - 第 3 个：initWorlds（N×256，默认 N=1）
    - 第 4 个：initTasks（1×1）

### 如何启用
- 必需环境变量
  ```
  export MADRONA_MWGPU_VERBOSE_COMPILE=1
  ```
  - 打开后会打印：
    - 每个 NVRTC 源文件的编译耗时
    - NVJITLINK 链接耗时
    - 每个初始化 kernel 的启动/完成日志与耗时
- 可选环境变量
  - 调整 initWorlds 线程数（降低资源压力）：
    ```
    export MADRONA_MWGPU_INIT_THREADS=128    # 也可设为 64、32 等
    ```
  - 指定 megakernel 执行配置（threads, blocksPerSM, numSMs）：
    ```
    export MADRONA_MWGPU_EXEC_CONFIG_OVERRIDE=256,1,84
    ```
  - 启用同步便于定位问题：
    ```
    export CUDA_LAUNCH_BLOCKING=1
    ```
  - 开启/关闭调试编译（会显著影响编译与运行性能）：
    ```
    # 建议默认关闭，需深入调试时再开启
    export MADRONA_MWGPU_FORCE_DEBUG=0
    ```
  - 缓存路径（建议设置为稳定、可写路径）：
    ```
    export MADRONA_MWGPU_KERNEL_CACHE="$(pwd)/build/mw_megakernel_cache.bin"
    export MADRONA_BVH_KERNEL_CACHE="$(pwd)/build/mw_bvh_cache.bin"
    ```

### 运行与重建
- 构建
  ```
  cd build
  cmake ..
  make -j
  ```
- 运行（示例，按项目现有 `run.sh`/脚本为准）
  ```
  export MADRONA_MWGPU_VERBOSE_COMPILE=1
  # 其他可选 env 见“可选环境变量”
  ./run.sh
  ```
- 命中缓存
  - 二次运行若源码/flags 无变化，应直接命中缓存，不再触发 NVRTC 编译
  - 如需强制重新编译，删除缓存文件再运行

### 日志解读与常见现象
- “Stack size … cannot be statically determined”
  - NVCC/NVJITLINK 的常见告警，不影响功能
- NVRTC 耗时长
  - 大源文件、模板/内联/优化级别高、启用调试（-g/--device-debug）都会显著放大编译时间
  - 关闭 `MADRONA_MWGPU_FORCE_DEBUG` 可明显加速
- CUDA_ERROR_LAUNCH_OUT_OF_RESOURCES（非 OOM）
  - 降低 `MADRONA_MWGPU_INIT_THREADS`（如 128/64）
  - 或调整 `MADRONA_MWGPU_EXEC_CONFIG_OVERRIDE`（降低 blocksPerSM 或 threads）

### 排错流程（不考虑 OOM）
1. 打开 `MADRONA_MWGPU_VERBOSE_COMPILE=1` 与 `CUDA_LAUNCH_BLOCKING=1`
2. 观察 [KERNEL] 序列：
   - 若第三个 kernel（initWorlds）失败，尝试：
     - `export MADRONA_MWGPU_INIT_THREADS=128`（或更低）
     - 如仍失败，调整 `MADRONA_MWGPU_EXEC_CONFIG_OVERRIDE`
3. 观察 NVRTC/NVJITLINK 耗时是否异常升高：
   - 关闭 `MADRONA_MWGPU_FORCE_DEBUG`
   - 确认缓存命中（不重复 NVRTC）
4. 仍有问题时，收集最后一个 [KERNEL] 的 blocks/threads 与错误码，进一步定位相关系统或模块

### 小贴士
- 变更编译参数或设备端源码都会导致重新 JIT，尽量固定缓存路径、避免每次清空 build。
- 选择空闲显存多的 GPU 有助于减少初始化时间波动（即便不考虑 OOM）。 