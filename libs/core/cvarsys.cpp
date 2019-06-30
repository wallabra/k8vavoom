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
#include "core.h"

#define USE_SIMPLE_HASHFN


void (*VCvar::ChangedCB) (VCvar *cvar, const VStr &oldValue) = nullptr;
void (*VCvar::CreatedCB) (VCvar *cvar) = nullptr;
void (*VCvar::UserInfoSetCB) (VCvar *cvar) = nullptr;
void (*VCvar::ServerInfoSetCB) (VCvar *cvar) = nullptr;

bool VCvar::Initialised = false;
bool VCvar::Cheating = false;

static bool hostInitComplete = false;


#define CVAR_HASH_SIZE  (512)
static VCvar *cvhBuckets[CVAR_HASH_SIZE] = {nullptr};


// ////////////////////////////////////////////////////////////////////////// //
static inline vuint32 cvnamehash (const char *buf) {
  if (!buf || !buf[0]) return 1;
#ifdef USE_SIMPLE_HASHFN
  return djbHashBufCI(buf, strlen(buf));
#else
  return fnvHashBufCI(buf, strlen(buf));
#endif
}


// `pat` must be in [a-z] range
static bool xstrEquCI (const char *s, const char *pat) {
  if (!s || !pat || !s[0] || !pat[0]) return false;
  while (*s && *(const vuint8 *)s <= ' ') ++s;
  while (*s && *pat) {
    const char c0 = (*s++)|32; // lowercase
    const char c1 = *pat++;
    if (c0 != c1) return false;
  }
  if (*pat || *(const vuint8 *)s > ' ') return false;
  while (*s && *(const vuint8 *)s <= ' ') ++s;
  return (s[0] == 0);
}



//==========================================================================
//
//  VCvar::ParseBool
//
//==========================================================================
bool VCvar::ParseBool (const char *s) {
  if (!s || !s[0]) return false;
  // skip leading spaces here, so `xstrEquCI()` will have less work to do
  while (*s && *(const vuint8 *)s <= ' ') ++s;
  // compare with known string values
  if (xstrEquCI(s, "true") ||
      xstrEquCI(s, "tan") ||
      xstrEquCI(s, "yes") ||
      xstrEquCI(s, "t") ||
      xstrEquCI(s, "y"))
  {
    return true;
  }
  // integer zero or floating point zero is false
  if (*s == '-' || *s == '+') ++s;
  if (s[0] != '0') return false;
  while (s[0] == '0') ++s;
  if (s[0] == '.') {
    ++s;
    if (s[0]) {
      if (s[0] != '0') return false;
      while (s[0] == '0') ++s;
    }
  }
  while (*s && *(const vuint8 *)s <= ' ') ++s;
  return !s[0];
}


//==========================================================================
//
//  VCvar::HostInitComplete
//
//==========================================================================
void VCvar::HostInitComplete () {
  hostInitComplete = true;
}


//==========================================================================
//
//  VCvar::VCvar
//
//==========================================================================
VCvar::VCvar (const char *AName, const char *ADefault, const char *AHelp, int AFlags)
  : Name(AName)
  , DefaultString(ADefault)
  , HelpString(AHelp)
  , defstrOwned(false)
  , Flags(AFlags)
  , IntValue(0)
  , FloatValue(0)
  , BoolValue(false)
  , nextInBucket(nullptr)
  , MeChangedCB(nullptr)
{
  if (!DefaultString) DefaultString = ""; // 'cause why not?

  if (!HelpString || !HelpString[0]) HelpString = "no help yet (FIXME!)";

  DoSet(DefaultString);
  if (Name && Name[0]) {
    insertIntoHash(); // insert into hash (this leaks on duplicate vars)
  }
}


