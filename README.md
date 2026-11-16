# GAND4LLM (GPU Accelerated Network DES)


# How to use this repository

# Build the environment dependency

## At the beggining, make sure that NVIDIA driver 560.35.05(both LM1 and LM2 already support), cuda == 12.5(recommended 12.5),  cmake>=3.24(reommended 3.27), torch, in your enviroment:

### 1. check the version of cuda using "$nvcc --version", if cuda is not >= 12.5, check whether there is cuda 12.5 using "ls /usr/local/cuda*"

If cuda 12.5 exists, specify the version of cuda in ~/.bashrc：
```bash 
export PATH=/usr/local/cuda-12.5/bin${PATH:+:${PATH}}
export LD_LIBRARY_PATH=/usr/local/cuda-12.5/lib64${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}
```
else install cuda 12.5.

### 2. install torch
```bash 
pip3 install torch torchvision torchaudio
```

### 3. install cmake3.27(recommended)：

Download cmake3.27 from https://github.com/Kitware/CMake/releases/download/v3.27.6/cmake-3.27.6-linux-x86_64.sh, then
```bash 
$bash cmake-3.27.6-linux-x86_64.sh
```
Specify the path of cmake in ~/.bashrc：
```bash 
export PATH=your_directory/cmake-3.27.6-linux-x86_64/bin:$PATH
```


# Download the code

## Next, you should fetch the multiverse repo and swtich to the right branch:
```bash
git clone --recursive https://github.com/harnets/multiverse.git
cd ./multiverse/
git checkout net 
```

if false or stucked,
```
cd ./multiverse/
git submodule update --init --recursive
```
or delete the multiverse and git clone again.


# Compile and Run

## 1. for Linux and MacOS: Run `cmake` and then `make` to build the simulator:
```bash
mkdir build
cd build
cmake ..
make -j # cores to build with
cd ..
```

## 2. setup the python components of the repository with `pip`:
```bash
pip install -e .
cd ..
```

## 3. run：
```bash
bash run.sh
```
if no error happens, success!!！


# How to inject traffic flows
Use the system of "comm_set_flow" in src/sim.cpp. In default, every NPU would execute the system at every frame.
For example, build up 15 flows
( 
1-th flow: NPU 0 --> NPU 1
2-th flow: NPU 1 --> NPU 2 
...
15-th flow: NPU 14 --> NPU 15
16-th flow: NPU 15 --> NPU 0
). 
!!! make sure each flow has a different flow_id.


```cpp

inline void comm_set_flow(Engine &ctx, NET_NPU_ID _net_npu_id,
                       NewFlowQueue &_new_flow_queue, SimTime &_sim_time,  
                       SimTimePerUpdate &_sim_time_per_update) {
    
    #if PRINT_SYS_LOG
    if (_net_npu_id.net_npu_id == 0) {
        printf("*********Enter into comm_set_flow, net_npu_id: %u\n", _net_npu_id.net_npu_id);
        printf("comm_set_flow: _new_flow_queue, before enqueue, len = %d\n", get_queue_len(_new_flow_queue));
    }
    #endif

    // if not set, every NPU would execute the system at every step/frame.
    if((_sim_time.sim_time % (1000000LL*1000) != 0)) {return;}


    // a step of ring all reduce
    if (_net_npu_id.net_npu_id < ctx.data().num_net_npu) {
        uint32_t src = _net_npu_id.net_npu_id;
        uint32_t dst = (_net_npu_id.net_npu_id+1)%(ctx.data().num_net_npu);
        uint64_t flow_size = 1LL * 1000* 1000;  //1MB
        // Lock only around shared resource access
        setFlow(ctx, src, dst, flow_size, flow_id);
    }

    // a round of all2all  
    // uint32_t batch_size = 32;
    // uint32_t token_size = 7*1024;
    // uint32_t top_k = 8;
    // uint64_t new_flow_size = batch_size * token_size * top_k / ctx.data().num_net_npu; // Unit is Byte
    // if (_net_npu_id.net_npu_id < ctx.data().num_net_npu) {
    //     for (uint32_t dst = 0; dst < ctx.data().num_net_npu; dst++) {
    //         if (dst == _net_npu_id.net_npu_id) continue;
    //         uint32_t src = _net_npu_id.net_npu_id;
    //         setFlow(ctx, src, dst, new_flow_size, flow_id);
    //     }
    // }

    #if PRINT_SYS_LOG
    if (_net_npu_id.net_npu_id == 0) {
        printf("Exiting create_flow for net_npu_id: %u\n", _net_npu_id.net_npu_id);
    }
    #endif
}

```

And you can also control the frequency of flow building by setting:
```cpp
    if((_sim_time.sim_time % (1000000LL*1000) != 0)) {return;}
```

# How to change the network topo
In scripts/train_with_topo_fib_transfer.py, you can set the topo file by:
```cpp
topology_file = "fattree_16_1024g_8gps_100Gbps_H100_no_scale_up"
```
And you shoudl set LOOKAHEAD_TIME, which should be set same with the minimum link latency in topo fule, defined in src/types.hpp.

# How to set the stop time of simulation.
In run.sh, you can set the stop time by: 
```bash
num_updates=1000 #  1000*LOOKAHEAD_TIME=1000*1000ns (LOOKAHEAD_TIME must be same with the minimum link latency in topo fule, degined in src/types.hpp)
```


# How to get the flow completion info
in check_flow_state system in src/sim.cpp, multiverse would print the flow completion info:
```cpp
    flow_num_finish = checkFlowFinish(ctx, _net_npu_id.net_npu_id, flows_finish);
```

