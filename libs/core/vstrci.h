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

// ////////////////////////////////////////////////////////////////////////// //
// this is special class to be used in hash table as case-insensitive string
class VStrCI : public VStr {
public:
  VStrCI (ENoInit) {}
  VStrCI () : VStr() {}
  VStrCI (const char *instr) : VStr(instr) {}
  VStrCI (const VStr &instr) : VStr(instr) {}
  VStrCI (const VStrCI &instr) : VStr(instr) {}

  // assignement operators
  inline VStrCI &operator = (const char *instr) { setContent(instr); return *this; }
  inline VStrCI &operator = (const VStr &instr) { assign(instr); return *this; }
  inline VStrCI &operator = (const VStrCI &instr) { assign(instr); return *this; }

  // comparison operators
  friend bool operator == (const VStrCI &S1, const char *S2) { return (ICmp(*S1, S2) == 0); }
  friend bool operator == (const VStrCI &S1, const VStr &S2) { return (S1.getData() == *S2 ? true : (ICmp(*S1, *S2) == 0)); }
  friend bool operator != (const VStrCI &S1, const char *S2) { return (ICmp(*S1, S2) != 0); }
  friend bool operator != (const VStrCI &S1, const VStr &S2) { return (S1.getData() == *S2 ? false : (ICmp(*S1, *S2) != 0)); }
  friend bool operator < (const VStrCI &S1, const char *S2) { return (ICmp(*S1, S2) < 0); }
  friend bool operator < (const VStrCI &S1, const VStr &S2) { return (S1.getData() == *S2 ? false : (ICmp(*S1, *S2) < 0)); }
  friend bool operator > (const VStrCI &S1, const char *S2) { return (ICmp(*S1, S2) > 0); }
  friend bool operator > (const VStrCI &S1, const VStr &S2) { return (S1.getData() == *S2 ? false : (ICmp(*S1, *S2) > 0)); }
  friend bool operator <= (const VStrCI &S1, const char *S2) { return (ICmp(*S1, S2) <= 0); }
  friend bool operator <= (const VStrCI &S1, const VStr &S2) { return (S1.getData() == *S2 ? true : (ICmp(*S1, *S2) <= 0)); }
  friend bool operator >= (const VStrCI &S1, const char *S2) { return (ICmp(*S1, S2) >= 0); }
  friend bool operator >= (const VStrCI &S1, const VStr &S2) { return (S1.getData() == *S2 ? true : (ICmp(*S1, *S2) >= 0)); }

  friend bool operator == (const VStr &S1, const VStrCI &S2) { return (*S1 == S2.getData() ? true : (ICmp(*S1, *S2) == 0)); }
  friend bool operator != (const VStr &S1, const VStrCI &S2) { return (*S1 == S2.getData() ? false : (ICmp(*S1, *S2) != 0)); }
  friend bool operator < (const VStr &S1, const VStrCI &S2) { return (*S1 == S2.getData() ? false : (ICmp(*S1, *S2) < 0)); }
  friend bool operator > (const VStr &S1, const VStrCI &S2) { return (*S1 == S2.getData() ? false : (ICmp(*S1, *S2) > 0)); }
  friend bool operator <= (const VStr &S1, const VStrCI &S2) { return (*S1 == S2.getData() ? true : (ICmp(*S1, *S2) <= 0)); }
  friend bool operator >= (const VStr &S1, const VStrCI &S2) { return (*S1 == S2.getData() ? true : (ICmp(*S1, *S2) >= 0)); }

  friend bool operator == (const VStrCI &S1, const VStrCI &S2) { return (S1.getData() == S2.getData() ? true : (ICmp(*S1, *S2) == 0)); }
  friend bool operator != (const VStrCI &S1, const VStrCI &S2) { return (S1.getData() == S2.getData() ? false : (ICmp(*S1, *S2) != 0)); }
  friend bool operator < (const VStrCI &S1, const VStrCI &S2) { return (S1.getData() == S2.getData() ? false : (ICmp(*S1, *S2) < 0)); }
  friend bool operator > (const VStrCI &S1, const VStrCI &S2) { return (S1.getData() == S2.getData() ? false : (ICmp(*S1, *S2) > 0)); }
  friend bool operator <= (const VStrCI &S1, const VStrCI &S2) { return (S1.getData() == S2.getData() ? true : (ICmp(*S1, *S2) <= 0)); }
  friend bool operator >= (const VStrCI &S1, const VStrCI &S2) { return (S1.getData() == S2.getData() ? true : (ICmp(*S1, *S2) >= 0)); }
};

static inline __attribute__((unused)) vuint32 GetTypeHash (const VStrCI &s) { return fnvHashStrCI(*s); }