//==========================================================================
//
//  VCvar::VCvar
//
//==========================================================================
VCvar::VCvar (const char *AName, const VStr &ADefault, const VStr &AHelp, int AFlags)
  : Name(AName)
  , HelpString("no help yet")
  , defstrOwned(true)
  , Flags(AFlags)
  , IntValue(0)
  , FloatValue(0)
  , BoolValue(false)
  , nextInBucket(nullptr)
  , MeChangedCB(nullptr)
{
  char *Tmp = new char[ADefault.Length()+1];
  VStr::Cpy(Tmp, *ADefault);
  DefaultString = Tmp;

  if (AHelp.Length() > 0) {
    Tmp = new char[AHelp.Length()+1];
    VStr::Cpy(Tmp, *AHelp);
    HelpString = Tmp;
  }

  DoSet(DefaultString);
  if (Name && Name[0]) {
    insertIntoHash(); // insert into hash (this leaks on duplicate vars)
    check(Initialised);
  }
}


//==========================================================================
//
//  VCvar::insertIntoHash
//
//  returns replaced cvar, or nullptr
//
//==========================================================================
VCvar *VCvar::insertIntoHash () {
  if (!this->Name || !this->Name[0]) return nullptr;
  if (CreatedCB) CreatedCB(this);
  vuint32 nhash = cvnamehash(this->Name);
  this->lnhash = nhash;
  VCvar *prev = nullptr;
  for (VCvar *cvar = cvhBuckets[nhash%CVAR_HASH_SIZE]; cvar; prev = cvar, cvar = cvar->nextInBucket) {
    if (cvar->lnhash == nhash && !VStr::ICmp(this->Name, cvar->Name)) {
      // replace it
      if (prev) {
        prev->nextInBucket = this;
      } else {
        cvhBuckets[nhash%CVAR_HASH_SIZE] = this;
      }
      this->nextInBucket = cvar->nextInBucket;
      return cvar;
    }
  }
  // new one
  this->nextInBucket = cvhBuckets[nhash%CVAR_HASH_SIZE];
  cvhBuckets[nhash%CVAR_HASH_SIZE] = this;
  return nullptr;
}


//==========================================================================
//
//  VCvar::Set
//
//==========================================================================
void VCvar::Set (int value) {
  Set(VStr(value));
}


//==========================================================================
//
//  VCvar::Set
//
//==========================================================================
void VCvar::Set (float value) {
  Set(VStr(value));
}


//==========================================================================
//
//  VCvar::Set
//
//==========================================================================
void VCvar::Set (const VStr &AValue) {
  if (Flags&CVAR_Latch) {
    LatchedString = AValue;
    return;
  }

  if (AValue == StringValue) return;

  if (Initialised) {
    if ((Flags&CVAR_Cheat) != 0 && !Cheating) {
      GLog.Logf("'%s' cannot be changed while cheating is disabled", Name);
      return;
    }
  }

  DoSet(AValue);

  if (Initialised) Flags |= CVAR_Modified;
}


//==========================================================================
//
//  VCvar::SetAsDefault
//
//==========================================================================
void VCvar::SetDefault (const VStr &value) {
  if (value.strEqu(DefaultString)) return;
  if (defstrOwned) {
    delete[] const_cast<char *>(DefaultString);
  }
  char *tmp = new char[value.length()+1];
  VStr::Cpy(tmp, *value);
  DefaultString = tmp;
  defstrOwned = true;
}


//==========================================================================
//
//  VCvar::DoSet
//
//  does the actual value assignement
//
//==========================================================================
void VCvar::DoSet (const VStr &AValue) {
  if (StringValue == AValue) return; // nothing to do
  VStr oldValue = StringValue; // we'll need it later

  StringValue = AValue;
  bool validInt = VStr::convertInt(*StringValue, &IntValue);
  bool validFloat = VStr::convertFloat(*StringValue, &FloatValue);

  if (!validInt) IntValue = 0;
  if (!validFloat) FloatValue = 0.0f;

  // interpret boolean
  if (validFloat) {
    // easy
    BoolValue = (isFiniteF(FloatValue) && FloatValue != 0);
  } else if (validInt) {
    // easy
    BoolValue = (IntValue != 0);
  } else {
    // try to parse as bool
    BoolValue = ParseBool(*StringValue);
    IntValue = (BoolValue ? 1 : 0);
    FloatValue = IntValue;
  }

  if (!validInt && validFloat) {
    if (FloatValue >= MIN_VINT32 && FloatValue <= MAX_VINT32) {
      IntValue = (int)FloatValue;
    } else {
      IntValue = (FloatValue < 0 ? MIN_VINT32 : MAX_VINT32);
    }
  }

  if (!validFloat && validInt) FloatValue = IntValue; // let's hope it fits

  if (MeChangedCB) MeChangedCB(this, oldValue);
  if (ChangedCB) ChangedCB(this, oldValue);

  if (Initialised) {
    if ((Flags&CVAR_UserInfo) && UserInfoSetCB) UserInfoSetCB(this);
    if ((Flags&CVAR_ServerInfo) && ServerInfoSetCB) ServerInfoSetCB(this);
  }
}


