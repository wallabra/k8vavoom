#!/bin/sh

rm decorate.txt decaldef.txt 2>/dev/null

echo 'Actor K8Gore_BloodBase {}' >>decorate.txt
echo 'Actor K8Gore_BloodBaseTransient : K8Gore_BloodBase {}' >>decorate.txt
echo "k8VaVoom { AllowBloodReplacement = true }" >>decorate.txt



echo "////////////////////////////////////////////////////////////////////////////////" >>decorate.txt
echo "// RED BLOOD" >>decorate.txt
echo "////////////////////////////////////////////////////////////////////////////////" >>decorate.txt

echo "////////////////////////////////////////////////////////////////////////////////" >>decaldef.txt
echo "// RED BLOOD" >>decaldef.txt
echo "////////////////////////////////////////////////////////////////////////////////" >>decaldef.txt

rdmd zprepro.d Shade=4f --append TransientSfx= Color= Translation= tpl.decorate.txt decorate.txt
rdmd zprepro.d Shade=4f --append TransientSfx= Color= Translation= tpl.decaldef.txt decaldef.txt


echo "" >>decorate.txt
echo "" >>decorate.txt
echo "" >>decorate.txt
echo "////////////////////////////////////////////////////////////////////////////////" >>decorate.txt
echo "// RED TRANSIENT BLOOD" >>decorate.txt
echo "////////////////////////////////////////////////////////////////////////////////" >>decorate.txt

echo "" >>decaldef.txt
echo "" >>decaldef.txt
echo "" >>decaldef.txt
echo "////////////////////////////////////////////////////////////////////////////////" >>decaldef.txt
echo "// RED TRANSIENT BLOOD" >>decaldef.txt
echo "////////////////////////////////////////////////////////////////////////////////" >>decaldef.txt

rdmd zprepro.d Shade=4f -DTRANSIENT --append TransientSfx=Transient Color=Transient Translation= tpl.decorate.txt decorate.txt
rdmd zprepro.d Shade=4f -DTRANSIENT --append TransientSfx=Transient Color=Transient Translation= tpl.decaldef.txt decaldef.txt



Translation='Translation "16:48=112:127", "64:79=112:127", "164:167=117:127", "168:191=112:127", "232:235=119:127", "236:239=123:127"'
echo "" >>decorate.txt
echo "" >>decorate.txt
echo "" >>decorate.txt
echo "////////////////////////////////////////////////////////////////////////////////" >>decorate.txt
echo "// GREEN BLOOD" >>decorate.txt
echo "////////////////////////////////////////////////////////////////////////////////" >>decorate.txt

echo "" >>decaldef.txt
echo "" >>decaldef.txt
echo "" >>decaldef.txt
echo "////////////////////////////////////////////////////////////////////////////////" >>decaldef.txt
echo "// GREEN BLOOD" >>decaldef.txt
echo "////////////////////////////////////////////////////////////////////////////////" >>decaldef.txt

rdmd zprepro.d Shade=4f -DGREEN --append TransientSfx= Color=Green "Translation=$Translation" tpl.decorate.txt decorate.txt
rdmd zprepro.d Shade=4f -DGREEN --append TransientSfx= Color=Green "Translation=$Translation" tpl.decaldef.txt decaldef.txt


echo "" >>decorate.txt
echo "" >>decorate.txt
echo "" >>decorate.txt
echo "////////////////////////////////////////////////////////////////////////////////" >>decorate.txt
echo "// GREEN TRANSIENT BLOOD" >>decorate.txt
echo "////////////////////////////////////////////////////////////////////////////////" >>decorate.txt

echo "" >>decaldef.txt
echo "" >>decaldef.txt
echo "" >>decaldef.txt
echo "////////////////////////////////////////////////////////////////////////////////" >>decaldef.txt
echo "// GREEN TRANSIENT BLOOD" >>decaldef.txt
echo "////////////////////////////////////////////////////////////////////////////////" >>decaldef.txt

rdmd zprepro.d Shade=4f -DGREEN -DTRANSIENT --append TransientSfx=Transient Color=Transient_Green "Translation=$Translation" tpl.decorate.txt decorate.txt
rdmd zprepro.d Shade=4f -DGREEN -DTRANSIENT --append TransientSfx=Transient Color=Transient_Green "Translation=$Translation" tpl.decaldef.txt decaldef.txt


Translation='Translation "16:48=240:247", "64:79=240:247", "164:167=201:207", "168:191=192:207", "232:235=100:207", "236:239=199:207"'
echo "" >>decorate.txt
echo "" >>decorate.txt
echo "" >>decorate.txt
echo "////////////////////////////////////////////////////////////////////////////////" >>decorate.txt
echo "// BLUE BLOOD" >>decorate.txt
echo "////////////////////////////////////////////////////////////////////////////////" >>decorate.txt

echo "" >>decaldef.txt
echo "" >>decaldef.txt
echo "" >>decaldef.txt
echo "////////////////////////////////////////////////////////////////////////////////" >>decaldef.txt
echo "// BLUE BLOOD" >>decaldef.txt
echo "////////////////////////////////////////////////////////////////////////////////" >>decaldef.txt

rdmd zprepro.d Shade=6f -DBLUE --append TransientSfx= Color=Blue "Translation=$Translation" tpl.decorate.txt decorate.txt
rdmd zprepro.d Shade=6f -DBLUE --append TransientSfx= Color=Blue "Translation=$Translation" tpl.decaldef.txt decaldef.txt


echo "" >>decorate.txt
echo "" >>decorate.txt
echo "" >>decorate.txt
echo "////////////////////////////////////////////////////////////////////////////////" >>decorate.txt
echo "// BLUE TRANSIENT BLOOD" >>decorate.txt
echo "////////////////////////////////////////////////////////////////////////////////" >>decorate.txt

echo "" >>decaldef.txt
echo "" >>decaldef.txt
echo "" >>decaldef.txt
echo "////////////////////////////////////////////////////////////////////////////////" >>decaldef.txt
echo "// BLUE TRANSIENT BLOOD" >>decaldef.txt
echo "////////////////////////////////////////////////////////////////////////////////" >>decaldef.txt

rdmd zprepro.d Shade=6f -DBLUE -DTRANSIENT --append TransientSfx=Transient Color=Transient_Blue "Translation=$Translation" tpl.decorate.txt decorate.txt
rdmd zprepro.d Shade=6f -DBLUE -DTRANSIENT --append TransientSfx=Transient Color=Transient_Blue "Translation=$Translation" tpl.decaldef.txt decaldef.txt
