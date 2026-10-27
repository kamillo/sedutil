#!/bin/bash
# Installs sed-s3-unlock (S3 resume support for OPAL drives). Run from the repo after `make`.
set -euo pipefail
[[ $EUID -eq 0 ]] || exec sudo "$0" "$@"

src=$(dirname "$(readlink -f "$0")")
cd "$src"

# everything here passes passwords as "-" on stdin, an older build takes it literally
help=$(../sedutil-cli 2>&1 || true)
if ! grep -q 'read as one line from stdin' <<<"$help"; then
    echo "../sedutil-cli does not read passwords from stdin, run make in the repo first" >&2
    exit 1
fi

install -m 0755 ../sedutil-cli /usr/local/sbin/sedutil-cli
install -m 0755 sed-s3-unlock /usr/local/sbin/sed-s3-unlock
install -m 0755 sed-s3-prompt /usr/local/bin/sed-s3-prompt
install -D -m 0755 sed-s3-unlock-user /usr/local/libexec/sed-s3-unlock-user
[[ -e /etc/sed-s3-unlock.conf ]] || install -m 0644 sed-s3-unlock.conf /etc/sed-s3-unlock.conf
install -m 0644 org.sedutil.s3unlock.policy /usr/share/polkit-1/actions/org.sedutil.s3unlock.policy
install -m 0644 sed-s3-unlock.service sed-s3-check.service /etc/systemd/system/
for unit in systemd-suspend systemd-hybrid-sleep systemd-suspend-then-hibernate; do
    install -D -m 0644 sed-s3-check.conf /etc/systemd/system/$unit.service.d/sed-s3-check.conf
done

systemctl daemon-reload
systemctl enable sed-s3-unlock.service

echo "Installed. Device: $(. /etc/sed-s3-unlock.conf; echo "$DEVICE") (edit /etc/sed-s3-unlock.conf if wrong)."
echo "Next: run sed-s3-prompt as your user and choose \"Zapamiętaj\" to seal the hash in TPM."