//==========================================================================
//
//  VCvar::Init
//
//==========================================================================
void VCvar::Init () {
  if (!Initialised) {
    /*
    for (vuint32 bkn = 0; bkn < CVAR_HASH_SIZE; ++bkn) {
      for (VCvar *cvar = cvhBuckets[bkn]; cvar; cvar = cvar->nextInBucket) {
        cvar->Register();
      }
    }
    */
    Initialised = true;
  }
}


//==========================================================================
//
//  VCvar::SendAllUserInfos
//
//==========================================================================
void VCvar::SendAllUserInfos () {
  if (!UserInfoSetCB) return;
  for (vuint32 bkn = 0; bkn < CVAR_HASH_SIZE; ++bkn) {
    for (VCvar *cvar = cvhBuckets[bkn]; cvar; cvar = cvar->nextInBucket) {
      if (cvar->Flags&CVAR_UserInfo) UserInfoSetCB(cvar);
    }
  }
}


//==========================================================================
//
//  VCvar::SendAllServerInfos
//
//==========================================================================
void VCvar::SendAllServerInfos () {
  if (!ServerInfoSetCB) return;
  for (vuint32 bkn = 0; bkn < CVAR_HASH_SIZE; ++bkn) {
    for (VCvar *cvar = cvhBuckets[bkn]; cvar; cvar = cvar->nextInBucket) {
      if (cvar->Flags&CVAR_ServerInfo) ServerInfoSetCB(cvar);
    }
  }
}


//==========================================================================
//
//  VCvar::dumpHashStats
//
//==========================================================================
void VCvar::AddAllVarsToAutocomplete (void (*addfn) (const char *name)) {
  if (!addfn) return;
  for (vuint32 bkn = 0; bkn < CVAR_HASH_SIZE; ++bkn) {
    for (VCvar *cvar = cvhBuckets[bkn]; cvar; cvar = cvar->nextInBucket) {
      if (!cvar->Name || !cvar->Name[0] || cvar->Name[0] == '_' ||
          (vuint8)cvar->Name[0] <= ' ' || (vuint8)cvar->Name[0] >= 128)
      {
        continue;
      }
      addfn(cvar->Name);
    }
  }
}


//==========================================================================
//
//  VCvar::dumpHashStats
//
//==========================================================================
void VCvar::dumpHashStats () {
  vuint32 bkused = 0, maxchain = 0;
  for (vuint32 bkn = 0; bkn < CVAR_HASH_SIZE; ++bkn) {
    VCvar *cvar = cvhBuckets[bkn];
    if (!cvar) continue;
    ++bkused;
    vuint32 chlen = 0;
    for (; cvar; cvar = cvar->nextInBucket) ++chlen;
    if (chlen > maxchain) maxchain = chlen;
  }
  GLog.Logf("CVAR statistics: %u buckets used, %u items in longest chain", bkused, maxchain);
}


