//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

#define Random()  ((float)(rand() & 0x7fff) / (float)0x8000)
#define RandomFull()  ((float)(rand() & 0x7fff) / (float)0x7fff)

// An output device.
class FOutputDevice : public VLogListener
{
public:
  // FOutputDevice interface.
  virtual ~FOutputDevice();

  // Simple text printing.
  void Log(const char *S);
  void Log(EName Type, const char *S);
  void Log(const VStr &S);
  void Log(EName Type, const VStr &S);
  void Logf(const char *Fmt, ...) __attribute__((format(printf, 2, 3)));
  void Logf(EName Type, const char *Fmt, ...) __attribute__((format(printf, 3, 4)));
};

//  Error logs.
extern FOutputDevice *GLogSysError;
extern FOutputDevice *GLogHostError;

int superatoi(const char *s);

int PassFloat(float f);

int ParseHex(const char *Str);
vuint32 M_ParseColour(VStr Name);

void M_RgbToHsv(vuint8, vuint8, vuint8, vuint8&, vuint8&, vuint8&);
void M_RgbToHsv(float, float, float, float&, float&, float&);
void M_HsvToRgb(vuint8, vuint8, vuint8, vuint8&, vuint8&, vuint8&);
void M_HsvToRgb(float, float, float, float&, float&, float&);


// see https://www.compuphase.com/cmetric.htm
static inline __attribute__((unused)) vint32 rgbDistanceSquared (vuint8 r0, vuint8 g0, vuint8 b0, vuint8 r1, vuint8 g1, vuint8 b1) {
  const vint32 rmean = ((vint32)r0+(vint32)r1)/2;
  const vint32 r = (vint32)r0-(vint32)r1;
  const vint32 g = (vint32)g0-(vint32)g1;
  const vint32 b = (vint32)b0-(vint32)b1;
  return (((512+rmean)*r*r)/256)+4*g*g+(((767-rmean)*b*b)/256);
}
