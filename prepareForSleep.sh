#!/bin/bash
# Saves the drive key in the kernel once, for this boot. With s3sleep/install.sh in
# place prefer sed-s3-prompt (GUI) or the sealed hash, which does this on every boot.

if [ "$EUID" -ne 0 ]; then
  exec sudo "$0" "$@"
fi

cd "$(dirname "$(readlink -f "$0")")" || exit 1

if [ -x /usr/local/sbin/sed-s3-unlock ]; then
  unlock=/usr/local/sbin/sed-s3-unlock
else
  # not installed: use the copy in this repo, it picks the sedutil-cli built here
  # (device from /etc/sed-s3-unlock.conf if present, /dev/nvme0n1 otherwise)
  unlock=./s3sleep/sed-s3-unlock
fi

### Input password as hidden charactors ###
IFS= read -r -s -p "Enter Password: " pswd
echo

printf '%s\n' "$pswd" | "$unlock" save
