#!/bin/bash
if [ "$1" == "clean" ]; then
    rm -f Makefile Makefile.in configure aclocal.m4
    rm -f missing depcomp install-sh
else
    autoreconf -fsi
fi

