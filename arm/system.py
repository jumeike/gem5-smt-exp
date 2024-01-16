# Copyright (c) 2016-2017, 2020 ARM Limited
# All rights reserved.
#
# The license below extends only to copyright in the software and shall
# not be construed as granting a license to any other intellectual
# property including but not limited to intellectual property relating
# to a hardware implementation of the functionality of the software
# licensed hereunder.  You may use the software subject to the license
# terms below provided that you ensure that this notice is replicated
# unmodified and in its entirety in all distributions of the software,
# modified or unmodified, in source code or in binary form.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""This script is the full system example script from the ARM
Research Starter Kit on System Modeling. More information can be found
at: http://www.arm.com/ResearchEnablement/SystemModeling
"""

import sys

import m5
from .devices import *
from .high_perfO3 import *

m5.util.addToPath("../gem5/configs/dist")
m5.util.addToPath("../gem5/configs")
from common.cores.arm import HPI
from common import SysPaths
from common import MemConfig

# Pre-defined CPU configurations. Each tuple must be ordered as : (cpu_class,
# l1_icache_class, l1_dcache_class, walk_cache_class, l2_Cache_class). Any of
# the cache class may be 'None' if the particular cache is not present.
cpu_types = {
    "atomic" : ( AtomicSimpleCPU, None, None, None, None),
    
    "timing" : (TimingSimpleCPU, L1I(), L1D(),
                    WalkCache(), L2()),

    "minor" : (MinorCPU, L1I(), L1D(), 
                    WalkCache(), L2()),

    "o3" : ( DerivO3CPU, L1I(), L1D(), 
                    WalkCache(), L2()),

    "hpi" : ( HPI.HPI,
              HPI.HPI_ICache, HPI.HPI_DCache,
              HPI.HPI_WalkCache,
              HPI.HPI_L2),

    "high_o3" : (high_perfO3, 
                high_perfO3_ICache, high_perfO3_DCache,
                high_perfO3WalkCache, 
                high_perfO3L2)
}

def create_cow_image(name):
    """Helper function to create a Copy-on-Write disk image"""
    image = CowDiskImage()
    image.child.image_file = SysPaths.disk(name)
    return image


def create(args):
    ''' Create and configure the system object. '''

    cpu_class = cpu_types[args.cpu][0]
    mem_mode = cpu_class.memory_mode()
    # Only simulate caches when using a timing CPU (e.g., the HPI model)
    want_caches = True if mem_mode == "timing" else False

    system = simpleSystem(ArmSystem,
                                  want_caches,
                                  args.mem_size,
                                  mem_mode=mem_mode,
                                  workload=ArmFsLinux(
                                      object_file=
                                      SysPaths.binary(args.kernel)))

    MemConfig.config_mem(args, system)

    # Add the PCI devices we need for this system. The base system
    # doesn't have any PCI devices by default since they are assumed
    # to be added by the configuration scripts needing them.
    system.pci_devices = [
        # Create a VirtIO block device for the system's boot
        # disk. Attach the disk image using gem5's Copy-on-Write
        # functionality to avoid writing changes to the stored copy of
        # the disk image.
        PciVirtIO(vio=VirtIOBlock(image=create_cow_image(args.disk_image))),
        IGbE_e1000(),
        IGbE_e1000(),
        IGbE_e1000(),
        IGbE_e1000()
    ]

    # Attach the PCI devices to the system. The helper method in the
    # system assigns a unique PCI bus ID to each of the devices and
    # connects them to the IO bus.
    for dev in system.pci_devices:
        system.attach_pci(dev)

    # Wire up the system's memory system
    system.connect()

    # Add CPU clusters to the system
    system.cpu_cluster = [
        CpuCluster(system,
                           args.num_cores,
                           args.cpu_freq, "1.0V",
                           *cpu_types[args.cpu]),
    ]

    # Create a cache hierarchy for the cluster. We are assuming that
    # clusters have core-private L1 caches and an L2 that's shared
    # within the cluster.
    for cluster in system.cpu_cluster:
        system.addCaches(want_caches, last_cache_level=2)

    # Setup gem5's minimal Linux boot loader.
    system.realview.setupBootLoader(system, SysPaths.binary, args.bootloader)

    if args.dtb:
        system.workload.dtb_filename = args.dtb
    else:
        # No DTB specified: autogenerate DTB
        system.workload.dtb_filename = \
            os.path.join(m5.options.outdir, 'system.dtb')
        system.generateDtb(system.workload.dtb_filename)

    # Linux boot command flags
    kernel_cmd = [
        # Tell Linux to use the simulated serial port as a console
        "console=ttyAMA0",
        # Hard-code timi
        "lpj=19988480",
        # Disable address space randomisation to get a consistent
        # memory layout.
        "norandmaps",
        # Tell Linux where to find the root disk image.
        "root=%s" % args.root_device,
        # Mount the root disk read-write by default.
        "rw",
        # Tell Linux about the amount of physical memory present.
        "mem=%s" % args.mem_size,
    ]
    system.workload.command_line = " ".join(kernel_cmd)

    return system


def addDistEthernet(root, options):

    root.switch = EtherSwitch(fabric_speed='100Gbps')
    # instantiate distEtherLinks to connect switch ports
    # to other gem5 instances
    root.switch.portlink = [DistEtherLink(speed = options.ethernet_linkspeed,
                                      delay = options.ethernet_linkdelay,
                                      dist_rank = options.dist_rank,
                                      dist_size = options.dist_size,
                                      server_name = options.dist_server_name,
                                      server_port = options.dist_server_port,
                                      sync_start = options.dist_sync_start,
                                      sync_repeat = options.dist_sync_repeat,
                                      is_switch = False,
                                      num_nodes = options.dist_size),
                                      EtherLink(speed = '100Gbps'),
                                      EtherLink(speed = '100Gbps'),
                                      EtherLink(speed = '100Gbps'),
                                      EtherLink(speed = '100Gbps')]

    for (i, link) in enumerate(root.switch.portlink):
        link.int0 = root.switch.interface[i]       

    for i in range (1, 5):
        root.switch.portlink[i].int1 = root.testsys.pci_devices[i].interface
