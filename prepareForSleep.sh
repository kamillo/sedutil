#!/bin/bash

if [ "$EUID" -ne 0 ]; then
  exec sudo "$0" "$@"
fi

# Marker for the waybar status module. /run is a tmpfs cleared on reboot, just like
# the key the kernel keeps in memory, so the marker lives exactly as long as the key.
MARKER=/run/sed-opal-s3-ready

cd "$(dirname "$(readlink -f "$0")")" || exit 1

### Input password as hidden charactors ###
read -s -p "Enter Password: "  pswd
echo

if ./sedutil-cli -v --prepareForS3Sleep 0 "$pswd" /dev/nvme0n1; then
  touch "$MARKER"
  echo "Password saved in kernel, resume from sleep will unlock the drive."
else
  rm -f "$MARKER"
  echo "Failed to save the password in kernel." >&2
  exit 1
fi
#./sedutil-cli -v --prepareForS3Sleep 0 $pswd /dev/sg0

# refresh waybar custom/sedsleep (signal 10)
pkill -RTMIN+10 waybar 2>/dev/null || true
