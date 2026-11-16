
num_env=1
num_updates=100
gpu_id=3
fattree_K=4
cc_method=1 #0 = dcqcn, 1 = hpcc(TBD)

cd ./build
cmake ..
make -j 60
cd ../
cd ./scripts/

# CUDA_LAUNCH_BLOCKING=1 
#python train.py --num-worlds $num_env --num-updates $num_updates --ckpt-dir ../build/ckpts --gpu_id $gpu_id --fattree_K $fattree_K --cc_method $cc_method --gpu-sim   #> cc_0524.log  #> cc_with_random_0523_v2.log 
#gdb --args 
python train_with_topo_fib_transfer.py --num-worlds $num_env --num-updates $num_updates --ckpt-dir ../build/ckpts --gpu_id $gpu_id --fattree_K $fattree_K --cc_method $cc_method --gpu-sim  #> cc_0603.log  



# for i in $(seq 31 60); do
#     echo "Running iteration $i..."
#     python train.py --num-worlds $num_env --num-updates $num_updates --ckpt-dir ../build/ckpts --gpu_id $gpu_id --fattree_K $fattree_K --cc_method $cc_method --gpu-sim > "../log/log_f_10_a2a_f_14KB_${i}_finished" 2>&1
#     sleep 5
# done
