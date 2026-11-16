#!/usr/bin/env python3
"""
Topology Generator for Network Simulation

This script generates network topology files in the required format:
Line 1: {num_nodes} {gpu_num_per_host} {num_nvs_switches} {num_sws} {len(links)} {gpu_type} {host_num}
Line 2: Switch node IDs (space-separated)
Line 3+: Links in format: node_A node_Z bandwidth latency loss_rate weight link_type

Node ID ordering:
- GPUs: 0 to (total_gpus - 1)
- NVSwitches: total_gpus to (total_gpus + total_nvswitches - 1)
- Network Switches: after NVSwitches

Link types:
- NVL: NVLink (intra-machine)
- PCIe: PCIe (intra-machine)
- ETH: Ethernet (inter-machine)
- IB: InfiniBand (inter-machine)

Note: Each link represents bidirectional connections (A->Z and Z->A).
"""

import argparse
from dataclasses import dataclass, field
from typing import List, Tuple, Optional
from enum import Enum


class LinkType(Enum):
    NVL = "NVL"    # NVLink (intra-machine)
    PCIe = "PCIe"  # PCIe (intra-machine)
    ETH = "ETH"    # Ethernet (inter-machine)
    IB = "IB"      # InfiniBand (inter-machine)


class TopoType(Enum):
    RAIL = "rail"              # Rail topology (hosts connect to switches in round-robin)
    RAIL_CHAIN = "rail_chain"  # Rail with chain-connected switches
    FATTREE = "fattree"        # Fat-tree topology
    FULL_MESH = "full_mesh"    # Full mesh among switches
    RING = "ring"              # Ring topology among switches
    SINGLE_SWITCH = "single"   # Single switch connecting all hosts


@dataclass
class Link:
    """Represents a network link between two nodes."""
    node_a: int
    node_z: int
    bandwidth: str = "100Gbps"
    latency: str = "1000"      # in nanoseconds or with unit
    loss_rate: str = "0.000"
    weight: int = 100
    link_type: LinkType = LinkType.ETH

    def to_string(self) -> str:
        return f"{self.node_a}\t{self.node_z}\t{self.bandwidth}\t{self.latency}\t{self.loss_rate}\t{self.weight}\t{self.link_type.value}"


@dataclass
class Topology:
    """Represents a complete network topology."""
    host_num: int                   # Number of host nodes (or total GPUs in hybrid mode)
    num_switches: int               # Total number of switches (NVSwitches + network switches)
    gpu_num_per_host: int = 0       # GPUs per host (for intra-machine topology)
    num_nvs_switches: int = 0       # Total number of NVSwitches (intra-machine)
    gpu_type: str = "H100"          # GPU type
    links: List[Link] = field(default_factory=list)
    switch_ids: List[int] = field(default_factory=list)

    @property
    def num_nodes(self) -> int:
        """Total number of nodes (hosts/GPUs + switches)."""
        return self.host_num + self.num_switches

    def add_link(self, node_a: int, node_z: int, bandwidth: str = "100Gbps",
                 latency: str = "1000", loss_rate: str = "0.000",
                 weight: int = 100, link_type: LinkType = LinkType.ETH):
        """Add a link to the topology."""
        self.links.append(Link(node_a, node_z, bandwidth, latency, loss_rate, weight, link_type))

    def generate_header(self) -> str:
        """Generate the first line of the topology file."""
        return f"{self.num_nodes}\t{self.gpu_num_per_host}\t{self.num_nvs_switches}\t{self.num_switches}\t{len(self.links)}\t{self.gpu_type}\t{self.host_num}"

    def generate_switch_line(self) -> str:
        """Generate the second line containing switch IDs."""
        return "\t".join(str(sw_id) for sw_id in self.switch_ids)

    def to_file(self, filepath: str):
        """Write the topology to a file."""
        with open(filepath, 'w') as f:
            f.write(self.generate_header() + "\n")
            f.write(self.generate_switch_line() + "\n")
            for link in self.links:
                f.write(link.to_string() + "\n")

    def __str__(self) -> str:
        lines = [self.generate_header(), self.generate_switch_line()]
        lines.extend(link.to_string() for link in self.links)
        return "\n".join(lines)


