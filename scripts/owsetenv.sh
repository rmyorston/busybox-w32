#!/bin/sh
echo Open Watcom Build Environment
echo "Cross compilation for Win32, Win64"
export WATCOM=/opt/watcom
#change WATCOM variable to your
#install path for Open Watcom

export PATH=$WATCOM/binl:$PATH
export INCLUDE=$WATCOM/h:$WATCOM/h/nt:$INCLUDE
export EDPATH=$WATCOM/eddat
export WIPFC=$WATCOM/wipfc

