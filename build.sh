#!/bin/bash
arch=`uname -m`
gcc -O2 ./coegen.c -o coegen-$arch
