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
static vuint32 LookupColourName (const VStr &Name) {
  static TMapNC<VName, vuint32> cmap; // names are lowercased
  static bool loaded = false;
  char tmpbuf[64];

  if (!loaded) {
    loaded = true;

    // check that X111R6RGB lump exists
    int Lump = W_CheckNumForName(NAME_x11r6rgb);
    if (Lump < 0) {
      GCon->Logf("X11R6RGB lump not found");
      return 0;
    }

    // read the lump
    VStream *Strm = W_CreateLumpReaderNum(Lump);
    int sz = Strm->TotalSize();
    char *Buf = new char[sz+2];
    if (sz) Strm->Serialise(Buf, sz);
    Buf[sz] = '\n';
    Buf[sz+1] = 0;
    if (Strm->IsError()) Buf[0] = 0;
    delete Strm;

    // parse it
    vuint8 *pBuf = (vuint8 *)Buf;
    for (;;) {
      vuint8 ch = *pBuf++;
      if (ch == 0) break;
      if (ch <= ' ') continue;
      if (ch == '!') {
        // comment, skip line
        while (*pBuf != '\n') ++pBuf;
        continue;
      }
      // should have three decimal numbers
      --pBuf;
      vuint8 *start = pBuf;
      int cc[3];
      cc[0] = cc[1] = cc[2] = -1;
      for (int f = 0; f < 3; ++f) {
        while (*pBuf && *pBuf <= ' ' && *pBuf != '\n') ++pBuf;
        if (pBuf[0] == '\n') break;
        int n = -1;
        while (*pBuf > ' ') {
          int d = VStr::digitInBase(*pBuf, 10);
          if (d < 0) { n = -1; break; }
          if (n < 0) n = 0;
          n = n*10+d;
          //GCon->Logf("  n=%d; d=%d; char=%c", n, d, (char)*pBuf);
          if (n > 255) { n = -1; break; }
          ++pBuf;
        }
        //GCon->Logf("DONE: n=%d", n);
        if (n >= 0 && pBuf[0] == '\n') n = -1;
        if (n < 0) { cc[0] = cc[1] = cc[2] = -1; break; }
        cc[f] = n;
      }
      if (cc[0] < 0 || cc[1] < 0 || cc[2] < 0 || pBuf[0] == '\n') {
        // invalid, skip line
       error:
        while (*pBuf != '\n') ++pBuf;
        *pBuf = 0;
        GCon->Logf(NAME_Warning, "Invalid color definition: <%s>", (char *)start);
        *pBuf = '\n';
        continue;
      }
      //GCon->Logf("CC: [0]=%d; [1]=%d; [2]=%d", cc[0], cc[1], cc[2]);
      // get name
      while (*pBuf != '\n' && *pBuf <= ' ') ++pBuf;
      if (pBuf[0] == '\n') continue;
      // collect name
      size_t tbpos = 0;
      while (pBuf[0] != '\n') {
        ch = *pBuf++;
        if (ch <= ' ') {
          /*
          if (tbpos && tmpbuf[tbpos-1] != ' ') {
            if (tbpos >= sizeof(tmpbuf)-1) goto error;
            tmpbuf[tbpos++] = ' ';
          }
          */
        } else {
          if (tbpos >= sizeof(tmpbuf)-1) goto error;
          if (ch >= 'A' && ch <= 'Z') ch += 32; // poor man's tolower
          tmpbuf[tbpos++] = (char)ch;
        }
      }
      // remove trailing spaces
      while (tbpos > 0 && tmpbuf[tbpos-1] == ' ') --tbpos;
      if (tbpos > 0) {
        vuint32 clr = 0xff000000U|(((vuint32)cc[0])<<16)|(((vuint32)cc[1])<<8)|((vuint32)cc[2]);
        tmpbuf[tbpos] = 0;
        VName n = VName(tmpbuf);
        cmap.put(n, clr);
        /*
        char *dstr = (char *)Z_Malloc(tbpos+1);
        strcpy(dstr, tmpbuf);
        cmap.put(dstr, clr);
        if (!cmap.find(dstr)) Sys_Error("!!! <%s>", dstr);
        *pBuf = 0;
        GCon->Logf("COLOR: %3d %3d %3d  <%s> %08x  <%s>", cc[0], cc[1], cc[2], dstr, clr, (char *)start);
        *pBuf = '\n';
        */
      }
    }
    GCon->Logf(NAME_Init, "loaded %d color names", cmap.length());

    if (!cmap.find(VName("ivory"))) Sys_Error("!!! IVORY");

    delete Buf;
  }

  // normalize color name
  size_t dpos = 0;
  for (const vuint8 *s = (const vuint8 *)(*Name); *s; ++s) {
    vuint8 ch = *s;
    if (ch == '"' || ch == '\'') continue; // why not?
    if (ch <= ' ') {
      /*
      if (dpos > 0 && tmpbuf[dpos-1] != ' ') {
        if (dpos >= sizeof(tmpbuf)-1) { dpos = 0; break; }
        tmpbuf[dpos++] = ' ';
      }
      */
    } else {
      if (dpos >= sizeof(tmpbuf)-1) { dpos = 0; break; }
      if (ch >= 'A' && ch <= 'Z') ch += 32; // poor man's tolower
      tmpbuf[dpos++] = (char)ch;
    }
  }
  if (dpos == 0) return 0;
  tmpbuf[dpos] = 0;

  if (tmpbuf[0] == '#') {
    //GCon->Logf("HTML COLOR <%s> (%u)", tmpbuf, (unsigned)dpos);
    // looks like an HTML-style colur
    if (dpos == 7) {
      vuint32 clr = 0;
      for (int f = 1; f < 7; ++f) {
        int d = VStr::digitInBase(tmpbuf[f], 16);
        if (d < 0) return 0;
        clr = (clr<<4)|(d&0x0f);
      }
      //GCon->Logf("HTML COLOR <%s>:<%s>=0x%08x", tmpbuf, *Name, clr);
      return clr|0xff000000U;
    } else if (dpos == 4) {
      vuint32 clr = 0;
      for (int f = 1; f < 4; ++f) {
        int d = VStr::digitInBase(tmpbuf[f], 16);
        if (d < 0) return 0;
        clr = (clr<<4)|(d&0x0f);
        clr = (clr<<4)|(d&0x0f);
      }
      //GCon->Logf("HTML COLOR <%s>:<%s>=0x%08x", tmpbuf, *Name, clr);
      return clr|0xff000000U;
    }
    return 0;
  }

  VName cnx = VName(tmpbuf, VName::Find);
  if (cnx == NAME_None) return 0;

  auto cpp = cmap.find(tmpbuf);
  /*
  if (cpp) {
    GCon->Logf("*** FOUND COLOR <%s> : <%s> : 0x%08x", *Name, tmpbuf, *cpp);
  } else {
    GCon->Logf("*** NOT FOUND COLOR <%s> : <%s>", *Name, tmpbuf);
  }
  */
  //if (cpp) GCon->Logf("*** FOUND COLOR <%s> : <%s> : 0x%08x", *Name, tmpbuf, *cpp);
  return (cpp ? *cpp : 0);
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
vuint32 M_ParseColour (const VStr &Name) {
  if (!Name.Length()) return 0xff000000U;
  vuint32 res = LookupColourName(Name);
  if (res) return res;
  vuint8 Col[3];
  if (Name[0] == '#') {
    // looks like an HTML-style colur
    if (Name.Length() == 7) {
      // #rrggbb format colour
      for (int i = 0; i < 3; ++i) {
        char Val[3];
        Val[0] = Name[i*2+1];
        Val[1] = Name[i*2+2];
        Val[2] = 0;
        Col[i] = ParseHex(Val);
      }
    } else if (Name.Length() == 4) {
      // #rgb format colour
      for (int i = 0; i < 3; ++i) {
        char Val[3];
        Val[0] = Name[i+1];
        Val[1] = Name[i+1];
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
    bool warnColor = false;
    // treat like space separated hex values
    const vuint8 *s = (const vuint8 *)(*Name);
    for (int i = 0; i < 3; ++i) {
      // skip whitespace and quotes
      while (*s && (*s <= ' ' || *s == '"' || *s == '\'')) ++s;
      if (!s[0] || VStr::digitInBase(s[0], 16) < 0) {
        GCon->Logf(NAME_Warning, "Invalid color <%s>", *Name);
        return 0xff000000U; // black
      }
      // parse hex
      int digCount = 0;
      int n = 0;
      while (*s) {
        if (s[0] <= ' ') break;
        int d = VStr::digitInBase(s[0], 16);
        if (d < 0) {
          GCon->Logf(NAME_Warning, "Invalid color <%s>", *Name);
          return 0xff000000U; // black
        }
        n = n*16+d;
        if (n > 0xffffff) n = 0xffff;
        ++s;
        ++digCount;
      }
      if (n > 255) { warnColor = true; n = 255; }
      if (digCount == 1) n = (n<<4)|n;
      Col[i] = n;
    }
    if (warnColor) GCon->Logf(NAME_Warning, "Invalid color <%s>", *Name);
    //GCon->Logf("*** COLOR <%s> is 0x%08x", *Name, 0xff000000U|(((vuint32)Col[0])<<16)|(((vuint32)Col[1])<<8)|((vuint32)Col[2]));
  }
  return 0xff000000U|(((vuint32)Col[0])<<16)|(((vuint32)Col[1])<<8)|((vuint32)Col[2]);
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
