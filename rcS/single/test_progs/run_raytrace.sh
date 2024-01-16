#!/bin/sh

#Run the benchmark
echo "Beginning Raytrace..."

cd /parsec/ext/splash2x/apps/raytrace/run
#m5 checkpoint
m5 resetstats && /parsec/ext/splash2x/apps/raytrace/inst/aarch64-linux.gcc/bin/raytrace 1 < input_1 && m5 dumpstats
echo "Done."

#End the simulation
echo "Ending the Simulation!"

/sbin/m5 exit
