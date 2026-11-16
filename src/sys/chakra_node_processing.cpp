#include "chakra_node_processing.hpp"

using namespace madrona;
using namespace madrona::math;
using namespace madrona::phys;

namespace madEscape::llm_system
{


    // Helper function to parse attribute values based on type
    void parseAttributeValue(AttributeDataType type, uint32_t data1, uint32_t data2, uint64_t &result)
    {
        switch (type)
        {
        case AttributeDataType::BOOL_VAL:
            result = (uint64_t)data1; // bool stored in data1
            break;
        case AttributeDataType::UINT32_VAL:
            result = (uint64_t)data1; // uint32 stored in data1
            break;
        case AttributeDataType::INT64_VAL:
            // Reconstruct int64 from two int32 values (simple version)
            {
                // 直接进行位操作，C++的类型转换会处理符号扩展
                uint64_t low_32 = (uint32_t)data1;  // 强制转换为无符号避免符号扩展
                uint64_t high_32 = (uint32_t)data2; // 强制转换为无符号避免符号扩展

                // 重构64位值并直接转换为有符号int64
                result = (int64_t)((high_32 << 32) | low_32);
            }
            break;
        case AttributeDataType::UINT64_VAL:
            // Reconstruct uint64 from two int32 values (simple version)
            {
                // 直接进行位操作重构无符号64位值
                uint64_t low_32 = (uint32_t)data1;  // 强制转换为无符号避免符号扩展
                uint64_t high_32 = (uint32_t)data2; // 强制转换为无符号避免符号扩展

                // 重构64位值并保持为无符号
                result = (high_32 << 32) | low_32;
            }
            break;
        default:
            result = 0;
            break;
        }
    }

    // Parse integer array into ChakraNodes array
    void parseChakraNodes(const ChakraNodesData &chakraNodesData, int row, ChakraNode parsedNodes[], int &outNodeCount, int intArrayLength, int maxNodes)
    {
       

        outNodeCount = 0; // Initialize output parameter
        int validNodes = 0; // Valid node count

        for (int i = 0; i < maxNodes; ++i)
        {
            int baseIndex = i * INTS_PER_NODE;
            if (baseIndex + INTS_PER_NODE > intArrayLength)
                break; // Ensure no out-of-bounds access

            // Parse single node
            for (int j = 0; j < NODE_NAME_LENGTH; ++j)
            {
                parsedNodes[validNodes].name[j] = chakraNodesData.data[row][baseIndex + j];
            }
            baseIndex += NODE_NAME_LENGTH;

            parsedNodes[validNodes].type = (NodeType)chakraNodesData.data[row][baseIndex];
            baseIndex += 1;

            parsedNodes[validNodes].id = chakraNodesData.data[row][baseIndex];

            // Initial node has no id
            if (parsedNodes[validNodes].id == INVALID_FLOW_ID)
            {
                parsedNodes[validNodes].id = 0;
            }

            baseIndex += 1;

            for (int j = 0; j < NODE_DATA_DEPS_LENGTH; ++j)
            {
                parsedNodes[validNodes].data_deps[j] = chakraNodesData.data[row][baseIndex + j];
            }
            baseIndex += NODE_DATA_DEPS_LENGTH;

            // Parse attributes using new 3-int format: [data1, data2, type_id]
            // Initialize all attributes to default values
            parsedNodes[validNodes].comm_para = 0;
            parsedNodes[validNodes].comm_size = 0;
            parsedNodes[validNodes].comm_src = 0;
            parsedNodes[validNodes].comm_dst = 0;
            parsedNodes[validNodes].comm_type = (CollectiveCommType)0;
            parsedNodes[validNodes].involved_dim_1 = false;
            parsedNodes[validNodes].involved_dim_2 = false;
            parsedNodes[validNodes].involved_dim_3 = false;

            // Parse 6 attributes, each occupying 3 ints
            for (int attr_idx = 0; attr_idx < 6; ++attr_idx)
            {
                uint32_t data1 = chakraNodesData.data[row][baseIndex];
                uint32_t data2 = chakraNodesData.data[row][baseIndex + 1];
                uint32_t type_id = chakraNodesData.data[row][baseIndex + 2];
                baseIndex += NODE_ATTR_PER_LENGTH;

                // Skip if no valid data
                if (type_id == INVALID_DEPENDENCY)
                    continue;

                AttributeDataType attr_type = (AttributeDataType)type_id;

                switch (attr_idx)
                {
                case 0: // comm_para
                    parseAttributeValue(attr_type, data1, data2, parsedNodes[validNodes].comm_para);
                    break;
                case 1: // comm_size
                    parseAttributeValue(attr_type, data1, data2, parsedNodes[validNodes].comm_size);
                    break;
                case 2: // comm_src
                    parseAttributeValue(attr_type, data1, data2, parsedNodes[validNodes].comm_src);
                    break;
                case 3: // comm_dst
                    parseAttributeValue(attr_type, data1, data2, parsedNodes[validNodes].comm_dst);
                    break;
                case 4: // comm_type
                {
                    uint64_t comm_type_val = 0;
                    parseAttributeValue(attr_type, data1, data2, comm_type_val);
                    parsedNodes[validNodes].comm_type = (CollectiveCommType)comm_type_val;
                }
                break;
                case 5: // involved_dim (boolList) - 3 bools packed in data1
                    if (attr_type == AttributeDataType::BOOL_LIST)
                    {
                        uint32_t packed_bools = data1;
                        parsedNodes[validNodes].involved_dim_1 = (packed_bools & (1 << 0)) != 0;
                        parsedNodes[validNodes].involved_dim_2 = (packed_bools & (1 << 1)) != 0;
                        parsedNodes[validNodes].involved_dim_3 = (packed_bools & (1 << 2)) != 0;
                    }
                    break;
                }
            }

            parsedNodes[validNodes].durationMicros = chakraNodesData.data[row][baseIndex];
            baseIndex += 1;

            // Check if all zeros (indicating padding area)
            bool isPadding = true;
            for (int j = i * INTS_PER_NODE; j < i * INTS_PER_NODE + INTS_PER_NODE; ++j)
            {
                if (chakraNodesData.data[row][j] != 0)
                {
                    isPadding = false;
                    break;
                }
            }
            if (isPadding)
            {
                break; // Ignore subsequent padding areas
            }

            // Increment validNodes after completing a node parse
            validNodes++;
        }

        // if (validNodes >= 2)
        // {
        //     // 打印parsedNodes[0],parsedNodes[1]的全部字段值
        //     parsedNodes[0].print();
        //     parsedNodes[1].print();
        // }
        
        // Set output parameter
        outNodeCount = validNodes;
    }

