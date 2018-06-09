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
    sh ../0run.sh -P../packages "$fn" boo foo zoo >"$ofname"
    res=$?
    if [ $res -ne 0 ]; then
      echo "FAILED"
      break
    fi
    echo "OK"
  fi
done


cd "$odir"
exit $res
