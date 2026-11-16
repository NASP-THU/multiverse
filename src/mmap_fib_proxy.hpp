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
            // Memory mapping file descriptors
            int fib_fd;
            int next_hop_fd;

            // Memory mapping pointers
            uint8_t* fib_data;
            uint8_t* next_hop_num_data;

            // Mapping sizes
            size_t fib_size;
            size_t next_hop_size;

            // Temporary file paths
            std::string fib_path;
            std::string next_hop_path;

        public:
            // Constructor - create memory mapping files
            FibProxy() : fib_fd(-1), next_hop_fd(-1),
            fib_data(nullptr), next_hop_num_data(nullptr) {
                // Calculate required file size - corrected dimensions to match Fib struct in types.hpp
                fib_size = (size_t)(MAX_SW_NUM + MAX_NET_NPU_NUM) *
                    (size_t)(MAX_SW_NUM + MAX_NET_NPU_NUM) *    // Fix: second dimension should be MAX_SW_NUM + MAX_NET_NPU_NUM
                    (size_t)MAX_ONE_SW_PORT_NUM * sizeof(uint8_t);

                next_hop_size = (size_t)(MAX_SW_NUM + MAX_NET_NPU_NUM) *
                    (size_t)(MAX_SW_NUM + MAX_NET_NPU_NUM) * sizeof(uint8_t); // Fix: second dimension should be MAX_SW_NUM + MAX_NET_NPU_NUM

                // Create unique temporary file names
                pid_t pid = getpid();
                fib_path = "/tmp/fib_array_" + std::to_string(pid);
                next_hop_path = "/tmp/next_hop_" + std::to_string(pid);

                // Create and configure files
                fib_fd = open(fib_path.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
                if (fib_fd == -1) {
                    throw std::runtime_error("Failed to create fib mapping file");
                }

                next_hop_fd = open(next_hop_path.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
                if (next_hop_fd == -1) {
                    close(fib_fd);
                    throw std::runtime_error("Failed to create next_hop mapping file");
                }

                // Set file sizes
                if (ftruncate(fib_fd, fib_size) == -1) {
                    cleanup();
                    throw std::runtime_error("Failed to resize fib mapping file");
                }

                if (ftruncate(next_hop_fd, next_hop_size) == -1) {
                    cleanup();
                    throw std::runtime_error("Failed to resize next_hop mapping file");
                }

                // Map files to memory
                fib_data = (uint8_t*)mmap(nullptr, fib_size, PROT_READ | PROT_WRITE,
                        MAP_SHARED, fib_fd, 0);
                if (fib_data == MAP_FAILED) {
                    fib_data = nullptr;
                    cleanup();
                    throw std::runtime_error("Failed to map fib file to memory");
                }

                next_hop_num_data = (uint8_t*)mmap(nullptr, next_hop_size, PROT_READ | PROT_WRITE,
                        MAP_SHARED, next_hop_fd, 0);
                if (next_hop_num_data == MAP_FAILED) {
                    next_hop_num_data = nullptr;
                    cleanup();
                    throw std::runtime_error("Failed to map next_hop file to memory");
                }

                // Initialize default values
                memset(fib_data, 255, fib_size);  // 255 is the invalid value for uint8_t
                memset(next_hop_num_data, 255, next_hop_size);  // 255 is the invalid value for uint8_t
            }

            // Destructor - cleanup resources
            ~FibProxy() {
                cleanup();
            }

            // Disable copying
            FibProxy(const FibProxy&) = delete;
            FibProxy& operator=(const FibProxy&) = delete;

            // Helper function - cleanup resources
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

            // Calculate 3D array index
            size_t calculate_fib_index(int32_t i, int32_t j, int32_t k) const {
                return (size_t)i * (MAX_SW_NUM + MAX_NET_NPU_NUM) * MAX_ONE_SW_PORT_NUM +
                    (size_t)j * MAX_ONE_SW_PORT_NUM + k;
            }

            // Calculate 2D array index
            size_t calculate_next_hop_index(int32_t i, int32_t j) const {
                return (size_t)i * (MAX_SW_NUM + MAX_NET_NPU_NUM) + j;
            }

            // Set single element value
            void set_entry(int32_t i, int32_t j, int32_t k, uint8_t val) {
                if (i >= 0 && i < MAX_SW_NUM + MAX_NET_NPU_NUM &&
                        j >= 0 && j < MAX_SW_NUM + MAX_NET_NPU_NUM &&
                        k >= 0 && k < MAX_ONE_SW_PORT_NUM) {
                    size_t idx = calculate_fib_index(i, j, k);
                    fib_data[idx] = val;
                }
            }

            // Get single element value
            uint8_t get_entry(int32_t i, int32_t j, int32_t k) const {
                if (i < 0 || i >= MAX_SW_NUM + MAX_NET_NPU_NUM ||
                        j < 0 || j >= MAX_SW_NUM + MAX_NET_NPU_NUM ||
                        k < 0 || k >= MAX_ONE_SW_PORT_NUM || fib_data == nullptr) {
                    return 255;  // Return 255 as invalid value for uint8_t
                }
                size_t idx = calculate_fib_index(i, j, k);
                return fib_data[idx];
            }

            // Set next_hop_num value
            void set_next_hop_num(int32_t i, int32_t j, uint8_t val) {
                if (i >= 0 && i < MAX_SW_NUM + MAX_NET_NPU_NUM &&
                        j >= 0 && j < MAX_SW_NUM + MAX_NET_NPU_NUM) {
                    size_t idx = calculate_next_hop_index(i, j);
                    next_hop_num_data[idx] = val;
                }
            }

            // Get next_hop_num value
            uint8_t get_next_hop_num(int32_t i, int32_t j) const {
                if (i >= 0 && i < MAX_SW_NUM + MAX_NET_NPU_NUM &&
                        j >= 0 && j < MAX_SW_NUM + MAX_NET_NPU_NUM) {
                    size_t idx = calculate_next_hop_index(i, j);
                    return next_hop_num_data[idx];
                }
                return 255;  // Return 255 as invalid value for uint8_t
            }

            // Convert to standard Fib struct (for passing to C++ core)
            Fib* toFib() const {
                Fib* result = new Fib();

                // Copy element by element (can be optimized to block copy)
                for (int32_t i = 0; i < MAX_SW_NUM + MAX_NET_NPU_NUM; i++) {
                    for (int32_t j = 0; j < MAX_SW_NUM + MAX_NET_NPU_NUM; j++) {
                        // Copy next_hop_num
                        size_t next_hop_idx = calculate_next_hop_index(i, j);
                        result->next_hop_num[i][j] = next_hop_num_data[next_hop_idx];

                        for (int32_t k = 0; k < MAX_ONE_SW_PORT_NUM; k++) {
                            // Copy fib
                            size_t fib_idx = calculate_fib_index(i, j, k);
                            result->fib[i][j][k] = fib_data[fib_idx];
                        }
                    }
                }
                return result;
            }

            // Set fib data from numpy array
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
                if (arr.dtype() != nb::dtype<uint8_t>()) {
                    throw std::invalid_argument("Array must have dtype uint8_t");
                }

                // Copy data element by element (slower but safer)
                uint8_t* data = static_cast<uint8_t*>(arr.data());
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

            // Set next_hop_num data from numpy array
            void set_next_hop_num_from_array(nb::ndarray<> arr) {
                if (arr.ndim() != 2 ||
                        arr.shape(0) > MAX_SW_NUM + MAX_NET_NPU_NUM ||
                        arr.shape(1) > MAX_SW_NUM + MAX_NET_NPU_NUM) {
                    throw std::invalid_argument("Invalid next_hop_num array dimensions");
                }

                uint8_t* data = static_cast<uint8_t*>(arr.data());
                for (size_t i = 0; i < arr.shape(0); i++) {
                    for (size_t j = 0; j < arr.shape(1); j++) {
                        size_t src_idx = i * arr.shape(1) + j;
                        size_t dst_idx = calculate_next_hop_index(i, j);
                        next_hop_num_data[dst_idx] = data[src_idx];
                    }
                }
            }

            // Get numpy view of fib data (no data copy)
            nb::ndarray<nb::numpy, uint8_t> get_fib_view() {
                if (fib_data == nullptr) {
                    throw std::runtime_error("Memory mapping is not initialized");
                }

                try {
                    return nb::ndarray<nb::numpy, uint8_t>(
                            fib_data,
                            {MAX_SW_NUM + MAX_NET_NPU_NUM, MAX_SW_NUM + MAX_NET_NPU_NUM, MAX_ONE_SW_PORT_NUM},
                            nb::handle()
                            );
                } catch (const std::exception& e) {
                    throw std::runtime_error(std::string("Failed to create numpy view: ") + e.what());
                }
            }

            // Get numpy view of next_hop_num data (no data copy)
            nb::ndarray<nb::numpy, uint8_t> get_next_hop_num_view() {
                return nb::ndarray<nb::numpy, uint8_t>(
                        next_hop_num_data,
                        {MAX_SW_NUM + MAX_NET_NPU_NUM, MAX_SW_NUM + MAX_NET_NPU_NUM},
                        nb::handle()
                        );
            }

            // Synchronize memory changes to disk
            void sync() {
                if (fib_data) msync(fib_data, fib_size, MS_SYNC);
                if (next_hop_num_data) msync(next_hop_num_data, next_hop_size, MS_SYNC);
            }
};
