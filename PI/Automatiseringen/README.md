Canbus commands used on the pi:

candump can0
sudo ip link set can0 type can bitrate 500000 loopback off
sudo ip link set can0 up

troubleshooting:
dmesg | grep -i mcp
sudo nano /boot/firmware/config.txt