#pragma once
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <stdexcept>
#include "types.hpp"
#include <madrona/py/bindings.hpp>
#include <nanobind/ndarray.h> // Include for nanobind::ndarray

namespace madEscape {

class FibProxy {
private:
    // 内存映射文件描述符
    int fib_fd;
    int next_hop_fd;
    
    // 内存映射指针
    int16_t* fib_data;
    int16_t* next_hop_num_data;
    
    // 映射大小
    size_t fib_size;
    size_t next_hop_size;
    
    // 临时文件路径
    std::string fib_path;
    std::string next_hop_path;
    
public:
    // 构造函数 - 创建内存映射文件
    FibProxy() : fib_fd(-1), next_hop_fd(-1), 
                 fib_data(nullptr), next_hop_num_data(nullptr) {
        // 计算所需文件大小 - 修正维度以匹配types.hpp中的Fib结构体
        fib_size = (size_t)(MAX_SW_NUM + MAX_NET_NPU_NUM) * 
                   (size_t)(MAX_SW_NUM + MAX_NET_NPU_NUM) *    // 修正：第二维应该是MAX_SW_NUM + MAX_NET_NPU_NUM
                   (size_t)MAX_ONE_SW_PORT_NUM * sizeof(int16_t);
        
        next_hop_size = (size_t)(MAX_SW_NUM + MAX_NET_NPU_NUM) * 
                       (size_t)(MAX_SW_NUM + MAX_NET_NPU_NUM) * sizeof(int16_t); // 修正：第二维应该是MAX_SW_NUM + MAX_NET_NPU_NUM
        
        // 创建唯一的临时文件名
        pid_t pid = getpid();
        fib_path = "/tmp/fib_array_" + std::to_string(pid);
        next_hop_path = "/tmp/next_hop_" + std::to_string(pid);
        
        // 创建并配置文件
        fib_fd = open(fib_path.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (fib_fd == -1) {
            throw std::runtime_error("Failed to create fib mapping file");
        }
        
        next_hop_fd = open(next_hop_path.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (next_hop_fd == -1) {
            close(fib_fd);
            throw std::runtime_error("Failed to create next_hop mapping file");
        }
        
        // 设置文件大小
        if (ftruncate(fib_fd, fib_size) == -1) {
            cleanup();
            throw std::runtime_error("Failed to resize fib mapping file");
        }
        
        if (ftruncate(next_hop_fd, next_hop_size) == -1) {
            cleanup();
            throw std::runtime_error("Failed to resize next_hop mapping file");
        }
        
        // 映射文件到内存
        fib_data = (int16_t*)mmap(nullptr, fib_size, PROT_READ | PROT_WRITE, 
                                 MAP_SHARED, fib_fd, 0);
        if (fib_data == MAP_FAILED) {
            fib_data = nullptr;
            cleanup();
            throw std::runtime_error("Failed to map fib file to memory");
        }
        
        next_hop_num_data = (int16_t*)mmap(nullptr, next_hop_size, PROT_READ | PROT_WRITE,
                                          MAP_SHARED, next_hop_fd, 0);
        if (next_hop_num_data == MAP_FAILED) {
            next_hop_num_data = nullptr;
            cleanup();
            throw std::runtime_error("Failed to map next_hop file to memory");
        }
        
        // 初始化默认值
        memset(fib_data, 0xFF, fib_size);  // -1 的二进制表示是全1
        memset(next_hop_num_data, 0, next_hop_size);
    }
    
    // 析构函数 - 清理资源
    ~FibProxy() {
        cleanup();
    }
    
    // 不允许拷贝
    FibProxy(const FibProxy&) = delete;
    FibProxy& operator=(const FibProxy&) = delete;

    // 辅助函数 - 清理资源
    void cleanup() {
        if (fib_data) {
            munmap(fib_data, fib_size);
            fib_data = nullptr;
        }
        
        if (next_hop_num_data) {
            munmap(next_hop_num_data, next_hop_size);
            next_hop_num_data = nullptr;
        }
        
        if (fib_fd != -1) {
            close(fib_fd);
            unlink(fib_path.c_str());
            fib_fd = -1;
        }
        
        if (next_hop_fd != -1) {
            close(next_hop_fd);
            unlink(next_hop_path.c_str());
            next_hop_fd = -1;
        }
    }
    
    // 计算3D数组索引
    size_t calculate_fib_index(int32_t i, int32_t j, int32_t k) const {
        return (size_t)i * (MAX_SW_NUM + MAX_NET_NPU_NUM) * MAX_ONE_SW_PORT_NUM + 
               (size_t)j * MAX_ONE_SW_PORT_NUM + k;
    }
    
    // 计算2D数组索引
    size_t calculate_next_hop_index(int32_t i, int32_t j) const {
        return (size_t)i * (MAX_SW_NUM + MAX_NET_NPU_NUM) + j;
    }
    
    // 设置单个元素值
    void set_entry(int32_t i, int32_t j, int32_t k, int16_t val) {
        if (i >= 0 && i < MAX_SW_NUM + MAX_NET_NPU_NUM &&
            j >= 0 && j < MAX_SW_NUM + MAX_NET_NPU_NUM &&
            k >= 0 && k < MAX_ONE_SW_PORT_NUM) {
            size_t idx = calculate_fib_index(i, j, k);
            fib_data[idx] = val;
        }
    }
    
    // 获取单个元素值
    int16_t get_entry(int32_t i, int32_t j, int32_t k) const {
        if (i < 0 || i >= MAX_SW_NUM + MAX_NET_NPU_NUM ||
            j < 0 || j >= MAX_SW_NUM + MAX_NET_NPU_NUM ||
            k < 0 || k >= MAX_ONE_SW_PORT_NUM || fib_data == nullptr) {
            return -1;
        }
        size_t idx = calculate_fib_index(i, j, k);
        return fib_data[idx];
    }
    
    // 设置 next_hop_num 值
    void set_next_hop_num(int32_t i, int32_t j, int16_t val) {
        if (i >= 0 && i < MAX_SW_NUM + MAX_NET_NPU_NUM &&
            j >= 0 && j < MAX_SW_NUM + MAX_NET_NPU_NUM) {
            size_t idx = calculate_next_hop_index(i, j);
            next_hop_num_data[idx] = val;
        }
    }
    
    // 获取 next_hop_num 值
    int16_t get_next_hop_num(int32_t i, int32_t j) const {
        if (i >= 0 && i < MAX_SW_NUM + MAX_NET_NPU_NUM &&
            j >= 0 && j < MAX_SW_NUM + MAX_NET_NPU_NUM) {
            size_t idx = calculate_next_hop_index(i, j);
            return next_hop_num_data[idx];
        }
        return 0;
    }
    
    // 转换为标准 Fib 结构体（用于传递给 C++ 核心）
    Fib* toFib() const {
        Fib* result = new Fib();
        
        // 逐个元素复制（可以优化为分块复制）
        for (int32_t i = 0; i < MAX_SW_NUM + MAX_NET_NPU_NUM; i++) {
            for (int32_t j = 0; j < MAX_SW_NUM + MAX_NET_NPU_NUM; j++) {
                // 复制 next_hop_num
                size_t next_hop_idx = calculate_next_hop_index(i, j);
                result->next_hop_num[i][j] = next_hop_num_data[next_hop_idx];
                
                for (int32_t k = 0; k < MAX_ONE_SW_PORT_NUM; k++) {
                    // 复制 fib
                    size_t fib_idx = calculate_fib_index(i, j, k);
                    result->fib[i][j][k] = fib_data[fib_idx];
                }
            }
        }
        return result;
    }
    
    // 从 numpy 数组设置 fib 数据
    void set_fib_from_array(nb::ndarray<> arr) {
        if (fib_data == nullptr) {
            throw std::runtime_error("Memory mapping is not initialized");
        }
        
        if (arr.ndim() != 3) {
            throw std::invalid_argument("fib must be a 3D array");
        }
        
        // Get the actual dimensions we can copy
        size_t dim1 = std::min(static_cast<size_t>(arr.shape(0)), 
                              static_cast<size_t>(MAX_SW_NUM + MAX_NET_NPU_NUM));
        size_t dim2 = std::min(static_cast<size_t>(arr.shape(1)), 
                              static_cast<size_t>(MAX_SW_NUM + MAX_NET_NPU_NUM));
        size_t dim3 = std::min(static_cast<size_t>(arr.shape(2)), 
                              static_cast<size_t>(MAX_ONE_SW_PORT_NUM));
        
        // Type checking
        if (arr.dtype() != nb::dtype<int16_t>()) {
            throw std::invalid_argument("Array must have dtype int16_t");
        }
        
        // Copy data element by element (slower but safer)
        int16_t* data = static_cast<int16_t*>(arr.data());
        for (size_t i = 0; i < dim1; i++) {
            for (size_t j = 0; j < dim2; j++) {
                for (size_t k = 0; k < dim3; k++) {
                    size_t src_idx = i * (arr.shape(1) * arr.shape(2)) + j * arr.shape(2) + k;
                    size_t dst_idx = calculate_fib_index(i, j, k);
                    fib_data[dst_idx] = data[src_idx];
                }
            }
        }
    }
    
    // 从 numpy 数组设置 next_hop_num 数据
    void set_next_hop_num_from_array(nb::ndarray<> arr) {
        if (arr.ndim() != 2 || 
            arr.shape(0) > MAX_SW_NUM + MAX_NET_NPU_NUM ||
            arr.shape(1) > MAX_SW_NUM + MAX_NET_NPU_NUM) {
            throw std::invalid_argument("Invalid next_hop_num array dimensions");
        }
        
        int16_t* data = static_cast<int16_t*>(arr.data());
        for (size_t i = 0; i < arr.shape(0); i++) {
            for (size_t j = 0; j < arr.shape(1); j++) {
                size_t src_idx = i * arr.shape(1) + j;
                size_t dst_idx = calculate_next_hop_index(i, j);
                next_hop_num_data[dst_idx] = data[src_idx];
            }
        }
    }
    
    // 获取 fib 数据的 numpy 视图（不复制数据）
    nb::ndarray<nb::numpy, int16_t> get_fib_view() {
        if (fib_data == nullptr) {
            throw std::runtime_error("Memory mapping is not initialized");
        }
        
        try {
            return nb::ndarray<nb::numpy, int16_t>(
                fib_data,
                {MAX_SW_NUM + MAX_NET_NPU_NUM, MAX_SW_NUM + MAX_NET_NPU_NUM, MAX_ONE_SW_PORT_NUM},
                nb::handle()
            );
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Failed to create numpy view: ") + e.what());
        }
    }
    
    // 获取 next_hop_num 数据的 numpy 视图（不复制数据）
    nb::ndarray<nb::numpy, int16_t> get_next_hop_num_view() {
        return nb::ndarray<nb::numpy, int16_t>(
            next_hop_num_data,
            {MAX_SW_NUM + MAX_NET_NPU_NUM, MAX_SW_NUM + MAX_NET_NPU_NUM},
            nb::handle()
        );
    }
    
    // 将内存中的更改同步到磁盘
    void sync() {
        if (fib_data) msync(fib_data, fib_size, MS_SYNC);
        if (next_hop_num_data) msync(next_hop_num_data, next_hop_size, MS_SYNC);
    }
};

// Python sparse dictionary-based solution
class SparseFib {
    public __init__():
        self.data = {}
        self.next_hop_data = {}
        
    def set_entry(self, i, j, k, val):
        if val != -1:  # Only store non-default values
            self.data[(i,j,k)] = val
    
    def get_entry(self, i, j, k):
        return self.data.get((i,j,k), -1)
        
    def set_next_hop_num(self, i, j, val):
        if val != 0:
            self.next_hop_data[(i,j)] = val
            
    def get_next_hop_num(self, i, j):
        return self.next_hop_data.get((i,j), 0)
        
    def to_full_arrays(self):
        """Convert sparse representation to full arrays when needed by C++"""
        fib_array = np.full((MAX_SW_NUM + MAX_NET_NPU_NUM, MAX_SW_NUM + MAX_NET_NPU_NUM, MAX_ONE_SW_PORT_NUM), 
                           -1, dtype=np.int16)
        next_hop = np.zeros((MAX_SW_NUM + MAX_NET_NPU_NUM, MAX_SW_NUM + MAX_NET_NPU_NUM), dtype=np.int16)
        
        for (i,j,k), val in self.data.items():
            fib_array[i,j,k] = val
            
        for (i,j), val in self.next_hop_data.items():
            next_hop[i,j] = val
            
        return fib_array, next_hop
}