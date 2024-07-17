#!/usr/bin/env bash

sudo setcap cap_setfcap=ep "./forbid.sh" # give forbid.sh ability to remove
sudo setcap cap_setfcap=ep "/usr/sbin/setcap" # give /setcap ability to remove
sudo setcap cap_sys_chroot+ep "./test" # give test ability to chroot
