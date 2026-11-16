### 加快 GPU 编译时间（NVRTC/JIT）

本项目在开启 GPU 路径时，会通过 NVRTC 对设备端 C++ 源码进行 JIT 编译，并使用 nvJitLink 链接生成 CUBIN。首次编译通常较慢；如果未开启缓存，每次进程启动都会重复编译。

下面提供一套最小化修改、立刻见效的加速方案。

---

### TL;DR（立即复制执行）

```bash
# 选择一个稳定、可写的缓存位置（建议放在 build/ 目录内）
export MADRONA_MWGPU_KERNEL_CACHE="$(pwd)/build/mw_megakernel_cache.bin"
export MADRONA_BVH_KERNEL_CACHE="$(pwd)/build/mw_bvh_cache.bin"

# 可选：打印编译/链接的详细 flags（用于确认是否仍在编译）
export MADRONA_MWGPU_VERBOSE_COMPILE=1

# 运行你的 GPU 程序（第一次仍会慢；后续将直接加载缓存，显著加速）
# 例如：
# bash run.sh
```

- 第一次运行：生成 `mw_megakernel_cache.bin` 和/或 `mw_bvh_cache.bin`。
- 第二次及之后：将直接 `cuModuleLoadData(...)`，通常秒级启动。

---

### 为什么会慢
- 使用 NVRTC 对大体量设备端代码 JIT 编译，默认还会走 LTO（`-dlto`/`-lto`），首次代价较高。
- 若启用 PGO，会生成多组 megakernel 配置并重复编译，进一步放大耗时。
- 不设置缓存文件路径时，每次都要重新编译和链接。

---

### 推荐配置

- **开启磁盘缓存（强烈推荐）**
  - Megakernel 缓存：`MADRONA_MWGPU_KERNEL_CACHE`
  - BVH 内核缓存：`MADRONA_BVH_KERNEL_CACHE`
  - 建议放到 `build/` 下，路径例如：
    ```bash
    export MADRONA_MWGPU_KERNEL_CACHE="$(pwd)/build/mw_megakernel_cache.bin"
    export MADRONA_BVH_KERNEL_CACHE="$(pwd)/build/mw_bvh_cache.bin"
    ```

- **开发期进一步加速（牺牲运行性能）**
  - 强制 Debug 编译（禁用重优化/LTO，显著缩短编译时间）：
    ```bash
    export MADRONA_MWGPU_FORCE_DEBUG=1
    ```
  - 注意：这会降低 GPU 运行时性能，仅建议在频繁重启/调试时开启。

- **避免多份重复编译**
  - 确保未开启 PGO：
    ```bash
    unset MADRONA_MWGPU_ENABLE_PGO
    ```

- **调试编译时长**
  - 查看 NVRTC/Linker 具体 flags：
    ```bash
    export MADRONA_MWGPU_VERBOSE_COMPILE=1
    ```

---

### 与现有脚本集成

如果你使用 `run.sh`/`run_cpu.sh`，可在脚本顶部加入：

```bash
# 缓存文件路径（建议保持稳定）
export MADRONA_MWGPU_KERNEL_CACHE="$(pwd)/build/mw_megakernel_cache.bin"
export MADRONA_BVH_KERNEL_CACHE="$(pwd)/build/mw_bvh_cache.bin"

# 可选：调试编译输出
# export MADRONA_MWGPU_VERBOSE_COMPILE=1

# 可选：开发期快速编译（牺牲运行性能）
# export MADRONA_MWGPU_FORCE_DEBUG=1

# 避免重复编译
unset MADRONA_MWGPU_ENABLE_PGO
```

---

### 何时需要清理/重建缓存
- 升级 CUDA Toolkit/驱动或更换 GPU 架构（如从 `sm_80` 换到 `sm_90`）。
- 变更设备端内核代码（NVRTC 编译的源码）或编译/链接 flags。
- 出现“加载后行为异常、但关闭缓存正常”的情况。

清理方式：直接删除缓存文件即可（例如删除 `build/mw_megakernel_cache.bin` / `build/mw_bvh_cache.bin`）。

建议按 GPU 架构区分缓存文件名，例如：
```bash
export MADRONA_MWGPU_KERNEL_CACHE="$(pwd)/build/mw_megakernel_cache_sm90.bin"
export MADRONA_BVH_KERNEL_CACHE="$(pwd)/build/mw_bvh_cache_sm90.bin"
```

---

### 常见问题
- Q：设置了缓存还是在编译？
  - A：检查缓存路径是否可写、是否被挂载到临时目录、以及是否每次 `cmake .. && make` 清理了 `build/`。开启 `MADRONA_MWGPU_VERBOSE_COMPILE=1` 观察是否仍在 NVRTC 编译。

- Q：能否彻底禁用 LTO 但保留优化？
  - A：当前实现中 LTO/优化模式由内部 `CompileConfig::OptMode` 控制，外部最简单的加速开关是 `MADRONA_MWGPU_FORCE_DEBUG=1`（会牺牲运行性能）。

- Q：PGO 会影响编译时长吗？
  - A：会。PGO 会编译多组不同的 megakernel 配置（例如每 SM 1..6 个 blocks），显著增加首次编译时间。请确保未设置 `MADRONA_MWGPU_ENABLE_PGO`。

---

### 参考（代码路径）
- 缓存环境变量读取：`external/madrona/src/mw/cuda_exec.cpp`
  - Megakernel：`MADRONA_MWGPU_KERNEL_CACHE`
  - BVH：`MADRONA_BVH_KERNEL_CACHE`
- 编译/链接参数组装与 LTO：`external/madrona/src/mw/cuda_exec.cpp`
- NVRTC 编译与 LTO 双路径：`external/madrona/src/mw/cpp_compile.cpp` 