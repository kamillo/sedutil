#!/bin/bash

scsi=/dev/sg0
dev=$(basename $(sg_map26 $scsi))
#drive=$(basename $(readlink /dev/disk/by-id/usb-Realtek_RTL9210B-CG_012345679148-0:0))
#path=/dev/$drive

### Input password as hidden charactors ### 
read -s -p "Enter Password: "  pswd

./sedutil-cli -v --setLockingRange 0 RW $pswd $scsi
./sedutil-cli -v --setMBREnable off $pswd $scsi
usbreset 0bda:9210
# echo -n "0000:00:14.0" | tee /sys/bus/pci/drivers/xhci_hcd/unbind
# echo -n "0000:00:14.0" | tee /sys/bus/pci/drivers/xhci_hcd/bind
echo 1 > /sys/block/$dev/device/rescan
partprobe
udevadm trigger
