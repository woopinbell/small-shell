#!/bin/sh
set -eu

mkdir /tmp/small-shell
cp -R /source/. /tmp/small-shell/
cd /tmp/small-shell

make clean
make CC=gcc test-asan
make CC=gcc test-ubsan
