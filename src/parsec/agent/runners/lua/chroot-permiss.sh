#!/usr/bin/env bash

sudo setcap cap_sys_chroot=ep "./chroot-in-shell.sh" # give chroot-in-shell.sh ability to chroot
sudo setcap cap_sys_chroot=ep "/usr/sbin/chroot" # give chroot ability to chroot (?)
sudo setcap cap_setfcap=ep "./chroot-forbid.sh" # give forbid.sh ability to remove
sudo setcap cap_setfcap=ep "/usr/sbin/setcap" # give setcap ability to remove (?)