def generate_rail_topology(
    host_num: int,
    num_switches: int,
    bandwidth: str = "100Gbps",
    latency: str = "1000",
    loss_rate: str = "0.000",
    weight: int = 100,
    link_type: LinkType = LinkType.ETH,
    gpu_type: str = "H00",
    add_switch_chain: bool = False
) -> Topology:
    """
    Generate a rail topology.
    
    In a rail topology, hosts are distributed among switches in a round-robin fashion.
    Each host connects to exactly one switch based on: switch_id = host_id % num_switches
    
    Args:
        host_num: Number of host nodes
        num_switches: Number of switches
        bandwidth: Link bandwidth (e.g., "100Gbps", "400Gbps")
        latency: Link latency (e.g., "1000", "5000ns")
        loss_rate: Packet loss rate (e.g., "0.000")
        weight: Link weight for routing
        link_type: Type of link (ETH, IB, etc.)
        gpu_type: GPU type string
        add_switch_chain: If True, add chain connections between switches
    
    Returns:
        Topology object
    """
    topo = Topology(
        host_num=host_num,
        num_switches=num_switches,
        gpu_type=gpu_type
    )
    
    # Switch IDs start after host IDs
    first_switch_id = host_num
    topo.switch_ids = list(range(first_switch_id, first_switch_id + num_switches))
    
    # Connect each host to its corresponding switch (round-robin)
    for host_id in range(host_num):
        switch_idx = host_id % num_switches
        switch_id = topo.switch_ids[switch_idx]
        topo.add_link(host_id, switch_id, bandwidth, latency, loss_rate, weight, link_type)
    
    # Optionally add chain connections between switches
    if add_switch_chain:
        for i in range(num_switches - 1):
            topo.add_link(
                topo.switch_ids[i], 
                topo.switch_ids[i + 1], 
                bandwidth, latency, loss_rate, weight, link_type
            )
    
    return topo


def generate_single_switch_topology(
    host_num: int,
    bandwidth: str = "400Gbps",
    latency: str = "5000ns",
    loss_rate: str = "0",
    weight: int = 100,
    link_type: LinkType = LinkType.ETH,
    gpu_type: str = "H100"
) -> Topology:
    """
    Generate a single-switch topology where all hosts connect to one switch.
    
    Args:
        host_num: Number of host nodes
        bandwidth: Link bandwidth
        latency: Link latency
        loss_rate: Packet loss rate
        weight: Link weight
        link_type: Link type
        gpu_type: GPU type
    
    Returns:
        Topology object
    """
    topo = Topology(
        host_num=host_num,
        num_switches=1,
        gpu_type=gpu_type
    )
    
    switch_id = host_num  # Switch ID is right after hosts
    topo.switch_ids = [switch_id]
    
    # Connect each host to the single switch
    for host_id in range(host_num):
        topo.add_link(switch_id, host_id, bandwidth, latency, loss_rate, weight, link_type)
    
    return topo


def generate_ring_topology(
    host_num: int,
    num_switches: int,
    host_bandwidth: str = "100Gbps",
    switch_bandwidth: str = "100Gbps",
    latency: str = "1000",
    loss_rate: str = "0.000",
    weight: int = 100,
    link_type: LinkType = LinkType.ETH,
    gpu_type: str = "H100"
) -> Topology:
    """
    Generate a ring topology with hosts connected to switches and switches forming a ring.
    
    Args:
        host_num: Number of host nodes
        num_switches: Number of switches
        host_bandwidth: Bandwidth for host-to-switch links
        switch_bandwidth: Bandwidth for switch-to-switch links
        latency: Link latency
        loss_rate: Packet loss rate
        weight: Link weight
        link_type: Link type
        gpu_type: GPU type
    
    Returns:
        Topology object
    """
    topo = Topology(
        host_num=host_num,
        num_switches=num_switches,
        gpu_type=gpu_type
    )
    
    first_switch_id = host_num
    topo.switch_ids = list(range(first_switch_id, first_switch_id + num_switches))
    
    # Connect hosts to switches (round-robin)
    for host_id in range(host_num):
        switch_idx = host_id % num_switches
        switch_id = topo.switch_ids[switch_idx]
        topo.add_link(host_id, switch_id, host_bandwidth, latency, loss_rate, weight, link_type)
    
    # Create ring among switches
    for i in range(num_switches):
        next_i = (i + 1) % num_switches
        topo.add_link(
            topo.switch_ids[i],
            topo.switch_ids[next_i],
            switch_bandwidth, latency, loss_rate, weight, link_type
        )
    
    return topo


