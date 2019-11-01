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
//#define Random()  ((float)(rand() & 0x7fff) / (float)0x8000)
//#define RandomFull()  ((float)(rand() & 0x7fff) / (float)0x7fff)


// this is used to compare floats like ints which is faster
#define FASI(var_) (*(const int32_t *)&(var_))


// An output device.
class FOutputDevice : public VLogListener {
public:
  // FOutputDevice interface
  virtual ~FOutputDevice () noexcept;

  // simple text printing
  void Log (const char *S) noexcept;
  void Log (EName Type, const char *S) noexcept;
  void Log (VStr S) noexcept;
  void Log (EName Type, VStr S) noexcept;
  void Logf (const char *Fmt, ...) noexcept __attribute__((format(printf, 2, 3)));
  void Logf (EName Type, const char *Fmt, ...) noexcept __attribute__((format(printf, 3, 4)));
};

// error logs
//extern FOutputDevice *GLogSysError;
//extern FOutputDevice *GLogHostError;


__attribute__((warn_unused_result)) int superatoi (const char *s) noexcept;

//__attribute__((warn_unused_result)) int ParseHex (const char *Str);
__attribute__((warn_unused_result)) vuint32 M_LookupColorName (const char *Name); // returns 0 if not found (otherwise high bit is set)
// this returns color with high byte set to `0xff` (and black color for unknown names)
// but when `retZeroIfInvalid` is `true`, it returns `0` for unknown color
__attribute__((warn_unused_result)) vuint32 M_ParseColor (const char *Name, bool retZeroIfInvalid=false);
