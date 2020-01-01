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
//**  Copyright (C) 2018-2020 Ketmar Dark
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
#include "core.h"


//==========================================================================
//
//  M_RgbToHsv
//
//==========================================================================
void M_RgbToHsv (vuint8 r, vuint8 g, vuint8 b, vuint8 &h, vuint8 &s, vuint8 &v) {
  vuint8 min = min3(r, g, b);
  vuint8 max = max3(r, g, b);
  v = max;
  if (max == min) {
    // gray
    s = 0;
    h = 0;
    return;
  }
  s = 255-255*min/max;
       if (max == r) h = 0+43*(g-b)/(max-min);
  else if (max == g) h = 85+43*(b-r)/(max-min);
  else h = 171+43*(r-g)/(max-min);
}


//==========================================================================
//
//  M_RgbToHsv
//
//==========================================================================
void M_RgbToHsv (float r, float g, float b, float &h, float &s, float &v) {
  float min = min3(r, g, b);
  float max = max3(r, g, b);
  v = max;
  if (max == min) {
    // gray
    s = 0;
    h = 0;
    return;
  }
  s = 1.0f-min/max;
  if (max == r) {
    h = 0.0f+60.0f*(g-b)/(max-min);
    if (h < 0) h += 360.0f;
  } else if (max == g) {
    h = 120.0f+60.0f*(b-r)/(max-min);
  } else {
    h = 240.0f+60.0f*(r-g)/(max-min);
  }
}


//==========================================================================
//
//  M_HsvToRgb
//
//==========================================================================
void M_HsvToRgb (vuint8 h, vuint8 s, vuint8 v, vuint8 &r, vuint8 &g, vuint8 &b) {
  if (s == 0) {
    // gray
    r = v;
    g = v;
    b = v;
    return;
  }
  int i = h/43;
  vuint8 f = (h-i*43)*255/43;
  vuint8 p = v*(255-s)/255;
  vuint8 q = v*(255-f*s/255)/255;
  vuint8 t = v*(255-(255-f)*s/255)/255;
  switch (i) {
    case 0:
      r = v;
      g = t;
      b = p;
      break;
    case 1:
      r = q;
      g = v;
      b = p;
      break;
    case 2:
      r = p;
      g = v;
      b = t;
      break;
    case 3:
      r = p;
      g = q;
      b = v;
      break;
    case 4:
      r = t;
      g = p;
      b = v;
      break;
    default:
      r = v;
      g = p;
      b = q;
      break;
  }
}


//==========================================================================
//
//  M_HsvToRgb
//
//==========================================================================
void M_HsvToRgb (float h, float s, float v, float &r, float &g, float &b) {
  if (s == 0) {
    // gray
    r = v;
    g = v;
    b = v;
    return;
  }
  int i = (int)(h/60.0f);
  float f = h/60.0f-i;
  float p = v*(1.0f-s);
  float q = v*(1.0f-f*s);
  float t = v*(1.0f-(1.0f-f)*s);
  switch (i) {
    case 0:
      r = v;
      g = t;
      b = p;
      break;
    case 1:
      r = q;
      g = v;
      b = p;
      break;
    case 2:
      r = p;
      g = v;
      b = t;
      break;
    case 3:
      r = p;
      g = q;
      b = v;
      break;
    case 4:
      r = t;
      g = p;
      b = v;
      break;
    default:
      r = v;
      g = p;
      b = q;
      break;
  }
}


/// Assumes the input `u` is already between 0 and 1 fyi.
/*
static inline float srgbToLinearRgb (float u) {
  return (u < 0.4045f ? u/12.92f : powf((u+0.055f)/1.055f, 2.4f));
}
*/


/// Converts hsl to rgb
void M_HslToRgb (float h, float s, float l, float &r, float &g, float &b) {
  h = fmodf(h, 360.0f);

  float C = (1.0f-fabsf(2.0f*l-1.0f))*s;

  float hPrime = h/60.0f;

  float X = C*(1.0f-fabsf(fmodf(hPrime, 2.0f)-1.0f));

  if (!isFiniteF(h)) {
    r = g = b = 0.0f;
  } else if (hPrime >= 0.0f && hPrime < 1.0f) {
    r = C;
    g = X;
    b = 0.0f;
  } else if (hPrime >= 1.0f && hPrime < 2.0f) {
    r = X;
    g = C;
    b = 0.0f;
  } else if (hPrime >= 2.0f && hPrime < 3.0f) {
    r = 0.0f;
    g = C;
    b = X;
  } else if (hPrime >= 3.0f && hPrime < 4.0f) {
    r = 0.0f;
    g = X;
    b = C;
  } else if (hPrime >= 4.0f && hPrime < 5.0f) {
    r = X;
    g = 0.0f;
    b = C;
  } else if (hPrime >= 5.0f && hPrime < 6.0f) {
    r = C;
    g = 0.0f;
    b = X;
  }

  float m = l-C/2.0f;

  r += m;
  g += m;
  b += m;
}


/// Converts an RGB color into an HSL triplet. useWeightedLightness will try to get a better value for luminosity for the human eye, which is more sensitive to green than red and more to red than blue. If it is false, it just does average of the rgb.
void M_RgbToHsl (float r, float g, float b, float &h, float &s, float &l/*, bool useWeightedLightness*//*=false*/) {
  float maxColor = max2(max2(r, g), b);
  float minColor = min2(min2(r, g), b);

  float L = (maxColor+minColor)/2.0f;

  /*
  if (useWeightedLightness) {
    // the colors don't affect the eye equally
    // this is a little more accurate than plain HSL numbers
    L = 0.2126f*srgbToLinearRgb(r)+0.7152f*srgbToLinearRgb(g)+0.0722f*srgbToLinearRgb(b);
    // maybe a better number is 299, 587, 114
  }
  */
  float S = 0.0f;
  float H = 0.0f;
  if (maxColor != minColor) {
    if (L < 0.5f) {
      S = (maxColor-minColor)/(maxColor+minColor);
    } else {
      S = (maxColor-minColor)/(2.0f-maxColor-minColor);
    }
    if (r == maxColor) {
      H = (g-b)/(maxColor-minColor);
    } else if (g == maxColor) {
      H = 2.0f+(b-r)/(maxColor-minColor);
    } else {
      H = 4.0f+(r-g)/(maxColor-minColor);
    }
  }

  H = H*60.0f;
  if (H < 0.0f) H += 360.0f;

  //return [H, S, L];
  h = H;
  s = S;
  l = L;
}
