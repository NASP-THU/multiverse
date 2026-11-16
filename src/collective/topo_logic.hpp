#pragma once

#include <madrona/components.hpp>
#include <madrona/math.hpp>
#include <madrona/rand.hpp>
#include <madrona/physics.hpp>
#include <madrona/render/ecs.hpp>
#include "../sim.hpp"
#include "../consts.hpp"
#include "../types.hpp"

#define NODES_MAXNUM_IN_RING 1000

namespace madEscape
{
    // RingTopology component - stores ring topology configuration for a dimension
    // One instance per dimension (Local, Horizontal, Vertical)
    struct RingTopology
    {
        Dimension dimension;
        int total_nodes_in_ring;
        int offset;

        // Default constructor for ECS component
        RingTopology()
            : dimension(Dimension::NA), 
              total_nodes_in_ring(0), 
              offset(1)
        {
        }

        // Initialize method - only needs dimension-level parameters
        // No need for specific node_id or index, as they can be calculated
        void initialize(Dimension dim, int total_nodes, int param_offset)
        {
            dimension = dim;
            total_nodes_in_ring = total_nodes;
            offset = param_offset;
        }

        // Calculate index from node_id using the standard formula
        inline int get_index_from_node_id(int node_id) const
        {
            return (node_id % (offset * total_nodes_in_ring)) / offset;
        }

        // Calculate receiver node_id based on direction
        int get_receiver_homogeneous(int node_id, Direction direction, int param_offset) const
        {
            int index = get_index_from_node_id(node_id);
            int base_id = node_id - (index * param_offset);

            if (direction == Direction::Clockwise)
            {
                if (index == total_nodes_in_ring - 1)
                {
                    // Wrap around to the first node in the ring
                    return base_id;
                }
                else
                {
                    return node_id + param_offset;
                }
            }
            else  // Anticlockwise
            {
                if (index == 0)
                {
                    // Wrap around to the last node in the ring
                    return base_id + (total_nodes_in_ring - 1) * param_offset;
                }
                else
                {
                    return node_id - param_offset;
                }
            }
        }
        // Get receiver node_id based on direction
        int get_receiver(int node_id, Direction direction) const
        {
            return get_receiver_homogeneous(node_id, direction, offset);
        }

        // Get sender node_id based on direction
        // Sender direction is opposite to receiver direction
        int get_sender(int node_id, Direction direction) const
        {
            // Sender in one direction = Receiver in opposite direction
            Direction opposite_dir = (direction == Direction::Clockwise) 
                                     ? Direction::Anticlockwise 
                                     : Direction::Clockwise;
            return get_receiver_homogeneous(node_id, opposite_dir, offset);
        }

        // Print all node_ids in the ring that contains the given node
        // Uses get_index_from_node_id, get_sender, get_receiver to traverse (for testing purposes)
        void print_ring_nodes(int node_id) const
        {
            const char* dim_name = (dimension == Dimension::Local) ? "Local" :
                                   (dimension == Dimension::Horizontal) ? "Horizontal" :
                                   (dimension == Dimension::Vertical) ? "Vertical" : "NA";

            printf("[TEST DIM] Ring Topology [Dimension: %s, Total Nodes: %d, Current node: %d, Offset: %d]\n",
                   dim_name, total_nodes_in_ring, node_id, offset);
            printf("[TEST DIM] Node IDs in ring: ");

            // Find the start of the ring (node with index=0) using get_sender
            int start = node_id;
            while (get_index_from_node_id(start) != 0)
            {
                start = get_sender(start, Direction::Clockwise);
            }

            // Traverse the ring from the start using get_receiver
            int current = start;
            for (int i = 0; i < total_nodes_in_ring; i++)
            {
                printf("%d ", current);
                current = get_receiver(current, Direction::Clockwise);
            }
            printf("\n");
        }
    };

    // RingTopoEntity Archetype - ECS entity to store RingTopology component
    // Three instances will be created globally for Local, Horizontal, and Vertical dimensions
    struct RingTopoEntity : public madrona::Archetype<RingTopology>
    {
    };

}; // namespace madEscape