    int filterNoDependencyNodes(const ChakraNodes &chakraNodes, ChakraNode filteredNodes[],bool &is_none_node,int maxNodes)
    {
        int count = 0; // No-dependency node count

        is_none_node = true;
        // Traverse all nodes
        for (size_t i = 0; i < MAX_CHAKRA_NODES_PER_NPU; ++i)
        {
            const ChakraNode &node = chakraNodes.nodes[i];

            // Skip invalid nodes
            if (node.type == NodeType::None)
            {
                continue;
            }
            is_none_node = false;

            // Check if there are no dependencies
            bool hasDependency = false;
            for (int j = 0; j < NODE_DATA_DEPS_LENGTH; ++j)
            {
                if (node.data_deps[j] != INVALID_DEPENDENCY)
                {
                    hasDependency = true;
                    break;
                }
            }

            // If no dependencies, add to result array
            if (!hasDependency)
            {
                if (count < maxNodes)
                {
                    filteredNodes[count++] = node;
                }
                else
                {
                    // Exceeded maximum limit, stop adding
                    break;
                }
            }
        }

        return count; // Return the number of no-dependency nodes
    }

    void removeNode(ChakraNodes &chakraNodes, uint32_t nodeId)
    {
        // Traverse all nodes, find target node and mark it as invalid
        for (size_t i = 0; i < MAX_CHAKRA_NODES_PER_NPU; ++i)
        {
            ChakraNode &node = chakraNodes.nodes[i];

            // If target node is found, mark it as invalid
            if (node.id == nodeId)
            {
                node.type = NodeType::None; // Mark as invalid node
                for (int j = 0; j < NODE_DATA_DEPS_LENGTH; ++j)
                {
                    node.data_deps[j] = INVALID_DEPENDENCY; // Clear dependencies
                }
                node.id = INVALID_DEPENDENCY; // Set node ID to invalid
                break;
            }
        }

        // Traverse all nodes, remove dependencies on target node
        for (size_t i = 0; i < MAX_CHAKRA_NODES_PER_NPU; ++i)
        {
            ChakraNode &node = chakraNodes.nodes[i];

            // Skip invalid nodes
            if (node.type == NodeType::None)
            {
                continue;
            }

            // Check and remove dependencies on target node
            for (int j = 0; j < NODE_DATA_DEPS_LENGTH; ++j)
            {
                if (node.data_deps[j] == nodeId)
                {
                    node.data_deps[j] = INVALID_DEPENDENCY; // Set to invalid dependency
                }
            }
        }
    }