//==========================================================================
//
//  VCvar::Shutdown
//
//  this is called only once on engine shutdown,
//  so don't bother with deletion
//
//==========================================================================
void VCvar::Shutdown () {
  if (Initialised) {
    dumpHashStats();
    for (vuint32 bkn = 0; bkn < CVAR_HASH_SIZE; ++bkn) {
      for (VCvar *cvar = cvhBuckets[bkn]; cvar; cvar = cvar->nextInBucket) {
        // free default value
        if (cvar->defstrOwned) {
          delete[] const_cast<char *>(cvar->DefaultString);
          cvar->DefaultString = ""; // set to some sensible value
        }
      }
    }
    Initialised = false;
  }
}


//==========================================================================
//
//  VCvar::Unlatch
//
//==========================================================================
void VCvar::Unlatch () {
  for (vuint32 bkn = 0; bkn < CVAR_HASH_SIZE; ++bkn) {
    for (VCvar *cvar = cvhBuckets[bkn]; cvar; cvar = cvar->nextInBucket) {
      if (cvar->LatchedString.IsNotEmpty()) {
        cvar->DoSet(cvar->LatchedString);
        cvar->LatchedString.Clean();
      }
    }
  }
}


//==========================================================================
//
//  VCvar::CreateNew
//
//==========================================================================
void VCvar::CreateNew (VName var_name, const VStr &ADefault, const VStr &AHelp, int AFlags) {
  if (var_name == NAME_None) return;
  VCvar *cvar = FindVariable(*var_name);
  if (!cvar) {
    new VCvar(*var_name, ADefault, AHelp, AFlags);
  } else {
    // delete old default value if necessary
    if (cvar->defstrOwned) delete[] const_cast<char *>(cvar->DefaultString);
    // set new default value
    {
      char *Tmp = new char[ADefault.Length()+1];
      VStr::Cpy(Tmp, *ADefault);
      cvar->DefaultString = Tmp;
      cvar->defstrOwned = true;
    }
    // set new help value
    if (AHelp.Length() > 0) {
      char *Tmp = new char[AHelp.Length()+1];
      VStr::Cpy(Tmp, *AHelp);
      cvar->HelpString = Tmp;
    } else {
      cvar->HelpString = "no help yet";
    }
    // update flags
    cvar->Flags = AFlags;
  }
}


//==========================================================================
//
//  VCvar::HasVar
//
//==========================================================================
bool VCvar::HasVar (const char *var_name) {
  return (FindVariable(var_name) != nullptr);
}


//==========================================================================
//
//  VCvar::HasModVar
//
//==========================================================================
bool VCvar::HasModVar (const char *var_name) {
  VCvar *var = FindVariable(var_name);
  return (var && var->IsModVar());
}


//==========================================================================
//
//  VCvar::HasModUserVar
//
//==========================================================================
bool VCvar::HasModUserVar (const char *var_name) {
  VCvar *var = FindVariable(var_name);
  return (var && var->IsModVar() && (var->Flags&CVAR_ServerInfo) == 0);
}


//==========================================================================
//
//  VCvar::CanBeModified
//
//==========================================================================
bool VCvar::CanBeModified (const char *var_name, bool modonly, bool noserver) {
  VCvar *var = FindVariable(var_name);
  if (!var) return false;
  if (modonly && !var->IsModVar()) return false;
  if (noserver && (var->Flags&CVAR_Latch) != 0) return false;
  if (var->Flags&(CVAR_Rom|CVAR_Init)) return false;
  if (!Cheating && (var->Flags&CVAR_Cheat) != 0) return false;
  return true;
}


//==========================================================================
//
//  VCvar::FindVariable
//
//==========================================================================
VCvar *VCvar::FindVariable (const char *name) {
  if (!name || name[0] == 0) return nullptr;
  vuint32 nhash = cvnamehash(name);
  for (VCvar *cvar = cvhBuckets[nhash%CVAR_HASH_SIZE]; cvar; cvar = cvar->nextInBucket) {
    if (cvar->lnhash == nhash && !VStr::ICmp(name, cvar->Name)) return cvar;
  }
  return nullptr;
}


//==========================================================================
//
//  VCvar::GetInt
//
//==========================================================================
int VCvar::GetInt (const char *var_name) {
  VCvar *var = FindVariable(var_name);
  return (var ? var->IntValue : 0);
}


