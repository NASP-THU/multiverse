#include <cstdio>  // for printf
#include <cstdlib> // for atoi
#include <cstring> // for strtok
#include <fstream> // for std::ifstream
#include <sstream> // for std::stringstream
#include <vector>  // for std::vector

const int k_ary = 4;
const int num_switch = 20; //k_ary*k_ary*5/4;
const int num_host = 16; //k_ary*k_ary*k_ary/4;
const char *path_file = "ecmp_paths_4.txt";

// const int k_ary = 8;
// const int num_switch = 80; //k_ary*k_ary*5/4;
// const int num_host = 128; //k_ary*k_ary*k_ary/4;
// const char *path_file = "ecmp_paths_8.txt";

int fib_table[num_switch][num_host][k_ary] = {{{-1}}};

// Check if an element is in an array
bool in_array(int *array, int size, int element) {
    for (int i = 0; i < size; i++) {
        if (array[i] == element) {
            return true;
        }
    }
    return false;
}

void generate_route_table(int num_receiver, int k_ary)
{

    for (int i = 0; i < num_switch; ++i) {
        for (int j = 0; j < num_host; ++j) {
            for (int k = 0; k < k_ary; ++k) {
                fib_table[i][j][k] = -1;
            }
        }
    }

    printf("*******Generate_fib_table*******\n");
    int num_edge_switch = k_ary*k_ary/2;
    int num_aggr_switch = k_ary*k_ary/2; // the number of aggragate switch
    int num_core_switch = k_ary*k_ary/4;

    // Read the paths from the file
    std::ifstream file(path_file);
    if (!file) {
        printf("Failed to open the file.\n");
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::vector<int> path;
        std::stringstream ss(line);
        int value;
        while (ss >> value) {
            path.push_back(value);
        }
        if (path.size() <= 3) { // dst node locate in the same rack with the current switch 
            int switch_id = path[2]-num_receiver;
            int dst_host_id = path[1];
            int next_hop_id = switch_id; //same
            
            int *next_hops = fib_table[switch_id][dst_host_id];
            if (!in_array(next_hops, k_ary, next_hop_id)) {
                // Find the first -1 in the array and fill it with the next hop id
                for (int j = 0; j < k_ary; j++) {
                    if (next_hops[j] == -1) {
                        next_hops[j] = next_hop_id;
                        break;
                    }
                }
            }
        }
        else {
            for (size_t i = 2; i < path.size()-1; i++) { // Skip the first and last nodes (hosts)
                int switch_id = path[i]-num_receiver;
                int dst_host_id = path[1];
                int next_hop_id = path[i + 1]; //-num_receiver;

                int *next_hops = fib_table[switch_id][dst_host_id];
                if (!in_array(next_hops, k_ary, next_hop_id)) {
                    // Find the first -1 in the array and fill it with the next hop id
                    for (int j = 0; j < k_ary; j++) {
                        if (next_hops[j] == -1) {
                            next_hops[j] = next_hop_id;
                            break;
                        }
                    }
                }
            }
        }
        
        // for (int i = 0; i < num_edge_switch + num_aggr_switch + num_core_switch; i++) {
        //     printf("Switch %d:\n", i);
        //     for (int j = 0; j < num_receiver; j++) {
        //         printf("  Destination host %d: ", j);
        //         int *next_hops = fib_table[i][j];
        //         for (int k = 0; k < k_ary; k++) {
        //             if (next_hops[k] != -1) {
        //                 printf("%d ", next_hops[k]);
        //             } else {
        //                 break;
        //             }
        //         }
        //         printf("\n");
        //     }
        // }
    }

    // After generating the FIB tables
    for (int i = 0; i < num_edge_switch + num_aggr_switch + num_core_switch; i++) {
        printf("Switch %d:\n", i+num_receiver);
        for (int j = 0; j < num_receiver; j++) {
            printf("  Destination host %d: ", j);
            int *next_hops = fib_table[i][j];
            for (int k = 0; k < k_ary; k++) {
                if (next_hops[k] != -1) {
                    printf("%d ", next_hops[k]);
                } else {
                    break;
                }
            }
            printf("\n");
        }
    }
}

int main() {
    generate_route_table(k_ary*k_ary*k_ary/4, k_ary);

    return 0;
}
