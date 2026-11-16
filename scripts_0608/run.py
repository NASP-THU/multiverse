import sys 
import numpy as np
import torch
import time
import math 
from datetime import datetime

from madrona_simple_example import GridWorld

num_worlds = int(sys.argv[1])
#num_worlds = math.ceil(port_num/2)

# num_packet_total = int(sys.argv[3]) # the number of packet (in multi-steps) in total will be sent
# num_packet_perSender = num_packet_total/port_num
# step_num = math.ceil(num_packet_perSender/10) # 10 = LOOKAHEADT_TIME in types.hpp

enable_gpu_sim = False
#if len(sys.argv) >= 3 and sys.argv[2] == '--gpu':
if sys.argv[2] == '--gpu':
    enable_gpu_sim = True


array_shape = [5,6]
walls = np.zeros(array_shape)
rewards = np.zeros(array_shape)
walls[3, 2:] = 1
start_cell = np.array([4,5])
end_cell = np.array([[4,5]])
rewards[4, 0] = -1
rewards[4, 5] = 1

grid_world = GridWorld(num_worlds, start_cell, end_cell, rewards, walls, enable_gpu_sim, 0)
#grid_world.vis_world()

# print(grid_world.observations.shape)

#print("port_num: ", port_num, "num_worlds: ", num_worlds, "\n") 
#print("num_packet_total: ", num_packet_total, "\n")
#print("step_num: ", step_num, "\n")


start_time = time.time()

step_num = 2 #100*100
for i in range(step_num):
    grid_world.step()

end_time = time.time()

execution_time = end_time - start_time

print(datetime.now())
print("step_num: ", step_num, "\n")
print(f"step function took {execution_time:.5f} seconds to execute.")

with open('./new_res.log', 'a') as f:
    finish_time = datetime.now()
    f.write(f"finish_time: {finish_time}\n")
    f.write(f"num_worlds: {num_worlds}\n")
    # f.write(f"num_packet_total: {num_packet_total}\n")
    f.write(f"step_num: {step_num}\n")
    f.write(f"step function took {execution_time:.5f} seconds to execute.\n\n")
    
