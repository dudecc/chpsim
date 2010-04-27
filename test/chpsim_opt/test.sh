#!/bin/bash

CHPSIM=../../chpsim
CHPSIM_CMD_PRE='-batch'
CHPSIM_CMD_POST=

test -d .logs || mkdir .logs

for i in *.chp
  do ln -sf $i last_test 2>/dev/null
  test -d .logs/$i || mkdir .logs/$i
  test -f .logs/$i/out || touch .logs/$i/out
  test -f .logs/$i/err || touch .logs/$i/err
  CHPSIM_OPT=`head -1 $i | sed sa//aa`
  $CHPSIM $CHPSIM_CMD_PRE $CHPSIM_OPT $i $CHPSIM_CMD_POST >out 2>err
  if diff out .logs/$i/out
    then rm out
    else echo -n "stdout has differences in $i, is this an error[Y/n]?"
    answer=
    read answer
    if test 'xn' = x$answer
      then rm .logs/$i/out; mv out .logs/$i/out
      else rm out err; exit 1
    fi
  fi
  if diff err .logs/$i/err
    then rm err
    else echo -n "stderr has differences in $i, is this an error[Y/n]?"
    answer=
    read answer
    if test 'xn' = x$answer
      then rm .logs/$i/err; mv err .logs/$i/err
      else rm err; exit 1
    fi
  fi
  rm last_test
done