//==========================================================================
//
//  VCvar::GetFloat
//
//==========================================================================
float VCvar::GetFloat (const char *var_name) {
  VCvar *var = FindVariable(var_name);
  return (var ? var->FloatValue : 0.0f);
}


//==========================================================================
//
//  VCvar::GetBool
//
//==========================================================================
bool VCvar::GetBool (const char *var_name) {
  VCvar *var = FindVariable(var_name);
  return (var ? var->BoolValue : false);
}


//==========================================================================
//
//  VCvar::GetCharp
//
//==========================================================================
const char *VCvar::GetCharp (const char *var_name) {
  VCvar *var = FindVariable(var_name);
  return (var ? *var->StringValue : "");
}


//==========================================================================
//
//  VCvar::GetString
//
//==========================================================================
const VStr &VCvar::GetString (const char *var_name) {
  VCvar *var = FindVariable(var_name);
  if (!var) return VStr::EmptyString;
  return var->StringValue;
}


//==========================================================================
//
//  VCvar::GetHelp
//
//==========================================================================
const char *VCvar::GetHelp (const char *var_name) {
  VCvar *var = FindVariable(var_name);
  if (!var) return nullptr;
  return var->HelpString;
}


//==========================================================================
//
//  VCvar::GetVarFlags
//
//==========================================================================
int VCvar::GetVarFlags (const char *var_name) {
  VCvar *var = FindVariable(var_name);
  if (!var) return -1;
  return var->GetFlags();
}


//==========================================================================
//
//  VCvar::Set
//
//==========================================================================
void VCvar::Set (const char *var_name, int value) {
  VCvar *var = FindVariable(var_name);
  if (!var) Sys_Error("Cvar_Set: variable %s not found\n", var_name);
  var->Set(value);
}


//==========================================================================
//
//  VCvar::Set
//
//==========================================================================
void VCvar::Set (const char *var_name, float value) {
  VCvar *var = FindVariable(var_name);
  if (!var) Sys_Error("Cvar_Set: variable %s not found\n", var_name);
  var->Set(value);
}


//==========================================================================
//
//  VCvar::Set
//
//==========================================================================
void VCvar::Set (const char *var_name, const VStr &value) {
  VCvar *var = FindVariable(var_name);
  if (!var) Sys_Error("Cvar_SetString: variable %s not found\n", var_name);
  var->Set(value);
}


//==========================================================================
//
//  VCvar::Command
//
//==========================================================================
bool VCvar::Command (const TArray<VStr> &Args) {
  VCvar *cvar = FindVariable(*Args[0]);
  if (!cvar) return false;

  bool needHelp = false;
  if (Args.Num() == 2) {
    for (const char *s = *(Args[1]); *s; ++s) {
      if (*s == ' ') continue;
      if (*s == '?') { needHelp = true; continue; }
      needHelp = false;
      break;
    }
  }

  // perform a variable print or set
  if (Args.Num() == 1) {
    GLog.Logf("%s is \"%s\"", cvar->Name, *cvar->StringValue.quote());
    if ((cvar->Flags&CVAR_Latch) && cvar->LatchedString.IsNotEmpty()) {
      GLog.Logf("Latched \"%s\"", *cvar->LatchedString.quote());
    }
  } else if (needHelp) {
    GLog.Logf("%s: %s", cvar->GetName(), cvar->GetHelp());
  } else {
    if (cvar->Flags&CVAR_Rom) {
      GLog.Logf("'%s' is read-only.", cvar->Name);
    } else if ((cvar->Flags&CVAR_Init) && hostInitComplete) {
      GLog.Logf("'%s' can be set only from command-line.", cvar->Name);
    } else {
      cvar->Set(Args[1]);
    }
  }
  return true;
}


//==========================================================================
//
//  VCvar::countCVars
//
//==========================================================================
vuint32 VCvar::countCVars () {
  vuint32 count = 0;
  for (vuint32 bkn = 0; bkn < CVAR_HASH_SIZE; ++bkn) {
    for (VCvar *cvar = cvhBuckets[bkn]; cvar; cvar = cvar->nextInBucket) {
      ++count;
    }
  }
  return count;
}


