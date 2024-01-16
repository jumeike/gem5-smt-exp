#!/bin/sh

#Run the benchmark
echo "Beginning Radiosity..."

cd /parsec/ext/splash2x/apps/radiosity/run
#m5 checkpoint
m5 resetstats && /parsec/ext/splash2x/apps/radiosity/inst/aarch64-linux.gcc/bin/radiosity 1 < input_1 && m5 dumpstats
echo "Done."

#End the simulation
echo "Ending the Simulation!"

/sbin/m5 exit
