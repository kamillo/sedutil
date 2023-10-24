#!/bin/bash

### Input password as hidden charactors ### 
read -s -p "Enter Password: "  pswd

./sedutil-cli -v --prepareForS3Sleep 0 $pswd /dev/nvme0n1
#./sedutil-cli -v --prepareForS3Sleep 0 $pswd /dev/sg0
