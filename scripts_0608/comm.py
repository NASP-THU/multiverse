import mmap
import posix_ipc
import struct
from input_helper import MadronaEvent as MadronaMsg


# init
numMessages=1
memory = posix_ipc.SharedMemory("myshm")
size=28*numMessages+4
map_file = mmap.mmap(memory.fd, size, mmap.MAP_SHARED, mmap.PROT_WRITE | mmap.PROT_READ)
# map_file = mmap.mmap(memory.fd, mmap.PAGESIZE, mmap.MAP_SHARED, mmap.PROT_WRITE | mmap.PROT_READ)
semaphore_a = posix_ipc.Semaphore("semA")
semaphore_b = posix_ipc.Semaphore("semB")


def _create_and_write_shared_memory(messages,map_file):
    num_messages = len(messages)
    header_size = struct.calcsize('i')  # num
    message_size = struct.calcsize('7i')  # size

    # write num
    map_file.seek(0)
    map_file.write(struct.pack('i', num_messages))

    # write MadronaMsg
    for msg in messages:
        map_file.write(msg.pack())

def receive_data():
    # wait msg
    semaphore_a.acquire()
    map_file.seek(0)
    # read msg num
    numMessages, = struct.unpack('i', map_file.read(4))  
    messages = []
    for _ in range(numMessages):
        data = map_file.read(28)  #  MadronaMsg = 24byte
        # message = MadronaMsg(data)
        unpacked_data = struct.unpack('7i', data)
        message = MadronaMsg(*unpacked_data)  
        messages.append(message)
    # print(f"Madrona: Received from Astrasim: {messages}")
    return messages

def send_data(messages):
    _create_and_write_shared_memory(messages,map_file)
    semaphore_b.release()

def comm_dispose():
    map_file.close()
    memory.close_fd()
    semaphore_a.close()
    semaphore_b.close()

# comm()