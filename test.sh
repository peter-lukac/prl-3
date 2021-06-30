#!/bin/bash

INPUT_LEN=$((1+$(awk -F"," '{print NF-1}' <<< "$1")))
CPU=1

if [ $(($INPUT_LEN/16)) -ge 4 ]
then
    CPU=16
elif [ $(($INPUT_LEN/8)) -ge 3 ]
then
    CPU=8
elif [ $(($INPUT_LEN/4)) -ge 2 ]
then
    CPU=4
elif [ $(($INPUT_LEN/2)) -ge 1 ]
then
    CPU=2
else
    CPU=1
fi


mpic++ --prefix /usr/local/share/OpenMPI -lm -o vid vid.cpp

mpirun --prefix /usr/local/share/OpenMPI -np $CPU vid $1

#rm vid

