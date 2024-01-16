#!/bin/sh

#Run the benchmark
echo "Beginning ocean_cp..."

#m5 checkpoint
m5 resetstats && /parsec/ext/splash2x/apps/ocean_cp/inst/aarch64-linux.gcc/bin/ocean_cp -n514 -p1 -e1e-07 -r20000 -t28800 && m5 dumpstats
echo "Done."

#End the simulation
echo "Ending the Simulation!"

/sbin/m5 exit
