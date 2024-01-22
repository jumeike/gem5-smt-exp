# # Define the interface name
interface="eth0"
# # Check if the interface exists
# # if ! ip link show "$interface" >/dev/null 2>&1; then
# #     echo "Error: Interface \"$interface\" does not exist."
# #     exit 1
# # fi

# # Display information about network interfaces
#  echo "********* Interfaces Information ************"
# # ip a

# # Set up the interface eth0
echo "********* Setting up the interface (eth0) ************"
ip link set $interface up

# Display IRQ information related to eth0
echo "********* IRQ Information ************"
grep $interface /proc/interrupts

interrupts_output=$(grep $interface /proc/interrupts)
interface_irqs=$(echo "$interrupts_output" | awk '{print $1}' | tr -d ':') # Remove colons


# Display IRQs related to the interface
echo "IRQs related to interface \"$interface\":"
echo "$interface_irqs"

# Define CPU cores
cpulist="0-1"
NCPUS=$(cat /proc/cpuinfo | grep -c processor)
ONLINE_CPUS=$(cat /proc/cpuinfo | grep processor | cut -d ":" -f 2)

echo "Online CPUs:"
echo $ONLINE_CPUS

# Assign IRQ affinity to each IRQ in the list
for irq in $interface_irqs; do
    echo "********* Assigning IRQ $irq to CPU Cores ************"
    cat /proc/irq/"$irq"/smp_affinity
    
    # Modify IRQ affinity (example: setting affinity to 1)
    echo "Modifying IRQ $irq affinity..."
    echo 1 > /proc/irq/"$irq"/smp_affinity
done

# Display modified IRQ affinity
echo "Modified IRQ affinity:"
for irq in $interface_irqs; do
    cat /proc/irq/"$irq"/smp_affinity
done

m5 exit