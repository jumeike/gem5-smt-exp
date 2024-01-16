### Steps to running blackscholes
1. clone repo
2. Open directory in vscode
3. Build gem5
4. Disk image (located at _`m5_binaries/disks/expanded-ubuntu-18.04-arm64-docker.img`_ to be used for FS simulation is available locally and can't be pushed to github due to large size
5. Run the simulation using:
```
./run-scripts/run.sh --take-checkpoint --num-cores 1 --num-threads 2 --num-smt-fetching-threads 1 --script run_blackscholes.sh --freq 3GHz
```
