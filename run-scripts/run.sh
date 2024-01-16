#!/bin/bash
#wbWidth=4 causes error when you run
export GIT_ROOT=$(pwd)
# CACHE_CONFIG="--caches --l2cache --l3cache --l3_size 16MB --l3_assoc 16 --ddio-enabled --l1i_size=64kB --l1i_assoc=4 \
# --l1d_size=64kB --l1d_assoc=4 --l2_size=1MB --l2_assoc=8 --cacheline_size=64"
CACHE_CONFIG="--caches --l2cache --num-l3caches 1 --l3_size 16MB --l3_assoc 16 --l1i_size=64kB --l1i_assoc=4 \
--l1d_size=64kB --l1d_assoc=4 --l2_size=1MB --l2_assoc=8 --cacheline_size=64"  
CPU_CONFIG="--param=system.cpu[0:4].l2cache.mshrs=46 --param=system.cpu[0:4].dcache.mshrs=20 \
  --param=system.cpu[0:4].icache.mshrs=20 --param=system.l3.ddio_way_part=4 \
  --param=system.switch_cpus[0:4].decodeWidth=4 --param=system.l3.is_llc=True \
  --param=system.switch_cpus[0:4].numROBEntries=128 --param=system.switch_cpus[0:4].numIQEntries=120 \
  --param=system.switch_cpus[0:4].LQEntries=68 --param=system.switch_cpus[0:4].SQEntries=72 \
  --param=system.switch_cpus[0:4].numPhysIntRegs=256 --param=system.switch_cpus[0:4].numPhysFloatRegs=256 \
  --param=system.switch_cpus[0:4].branchPred.BTBEntries=8192 --param=system.switch_cpus[0:4].issueWidth=8 \
  --param=system.switch_cpus[0:4].commitWidth=8 --param=system.switch_cpus[0:4].dispatchWidth=8 \
  --param=system.switch_cpus[0:4].fetchWidth=4 --param=system.switch_cpus[0:4].wbWidth=8 \
  --param=system.switch_cpus[0:4].squashWidth=8 --param=system.switch_cpus[0:4].renameWidth=8"

function usage {
  echo "Usage: $0 --num-nics <num_nics> [--script <script>] [--packet-rate <packet_rate>] [--packet-size <packet_size>] [--loadgen-find-bw] [--take-checkpoint] [-h|--help]"
  echo "  --num-nics <num_nics> : number of NICs to use"
  echo "  --script <script> : guest script to run"
  echo "  --packet-rate <packet_rate> : packet rate in PPS"
  echo "  --packet-size <packet_size> : packet size in bytes"
  echo "  --loadgen-find-bw : run loadgen in find bandwidth mode"
  echo "  --take-checkpoint : take checkpoint after running"
  echo "  -h --help : print this message"
  exit 1
}

function setup_dirs {
  mkdir -p "$CKPT_DIR"
  mkdir -p "$RUNDIR"
}

function run_simulation {
  "$GEM5_DIR/build/ARM/gem5.$GEM5TYPE" $DEBUG_FLAGS --outdir="$RUNDIR" \
  "$GEM5_DIR"/configs/example/fs_smt.py --cpu-type=$CPUTYPE \
  --kernel="$RESOURCES/binaries/vmlinux.arm64" --disk="$RESOURCES/disks/expanded-ubuntu-18.04-arm64-docker.img" --bootloader="$RESOURCES/binaries/boot.arm64" --root=/dev/sda1 \
  --num-cpus=$(($num_cores)) --mem-type=DDR4_2400_16x4 --mem-channels=1 --mem-size=512MB --script="$GUEST_SCRIPT_DIR/$GUEST_SCRIPT" \
  --checkpoint-dir="$CKPT_DIR" $CONFIGARGS
}

if [[ -z "${GIT_ROOT}" ]]; then
  echo "Please export env var GIT_ROOT to point to the root of the CAL-DPDK-GEM5 repo"
  exit 1
fi

GEM5_DIR=${GIT_ROOT}/gem5
RESOURCES=${GIT_ROOT}/m5_binaries
# RESOURCES=${GIT_ROOT}/resources
GUEST_SCRIPT_DIR=${GIT_ROOT}/rcS/single/test_progs

# parse command line arguments
TEMP=$(getopt -o 'h' --long take-checkpoint,num-cores:,script:,num-threads:,num-smt-fetching-threads:,smt-mode,freq:,help -n 'gem5-smt' -- "$@")

