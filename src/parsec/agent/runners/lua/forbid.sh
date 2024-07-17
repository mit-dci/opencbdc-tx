#!/usr/bin/env bash

/usr/sbin/setcap -r "./test"
/usr/sbin/setcap -r "./forbid.sh"
/usr/sbin/setcap -r "/usr/sbin/setcap"