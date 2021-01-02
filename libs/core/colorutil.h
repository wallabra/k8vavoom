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
//**  Copyright (C) 2018-2021 Ketmar Dark
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


static VVA_OKUNUSED inline void UnpackRGBf (const vuint32 clr, float &r, float &g, float &b) noexcept {
  r = ((clr>>16)&0xffu)/255.0f;
  g = ((clr>>8)&0xffu)/255.0f;
  b = (clr&0xffu)/255.0f;
}

static VVA_OKUNUSED inline void UnpackRGBAf (const vuint32 clr, float &r, float &g, float &b, float &a) noexcept {
  r = ((clr>>16)&0xffu)/255.0f;
  g = ((clr>>8)&0xffu)/255.0f;
  b = (clr&0xffu)/255.0f;
  a = ((clr>>24)&0xffu)/255.0f;
}

static VVA_OKUNUSED inline vuint32 PackRGBf (const float r, const float g, const float b) noexcept {
  return
   0xff000000u|
   (((vuint32)(clampToByte((int)(clampval(r, 0.0f, 1.0f)*255.0f))))<<16)|
   (((vuint32)(clampToByte((int)(clampval(g, 0.0f, 1.0f)*255.0f))))<<8)|
   ((vuint32)(clampToByte((int)(clampval(b, 0.0f, 1.0f)*255.0f))));
}

static VVA_OKUNUSED inline vuint32 PackRGBAf (const float r, const float g, const float b, const float a) noexcept {
  return
   (((vuint32)(clampToByte((int)(clampval(a, 0.0f, 1.0f)*255.0f))))<<24)|
   (((vuint32)(clampToByte((int)(clampval(r, 0.0f, 1.0f)*255.0f))))<<16)|
   (((vuint32)(clampToByte((int)(clampval(g, 0.0f, 1.0f)*255.0f))))<<8)|
   ((vuint32)(clampToByte((int)(clampval(b, 0.0f, 1.0f)*255.0f))));
}

static VVA_OKUNUSED inline void UnpackRGB (const vuint32 clr, vuint8 &r, vuint8 &g, vuint8 &b) noexcept {
  r = (clr>>16)&0xffu;
  g = (clr>>8)&0xffu;
  b = clr&0xffu;
}

static VVA_OKUNUSED inline void UnpackRGBA (const vuint32 clr, vuint8 &r, vuint8 &g, vuint8 &b, vuint8 &a) noexcept {
  r = (clr>>16)&0xffu;
  g = (clr>>8)&0xffu;
  b = clr&0xffu;
  a = (clr>>24)&0xffu;
}

static VVA_OKUNUSED inline vuint32 PackRGB (const int r, const int g, const int b) noexcept {
  return
   0xff000000u|
   (((vuint32)(clampToByte(r)))<<16)|
   (((vuint32)(clampToByte(g)))<<8)|
   ((vuint32)clampToByte(b));
}

static VVA_OKUNUSED inline vuint32 PackRGBA (const int r, const int g, const int b, const int a) noexcept {
  return
   (((vuint32)(clampToByte(a)))<<24)|
   (((vuint32)(clampToByte(r)))<<16)|
   (((vuint32)(clampToByte(g)))<<8)|
   ((vuint32)clampToByte(b));
}


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
