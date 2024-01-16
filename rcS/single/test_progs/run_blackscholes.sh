#!/bin/bash

#Run the benchmark

echo "Beginning Blackscholes..."
PARSEC_DIR=/root/parsec-3.0/

cd $PARSEC_DIR
pwd

source ./env.sh

#run the package with a particular input i and number of threads n
m5 resetstats && parsecmgmt -a run -p blackscholes -i simmedium -n 4 && m5 dumpstats
echo "Done."

#End the simulation
echo "Ending the Simulation!"

/sbin/m5 exit
