#!/bin/sh

#Run the benchmark
echo "Beginning Barnes..."

cd /parsec/ext/splash2x/apps/barnes/run
#m5 checkpoint
m5 resetstats && /parsec/ext/splash2x/apps/barnes/inst/aarch64-linux.gcc/bin/barnes 1 < input_1 && m5 dumpstats
echo "Done."

#End the simulation
echo "Ending the Simulation!"

/sbin/m5 exit
