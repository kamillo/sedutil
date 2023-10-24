#!/bin/bash

### Input password as hidden charactors ### 
read -s -p "Enter Password: "  pswd

./sedutil-cli -v --disableLockingRange 0 $pswd /dev/sda
./sedutil-cli -v --setMBREnable off $pswd /dev/sda
