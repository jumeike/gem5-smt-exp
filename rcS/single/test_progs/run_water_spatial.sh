#!/bin/sh

#Run the benchmark
echo "Beginning water_spatial..."

cd /parsec/ext/splash2x/apps/water_spatial/run
#m5 checkpoint
m5 resetstats && /parsec/ext/splash2x/apps/water_spatial/inst/aarch64-linux.gcc/bin/water_spatial 1 < input_1 && m5 dumpstats
echo "Done."

#End the simulation
echo "Ending the Simulation!"

/sbin/m5 exit
