//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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
//#define Random()  ((float)(rand() & 0x7fff) / (float)0x8000)
//#define RandomFull()  ((float)(rand() & 0x7fff) / (float)0x7fff)


// this is used to compare floats like ints which is faster
#define FASI(var_) (*(const int32_t *)&(var_))


// An output device.
class FOutputDevice : public VLogListener {
public:
  // FOutputDevice interface
  virtual ~FOutputDevice();

  // simple text printing
  void Log (const char *S);
  void Log (EName Type, const char *S);
  void Log (VStr S);
  void Log (EName Type, VStr S);
  void Logf (const char *Fmt, ...) __attribute__((format(printf, 2, 3)));
  void Logf (EName Type, const char *Fmt, ...) __attribute__((format(printf, 3, 4)));
};

// error logs
//extern FOutputDevice *GLogSysError;
//extern FOutputDevice *GLogHostError;


__attribute__((warn_unused_result)) int superatoi (const char *s);

//__attribute__((warn_unused_result)) int ParseHex (const char *Str);
__attribute__((warn_unused_result)) vuint32 M_LookupColorName (const char *Name); // returns 0 if not found (otherwise high bit is set)
__attribute__((warn_unused_result)) vuint32 M_ParseColor (const char *Name, bool retZeroIfInvalid=false);

void M_RgbToHsv (vuint8, vuint8, vuint8, vuint8&, vuint8&, vuint8&);
void M_RgbToHsv (float, float, float, float&, float&, float&);
void M_HsvToRgb (vuint8, vuint8, vuint8, vuint8&, vuint8&, vuint8&);
void M_HsvToRgb (float, float, float, float&, float&, float&);


// see https://www.compuphase.com/cmetric.htm
static inline __attribute__((unused)) __attribute__((const)) __attribute__((warn_unused_result))
vint32 rgbDistanceSquared (vuint8 r0, vuint8 g0, vuint8 b0, vuint8 r1, vuint8 g1, vuint8 b1) {
  const vint32 rmean = ((vint32)r0+(vint32)r1)/2;
  const vint32 r = (vint32)r0-(vint32)r1;
  const vint32 g = (vint32)g0-(vint32)g1;
  const vint32 b = (vint32)b0-(vint32)b1;
  return (((512+rmean)*r*r)/256)+4*g*g+(((767-rmean)*b*b)/256);
}


static inline __attribute__((unused)) __attribute__((const)) __attribute__((warn_unused_result))
vint32 scaleInt (vint32 a, vint32 b, vint32 c) {
  return (vint32)(((vint64)a*b)/c);
}


static inline __attribute__((unused)) __attribute__((const)) __attribute__((warn_unused_result))
vuint32 scaleUInt (vuint32 a, vuint32 b, vuint32 c) {
  return (vuint32)(((vuint64)a*b)/c);
}


//==========================================================================
//
//  sRGBungamma
//
//  inverse of sRGB "gamma" function. (approx 2.2)
//
//==========================================================================
static inline __attribute__((unused)) __attribute__((warn_unused_result))
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
static inline __attribute__((unused)) __attribute__((warn_unused_result))
int sRGBgamma (double v) {
  if (v <= 0.0031308) v *= 12.92; else v = 1.055*pow(v, 1.0/2.4)-0.055;
  return int(v*255+0.5);
}


//==========================================================================
//
//  colorIntensity
//
//==========================================================================
static inline __attribute__((unused)) __attribute__((warn_unused_result))
vuint8 colorIntensity (int r, int g, int b) {
  // sRGB luminance(Y) values
  const double rY = 0.212655;
  const double gY = 0.715158;
  const double bY = 0.072187;
  return clampToByte(sRGBgamma(rY*sRGBungamma(r)+gY*sRGBungamma(g)+bY*sRGBungamma(b)));
}


//==========================================================================
//
//  PlaneAngles2D
//
//==========================================================================
static inline __attribute__((unused)) __attribute__((warn_unused_result))
float PlaneAngles2D (const TPlane *from, const TPlane *to) {
  float afrom = VectorAngleYaw(from->normal);
  float ato = VectorAngleYaw(to->normal);
  return AngleMod(AngleMod(ato-afrom+180)-180);
}


//==========================================================================
//
//  PlaneAngles2DFlipTo
//
//==========================================================================
static inline __attribute__((unused)) __attribute__((warn_unused_result))
float PlaneAngles2DFlipTo (const TPlane *from, const TPlane *to) {
  float afrom = VectorAngleYaw(from->normal);
  float ato = VectorAngleYaw(-to->normal);
  return AngleMod(AngleMod(ato-afrom+180)-180);
}


//==========================================================================
//
//  IsCircleTouchBox2D
//
//==========================================================================
static inline __attribute__((unused)) __attribute__((warn_unused_result))
bool IsCircleTouchBox2D (const float cx, const float cy, float radius, const float bbox2d[4]) {
  if (radius < 1.0f) return false;

  const float bbwHalf = (bbox2d[BOX2D_RIGHT]+bbox2d[BOX2D_LEFT])*0.5f;
  const float bbhHalf = (bbox2d[BOX2D_TOP]+bbox2d[BOX2D_BOTTOM])*0.5f;

  // the distance between the center of the circle and the center of the box
  // not a const, because we'll modify the variables later
  float cdistx = fabsf(cx-(bbox2d[BOX2D_LEFT]+bbwHalf));
  float cdisty = fabsf(cy-(bbox2d[BOX2D_BOTTOM]+bbhHalf));

  // easy cases: either completely outside, or completely inside
  if (cdistx > bbwHalf+radius || cdisty > bbhHalf+radius) return false;
  if (cdistx <= bbwHalf || cdisty <= bbhHalf) return true;

  // hard case: touching a corner
  cdistx -= bbwHalf;
  cdisty -= bbhHalf;
  const float cdistsq = cdistx*cdistx+cdisty*cdisty;
  return (cdistsq <= radius*radius);
}


//==========================================================================
//
//  size2human
//
//==========================================================================
static inline __attribute__((unused)) __attribute__((warn_unused_result))
VStr size2human (vuint32 size) {
       if (size < 1024*1024) return va("%u%s KB", size/1024, (size%1024 >= 512 ? ".5" : ""));
  else if (size < 1024*1024*1024) return va("%u%s MB", size/(1024*1024), (size%(1024*1024) >= 1024 ? ".5" : ""));
  else return va("%u%s GB", size/(1024*1024*1024), (size%(1024*1024*1024) >= 1024*1024 ? ".5" : ""));
}