# check for parsing errors
if [ $? != 0 ]; then
  echo "Error: unable to parse command line arguments" >&2
  exit 1
fi

eval set -- "$TEMP"

while true; do
  case "$1" in
  --num-cores)
    num_cores="$2"
    shift 2
    ;;
  --take-checkpoint)
    checkpoint=1
    shift 1
    ;;
  --script)
    GUEST_SCRIPT="$2"
    shift 2
    ;;
  --num-threads)
    num_threads="$2"
    shift 2
    ;;
  --num-smt-fetching-threads)
    smt_fetching_threads="$2"
    shift 2
    ;;
  # --smt-mode)
  #   SMT_MODE="True"
  #   shift 1
  #   ;;
  --freq)
    Freq=$2
    shift 2
    ;;
  -h | --help)
    usage
    ;;
  --)
    shift
    break
    ;;
  *) break ;;
  esac
done

CKPT_DIR=${GIT_ROOT}/ckpts/"num-cores-"$num_cores"-num-threads-"$num_threads-$GUEST_SCRIPT
# CKPT_DIR=${GIT_ROOT}/ckpts/"ckpts-with-new-vmlinux"/$num_nics"NIC"-$GUEST_SCRIPT
if [[ -z "$num_cores" ]]; then
  echo "Error: missing argument --num-nics" >&2
  usage
fi

if [[ -n "$checkpoint" ]]; then
  # RUNDIR=${GIT_ROOT}/rundir/$num_nics"NIC-ckp"-$GUEST_SCRIPT
  RUNDIR=${GIT_ROOT}/rundir/"num-cores-"$num_cores"-num-threads-"$num_threads-$GUEST_SCRIPT
  setup_dirs
  echo "Taking Checkpoint for workload=$GUEST_SCRIPT" >&2
  GEM5TYPE="opt"
  DEBUG_FLAGS="--debug-flags=O3CPUAll"
  CPUTYPE="DerivO3CPU"
  CONFIGARGS="--max-checkpoints 1 --maxinsts 1 --cpu-clock=$Freq --caches --num-threads=$num_threads \
  --param=system.cpu[0:1].smtNumFetchingThreads=$smt_fetching_threads --param=system.cpu[0:1].numPhysVecPredRegs=$(($num_cores*$num_threads*32)) \
  --param=system.cpu[0:1].numROBEntries=64 --param=system.cpu[0:1].numIQEntries=60 \
  --param=system.cpu[0:1].LQEntries=64 --param=system.cpu[0:1].SQEntries=64"
  # CONFIGARGS="--max-checkpoints 1 -r 1 --cpu-clock=$Freq --loadgen-start=2628842328231400"
  run_simulation
  exit 0
else
  if [[ -z "$PACKET_SIZE" ]]; then
    echo "Error: missing argument --packet_size" >&2
    usage
  fi

  if [[ -z "$PACKET_RATE" ]]; then
    echo "Error: missing argument --packet_rate" >&2
    usage
  fi
  ((RATE = PACKET_RATE * PACKET_SIZE * 8 / 1024 / 1024 / 1024))
  RUNDIR=${GIT_ROOT}/rundir/dpdk-testpmd-freq-droprate/$num_nics"NIC-"$PACKET_SIZE"SIZE-"$PACKET_RATE"RATE-"$RATE"Gbps-ddio-enabled"-$GUEST_SCRIPT
  setup_dirs
# /dpdk-testpmd-freq-scaling-test
  echo "Running NICs=$num_nics at $RATE GBPS" >&2
  CPUTYPE="O3_ARM_v7a_3"
  GEM5TYPE="opt"
  LOADGENMODE=${LOADGENMODE:-"Static"}
  DEBUG_FLAGS="--debug-flags=LoadgenDebug" #--debug-start=33952834348" #EthernetAll,EthernetDesc,LoadgenDebug
  CONFIGARGS="$CACHE_CONFIG $CPU_CONFIG  --cpu-clock=$Freq -r 2 --loadgen-start=26488422623982 --rel-max-tick=400010000000 --packet-rate=$PACKET_RATE --packet-size=$PACKET_SIZE --loadgen-mode=$LOADGENMODE \
  --warmup-dpdk 200000000000"
  run_simulation > ${RUNDIR}/simout
  exit
fi
