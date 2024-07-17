#!/usr/bin/env bash

/usr/sbin/setcap -r "./chroot-in-shell.sh"
/usr/sbin/setcap -r "/usr/sbin/chroot"
/usr/sbin/setcap -r "./chroot-forbid.sh"
/usr/sbin/setcap -r "/usr/sbin/setcap"