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
#include "gamedefs.h"


class VLogSysError : public FOutputDevice {
public:
  virtual void Serialise (const char *V, EName Event) override;
};

class VLogHostError : public FOutputDevice {
public:
  virtual void Serialise (const char *V, EName Event) override;
};


static VLogSysError LogSysError;
static VLogHostError LogHostError;

FOutputDevice *GLogSysError = &LogSysError;
FOutputDevice *GLogHostError = &LogHostError;

static BJPRNGCtx bjprng_ctx;


//==========================================================================
//
//  FOutputDevice implementation
//
//==========================================================================
FOutputDevice::~FOutputDevice () {}
void FOutputDevice:: Log (const char *S) { Serialise(S, NAME_Log); }
void FOutputDevice::Log (EName Type, const char *S) { Serialise(S, Type); }
void FOutputDevice::Log (const VStr &S) { Serialise(*S, NAME_Log); }
void FOutputDevice::Log (EName Type, const VStr &S) { Serialise(*S, Type); }


static char string[32768];

__attribute__((format(printf, 2, 3))) void FOutputDevice::Logf (const char *Fmt, ...) {
  va_list argptr;

  va_start(argptr, Fmt);
  vsnprintf(string, sizeof(string), Fmt, argptr);
  va_end(argptr);

  Serialise(string, NAME_Log);
}

__attribute__((format(printf, 3, 4))) void FOutputDevice::Logf (EName Type, const char *Fmt, ...) {
  va_list argptr;

  va_start(argptr, Fmt);
  vsnprintf(string, sizeof(string), Fmt, argptr);
  va_end(argptr);

  Serialise(string, Type);
}


//==========================================================================
//
//  VLogSysError
//
//==========================================================================
void VLogSysError::Serialise (const char *V, EName) {
  Sys_Error("%s", V);
}


//==========================================================================
//
//  VLogHostError
//
//==========================================================================
void VLogHostError::Serialise (const char *V, EName) {
  Host_Error("%s", V);
}


//==========================================================================
//
//  superatoi
//
//==========================================================================
int superatoi (const char *s) {
  int n = 0, r = 10, x, mul = 1;
  const char *c=s;

  for (; *c; ++c) {
    x = (*c&223)-16;
    if (x == -3) {
      mul = -mul;
    } else if (x == 72 && r == 10) {
      n -= (r = n);
      if (!r) r = 16;
      if (r < 2 || r > 36) return -1;
    } else {
      if (x > 10) x -= 39;
      if (x >= r) return -1;
      n = (n*r)+x;
    }
  }
  return mul*n;
}


//==========================================================================
//
//  LookupColourName
//
//==========================================================================
static VStr LookupColourName (VStr &Name) {
  guard(LookupColourName);
  // check that X111R6RGB lump exists
  int Lump = W_CheckNumForName(NAME_x11r6rgb);
  if (Lump < 0) {
    GCon->Logf("X11R6RGB lump not found");
    return Name;
  }

  // read the lump
  VStream *Strm = W_CreateLumpReaderNum(Lump);
  char *Buf = new char[Strm->TotalSize()+1];
  Strm->Serialise(Buf, Strm->TotalSize());
  Buf[Strm->TotalSize()] = 0;
  char *BufEnd = Buf+Strm->TotalSize();
  delete Strm;

  vuint8 Col[3];
  int Count = 0;
  for (char *pBuf = Buf; pBuf < BufEnd; ) {
    if (*(const vuint8 *)pBuf <= ' ') {
      // skip whitespace
      ++pBuf;
    } else if (Count == 0 && *pBuf == '!') {
      // skip comment
      while (pBuf < BufEnd && *pBuf != '\n') ++pBuf;
    } else if (Count < 3) {
      // parse colour component
      char *pEnd;
      Col[Count] = strtoul(pBuf, &pEnd, 10);
      if (pEnd == pBuf) {
        GCon->Logf("Bad colour component value");
        break;
      }
      pBuf = pEnd;
      ++Count;
    } else {
      // colour name
      char *Start = pBuf;
      while (pBuf < BufEnd && *pBuf != '\n') ++pBuf;
      //  Skip trailing whitespace
      while (pBuf > Start && (vuint8)pBuf[-1] >= 0 && (vuint8)pBuf[-1] <= ' ') --pBuf;
      if (pBuf == Start) {
        GCon->Logf("Missing name of the colour");
        break;
      }
      *pBuf = 0;
      if ((size_t)(pBuf-Start) == (size_t)Name.Length() && Name.ICmp(Start) == 0) {
        char ValBuf[16];
        snprintf(ValBuf, sizeof(ValBuf), "#%02x%02x%02x", Col[0], Col[1], Col[2]);
        delete[] Buf;
        Buf = nullptr;
        return VStr(ValBuf);
      }
      Count = 0;
    }
  }
  delete[] Buf;
  Buf = nullptr;
  return Name;
  unguard;
}