def generate_full_mesh_topology(
    host_num: int,
    num_switches: int,
    host_bandwidth: str = "100Gbps",
    switch_bandwidth: str = "100Gbps",
    latency: str = "1000",
    loss_rate: str = "0.000",
    weight: int = 100,
    link_type: LinkType = LinkType.ETH,
    gpu_type: str = "H100"
) -> Topology:
    """
    Generate a full mesh topology with hosts connected to switches and switches fully connected.
    
    Args:
        host_num: Number of host nodes
        num_switches: Number of switches
        host_bandwidth: Bandwidth for host-to-switch links
        switch_bandwidth: Bandwidth for switch-to-switch links
        latency: Link latency
        loss_rate: Packet loss rate
        weight: Link weight
        link_type: Link type
        gpu_type: GPU type
    
    Returns:
        Topology object
    """
    topo = Topology(
        host_num=host_num,
        num_switches=num_switches,
        gpu_type=gpu_type
    )
    
    first_switch_id = host_num
    topo.switch_ids = list(range(first_switch_id, first_switch_id + num_switches))
    
    # Connect hosts to switches (round-robin)
    for host_id in range(host_num):
        switch_idx = host_id % num_switches
        switch_id = topo.switch_ids[switch_idx]
        topo.add_link(host_id, switch_id, host_bandwidth, latency, loss_rate, weight, link_type)
    
    # Create full mesh among switches
    for i in range(num_switches):
        for j in range(i + 1, num_switches):
            topo.add_link(
                topo.switch_ids[i],
                topo.switch_ids[j],
                switch_bandwidth, latency, loss_rate, weight, link_type
            )
    
    return topo


