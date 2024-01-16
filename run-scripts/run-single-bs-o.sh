#!/bin/bash

export M5_PATH=$(pwd)/../m5_binaries/
GEM5_DIR=$(pwd)/../gem5
rm -rf $(pwd)/m1s_o3_bs_single
IMG=$M5_PATH/disks/expanded-ubuntu-18.04-arm64-docker.img
VMLINUX=$(pwd)/../resources/vmlinux
Bootld=$(pwd)/../resources/boot.arm64
CheckPoint=$(pwd)/checkpoint
OutputDir=$(pwd)/m1s_o3_bs_single

FS_CONFIG=$(pwd)/../gem5/configs/example/fs.py
GEM5_EXE=$GEM5_DIR/build/ARM/gem5.opt

SCRIPT=$(pwd)/rcS/single/hack_back_ckpt.rcS
#SCRIPT=$(pwd)/rcS/single/test_progs/run_blackscholes.sh
NNODES=2

$GEM5_EXE --outdir=$OutputDir $FS_CONFIG		\
                    --kernel=$VMLINUX          		\
                    --disk=$IMG                		\
		    --root-device=/dev/vda1             \
                    --script=$SCRIPT        		\
                    --bootloader=$Bootld 		\
                    --cpu-type=DerivO3CPU		\
                    --num-cpus=1                        \
		    --checkpoint-dir=$CheckPoint	\
		    --smt				\
		    --caches				\
		   #--mem-size=2048MB