//==========================================================================
//
//  ParseHex
//
//==========================================================================
int ParseHex (const char *Str) {
  int Ret = 0;
  int Mul = 1;
  const char *c = Str;
  if (*c == '-') {
    ++c;
    Mul = -1;
  }
  for (; *c; ++c) {
         if (*c >= '0' && *c <= '9') Ret = (Ret<<4)+*c-'0';
    else if (*c >= 'a' && *c <= 'f') Ret = (Ret<<4)+*c-'a'+10;
    else if (*c >= 'A' && *c <= 'F') Ret = (Ret<<4)+*c-'A'+10;
  }
  return Ret*Mul;
}


//==========================================================================
//
//  M_ParseColour
//
//==========================================================================
vuint32 M_ParseColour (VStr Name) {
  if (!Name.Length()) return 0xff000000;
  VStr Str = LookupColourName(Name);
  vuint8 Col[3];
  if (Str[0] == '#') {
    // looks like an HTML-style colur
    if (Str.Length() == 7) {
      // #rrggbb format colour
      for (int i = 0; i < 3; ++i) {
        char Val[3];
        Val[0] = Str[i*2+1];
        Val[1] = Str[i*2+2];
        Val[2] = 0;
        Col[i] = ParseHex(Val);
      }
    } else if (Str.Length() == 4) {
      // #rgb format colour
      for (int i = 0; i < 3; ++i) {
        char Val[3];
        Val[0] = Str[i+1];
        Val[1] = Str[i+1];
        Val[2] = 0;
        Col[i] = ParseHex(Val);
      }
    } else {
      // assume it's a bad colour value, set it to black
      Col[0] = 0;
      Col[1] = 0;
      Col[2] = 0;
    }
  } else {
    // treat like space separated hex values
    int Idx = 0;
    for (int i = 0; i < 3; ++i) {
      // skip whitespace and quotes
      while (Idx < Str.Length() && (((vuint8)Str[Idx] <= ' ') || ((vuint8)Str[Idx] <= '\"'))) ++Idx;
      int Count = 0;
      char Val[3];
      while (Idx < Str.Length() && ((vuint8)Str[Idx] != ' ')) {
        if (Count < 2) Val[Count++] = Str[Idx];
        ++Idx;
      }
      if (Count == 0) {
        Col[i] = 0;
      } else {
        if (Count == 1) Val[1] = Val[0];
        Val[2] = 0;
        Col[i] = ParseHex(Val);
      }
    }
  }
  return 0xff000000|(Col[0]<<16)|(Col[1]<<8)|Col[2];
}


//==========================================================================
//
//  M_RgbToHsv
//
//==========================================================================
void M_RgbToHsv (vuint8 r, vuint8 g, vuint8 b, vuint8 &h, vuint8 &s, vuint8 &v) {
  vuint8 min = MIN(MIN(r, g), b);
  vuint8 max = MAX(MAX(r, g), b);
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
  float min = MIN(MIN(r, g), b);
  float max = MAX(MAX(r, g), b);
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


//==========================================================================
//
//  RandomInit
//
//  call this to seed with random seed
//
//==========================================================================
void RandomInit () {
  vint32 rn;
  do { ed25519_randombytes(&rn, sizeof(rn)); } while (!rn);
  bjprng_raninit(&bjprng_ctx, rn);
}


//==========================================================================
//
//  Random
//
//==========================================================================
float Random () {
  for (;;) {
    float v = ((double)bjprng_ranval(&bjprng_ctx))/((double)0xffffffffu);
    if (!isFiniteF(v)) continue;
    if (v < 1.0f) return v;
  }
}


//==========================================================================
//
//  RandomFull
//
//==========================================================================
float RandomFull () {
  for (;;) {
    float v = ((double)bjprng_ranval(&bjprng_ctx))/((double)0xffffffffu);
    if (!isFiniteF(v)) continue;
    return v;
  }
}


//==========================================================================
//
//  P_Random
//
//==========================================================================
vuint8 P_Random () {
  return bjprng_ranval(&bjprng_ctx)&0xff;
}