// ////////////////////////////////////////////////////////////////////////// //
extern "C" {
  static int vcvcmp (const void *aa, const void *bb, void *udata) {
    const VCvar *a = *(const VCvar **)aa;
    const VCvar *b = *(const VCvar **)bb;
    if (a == b) return 0;
    // mod vars should came last
    if (a->IsModVar()) {
      if (!b->IsModVar()) return 1;
    } else if (b->IsModVar()) {
      if (!a->IsModVar()) return -1;
    }
    return VStr::ICmp(a->GetName(), b->GetName());
  }
}


//==========================================================================
//
//  VCvar::getSortedList
//
//  contains `countCVars()` elements, must be `delete[]`d.
//  can return `nullptr`.
//
//==========================================================================
VCvar **VCvar::getSortedList () {
  vuint32 count = countCVars();
  if (count == 0) return nullptr;

  // allocate array
  VCvar **list = new VCvar*[count];

  // fill it
  count = 0; // reuse counter, why not?
  for (vuint32 bkn = 0; bkn < CVAR_HASH_SIZE; ++bkn) {
    for (VCvar *cvar = cvhBuckets[bkn]; cvar; cvar = cvar->nextInBucket) {
      list[count++] = cvar;
    }
  }

  // sort it
  if (count > 1) timsort_r(list, count, sizeof(list[0]), &vcvcmp, nullptr);

  return list;
}


//==========================================================================
//
//  VCvar::WriteVariablesToFile
//
//==========================================================================
void VCvar::WriteVariablesToStream (VStream *st, bool saveDefaultValues) {
  if (!st) return;
  if (st->IsLoading()) return; // wtf?!
  vuint32 count = countCVars();
  VCvar **list = getSortedList();
  for (vuint32 n = 0; n < count; ++n) {
    VCvar *cvar = list[n];
    if (cvar->Flags&(CVAR_Archive|CVAR_AlwaysArchive)) {
      // do not write variables with default values
      if (!saveDefaultValues && !(cvar->Flags&CVAR_AlwaysArchive)) {
        if (cvar->StringValue.Cmp(cvar->DefaultString) == 0) continue;
      }
      if (cvar->Flags&CVAR_FromMod) {
        st->writef("cvarinfovar");
        if (cvar->Flags&CVAR_ServerInfo) st->writef(" server"); else st->writef(" user");
        if (cvar->Flags&CVAR_Cheat) st->writef(" cheat");
        if (cvar->Flags&CVAR_Latch) st->writef(" latch");
        st->writef(" %s \"%s\"\n", cvar->Name, *cvar->StringValue.quote());
      } else {
        st->writef("%s \"%s\"\n", cvar->Name, *cvar->StringValue.quote());
      }
      if (st->IsError()) break;
    }
  }
  delete[] list;
}


// ////////////////////////////////////////////////////////////////////////// //
VCvarI &VCvarI::operator = (const VCvarB &v) { Set(v.asBool() ? 1 : 0); return *this; }
VCvarI &VCvarI::operator = (const VCvarI &v) { Set(v.IntValue); return *this; }

VCvarF &VCvarF::operator = (const VCvarB &v) { Set(v.asBool() ? 1.0f : 0.0f); return *this; }
VCvarF &VCvarF::operator = (const VCvarI &v) { Set((float)v.asInt()); return *this; }
VCvarF &VCvarF::operator = (const VCvarF &v) { Set(v.FloatValue); return *this; }

VCvarB &VCvarB::operator = (const VCvarB &v) { Set(v.BoolValue ? 1 : 0); return *this; }
VCvarB &VCvarB::operator = (const VCvarI &v) { Set(v.asInt() ? 1 : 0); return *this; }
VCvarB &VCvarB::operator = (const VCvarF &v) { Set(v.asFloat() ? 1 : 0); return *this; }
