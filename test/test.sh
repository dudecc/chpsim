#!/bin/bash

dirlist='chpsim_general chpsim_debug chpsim_opt'

for i in $dirlist
  do ln -sf $i/last_test last_test 2>/dev/null
  cd $i
  ./test.sh || { echo Error in $i; exit 1; }
  cd ..
  rm last_test
done

