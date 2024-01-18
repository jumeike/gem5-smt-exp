# Copyright (c) 2012-2013, 2015-2016 ARM Limited
# Copyright (c) 2020 Barkhausen Institut
# All rights reserved
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
# Copyright (c) 2010 Advanced Micro Devices, Inc.
# All rights reserved.
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

# Configure the M5 cache hierarchy config in one place
#

import m5
from m5.objects import *
from common.Caches import *
from common import ObjectList

def _get_hwp(hwp_option):
    if hwp_option == None:
        return NULL

    hwpClass = ObjectList.hwp_list.get(hwp_option)
    return hwpClass()

def _get_cache_opts(level, options):
    opts = {}

    size_attr = '{}_size'.format(level)
    if hasattr(options, size_attr):
        opts['size'] = getattr(options, size_attr)

    assoc_attr = '{}_assoc'.format(level)
    if hasattr(options, assoc_attr):
        opts['assoc'] = getattr(options, assoc_attr)

    prefetcher_attr = '{}_hwp_type'.format(level)
    if hasattr(options, prefetcher_attr):
        opts['prefetcher'] = _get_hwp(getattr(options, prefetcher_attr))

    return opts

def config_cache(options, system):
    if options.external_memory_system and (options.caches or options.l2cache):
        print("External caches and internal caches are exclusive options.\n")
        sys.exit(1)

    if options.external_memory_system:
        ExternalCache = ExternalCacheFactory(options.external_memory_system)

    if options.cpu_type == "O3_ARM_v7a_3":
        try:
            import cores.arm.O3_ARM_v7a as core
        except:
            print("O3_ARM_v7a_3 is unavailable. Did you compile the O3 model?")
            sys.exit(1)

        dcache_class, icache_class, l2_cache_class, walk_cache_class = \
            core.O3_ARM_v7a_DCache, core.O3_ARM_v7a_ICache, \
            core.O3_ARM_v7aL2, \
            core.O3_ARM_v7aWalkCache
    elif options.cpu_type == "HPI":
        try:
            import cores.arm.HPI as core
        except:
            print("HPI is unavailable.")
            sys.exit(1)

        dcache_class, icache_class, l2_cache_class, walk_cache_class = \
            core.HPI_DCache, core.HPI_ICache, core.HPI_L2, core.HPI_WalkCache
    elif options.smt:
        dcache_class, icache_class, dcache_class_smt, icache_class_smt, l2_cache_class, walk_cache_class = \
            L1_DCache, L1_ICache, L1_DCache, L1_ICache, L2Cache, None
    else:
        dcache_class, icache_class, l2_cache_class, walk_cache_class = \
            L1_DCache, L1_ICache, L2Cache, None

        if buildEnv['TARGET_ISA'] in ['x86', 'riscv']:
            walk_cache_class = PageTableWalkerCache

    # Set the cache line size of the system
    system.cache_line_size = options.cacheline_size

    # If elastic trace generation is enabled, make sure the memory system is
    # minimal so that compute delays do not include memory access latencies.
    # Configure the compulsory L1 caches for the O3CPU, do not configure
    # any more caches.
    if options.l2cache and options.elastic_trace_en:
        fatal("When elastic trace is enabled, do not configure L2 caches.")

    #if options.l2cache:
    # Add L3 cache (for IDIO)
    if options.l3cache:
        if not options.l2cache:
            fatal("L3 cache cannot exist without L2 cache")

        system.l3 = l2_cache_class(clk_domain = system.cpu_clk_domain,
                                    size = options.l3_size,
                                    assoc = options.l3_assoc)
        if options.disable_snoop_filter:
            system.tol3bus = L3XBar(clk_domain = system.clk_domain, snoop_filter = NULL)
        else:
            system.tol3bus = L3XBar(clk_domain = system.clk_domain,
            snoop_filter=SnoopFilter(lookup_latency = 0, is_for_l3x = True, max_capacity="32MB"))

        system.l3.cpu_side = system.tol3bus.master
        system.l3.mem_side = system.membus.slave

    elif options.l2cache:
        # Provide a clock for the L2 and the L1-to-L2 bus here as they
        # are not connected using addTwoLevelCacheHierarchy. Use the
        # same clock as the CPUs.
        system.l2 = l2_cache_class(clk_domain=system.cpu_clk_domain,
                                   **_get_cache_opts('l2', options))

        system.tol2bus = L2XBar(clk_domain = system.cpu_clk_domain)
        system.l2.cpu_side = system.tol2bus.master
        system.l2.mem_side = system.membus.slave

    if options.memchecker:
        system.memchecker = MemChecker()

    # if options.smt:
    #     # Create 2 instances of icache, dcache, and l2caches
    #     icache_smt1 = icache_class(**_get_cache_opts('l1i', options))
    #     dcache_smt1 = dcache_class(**_get_cache_opts('l1d', options))
    #     icache_smt2 = icache_class_smt(**_get_cache_opts('l1i', options))
    #     dcache_smt2 = dcache_class_smt(**_get_cache_opts('l1d', options))
    #     icache_smt3 = icache_class(**_get_cache_opts('l1i', options))
    #     dcache_smt3 = dcache_class(**_get_cache_opts('l1d', options))
    #     icache_smt4 = icache_class_smt(**_get_cache_opts('l1i', options))
    #     dcache_smt4 = dcache_class_smt(**_get_cache_opts('l1d', options))
    #     l2_smt1 = l2_cache_class(size=options.l2_size, assoc=options.l2_assoc)
    #     l2_smt2 = l2_cache_class(size=options.l2_size, assoc=options.l2_assoc)
    #     system.cpu[0].addTwoLevelCacheHierarchyMerged(icache_smt1, dcache_smt1, icache_smt2, dcache_smt2, system.cpu[1],l2_smt1)
    #     # system.cpu[1].addTwoLevelCacheHierarchy(icache_smt1, dcache_smt1, l2_smt1)
    #     system.cpu[2].addTwoLevelCacheHierarchyMerged(icache_smt3, dcache_smt3, icache_smt4, dcache_smt4, system.cpu[3], l2_smt2)
    #     # system.cpu[3].addTwoLevelCacheHierarchy(icache_smt2, dcache_smt2, l2_smt2)
    for i in range(0,options.num_cpus,2):
        print(f"run: {i}")
        if options.caches:
            icache = icache_class(**_get_cache_opts('l1i', options))
            dcache = dcache_class(**_get_cache_opts('l1d', options))
            
            # JOHNSON
            icache_smt1 = icache_class(**_get_cache_opts('l1i', options))
            dcache_smt1 = dcache_class(**_get_cache_opts('l1d', options))
            icache_smt2 = icache_class_smt(**_get_cache_opts('l1i', options))
            dcache_smt2 = dcache_class_smt(**_get_cache_opts('l1d', options))

            # SHIN make L2 as MLC of IDIO
            if options.mlc_adaptive_ddio:
                l2 = l2_cache_class(size=options.l2_size, assoc=options.l2_assoc, is_mlc=True, mlc_idx = i, mlc_ddio = True,
                                    prefetcher=MultiPrefetcher(
                                        prefetchers=[StridePrefetcher(degree=8, latency = 1),
                                                    MlcPrefetcher()]))
            else:
                l2 = l2_cache_class(size=options.l2_size, assoc=options.l2_assoc)

            # If we have a walker cache specified, instantiate two
            # instances here
            if walk_cache_class:
                iwalkcache = walk_cache_class()
                dwalkcache = walk_cache_class()
            else:
                iwalkcache = None
                dwalkcache = None

            if options.memchecker:
                dcache_mon = MemCheckerMonitor(warn_only=True)
                dcache_real = dcache

                # Do not pass the memchecker into the constructor of
                # MemCheckerMonitor, as it would create a copy; we require
                # exactly one MemChecker instance.
                dcache_mon.memchecker = system.memchecker

                # Connect monitor
                dcache_mon.mem_side = dcache.cpu_side

                # Let CPU connect to monitors
                dcache = dcache_mon

            # When connecting the caches, the clock is also inherited
            # from the CPU in question
            
            # SHIN.
            # system.cpu[i].addPrivateSplitL1Caches(icache, dcache,
            #                                      iwalkcache, dwalkcache)

            if options.l3cache:
                if options.smt:
                    system.cpu[i].addTwoLevelCacheHierarchyMerged(icache_smt1, dcache_smt1, icache_smt2, dcache_smt2, system.cpu[i+1],l2)
                else:
                    system.cpu[i].addTwoLevelCacheHierarchy(icache, dcache, l2)
            else:
                system.cpu[i].addPrivateSplitL1Caches(icache, dcache,iwalkcache, dwalkcache)

            if options.memchecker:
                # The mem_side ports of the caches haven't been connected yet.
                # Make sure connectAllPorts connects the right objects.
                system.cpu[i].dcache = dcache_real
                system.cpu[i].dcache_mon = dcache_mon

        elif options.external_memory_system:
            # These port names are presented to whatever 'external' system
            # gem5 is connecting to.  Its configuration will likely depend
            # on these names.  For simplicity, we would advise configuring
            # it to use this naming scheme; if this isn't possible, change
            # the names below.
            if buildEnv['TARGET_ISA'] in ['x86', 'arm', 'riscv']:
                system.cpu[i].addPrivateSplitL1Caches(
                        ExternalCache("cpu%d.icache" % i),
                        ExternalCache("cpu%d.dcache" % i),
                        ExternalCache("cpu%d.itb_walker_cache" % i),
                        ExternalCache("cpu%d.dtb_walker_cache" % i))
            else:
                system.cpu[i].addPrivateSplitL1Caches(
                        ExternalCache("cpu%d.icache" % i),
                        ExternalCache("cpu%d.dcache" % i))
            
        system.cpu[i].createInterruptController()
        system.cpu[i+1].createInterruptController()
        if options.l3cache:
            system.cpu[i].connectAllPorts(system.tol3bus, system.membus)
            # system.cpu[i+1].connectAllPorts(system.tol3bus, system.membus)
        elif options.l2cache:
            system.cpu[i].connectAllPorts(system.tol2bus, system.membus)
            # system.cpu[i+1].connectAllPorts(system.tol2bus, system.membus)
        elif options.external_memory_system:
            system.cpu[i].connectUncachedPorts(system.membus)
            # system.cpu[i+1].connectUncachedPorts(system.membus)
        else:
            system.cpu[i].connectAllPorts(system.membus)
            # system.cpu[i+1].connectAllPorts(system.membus)

    return system

# ExternalSlave provides a "port", but when that port connects to a cache,
# the connecting CPU SimObject wants to refer to its "cpu_side".
# The 'ExternalCache' class provides this adaptation by rewriting the name,
# eliminating distracting changes elsewhere in the config code.
class ExternalCache(ExternalSlave):
    def __getattr__(cls, attr):
        if (attr == "cpu_side"):
            attr = "port"
        return super(ExternalSlave, cls).__getattr__(attr)

    def __setattr__(cls, attr, value):
        if (attr == "cpu_side"):
            attr = "port"
        return super(ExternalSlave, cls).__setattr__(attr, value)

def ExternalCacheFactory(port_type):
    def make(name):
        return ExternalCache(port_data=name, port_type=port_type,
                             addr_ranges=[AllMemory])
    return make