def generate_fattree_topology(
    k: int,  # Fat-tree radix (number of ports per switch)
    host_bandwidth: str = "100Gbps",
    switch_bandwidth: str = "100Gbps",
    latency: str = "1000",
    loss_rate: str = "0.000",
    weight: int = 100,
    link_type: LinkType = LinkType.ETH,
    gpu_type: str = "H100"
) -> Topology:
    """
    Generate a fat-tree topology.
    
    A k-ary fat-tree has:
    - k pods
    - Each pod has k/2 edge switches and k/2 aggregation switches
    - (k/2)^2 core switches
    - k^3/4 hosts
    
    Args:
        k: Fat-tree radix (must be even)
        host_bandwidth: Bandwidth for host-to-switch links
        switch_bandwidth: Bandwidth for switch-to-switch links
        latency: Link latency
        loss_rate: Packet loss rate
        weight: Link weight
        link_type: Link type
        gpu_type: GPU type
    
    Returns:
        Topology object
    """
    if k % 2 != 0:
        raise ValueError("Fat-tree radix k must be even")
    
    # Calculate number of nodes
    num_pods = k
    num_edge_per_pod = k // 2
    num_agg_per_pod = k // 2
    num_core = (k // 2) ** 2
    num_hosts_per_edge = k // 2
    
    total_hosts = num_pods * num_edge_per_pod * num_hosts_per_edge  # k^3/4
    total_edge_switches = num_pods * num_edge_per_pod
    total_agg_switches = num_pods * num_agg_per_pod
    total_switches = total_edge_switches + total_agg_switches + num_core
    
    topo = Topology(
        host_num=total_hosts,
        num_switches=total_switches,
        gpu_type=gpu_type
    )
    
    # Assign node IDs
    # Hosts: 0 to total_hosts-1
    # Edge switches: total_hosts to total_hosts + total_edge_switches - 1
    # Aggregation switches: after edge switches
    # Core switches: after aggregation switches
    
    edge_start = total_hosts
    agg_start = edge_start + total_edge_switches
    core_start = agg_start + total_agg_switches
    
    # Populate switch IDs
    topo.switch_ids = list(range(edge_start, edge_start + total_switches))
    
    # Helper function to get switch ID
    def edge_id(pod, idx):
        return edge_start + pod * num_edge_per_pod + idx
    
    def agg_id(pod, idx):
        return agg_start + pod * num_agg_per_pod + idx
    
    def core_id(idx):
        return core_start + idx
    
    # Connect hosts to edge switches
    host_id = 0
    for pod in range(num_pods):
        for edge_idx in range(num_edge_per_pod):
            edge_switch = edge_id(pod, edge_idx)
            for _ in range(num_hosts_per_edge):
                topo.add_link(host_id, edge_switch, host_bandwidth, latency, loss_rate, weight, link_type)
                host_id += 1
    
    # Connect edge switches to aggregation switches within each pod
    for pod in range(num_pods):
        for edge_idx in range(num_edge_per_pod):
            edge_switch = edge_id(pod, edge_idx)
            for agg_idx in range(num_agg_per_pod):
                agg_switch = agg_id(pod, agg_idx)
                topo.add_link(edge_switch, agg_switch, switch_bandwidth, latency, loss_rate, weight, link_type)
    
    # Connect aggregation switches to core switches
    for pod in range(num_pods):
        for agg_idx in range(num_agg_per_pod):
            agg_switch = agg_id(pod, agg_idx)
            # Each agg switch connects to k/2 core switches
            for core_offset in range(k // 2):
                core_switch = core_id(agg_idx * (k // 2) + core_offset)
                topo.add_link(agg_switch, core_switch, switch_bandwidth, latency, loss_rate, weight, link_type)
    
    return topo


def generate_hybrid_topology(
    num_hosts: int,
    gpus_per_host: int,
    nvswitches_per_host: int = 1,
    num_network_switches: int = 8,
    # NVLink parameters
    nvlink_bandwidth: str = "900Gbps",
    nvlink_latency: str = "1000",
    # Network parameters
    network_bandwidth: str = "100Gbps",
    network_latency: str = "1000",
    loss_rate: str = "0.000",
    weight: int = 100,
    network_link_type: LinkType = LinkType.ETH,
    gpu_type: str = "H100",
    add_switch_chain: bool = False,
    rail_connection_mode: str = "all"  # "all" = all GPUs connect to rails, "first" = only first GPU
) -> Topology:
    """
    Generate a hybrid topology with intra-host NVLink and inter-host network.
    
    Node ID ordering:
    - GPUs: 0 to (total_gpus - 1)
    - NVSwitches: total_gpus to (total_gpus + total_nvswitches - 1)
    - Network Switches: after NVSwitches
    
    Each host has its own NVSwitch(es) that only connect to GPUs within that host.
    
    Args:
        num_hosts: Number of hosts
        gpus_per_host: Number of GPUs per host
        nvswitches_per_host: Number of NVSwitches per host (each connects only to its host's GPUs)
        num_network_switches: Number of network switches (for rail topology)
        nvlink_bandwidth: NVLink bandwidth
        nvlink_latency: NVLink latency
        network_bandwidth: Network bandwidth
        network_latency: Network latency
        loss_rate: Loss rate
        weight: Link weight
        network_link_type: Type of network link
        gpu_type: GPU type
        add_switch_chain: Add chain connections between network switches
        rail_connection_mode: "all" = each GPU connects to corresponding rail switch,
                              "first" = only first GPU of each host connects
    
    Returns:
        Topology object
    """
    total_gpus = num_hosts * gpus_per_host
    total_nvswitches = num_hosts * nvswitches_per_host
    total_switches = total_nvswitches + num_network_switches
    
    topo = Topology(
        host_num=total_gpus,
        num_switches=total_switches,
        gpu_num_per_host=gpus_per_host,
        num_nvs_switches=total_nvswitches,
        gpu_type=gpu_type
    )
    
    # Node ID assignment:
    # GPUs: 0 to total_gpus - 1
    # NVSwitches: total_gpus to total_gpus + total_nvswitches - 1
    # Network switches: after NVSwitches
    
    nvswitch_start = total_gpus
    network_switch_start = nvswitch_start + total_nvswitches
    
    # Build switch IDs list (NVSwitches first, then network switches)
    topo.switch_ids = list(range(nvswitch_start, nvswitch_start + total_switches))
    
    # ========================================
    # Add intra-host NVLink connections
    # Each host's NVSwitch(es) only connect to that host's GPUs
    # ========================================
    for host_idx in range(num_hosts):
        gpu_base = host_idx * gpus_per_host
        nvswitch_base = nvswitch_start + host_idx * nvswitches_per_host
        
        if nvswitches_per_host > 0:
            # Connect this host's GPUs to this host's NVSwitches
            for nvs_offset in range(nvswitches_per_host):
                nvswitch_id = nvswitch_base + nvs_offset
                for gpu_offset in range(gpus_per_host):
                    gpu_id = gpu_base + gpu_offset
                    topo.add_link(
                        gpu_id, nvswitch_id, nvlink_bandwidth, nvlink_latency,
                        loss_rate, int(weight/100), LinkType.NVL
                    )
        else:
            # No NVSwitch: direct GPU-to-GPU connections within host (full mesh)
            for i in range(gpus_per_host):
                for j in range(i + 1, gpus_per_host):
                    topo.add_link(
                        gpu_base + i, gpu_base + j, nvlink_bandwidth, nvlink_latency,
                        loss_rate, weight, LinkType.NVL
                    )
    
    # ========================================
    # Add inter-host network connections (rail topology)
    # ========================================
    if rail_connection_mode == "all":
        # Each GPU connects to its corresponding rail switch
        # GPU i in each host connects to network_switch (i % num_network_switches)
        for host_idx in range(num_hosts):
            gpu_base = host_idx * gpus_per_host
            for gpu_offset in range(gpus_per_host):
                gpu_id = gpu_base + gpu_offset
                # GPU offset determines which rail switch it connects to
                switch_idx = gpu_offset % num_network_switches
                network_switch_id = network_switch_start + switch_idx
                topo.add_link(
                    gpu_id, network_switch_id, network_bandwidth, network_latency,
                    loss_rate, weight, network_link_type
                )
    else:  # "first" mode
        # Only first GPU of each host connects to network
        for host_idx in range(num_hosts):
            gpu_id = host_idx * gpus_per_host
            switch_idx = host_idx % num_network_switches
            network_switch_id = network_switch_start + switch_idx
            topo.add_link(
                gpu_id, network_switch_id, network_bandwidth, network_latency,
                loss_rate, weight, network_link_type
            )
    
    # ========================================
    # Optionally add chain connections between network switches
    # ========================================
    if add_switch_chain:
        for i in range(num_network_switches - 1):
            topo.add_link(
                network_switch_start + i,
                network_switch_start + i + 1,
                network_bandwidth, network_latency,
                loss_rate, weight, network_link_type
            )
    
    return topo


def generate_rail_with_nvlink_topology(
    num_hosts: int,
    gpus_per_host: int,
    nvswitches_per_host: int = 1,
    num_rail_switches: int = 8,
    # NVLink parameters
    nvlink_bandwidth: str = "900Gbps",
    nvlink_latency: str = "100",
    # Network parameters
    network_bandwidth: str = "100Gbps",
    network_latency: str = "1000",
    loss_rate: str = "0.000",
    weight: int = 100,
    network_link_type: LinkType = LinkType.ETH,
    gpu_type: str = "H100",
    add_switch_chain: bool = True
) -> Topology:
    """
    Generate a rail topology with NVLink intra-host connections.
    
    This is a convenience wrapper for generate_hybrid_topology with rail_connection_mode="all".
    
    Node ID ordering:
    - GPUs: 0 to (num_hosts * gpus_per_host - 1)
    - NVSwitches: after GPUs (each host has nvswitches_per_host NVSwitches)
    - Rail Switches: after NVSwitches
    
    Each GPU i (within a host) connects to rail switch (i % num_rail_switches).
    This means GPU 0 of all hosts connect to rail 0, GPU 1 to rail 1, etc.
    
    Args:
        num_hosts: Number of hosts
        gpus_per_host: Number of GPUs per host
        nvswitches_per_host: Number of NVSwitches per host
        num_rail_switches: Number of rail switches (usually equals gpus_per_host)
        nvlink_bandwidth: NVLink bandwidth
        nvlink_latency: NVLink latency
        network_bandwidth: Network bandwidth
        network_latency: Network latency
        loss_rate: Loss rate
        weight: Link weight
        network_link_type: Type of network link
        gpu_type: GPU type
        add_switch_chain: Add chain connections between rail switches
    
    Returns:
        Topology object
    """
    return generate_hybrid_topology(
        num_hosts=num_hosts,
        gpus_per_host=gpus_per_host,
        nvswitches_per_host=nvswitches_per_host,
        num_network_switches=num_rail_switches,
        nvlink_bandwidth=nvlink_bandwidth,
        nvlink_latency=nvlink_latency,
        network_bandwidth=network_bandwidth,
        network_latency=network_latency,
        loss_rate=loss_rate,
        weight=weight,
        network_link_type=network_link_type,
        gpu_type=gpu_type,
        add_switch_chain=add_switch_chain,
        rail_connection_mode="all"
    )


def generate_hybrid_no_nvswitch_topology(
    num_hosts: int,
    gpus_per_host: int,
    num_network_switches: int = 8,
    # Network parameters
    network_bandwidth: str = "100Gbps",
    network_latency: str = "1000",
    loss_rate: str = "0.000",
    weight: int = 100,
    network_link_type: LinkType = LinkType.ETH,
    gpu_type: str = "H100",
    add_switch_chain: bool = False,
    rail_connection_mode: str = "all"  # "all" = all GPUs connect to rails, "first" = only first GPU
) -> Topology:
    """
    Generate a hybrid topology with ONLY GPUs and network switches (NO NVSwitch nodes).
    
    This is similar to generate_hybrid_topology but without any NVSwitch nodes or
    intra-host NVLink connections. Only GPU-to-network-switch connections are created.
    
    Node ID ordering:
    - GPUs: 0 to (total_gpus - 1)
    - Network Switches: total_gpus to (total_gpus + num_network_switches - 1)
    
    Args:
        num_hosts: Number of hosts
        gpus_per_host: Number of GPUs per host
        num_network_switches: Number of network switches (for rail topology)
        network_bandwidth: Network bandwidth
        network_latency: Network latency
        loss_rate: Loss rate
        weight: Link weight
        network_link_type: Type of network link
        gpu_type: GPU type
        add_switch_chain: Add chain connections between network switches
        rail_connection_mode: "all" = each GPU connects to corresponding rail switch,
                              "first" = only first GPU of each host connects
    
    Returns:
        Topology object
    """
    total_gpus = num_hosts * gpus_per_host
    total_nvswitches = 0  # No NVSwitch nodes
    total_switches = num_network_switches
    
    topo = Topology(
        host_num=total_gpus,
        num_switches=total_switches,
        gpu_num_per_host=gpus_per_host,
        num_nvs_switches=total_nvswitches,
        gpu_type=gpu_type
    )
    
    # Node ID assignment:
    # GPUs: 0 to total_gpus - 1
    # Network switches: total_gpus to total_gpus + num_network_switches - 1
    
    network_switch_start = total_gpus
    
    # Build switch IDs list (only network switches, no NVSwitches)
    topo.switch_ids = list(range(network_switch_start, network_switch_start + num_network_switches))
    
    # ========================================
    # Add inter-host network connections (rail topology)
    # No intra-host NVLink connections in this mode
    # ========================================
    if rail_connection_mode == "all":
        # Each GPU connects to its corresponding rail switch
        # GPU i in each host connects to network_switch (i % num_network_switches)
        for host_idx in range(num_hosts):
            gpu_base = host_idx * gpus_per_host
            for gpu_offset in range(gpus_per_host):
                gpu_id = gpu_base + gpu_offset
                # GPU offset determines which rail switch it connects to
                switch_idx = gpu_offset % num_network_switches
                network_switch_id = network_switch_start + switch_idx
                topo.add_link(
                    gpu_id, network_switch_id, network_bandwidth, network_latency,
                    loss_rate, weight, network_link_type
                )
    else:  # "first" mode
        # Only first GPU of each host connects to network
        for host_idx in range(num_hosts):
            gpu_id = host_idx * gpus_per_host
            switch_idx = host_idx % num_network_switches
            network_switch_id = network_switch_start + switch_idx
            topo.add_link(
                gpu_id, network_switch_id, network_bandwidth, network_latency,
                loss_rate, weight, network_link_type
            )
    
    # ========================================
    # Optionally add chain connections between network switches
    # ========================================
    if add_switch_chain:
        for i in range(num_network_switches - 1):
            topo.add_link(
                network_switch_start + i,
                network_switch_start + i + 1,
                network_bandwidth, network_latency,
                loss_rate, weight, network_link_type
            )
    
    return topo


def main():
    parser = argparse.ArgumentParser(
        description="Generate network topology files",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Generate simple rail topology with 1024 hosts and 8 switches
  python gen_roft.py --type rail --hosts 1024 --switches 8 -o topo_rail_1024_8.txt

  # Generate rail topology with chain-connected switches
  python gen_roft.py --type rail_chain --hosts 256 --switches 8 -o topo_rail_256_8_chain.txt

  # Generate single-switch topology
  python gen_roft.py --type single --hosts 16 -o topo_16_1switch.txt

  # Generate ring topology
  python gen_roft.py --type ring --hosts 128 --switches 4 -o topo_ring_128_4.txt

  # Generate full mesh topology
  python gen_roft.py --type full_mesh --hosts 64 --switches 4 -o topo_mesh_64_4.txt

  # Generate fat-tree topology (k=4, gives 16 hosts)
  python gen_roft.py --type fattree --fattree-k 4 -o topo_fattree_k4.txt

  # Generate hybrid topology with NVLink + network (each GPU connects to rails)
  python gen_roft.py --type hybrid --hosts 16 --gpus-per-host 8 --nvswitches 1 --switches 8 -o topo_hybrid.txt

  # Generate rail topology with NVLink (shorthand for hybrid with all GPUs on rails)
  python gen_roft.py --type rail_nvlink --hosts 128 --gpus-per-host 8 --nvswitches 1 --switches 8 --chain -o topo_rail_nvlink.txt

  # Generate hybrid topology WITHOUT NVSwitch (only GPUs and network switches)
  python gen_roft.py --type hybrid_no_nvs --hosts 16 --gpus-per-host 8 --switches 8 -o topo_hybrid_no_nvs.txt
        """
    )
    
    parser.add_argument("--type", "-t", type=str, required=True,
                        choices=["rail", "rail_chain", "single", "ring", "full_mesh", "fattree", "hybrid", "rail_nvlink", "hybrid_no_nvs"],
                        help="Topology type")
    parser.add_argument("--hosts", "-n", type=int, default=16,
                        help="Number of hosts (default: 16)")
    parser.add_argument("--switches", "-s", type=int, default=1,
                        help="Number of network switches (default: 1)")
    parser.add_argument("--bandwidth", "-b", type=str, default="100Gbps",
                        help="Link bandwidth (default: 100Gbps)")
    parser.add_argument("--latency", "-l", type=str, default="1000",
                        help="Link latency (default: 1000)")
    parser.add_argument("--loss-rate", type=str, default="0.000",
                        help="Packet loss rate (default: 0.000)")
    parser.add_argument("--weight", "-w", type=int, default=100,
                        help="Link weight (default: 100)")
    parser.add_argument("--link-type", type=str, default="ETH",
                        choices=["ETH", "IB", "NVL", "PCIe"],
                        help="Link type (default: ETH)")
    parser.add_argument("--gpu-type", type=str, default="H100",
                        help="GPU type string (default: H100)")
    parser.add_argument("--output", "-o", type=str, required=True,
                        help="Output file path")
    
    # Fat-tree specific
    parser.add_argument("--fattree-k", type=int, default=4,
                        help="Fat-tree radix k (default: 4, must be even)")
    
    # Hybrid/NVLink topology specific
    parser.add_argument("--gpus-per-host", type=int, default=8,
                        help="Number of GPUs per host for hybrid topology (default: 8)")
    parser.add_argument("--nvswitches-per-host", type=int, default=1,
                        help="Number of NVSwitches per host for hybrid topology (default: 1)")
    parser.add_argument("--nvlink-bandwidth", type=str, default="900Gbps",
                        help="NVLink bandwidth for hybrid topology (default: 900Gbps)")
    parser.add_argument("--nvlink-latency", type=str, default="100",
                        help="NVLink latency for hybrid topology (default: 100)")
    parser.add_argument("--chain", action="store_true",
                        help="Add chain connections between network switches")
    parser.add_argument("--rail-mode", type=str, default="all",
                        choices=["all", "first"],
                        help="Rail connection mode: 'all' = all GPUs connect to rails, 'first' = only first GPU (default: all)")
    
    args = parser.parse_args()
    
    link_type = LinkType[args.link_type]
    
    if args.type == "rail":
        topo = generate_rail_topology(
            host_num=args.hosts,
            num_switches=args.switches,
            bandwidth=args.bandwidth,
            latency=args.latency,
            loss_rate=args.loss_rate,
            weight=args.weight,
            link_type=link_type,
            gpu_type=args.gpu_type,
            add_switch_chain=False
        )
    elif args.type == "rail_chain":
        topo = generate_rail_topology(
            host_num=args.hosts,
            num_switches=args.switches,
            bandwidth=args.bandwidth,
            latency=args.latency,
            loss_rate=args.loss_rate,
            weight=args.weight,
            link_type=link_type,
            gpu_type=args.gpu_type,
            add_switch_chain=True
        )
    elif args.type == "single":
        topo = generate_single_switch_topology(
            host_num=args.hosts,
            bandwidth=args.bandwidth,
            latency=args.latency,
            loss_rate=args.loss_rate,
            weight=args.weight,
            link_type=link_type,
            gpu_type=args.gpu_type
        )
    elif args.type == "ring":
        topo = generate_ring_topology(
            host_num=args.hosts,
            num_switches=args.switches,
            host_bandwidth=args.bandwidth,
            switch_bandwidth=args.bandwidth,
            latency=args.latency,
            loss_rate=args.loss_rate,
            weight=args.weight,
            link_type=link_type,
            gpu_type=args.gpu_type
        )
    elif args.type == "full_mesh":
        topo = generate_full_mesh_topology(
            host_num=args.hosts,
            num_switches=args.switches,
            host_bandwidth=args.bandwidth,
            switch_bandwidth=args.bandwidth,
            latency=args.latency,
            loss_rate=args.loss_rate,
            weight=args.weight,
            link_type=link_type,
            gpu_type=args.gpu_type
        )
    elif args.type == "fattree":
        topo = generate_fattree_topology(
            k=args.fattree_k,
            host_bandwidth=args.bandwidth,
            switch_bandwidth=args.bandwidth,
            latency=args.latency,
            loss_rate=args.loss_rate,
            weight=args.weight,
            link_type=link_type,
            gpu_type=args.gpu_type
        )
    elif args.type == "hybrid":
        topo = generate_hybrid_topology(
            num_hosts=args.hosts,
            gpus_per_host=args.gpus_per_host,
            nvswitches_per_host=args.nvswitches_per_host,
            num_network_switches=args.switches,
            nvlink_bandwidth=args.nvlink_bandwidth,
            nvlink_latency=args.nvlink_latency,
            network_bandwidth=args.bandwidth,
            network_latency=args.latency,
            loss_rate=args.loss_rate,
            weight=args.weight,
            network_link_type=link_type,
            gpu_type=args.gpu_type,
            add_switch_chain=args.chain,
            rail_connection_mode=args.rail_mode
        )
    elif args.type == "rail_nvlink":
        topo = generate_rail_with_nvlink_topology(
            num_hosts=args.hosts,
            gpus_per_host=args.gpus_per_host,
            nvswitches_per_host=args.nvswitches_per_host,
            num_rail_switches=args.switches,
            nvlink_bandwidth=args.nvlink_bandwidth,
            nvlink_latency=args.nvlink_latency,
            network_bandwidth=args.bandwidth,
            network_latency=args.latency,
            loss_rate=args.loss_rate,
            weight=args.weight,
            network_link_type=link_type,
            gpu_type=args.gpu_type,
            add_switch_chain=args.chain
        )
    elif args.type == "hybrid_no_nvs":
        topo = generate_hybrid_no_nvswitch_topology(
            num_hosts=args.hosts,
            gpus_per_host=args.gpus_per_host,
            num_network_switches=args.switches,
            network_bandwidth=args.bandwidth,
            network_latency=args.latency,
            loss_rate=args.loss_rate,
            weight=args.weight,
            network_link_type=link_type,
            gpu_type=args.gpu_type,
            add_switch_chain=args.chain,
            rail_connection_mode=args.rail_mode
        )
    else:
        raise ValueError(f"Unknown topology type: {args.type}")
    
    # Write topology to file
    topo.to_file(args.output)
    print(f"Topology written to {args.output}")
    print(f"  Total Nodes: {topo.num_nodes}")
    print(f"    - GPUs/Hosts: {topo.host_num}")
    print(f"    - NVSwitches: {topo.num_nvs_switches}")
    print(f"    - Network Switches: {topo.num_switches - topo.num_nvs_switches}")
    print(f"  Links: {len(topo.links)}")
    print(f"  GPU Type: {topo.gpu_type}")
    if topo.gpu_num_per_host > 0:
        print(f"  GPUs per Host: {topo.gpu_num_per_host}")


if __name__ == "__main__":
    main()
