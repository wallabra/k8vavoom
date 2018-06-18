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


echo "=== RUNNING TESTS ==="
mkdir _xout 2>/dev/null
for fn in *.vc; do
  echo -n "$fn: "
  ofname=`basename "$fn" .vc`
  ifname="outfiles/${ofname}.out"
  ofname="_xout/${ofname}.out"
  sh ../0run.sh -pakdir ../packages -P. "$fn" boo foo zoo >"$ofname" 2>"$ofname.err"
  res=$?
  fline=`head -n 1 "$fn"`
  expectfail="ona"
  if [ "z$fline" = "z// FAIL" ]; then
    expectfail="tan"
  elif [ "z$fline" = "z//FAIL" ]; then
    expectfail="tan"
  fi
  if [ $expectfail = ona ]; then
    if [ $res -ne 0 ]; then
      echo "FAILED (retcode)"
      echo "=========================="
      cat "$ofname.err"
      break
    fi
    cmp "$ifname" "$ofname"
  else
    if [ $res -eq 0 ]; then
      echo "FAILED (retcode)"
      break
    fi
    cmp "$ifname.err" "$ofname.err"
  fi
  res=$?
  if [ $res -ne 0 ]; then
    echo "FAILED (output)"
    break
  fi
  echo "OK"
done


cd "$odir"
exit $res
