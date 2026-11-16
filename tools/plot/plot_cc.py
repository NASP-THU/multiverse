import re
import matplotlib.pyplot as plt
import numpy as np
from collections import defaultdict
from matplotlib import rcParams

font = {'family': 'sans-serif', 'size': 14}  # 统一字体为 sans-serif，大小为 14
rcParams.update({'font.size': font['size'], 'font.family': font['family']})


# 支持任意条流（每条流由 sender 和 receiver 共同标识）
pattern = re.compile(
    r'In .+?\. sender: (\d+), receiver: (\d+), flow id: \d+, sim_time: (\d+), target rate is [\d\.]+\s+Gbps, and current rate is ([\d\.]+)\s+Gbps'
)

# 用 (sender, receiver) 作为 key
data = defaultdict(list)
file_name = '1202_1024gpus_net_5MB_1_to_2_0' #'1124_gpu_1024gpus_1_to_2_50M_43'
log_file_path = '/data1/guifei/test/multiverse-dev/scripts/' + file_name  # 修改为实际路径

with open(log_file_path, 'r') as f:
    for line in f:
        match = pattern.search(line)
        if match:
            print("matched line: ", line)
            sender = int(match.group(1))
            receiver = int(match.group(2))
            sim_time = int(match.group(3))
            rate = float(match.group(4))
            data[(sender, receiver)].append((sim_time/1000/1000, rate))


# for flow, records in data.items():
#     print(f"Flow {flow}: {records}")

for flow, records in data.items():
    # 过滤 sim_time <= 10000000 的数据点
    filtered_records = [(sim_time, rate) for sim_time, rate in records]
    if filtered_records:  # 只打印有数据的流
        print(f"Flow {flow}: {filtered_records}")
        
# 按 sim_time 排序
for flow in data:
    data[flow].sort(key=lambda x: x[0])

plt.figure(figsize=(12, 7))

# 颜色映射
colors = plt.cm.tab10.colors + plt.cm.tab20.colors

# # 绘制每条流
# for i, flow in enumerate(sorted(data.keys())):
#     if data[flow]:
#         if flow[0] < flow[1]:
#             sim_time, rate = zip(*data[flow])
#             color_idx = i % len(colors)
#             plt.plot(sim_time, rate, label=f'S{flow[0]}-R{flow[1]}', color=colors[color_idx])
#             plt.scatter(sim_time, rate, color=colors[color_idx], s=20, marker='o') 


for i, flow in enumerate(sorted(data.keys())):
    if data[flow]:
        # 移除 flow[0] < flow[1] 的限制，绘制所有流
        # 过滤 sim_time < 10000000 的数据点
        filtered_data = [(sim_time, rate) for sim_time, rate in data[flow]]
        
        if filtered_data:  # 确保过滤后还有数据
            sim_time, rate = zip(*filtered_data)
            color_idx = i % len(colors)
            plt.plot(sim_time, rate, label=f'S{flow[0]}-R{flow[1]}', color=colors[color_idx])
            plt.scatter(sim_time, rate, color=colors[color_idx], s=20, marker='o')


plt.xlabel('Simulation Time (ms)')
plt.ylabel('Current Rate (Gbps)')
plt.title('Flow Rates VS Time(Multiverse-DCQCN)')
plt.legend(loc='best', ncol=min(5, len(data)))
plt.grid(True)
plt.tight_layout()

# 移除统计信息部分
# stats_text = ""
# for flow in sorted(data.keys()):
#     rates = [r for _, r in data[flow]]
#     if rates:
#         stats_text += f"S{flow[0]}-R{flow[1]}: Avg={np.mean(rates):.2f}, Max={np.max(rates):.2f}, Min={np.min(rates):.2f} Gbps\n"
# plt.figtext(0.02, 0.02, stats_text, va="bottom", ha="left", fontsize=8, bbox={"facecolor":"white", "alpha":0.8, "pad":5})

plt.savefig('flow_rates_' + file_name + '.png', dpi=300)
# plt.show()














# import re
# import matplotlib.pyplot as plt

# # 正则表达式匹配 sender、sim_time、current rate
# pattern = re.compile(
#     r'sender: (\d), sim_time: (\d+),.*current rate is ([\d\.]+) Gbps'
# )

# # 存储数据
# data = {0: [], 1: [], 2: []}

# with open('/data3/guifei/projects/goats/madrona_escape_room/log_cc/log_cc_50M_0_2_to_3', 'r') as f:
#     for line in f:
#         match = pattern.search(line)
#         if match:
#             sender = int(match.group(1))
#             if sender not in (0, 1, 2):
#                 continue  # 跳过不是0、1、3的sender
#             sim_time = int(match.group(2))
#             rate = float(match.group(3))
#             data[sender].append((sim_time, rate))

# # 分别排序
# for sender in data:
#     data[sender].sort(key=lambda x: x[0])

# # 拆分为x、y
# sim_time_0, rate_0 = zip(*data[0]) if data[0] else ([], [])
# sim_time_1, rate_1 = zip(*data[1]) if data[1] else ([], [])
# sim_time_2, rate_2 = zip(*data[2]) if data[2] else ([], [])

# print("simtime-0: ", sim_time_0)
# print("rate-0: ", rate_0)
# print("simtime-1: ", sim_time_1)
# print("rate-1: ", rate_1)

# plt.figure(figsize=(10,6))
# plt.plot(sim_time_0, rate_0, label='Sender 0')
# plt.plot(sim_time_1, rate_1, label='Sender 1')
# plt.plot(sim_time_2, rate_2, label='Sender 2')
# plt.xlabel('Sim Time')
# plt.ylabel('Current Rate (Gbps)')
# plt.title('Current Rate vs Sim Time (Sorted)')
# plt.legend()
# plt.grid(True)
# plt.tight_layout()
# plt.show()