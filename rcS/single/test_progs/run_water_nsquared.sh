#!/bin/sh

#Run the benchmark
echo "Beginning water_nsquared..."

cd /parsec/ext/splash2x/apps/water_nsquared/run
#m5 checkpoint
m5 resetstats && /parsec/ext/splash2x/apps/water_nsquared/inst/aarch64-linux.gcc/bin/water_nsquared 1 < input_1 && m5 dumpstats
echo "Done."

#End the simulation
echo "Ending the Simulation!"

/sbin/m5 exit
