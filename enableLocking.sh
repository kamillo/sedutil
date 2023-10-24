#!/bin/bash

### Input password as hidden charactors ### 
read -s -p "Enter Password: "  pswd

./sedutil-cli -v -–enableLockingRange 0 $pswd /dev/sda
./sedutil-cli -v –-setMBREnable on $pswd /dev/sda
