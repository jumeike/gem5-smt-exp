#!/bin/sh

#Run the benchmark
echo "Beginning FMM..."

cd /parsec/ext/splash2x/apps/fmm/run
#m5 checkpoint
m5 resetstats && /parsec/ext/splash2x/apps/fmm/inst/aarch64-linux.gcc/bin/fmm 1 < input_1 && m5 dumpstats
echo "Done."

#End the simulation
echo "Ending the Simulation!"

/sbin/m5 exit
