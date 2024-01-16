from m5.objects import *


class high_perfO3(DerivO3CPU):
    LQEntries = 128
    SQEntries = 128
    LSQDepCheckShift = 0
    numIQEntries = 128
    numROBEntries = 384
    switched_out = False

# Instruction Cache
class high_perfO3_ICache(Cache):
    tag_latency = 1
    data_latency = 1
    response_latency = 1
    mshrs = 6
    tgts_per_mshr = 8
    size = '64kB'
    assoc = 8
    is_read_only = True
    # Writeback clean lines as well
    writeback_clean = True

# Data Cache
class high_perfO3_DCache(Cache):
    tag_latency = 1
    data_latency = 1
    response_latency = 1
    mshrs = 6
    tgts_per_mshr = 8
    size = '64kB'
    assoc = 8
    write_buffers = 16
    # Consider the L2 a victim cache also for clean lines
    writeback_clean = True

# TLB Cache
# Use a cache as a L2 TLB
class high_perfO3WalkCache(Cache):
    tag_latency = 4
    data_latency = 4
    response_latency = 4
    mshrs = 6
    tgts_per_mshr = 8
    size = '1kB'
    assoc = 8
    write_buffers = 16
    is_read_only = True
    # Writeback clean lines as well
    writeback_clean = True

# L2 Cache
class high_perfO3L2(Cache):
    tag_latency = 12
    data_latency = 12
    response_latency = 12
    mshrs = 16
    tgts_per_mshr = 8
    size = '4MB'
    assoc = 16
    write_buffers = 8
    prefetch_on_access = True
    clusivity = 'mostly_excl'
    # Simple stride prefetcher
    prefetcher = StridePrefetcher(degree=8, latency = 1)
    tags = BaseSetAssoc()