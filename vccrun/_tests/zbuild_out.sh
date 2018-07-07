#!/bin/sh

odir=`pwd`
mdir=`dirname "$0"`
cd "$mdir"
mdir=`pwd`

## sh ../0build.sh
## res=$?
## if [ $res -ne 0 ]; then
##   cd "$odir"
##   exit $res
## fi


echo "=== BUILDING TEST RESULTS ==="
mkdir outfiles 2>/dev/null
for fn in *.vc; do
  echo -n "$fn: "
  ofname=`basename "$fn" .vc`
  ofname="outfiles/${ofname}.out"
  if [ -f "$ofname" ]; then
    echo "SKIP"
  else
    sh ../0run.sh -DVCCRUN_PACKAGE_CONSTANT_TEST -pakdir ../packages -P. "$fn" boo foo zoo >"$ofname" 2>"$ofname.err"
    res=$?
    fline=`head -n 1 "$fn"`
    expectfail="ona"
    if [ "z$fline" = "z// FAIL" ]; then
      expectfail="tan"
    elif [ "z$fline" = "z//FAIL" ]; then
      expectfail="tan"
    fi
    #echo "expectfail: $expectfail; fline=$fline|"
    if [ $expectfail = ona ]; then
      if [ $res -ne 0 ]; then
        echo "FAILED"
        break
      fi
    else
      if [ $res -eq 0 ]; then
        echo "FAILED"
        break
      fi
    fi
    echo "OK"
  fi
done


cd "$odir"
exit $res
