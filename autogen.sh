#!/bin/bash
if [ "$1" == "clean" ]; then
    rm -f Makefile Makefile.in configure aclocal.m4
    rm -f missing depcomp install-sh  config.guess config.sub ltmain.sh
    find m4/ -lname \* -exec rm -f {} \;
else
    autoreconf -fsi
fi

