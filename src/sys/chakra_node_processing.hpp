#pragma once

#include "../sim.hpp"
#include "../types.hpp"

namespace madEscape::llm_system
{
    // Import madrona types into current namespace
    using madrona::ECSRegistry;

    // Chakra node parsing and processing functions
    void parseAttributeValue(AttributeDataType type, uint32_t data1, uint32_t data2, uint64_t &result);

    void parseChakraNodes(const ChakraNodesData &chakraNodesData, int row, ChakraNode parsedNodes[],
                          int &outNodeCount, int intArrayLength = CHAKRA_NODES_DATA_LENGTH, int maxNodes = MAX_CHAKRA_NODES_PER_NPU);

    int filterNoDependencyNodes(const ChakraNodes &chakraNodes, ChakraNode filteredNodes[],bool &is_none_node,
                                int maxNodes = CURRENT_EXEC_NODES_MAX);
    
    int checkRecvNodeDependencies(const ChakraNodes &chakraNodes, uint32_t independentRecvNodes[], 
        int maxNodes = CURRENT_EXEC_NODES_MAX);

    void removeNode(ChakraNodes &chakraNodes, uint32_t nodeId);

    // Utility functions for node processing
    const char *CollectiveCommTypeToString(CollectiveCommType type);
    const char *DimensionToString(Dimension type);
}