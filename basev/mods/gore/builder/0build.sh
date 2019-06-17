#!/bin/sh

odir=`pwd`
mdir=`dirname "$0"`
cd "$mdir"


DECAL_SRC="tpl.decaldef.txt"
DECAL_DEST="../decaldef.txt"

DECORATE_SRC="tpl.decorate.txt"
DECORATE_DEST="../decorate.txt"


if [ "z$1" = "zolddecals" ]; then
  DECAL_SELECTOR="-DOLDDECALS"
  DECAL_SHADE_RED="4f"
  DECAL_SHADE_GREEN="4f"
  DECAL_SHADE_BLUE="6f"
  DECAL_SHADE_RED_TRANSIENT="4f"
  DECAL_SHADE_GREEN_TRANSIENT="4f"
  DECAL_SHADE_BLUE_TRANSIENT="6f"
else
  DECAL_SELECTOR="-DNEWDECALS"
  DECAL_SHADE_RED="50"
  DECAL_SHADE_GREEN="50"
  DECAL_SHADE_BLUE="50"
  # still using old decals
  DECAL_SHADE_RED_TRANSIENT="4f"
  DECAL_SHADE_GREEN_TRANSIENT="4f"
  DECAL_SHADE_BLUE_TRANSIENT="6f"
fi


rm $DECORATE_DEST $DECAL_DEST 2>/dev/null

echo 'Actor K8Gore_BloodBase {}' >>$DECORATE_DEST
echo 'Actor K8Gore_BloodBaseTransient : K8Gore_BloodBase {}' >>$DECORATE_DEST
echo "k8VaVoom { AllowBloodReplacement = true }" >>$DECORATE_DEST



echo "////////////////////////////////////////////////////////////////////////////////" >>$DECORATE_DEST
echo "// RED BLOOD" >>$DECORATE_DEST
echo "////////////////////////////////////////////////////////////////////////////////" >>$DECORATE_DEST

echo "////////////////////////////////////////////////////////////////////////////////" >>$DECAL_DEST
echo "// RED BLOOD" >>$DECAL_DEST
echo "////////////////////////////////////////////////////////////////////////////////" >>$DECAL_DEST

rdmd zprepro.d $DECAL_SELECTOR Shade=4f --append TransientSfx= Color= Translation= $DECORATE_SRC $DECORATE_DEST
rdmd zprepro.d $DECAL_SELECTOR Shade=$DECAL_SHADE_RED --append TransientSfx= Color= Translation= $DECAL_SRC $DECAL_DEST


echo "" >>$DECORATE_DEST
echo "" >>$DECORATE_DEST
echo "" >>$DECORATE_DEST
echo "////////////////////////////////////////////////////////////////////////////////" >>$DECORATE_DEST
echo "// RED TRANSIENT BLOOD" >>$DECORATE_DEST
echo "////////////////////////////////////////////////////////////////////////////////" >>$DECORATE_DEST

echo "" >>$DECAL_DEST
echo "" >>$DECAL_DEST
echo "" >>$DECAL_DEST
echo "////////////////////////////////////////////////////////////////////////////////" >>$DECAL_DEST
echo "// RED TRANSIENT BLOOD" >>$DECAL_DEST
echo "////////////////////////////////////////////////////////////////////////////////" >>$DECAL_DEST

rdmd zprepro.d $DECAL_SELECTOR Shade=4f -DTRANSIENT --append TransientSfx=Transient Color=Transient Translation= $DECORATE_SRC $DECORATE_DEST
rdmd zprepro.d $DECAL_SELECTOR Shade=$DECAL_SHADE_RED_TRANSIENT -DTRANSIENT --append TransientSfx=Transient Color=Transient Translation= $DECAL_SRC $DECAL_DEST



Translation='Translation "16:48=112:127", "64:79=112:127", "164:167=117:127", "168:191=112:127", "232:235=119:127", "236:239=123:127"'
echo "" >>$DECORATE_DEST
echo "" >>$DECORATE_DEST
echo "" >>$DECORATE_DEST
echo "////////////////////////////////////////////////////////////////////////////////" >>$DECORATE_DEST
echo "// GREEN BLOOD" >>$DECORATE_DEST
echo "////////////////////////////////////////////////////////////////////////////////" >>$DECORATE_DEST

echo "" >>$DECAL_DEST
echo "" >>$DECAL_DEST
echo "" >>$DECAL_DEST
echo "////////////////////////////////////////////////////////////////////////////////" >>$DECAL_DEST
echo "// GREEN BLOOD" >>$DECAL_DEST
echo "////////////////////////////////////////////////////////////////////////////////" >>$DECAL_DEST

