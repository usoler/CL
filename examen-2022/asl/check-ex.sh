#!/bin/bash

#./asl ../examples/$1.asl > tmp.t
#../tvm/tvm-linux tmp.t < ../examples/$1.in > tmp.out
#diff ../examples/$1.out tmp.out > tmp.diff
#cat tmp.diff
#rm -f tmp.*

./asl ../jps/$1.asl > tmp.t
../tvm/tvm-linux tmp.t < ../jps/$1.in | diff -y - ../jps/$1.out
rm -f tmp.t

