# Multiverse development

GPU Accelerated LLM tranining simulator for LLM  training

# How to use this repository

This repository is extended from madrona_escape_room .
So, you should install madrona_escape_room (https://github.com/shacklettbp/madrona_escape_room) at the beggining. And then you replace some original codes with our developed and modified codes. 


# In the first phase: install madrona_escape_room
## At the beggining, make sure that NVIDIA driver 560.35.05(both LM1 and LM2 already support), cuda == 12.5(recommended 12.5),  cmake>=3.24(reommended 3.27), torch, in your enviroment:

### 1 check the version of cuda using "$nvcc --version", if cuda is not >= 12.5, check whether there is cuda 12.5 using "ls /usr/local/cuda*"

If cuda 12.5 exists, specify the version of cuda in ~/.bashrc：
```bash 
export PATH=/usr/local/cuda-12.5/bin${PATH:+:${PATH}}
export LD_LIBRARY_PATH=/usr/local/cuda-12.5/lib64${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}
```
else install cuda 12.5. (Both server LM1 and LM2 support cuda 12.5)

### 2 install torch
```bash 
pip3 install torch torchvision torchaudio
```

### 3 install cmake3.27(recommended)：

Download cmake3.27 from https://github.com/Kitware/CMake/releases/download/v3.27.6/cmake-3.27.6-linux-x86_64.sh, then
```bash 
$bash cmake-3.27.6-linux-x86_64.sh
```
Specify the path of cmake in ~/.bashrc：
```bash 
export PATH=your_directory/cmake-3.27.6-linux-x86_64/bin:$PATH
```



## Next, you should fetch the madrona_escape_room repo:
```bash
git clone --recursive git@github.com:ShawnshanksGui/multiverse-dev.git
cd ./multiverse-dev/
```

if false，
```
cd ./multiverse-dev/
git submodule update --init --recursive
```


## Thirdly, for Linux and MacOS: Run `cmake` and then `make` to build the simulator:
```bash
mkdir build
cd build
cmake ..
make -j # cores to build with
cd ..
```

Fourth, setup the python components of the repository with `pip`:
```bash
pip install -e .
cd ..
```

Fifth, check whether code is normally built up:
```bash
bash run.sh
```


# In the second pahse, switch to your branch and test it! (take branch 'net' as a example)

```bash
git checkout net
bash run.sh
```
if no error happens, success!!! 
You can start your development of multiverse! Have fun and make a difference😂!


# (optional) if you have already developed a lot code in your current local repo, you replace your code with the code in multiverse repo

```
cp -r ./your_repo/src ./your_repo/scripts ./your_repo/run.sh ./multiverse-dev/
cd ./multiverse-dev
mkdir build
cd build
cmake ..
make -j   # cores to build with
cd ..
pip install -e .
cd ..
bash run.sh
```


# How to inject traffic flows
Use the system of "comm_set_flow". Every NPUs would execute the system at every frame.
For example, build up 15 flows
( 
1-th flow: NPU 0 --> NPU 1
2-th flow: NPU 1 --> NPU 2 
...
15-th flow: NPU 14 --> NPU 15
16-th flow: NPU 15 --> NPU 0
). 
And make sure each flow has a different flow_id.

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

    if((_sim_time.sim_time % (1000000LL*1000) != 0)) {return;}


    //a step of ring all reduce
    if (_net_npu_id.net_npu_id < ctx.data().num_net_npu) {
        uint32_t src = _net_npu_id.net_npu_id;
        uint32_t dst = (_net_npu_id.net_npu_id+1)%(ctx.data().num_net_npu);
        uint64_t flow_size = 1LL * 1000* 1000; 
        // Lock only around shared resource access
        setFlow(ctx, src, dst, flow_size, flow_id);
    }

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