    const char *CollectiveCommTypeToString(CollectiveCommType type)
    {
        switch (type)
        {
        case CollectiveCommType::ALL_REDUCE:
            return "ALL_REDUCE";
        case CollectiveCommType::REDUCE:
            return "REDUCE";
        case CollectiveCommType::ALL_GATHER:
            return "ALL_GATHER";
        case CollectiveCommType::GATHER:
            return "GATHER";
        case CollectiveCommType::SCATTER:
            return "SCATTER";
        case CollectiveCommType::BROADCAST:
            return "BROADCAST";
        case CollectiveCommType::ALL_TO_ALL:
            return "ALL_TO_ALL";
        case CollectiveCommType::REDUCE_SCATTER:
            return "REDUCE_SCATTER";
        case CollectiveCommType::REDUCE_SCATTER_BLOCK:
            return "REDUCE_SCATTER_BLOCK";
        case CollectiveCommType::BARRIER:
            return "BARRIER";
        default:
            return "UNKNOWN";
        }
    }

    const char *DimensionToString(Dimension type)
    {
        switch (type)
        {
        case Dimension::Local:
            return "Local";
        case Dimension::Horizontal:
            return "Horizontal";
        case Dimension::Vertical:
            return "Vertical";
        default:
            return "UNKNOWN";
        }
    }

    // 检查RECV类型节点的依赖关系
    int checkRecvNodeDependencies(const ChakraNodes &chakraNodes, uint32_t independentRecvNodes[], int maxNodes)
    {
        // 第一步：收集所有RECV节点的ID
        uint32_t recvNodeIds[MAX_CHAKRA_NODES_PER_NPU];
        int recvNodeCount = 0;
        
        for (size_t i = 0; i < MAX_CHAKRA_NODES_PER_NPU; ++i)
        {
            const ChakraNode &node = chakraNodes.nodes[i];
            
            // 跳过无效节点
            if (node.type == NodeType::None)
            {
                continue;
            }
            
            // 收集所有RECV节点的ID
            if (node.type == NodeType::COMM_RECV_NODE)
            {
                if (recvNodeCount < MAX_CHAKRA_NODES_PER_NPU)
                {
                    recvNodeIds[recvNodeCount++] = node.id;
                }
            }
        }
        
        printf("找到 %d 个RECV类型节点\n", recvNodeCount);
        
        // 第二步：检查每个RECV节点的依赖关系
        int independentCount = 0; // 不依赖其他RECV节点的RECV节点数量
        
        for (size_t i = 0; i < MAX_CHAKRA_NODES_PER_NPU; ++i)
        {
            const ChakraNode &node = chakraNodes.nodes[i];
            
            // 跳过无效节点
            if (node.type == NodeType::None)
            {
                continue;
            }
            
            // 只检查RECV节点
            if (node.type == NodeType::COMM_RECV_NODE)
            {
                bool dependsOnRecv = false;
                
                // 检查该RECV节点的所有依赖
                for (int j = 0; j < 10; ++j)
                {
                    if (node.data_deps[j] != INVALID_DEPENDENCY)
                    {
                        // 检查依赖是否是其他RECV节点的ID
                        for (int k = 0; k < recvNodeCount; ++k)
                        {
                            if (node.data_deps[j] == recvNodeIds[k] && node.data_deps[j] != node.id)
                            {
                                dependsOnRecv = true;
                                break;
                            }
                        }
                        if (dependsOnRecv)
                        {
                            break;
                        }
                    }
                }
                
                // 如果该RECV节点不依赖其他RECV节点
                if (!dependsOnRecv)
                {
                    if (independentCount < maxNodes)
                    {
                        independentRecvNodes[independentCount] = node.id;
                        printf("发现不依赖其他RECV节点的RECV节点，ID: %u (序号: %zu)\n", node.id, i);
                    }
                    independentCount++;
                }
            }
        }
        
        // 输出结果
        if (independentCount == 0)
        {
            printf("错误：没有找到不依赖其他RECV节点的RECV节点\n");
        }
        else if (independentCount == 1)
        {
            printf("正确：只有一个不依赖其他RECV节点的RECV节点，ID: %u\n", independentRecvNodes[0]);
        }
        else
        {
            printf("错误：发现 %d 个不依赖其他RECV节点的RECV节点：\n", independentCount);
            for (int i = 0; i < independentCount && i < maxNodes; ++i)
            {
                printf("  - 节点ID: %u\n", independentRecvNodes[i]);
            }
        }
        
        return independentCount;
    }


}