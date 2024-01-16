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

import m5
from m5.util import addToPath
import argparse
from arm.system import *

m5.util.addToPath("../gem5/configs/dist")
import sw

def addOptions(parser):
   # Options for distributed simulation (i.e. dist-gem5)
    parser.add_argument("--dist", action="store_true", help="Distributed gem5"\
                      " simulation.")
    parser.add_argument("--is-switch", action="store_true",
                        help="Select the network switch simulator process for"\
                      " a distributed gem5 run.")
    parser.add_argument("--dist-rank", default=0, action="store", type=int,
                      help="Rank of this system within the dist gem5 run.")
    parser.add_argument("--dist-size", default=0, action="store", type=int,
                      help="Number of gem5 processes within the dist gem5"\
                      " run.")
    parser.add_argument("--dist-server-name",
                      default="127.0.0.1",
                      action="store", type=str,
                      help="Name of the message server host\nDEFAULT:"\
                      " localhost")
    parser.add_argument("--dist-server-port",
                      default=2200,
                      action="store", type=int,
                      help="Message server listen port\nDEFAULT: 2200")
    parser.add_argument("--dist-sync-repeat",
                      default="0us",
                      action="store", type=str,
                      help="Repeat interval for synchronisation barriers"\
                      " among dist-gem5 processes\nDEFAULT:"\
                      " --ethernet-linkdelay")
    parser.add_argument("--dist-sync-start",
                      default="1000000000000t",
                      action="store", type=str,
                      help="Time to schedule the first dist synchronisation"\
                      " barrier\nDEFAULT:1000000000000t")
    parser.add_argument("--ethernet-linkspeed", default="100Gbps",
                        action="store", type=str,
                        help="Link speed in bps\nDEFAULT: 100Gbps")
    parser.add_argument("--ethernet-linkdelay", default="10us",
                      action="store", type=str,
                      help="Link delay in seconds\nDEFAULT: 10us")
    parser.add_argument("--etherdump", action="store", type=str, default="",
                        help="Specify the filename to dump a pcap capture of"\
                        " the ethernet traffic")
    # Used by util/dist/gem5-dist.sh
    parser.add_argument("--checkpoint_dir", type=str,
                        default=m5.options.outdir,
                        help="Directory to save/read checkpoints")
    parser.add_argument("--restore", action="store_true", 
                        help="restore from a checkpoint")
    parser.add_argument("--dtb", type=str, default=None,
                        help="DTB file to load")
    parser.add_argument("--kernel", type=str, default=None,
                        help="Linux kernel")
    parser.add_argument("--disk-image", type=str,
                        default=None,
                        help="Disk to instantiate")
    parser.add_argument("--root-device", type=str,
                        default='/dev/vda',
                        help="OS device name for root partition (default: {})"
                             .format('/dev/vda'))
    parser.add_argument("--bootscript", action="append", type=str, default=[],
                        help="Linux bootscripts")
    parser.add_argument("--cpu", type=str, choices=list(cpu_types.keys()),
                        default="atomic",
                        help="CPU model to use")
    parser.add_argument("--cpu-freq", type=str, default="4GHz")
    parser.add_argument("--num-cores", type=int, default=1,
                        help="Number of CPU cores")
    parser.add_argument("--mem-type", default="DDR3_1600_8x8",
                        help = "type of memory to use")
    parser.add_argument("--mem-channels", type=int, default=1,
                        help = "number of memory channels")
    parser.add_argument("--mem-ranks", type=int, default=None,
                        help = "number of memory ranks per channel")
    parser.add_argument("--mem-size", action="store", type=str,
                        default="2GB",
                        help="Specify the physical memory size"),
    parser.add_argument("--bootloader", action="append",
                        help="executable file that runs before the --kernel")
    parser.add_argument("--dual", action="store_true", help="Dual mode gem5"\
                      " simulation.")

def buildDist(options):
    if options.is_switch:
        root = Root(full_system = True,
                    system = sw.build_switch(options))
    else:
        root = Root(full_system = True)
        root.testsys =  create(options)
        addDistEthernet(root, options)
        root.testsys.readfile = options.bootscript[0]


def buildDual(options):
    root = Root(full_system = True)
    root.testsys =  create(options)
    root.drivesys =  create(options)
    root.etherlink = EtherLink(speed = options.ethernet_linkspeed,
                               delay = options.ethernet_linkdelay)
    root.etherlink.int0 = root.testsys.pci_devices[1].interface
    root.etherlink.int1 = root.drivesys.pci_devices[1].interface

    root.testsys.readfile = options.bootscript[0]
    root.drivesys.readfile = options.bootscript[1]

def buildSingle(options):
    root = Root(full_system = True)
    root.testsys = create(options)
    root.testsys.readfile = options.bootscript[0]

def main():
    parser = argparse.ArgumentParser(epilog=__doc__)
    addOptions(parser)

    args = parser.parse_args()

    if args.dist or args.is_switch:
        buildDist(args)
    elif args.dual:
        buildDual(args)
    else:
        buildSingle(args)

    # instantiate all of the objects we've created above
    if args.restore:
        m5.instantiate(args.checkpoint_dir)
    else:
        m5.instantiate()
    
    print("Running the simulation")
    exit_event = m5.simulate()

    # Handle exit condtions
    if (exit_event.getCause()) == "checkpoint":
        print("Taking Checkpoint")
        m5.checkpoint(args.checkpoint_dir)
        print("Success!")
    elif exit_event.getCause() != "m5_exit instruction encountered":
        print(exit_event.getCause())
        exit(1)
    else:
        print("Success!")
        exit(0)



if __name__ == "__m5_main__":
    main()
