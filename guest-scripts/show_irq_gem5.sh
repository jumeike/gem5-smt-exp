#!/bin/bash


if [ -z $1 ]; then
        echo "usage: $0 <cpu list> "
        echo "       <cpu list> can be either a comma separated list of single core numbers (0,1,2,3) or core groups (0-3)"
        exit 1
fi
cpulist=$1
NCPUS=$(cat /proc/cpuinfo | grep -c processor)
ONLINE_CPUS=$(cat /proc/cpuinfo | grep processor | cut -d ":" -f 2)

interface=eth0
pci_dev=$(ethtool -i $interface | grep "bus-info:" | cut -d ' ' -f 2)
infiniband_device_irqs_path="/sys/class/infiniband/$interface/device/msi_irqs"
net_device_irqs_path="/sys/class/net/$interface/device/msi_irqs"
interface_in_proc_interrupts=$(grep -P "$interface[^0-9,a-z,A-Z]" /proc/interrupts | cut -d":" -f1)
pci_in_proc_interrupts=$(grep "$pci_dev" /proc/interrupts | grep -v "async" | cut -d":" -f1)

if [ -d "$infiniband_device_irqs_path" ]; then
    irq_list=$(/bin/ls -1 "$infiniband_device_irqs_path" | tail -n +2)
elif [ "$interface_in_proc_interrupts" != "" ]; then
    irq_list=$interface_in_proc_interrupts
elif [ "$pci_in_proc_interrupts" != "" ]; then
    irq_list=$pci_in_proc_interrupts
elif [ -d "$net_device_irqs_path" ]; then
    irq_list=$(/bin/ls -1 "$net_device_irqs_path" | tail -n +2)
else
    echo "Error - interface or device \"$interface\" does not exist" 1>&2
    exit 1
fi
IRQS=$(echo "$irq_list" | sort -g)
echo "$IRQS"

if [ -z "$IRQS" ] ; then
        echo No IRQs found for $interface.
        exit 1
fi

# assigning the IRQs to the cores

CORES=$( echo $cpulist | sed 's/,/ /g' | wc -w )
for word in $(seq 1 $CORES)
do
	SEQ=$(echo $cpulist | cut -d "," -f $word | sed 's/-/ /')
	if [ "$(echo $SEQ | wc -w)" != "1" ]; then
		CPULIST="$CPULIST $( echo $(seq $SEQ) | sed 's/ /,/g' )"
	fi
done
if [ "$CPULIST" != "" ]; then
	cpulist=$(echo $CPULIST | sed 's/ /,/g')
fi
CORES=$( echo $cpulist | sed 's/,/ /g' | wc -w )


echo Discovered irqs for $interface: $IRQS
I=1
for IRQ in $IRQS
do
	core_id=$(echo $cpulist | cut -d "," -f $I)
	online=1
	if [ $core_id -ge $NCPUS ]
	then
		online=0
		for online_cpu in $ONLINE_CPUS
		do
			if [ "$online_cpu" == "$core_id" ]
			then
				online=1
				break
			fi
		done
	fi
	if [ $online -eq 0 ]
	then
		echo "irq $IRQ: Error - core $core_id does not exist"
	else
		echo Assign irq $IRQ core_id $core_id
	        affinity=$( core_to_affinity $core_id )
	        set_irq_affinity $IRQ $affinity
	fi
	I=$(( (I%CORES) + 1 ))
done