rdmd zprepro.d $DECAL_SELECTOR Shade=4f -DGREEN --append TransientSfx= Color=Green "Translation=$Translation" $DECORATE_SRC $DECORATE_DEST
rdmd zprepro.d $DECAL_SELECTOR Shade=$DECAL_SHADE_GREEN -DGREEN --append TransientSfx= Color=Green "Translation=$Translation" $DECAL_SRC $DECAL_DEST


echo "" >>$DECORATE_DEST
echo "" >>$DECORATE_DEST
echo "" >>$DECORATE_DEST
echo "////////////////////////////////////////////////////////////////////////////////" >>$DECORATE_DEST
echo "// GREEN TRANSIENT BLOOD" >>$DECORATE_DEST
echo "////////////////////////////////////////////////////////////////////////////////" >>$DECORATE_DEST

echo "" >>$DECAL_DEST
echo "" >>$DECAL_DEST
echo "" >>$DECAL_DEST
echo "////////////////////////////////////////////////////////////////////////////////" >>$DECAL_DEST
echo "// GREEN TRANSIENT BLOOD" >>$DECAL_DEST
echo "////////////////////////////////////////////////////////////////////////////////" >>$DECAL_DEST

rdmd zprepro.d $DECAL_SELECTOR Shade=4f -DGREEN -DTRANSIENT --append TransientSfx=Transient Color=Transient_Green "Translation=$Translation" $DECORATE_SRC $DECORATE_DEST
rdmd zprepro.d $DECAL_SELECTOR Shade=$DECAL_SHADE_GREEN_TRANSIENT -DGREEN -DTRANSIENT --append TransientSfx=Transient Color=Transient_Green "Translation=$Translation" $DECAL_SRC $DECAL_DEST


Translation='Translation "16:48=240:247", "64:79=240:247", "164:167=201:207", "168:191=192:207", "232:235=100:207", "236:239=199:207"'
echo "" >>$DECORATE_DEST
echo "" >>$DECORATE_DEST
echo "" >>$DECORATE_DEST
echo "////////////////////////////////////////////////////////////////////////////////" >>$DECORATE_DEST
echo "// BLUE BLOOD" >>$DECORATE_DEST
echo "////////////////////////////////////////////////////////////////////////////////" >>$DECORATE_DEST

echo "" >>$DECAL_DEST
echo "" >>$DECAL_DEST
echo "" >>$DECAL_DEST
echo "////////////////////////////////////////////////////////////////////////////////" >>$DECAL_DEST
echo "// BLUE BLOOD" >>$DECAL_DEST
echo "////////////////////////////////////////////////////////////////////////////////" >>$DECAL_DEST

rdmd zprepro.d $DECAL_SELECTOR Shade=6f -DBLUE --append TransientSfx= Color=Blue "Translation=$Translation" $DECORATE_SRC $DECORATE_DEST
rdmd zprepro.d $DECAL_SELECTOR Shade=$DECAL_SHADE_BLUE -DBLUE --append TransientSfx= Color=Blue "Translation=$Translation" $DECAL_SRC $DECAL_DEST


echo "" >>$DECORATE_DEST
echo "" >>$DECORATE_DEST
echo "" >>$DECORATE_DEST
echo "////////////////////////////////////////////////////////////////////////////////" >>$DECORATE_DEST
echo "// BLUE TRANSIENT BLOOD" >>$DECORATE_DEST
echo "////////////////////////////////////////////////////////////////////////////////" >>$DECORATE_DEST

echo "" >>$DECAL_DEST
echo "" >>$DECAL_DEST
echo "" >>$DECAL_DEST
echo "////////////////////////////////////////////////////////////////////////////////" >>$DECAL_DEST
echo "// BLUE TRANSIENT BLOOD" >>$DECAL_DEST
echo "////////////////////////////////////////////////////////////////////////////////" >>$DECAL_DEST

rdmd zprepro.d $DECAL_SELECTOR Shade=6f -DBLUE -DTRANSIENT --append TransientSfx=Transient Color=Transient_Blue "Translation=$Translation" $DECORATE_SRC $DECORATE_DEST
rdmd zprepro.d $DECAL_SELECTOR Shade=$DECAL_SHADE_BLUE_TRANSIENT -DBLUE -DTRANSIENT --append TransientSfx=Transient Color=Transient_Blue "Translation=$Translation" $DECAL_SRC $DECAL_DEST


cd "$odir"
