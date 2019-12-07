//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, version 3 of the License ONLY.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**  You should have received a copy of the GNU General Public License
//**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//**
//**************************************************************************


void M_RgbToHsv (vuint8 r, vuint8 g, vuint8 b, vuint8 &h, vuint8 &s, vuint8 &v);
void M_RgbToHsv (float r, float g, float b, float &h, float &s, float &v);

void M_HsvToRgb (vuint8 h, vuint8 s, vuint8 v, vuint8 &r, vuint8 &g, vuint8 &b);
void M_HsvToRgb (float h, float s, float v, float &r, float &g, float &b);

void M_RgbToHsl (float r, float g, float b, float &h, float &s, float &l);
void M_HslToRgb (float h, float s, float l, float &r, float &g, float &b);


//==========================================================================
//
//  rgbDistanceSquared
//
//  calculate distance between tro colors
//  see https://www.compuphase.com/cmetric.htm
//
//==========================================================================
static inline VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
vint32 rgbDistanceSquared (vuint8 r0, vuint8 g0, vuint8 b0, vuint8 r1, vuint8 g1, vuint8 b1) {
  const vint32 rmean = ((vint32)r0+(vint32)r1)/2;
  const vint32 r = (vint32)r0-(vint32)r1;
  const vint32 g = (vint32)g0-(vint32)g1;
  const vint32 b = (vint32)b0-(vint32)b1;
  return (((512+rmean)*r*r)/256)+4*g*g+(((767-rmean)*b*b)/256);
}


//==========================================================================
//
//  sRGBungamma
//
//  inverse of sRGB "gamma" function. (approx 2.2)
//
//==========================================================================
static inline VVA_OKUNUSED VVA_CHECKRESULT
double sRGBungamma (int ic) {
  const double c = ic/255.0;
  if (c <= 0.04045) return c/12.92;
  return pow((c+0.055)/1.055, 2.4);
}


//==========================================================================
//
//  sRGBungamma
//
//  sRGB "gamma" function (approx 2.2)
//
//==========================================================================
static inline VVA_OKUNUSED VVA_CHECKRESULT
int sRGBgamma (double v) {
  if (v <= 0.0031308) v *= 12.92; else v = 1.055*pow(v, 1.0/2.4)-0.055;
  return int(v*255+0.5);
}


//==========================================================================
//
//  colorIntensity
//
//==========================================================================
static inline VVA_OKUNUSED VVA_CHECKRESULT
vuint8 colorIntensity (int r, int g, int b) {
  // sRGB luminance(Y) values
  const double rY = 0.212655;
  const double gY = 0.715158;
  const double bY = 0.072187;
  return clampToByte(sRGBgamma(rY*sRGBungamma(r)+gY*sRGBungamma(g)+bY*sRGBungamma(b)));
}
