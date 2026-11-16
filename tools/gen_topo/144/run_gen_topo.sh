# for diameter in 2 3 5; do
    
diameter=1
num_gpu=120
bw=100
lat=1000

# python3 generate_topology_v3.py ${diameter} ${gpu_num} -o topo_${gpu_num}_${diameter}_${bw}_${lat}.txt -i final_d3.png -b ${bw} -l ${lat} 2>&1

python3 generate_topology_v3.py ${diameter} ${num_gpu} -o topo_${num_gpu}_${diameter}_${bw}_${lat}.txt -i topo_${num_gpu}_${diameter}_${bw}_${lat}.png -b ${bw} -l ${lat} 2>&1

# done
