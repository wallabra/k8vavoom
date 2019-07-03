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


// An output device.
class FOutputDevice : public VLogListener {
public:
  // FOutputDevice interface
  virtual ~FOutputDevice();

  // simple text printing
  void Log (const char *S);
  void Log (EName Type, const char *S);
  void Log (const VStr &S);
  void Log (EName Type, const VStr &S);
  void Logf (const char *Fmt, ...) __attribute__((format(printf, 2, 3)));
  void Logf (EName Type, const char *Fmt, ...) __attribute__((format(printf, 3, 4)));
};

// error logs
extern FOutputDevice *GLogSysError;
extern FOutputDevice *GLogHostError;


__attribute__((warn_unused_result)) int superatoi (const char *s);

__attribute__((warn_unused_result)) int ParseHex (const char *Str);
__attribute__((warn_unused_result)) vuint32 M_LookupColorName (const char *Name); // returns 0 if not found (otherwise high bit is set)
__attribute__((warn_unused_result)) vuint32 M_ParseColor (const char *Name);

void M_RgbToHsv (vuint8, vuint8, vuint8, vuint8&, vuint8&, vuint8&);
void M_RgbToHsv (float, float, float, float&, float&, float&);
void M_HsvToRgb (vuint8, vuint8, vuint8, vuint8&, vuint8&, vuint8&);
void M_HsvToRgb (float, float, float, float&, float&, float&);


// http://burtleburtle.net/bob/rand/smallprng.html
struct BJPRNGCtx {
  vuint32 a, b, c, d;
};

#define bjprng_rot(x,k) (((x)<<(k))|((x)>>(32-(k))))
static inline __attribute__((unused)) __attribute__((warn_unused_result))
vuint32 bjprng_ranval (BJPRNGCtx *x) {
  vuint32 e = x->a-bjprng_rot(x->b, 27);
  x->a = x->b^bjprng_rot(x->c, 17);
  x->b = x->c+x->d;
  x->c = x->d+e;
  x->d = e+x->a;
  return x->d;
}

static inline __attribute__((unused)) void bjprng_raninit (BJPRNGCtx *x, vuint32 seed) {
  x->a = 0xf1ea5eed;
  x->b = x->c = x->d = seed;
  for (unsigned i = 0; i < 32; ++i) {
    //(void)bjprng_ranval(x);
    vuint32 e = x->a-bjprng_rot(x->b, 27);
    x->a = x->b^bjprng_rot(x->c, 17);
    x->b = x->c+x->d;
    x->c = x->d+e;
    x->d = e+x->a;
  }
}


// initialized with `RandomInit()`
extern BJPRNGCtx g_bjprng_ctx;


static inline __attribute__((unused)) __attribute__((warn_unused_result))
vuint32 GenRandomU31 () { return bjprng_ranval(&g_bjprng_ctx)&0x7fffffffu; }


void RandomInit (); // call this to seed with random seed
__attribute__((warn_unused_result)) float Random (); // [0..1)
__attribute__((warn_unused_result)) float RandomFull (); // [0..1]
__attribute__((warn_unused_result)) float RandomBetween (float minv, float maxv); // [minv..maxv]
// [0..255]
static inline __attribute__((unused)) __attribute__((warn_unused_result))
vuint8 P_Random () { return bjprng_ranval(&g_bjprng_ctx)&0xff; }


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
