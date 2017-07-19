#!/bin/sh
if [ `id -u` -ne 0 ]; then
    echo "You must be root to run this script"
    exit 1
fi

apt install ntp
service ntp stop
ntpd -q -g
service ntp start
