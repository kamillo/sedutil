#!/bin/bash

### Input password as hidden charactors ###
IFS= read -r -s -p "Enter Password: " pswd
echo

printf '%s\n' "$pswd" | ./sedutil-cli -v --enableLockingRange 0 - /dev/sda
printf '%s\n' "$pswd" | ./sedutil-cli -v --setMBREnable on - /dev/sda
