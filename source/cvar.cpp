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
#include "net/network.h"
#include "sv_local.h"

#if 0
#define USE_SIMPLE_HASHFN

bool VCvar::Initialised = false;
bool VCvar::Cheating;

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
static bool xstrcmpCI (const char *s, const char *pat) {
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


// ////////////////////////////////////////////////////////////////////////// //
VCvar::VCvar(const char *AName, const char *ADefault, const char *AHelp, int AFlags)
  : Name(AName)
  , DefaultString(ADefault)
  , HelpString(AHelp)
  , defstrOwned(false)
  , Flags(AFlags)
  , IntValue(0)
  , FloatValue(0)
  , BoolValue(false)
  , nextInBucket(nullptr)
{
  if (!DefaultString) DefaultString = ""; // 'cause why not?

  if (!HelpString || !HelpString[0]) HelpString = "no help yet (FIXME!)";

  DoSet(DefaultString);
  if (Name && Name[0]) {
    insertIntoHash(); // insert into hash (this leaks on duplicate vars)
    if (Initialised) Register();
  }
}


VCvar::VCvar(const char *AName, const VStr &ADefault, const VStr &AHelp, int AFlags)
  : Name(AName)
  , HelpString("no help yet")
  , defstrOwned(true)
  , Flags(AFlags)
  , IntValue(0)
  , FloatValue(0)
  , BoolValue(false)
  , nextInBucket(nullptr)
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
    Register();
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// returns replaced cvar, or nullptr
VCvar *VCvar::insertIntoHash () {
  if (!this->Name || !this->Name[0]) return nullptr;
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


// ////////////////////////////////////////////////////////////////////////// //
void VCvar::Register () {
  VCommand::AddToAutoComplete(Name);
  //DoSet(DefaultString);
}


// ////////////////////////////////////////////////////////////////////////// //
void VCvar::Set (int value) {
  Set(VStr(value));
}


void VCvar::Set (float value) {
  Set(VStr(value));
}


void VCvar::Set (const VStr &AValue) {
  if (Flags&CVAR_Latch) {
    LatchedString = AValue;
    return;
  }

  if (AValue == StringValue) return;

  if (Initialised) {
    if ((Flags&CVAR_Cheat) != 0 && !Cheating) {
      GCon->Logf("'%s' cannot be changed while cheating is disabled", Name);
      return;
    }
  }

  DoSet(AValue);

  if (Initialised) Flags |= CVAR_Modified;
}


// ////////////////////////////////////////////////////////////////////////// //
// does the actual value assignement
void VCvar::DoSet (const VStr &AValue) {
  StringValue = AValue;
  bool validInt = VStr::convertInt(*StringValue, &IntValue);
  bool validFloat = VStr::convertFloat(*StringValue, &FloatValue);

  if (!validInt) IntValue = 0;
  if (!validFloat) FloatValue = 0.0f;

  //fprintf(stderr, "::: <%s>: s=<%s>; i(%d)=%d; f(%d)=%f\n", Name, *StringValue, (validInt ? 1 : 0), IntValue, (validFloat ? 1 : 0), FloatValue);

  // interpret boolean
  if (validFloat) {
    // easy
    BoolValue = (FloatValue != 0);
  } else if (validInt) {
    // easy
    BoolValue = (IntValue != 0);
  } else {
    // check various strings
    BoolValue =
      xstrcmpCI(*StringValue, "true") ||
      xstrcmpCI(*StringValue, "on") ||
      xstrcmpCI(*StringValue, "tan") ||
      xstrcmpCI(*StringValue, "yes");
    IntValue = (BoolValue ? 1 : 0);
    FloatValue = IntValue;
    //fprintf(stderr, "CVAR: badint; str=<%s>; b=%d; i=%d; f=%f\n", *StringValue, (BoolValue ? 1 : 0), IntValue, FloatValue);
    //fprintf(stderr, "::: BOOL: <%s>: s=<%s>; i(%d)=%d; f(%d)=%f; b=%d\n", Name, *StringValue, (validInt ? 1 : 0), IntValue, (validFloat ? 1 : 0), FloatValue, (BoolValue ? 1 : 0));
  }
  if (!validInt && validFloat) {
    if (FloatValue >= MIN_VINT32 && FloatValue <= MAX_VINT32) {
      IntValue = (int)FloatValue;
    } else {
      IntValue = (FloatValue < 0 ? MIN_VINT32 : MAX_VINT32);
    }
  }

  if (Initialised) {
#ifdef CLIENT
    if (Flags&CVAR_UserInfo) {
      Info_SetValueForKey(cls.userinfo, Name, *StringValue);
      if (cl) {
        if (GGameInfo->NetMode == NM_TitleMap ||
            GGameInfo->NetMode == NM_Standalone ||
            GGameInfo->NetMode == NM_ListenServer)
        {
          VCommand::ExecuteString(VStr("setinfo \"")+Name+"\" \""+StringValue+"\"\n", VCommand::SRC_Client, cl);
        } else if (cl->Net) {
          cl->Net->SendCommand(VStr("setinfo \"")+Name+"\" \""+StringValue+"\"\n");
        }
      }
    }
#endif

#ifdef SERVER
    if (Flags&CVAR_ServerInfo) {
      Info_SetValueForKey(svs.serverinfo, Name, *StringValue);
      if (GGameInfo && GGameInfo->NetMode != NM_None && GGameInfo->NetMode != NM_Client) {
        for (int i = 0; i < MAXPLAYERS; ++i) {
          if (GGameInfo->Players[i]) {
            GGameInfo->Players[i]->eventClientSetServerInfo(Name, StringValue);
          }
        }
      }
    }
#endif
  }
}


// ////////////////////////////////////////////////////////////////////////// //
bool VCvar::IsModified () {
  bool ret = !!(Flags&CVAR_Modified);
  // clear modified flag
  Flags &= ~CVAR_Modified;
  return ret;
}


// ////////////////////////////////////////////////////////////////////////// //
void VCvar::Init () {
  if (!Initialised) {
    for (vuint32 bkn = 0; bkn < CVAR_HASH_SIZE; ++bkn) {
      for (VCvar *cvar = cvhBuckets[bkn]; cvar; cvar = cvar->nextInBucket) {
        cvar->Register();
      }
    }
    Initialised = true;
  }
}


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
  GCon->Logf("CVAR statistics: %u buckets used, %u items in longest chain", bkused, maxchain);
}


// ////////////////////////////////////////////////////////////////////////// //
// this is called only once on engine shutdown, so don't bother with deletion
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


// ////////////////////////////////////////////////////////////////////////// //
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


// ////////////////////////////////////////////////////////////////////////// //
void VCvar::SetCheating (bool new_state) {
  Cheating = new_state;
  /*
  if (!Cheating) {
    for (vuint32 bkn = 0; bkn < CVAR_HASH_SIZE; ++bkn) {
      for (VCvar *cvar = cvhBuckets[bkn]; cvar; cvar = cvar->nextInBucket) {
        if (cvar->Flags&CVAR_Cheat) cvar->DoSet(cvar->DefaultString);
      }
    }
  }
  */
}


// ////////////////////////////////////////////////////////////////////////// //
bool VCvar::GetCheating () {
  return Cheating;
  /*
  if (!Cheating) {
    for (vuint32 bkn = 0; bkn < CVAR_HASH_SIZE; ++bkn) {
      for (VCvar *cvar = cvhBuckets[bkn]; cvar; cvar = cvar->nextInBucket) {
        if (cvar->Flags&CVAR_Cheat) cvar->DoSet(cvar->DefaultString);
      }
    }
  }
  */
}


// ////////////////////////////////////////////////////////////////////////// //
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


// ////////////////////////////////////////////////////////////////////////// //
bool VCvar::HasVar (const char *var_name) {
  return (FindVariable(var_name) != nullptr);
}


bool VCvar::HasModVar (const char *var_name) {
  VCvar *var = FindVariable(var_name);
  return (var && var->isModVar());
}


bool VCvar::HasModUserVar (const char *var_name) {
  VCvar *var = FindVariable(var_name);
  return (var && var->isModVar() && (var->Flags&CVAR_ServerInfo) == 0);
}


bool VCvar::CanBeModified (const char *var_name, bool modonly, bool noserver) {
  VCvar *var = FindVariable(var_name);
  if (!var) return false;
  if (modonly && !var->isModVar()) return false;
  if (noserver && (var->Flags&CVAR_Latch) != 0) return false;
  if (var->Flags&(CVAR_Rom|CVAR_Init)) return false;
  if (!Cheating && (var->Flags&CVAR_Cheat) != 0) return false;
  return true;
}


VCvar *VCvar::FindVariable (const char *name) {
  if (!name || name[0] == 0) return nullptr;
  vuint32 nhash = cvnamehash(name);
  for (VCvar *cvar = cvhBuckets[nhash%CVAR_HASH_SIZE]; cvar; cvar = cvar->nextInBucket) {
    if (cvar->lnhash == nhash && !VStr::ICmp(name, cvar->Name)) return cvar;
  }
  return nullptr;
}


// ////////////////////////////////////////////////////////////////////////// //
int VCvar::GetInt (const char *var_name) {
  VCvar *var = FindVariable(var_name);
  return (var ? var->IntValue : 0);
}


float VCvar::GetFloat (const char *var_name) {
  VCvar *var = FindVariable(var_name);
  return (var ? var->FloatValue : 0.0f);
}


bool VCvar::GetBool (const char *var_name) {
  VCvar *var = FindVariable(var_name);
  return (var ? var->BoolValue : false);
}


const char *VCvar::GetCharp (const char *var_name) {
  VCvar *var = FindVariable(var_name);
  return (var ? *var->StringValue : "");
}


VStr VCvar::GetString (const char *var_name) {
  VCvar *var = FindVariable(var_name);
  if (!var) return VStr();
  return var->StringValue;
}


// ////////////////////////////////////////////////////////////////////////// //
const char *VCvar::GetHelp (const char *var_name) {
  VCvar *var = FindVariable(var_name);
  if (!var) return nullptr;
  return var->HelpString;
}


// ////////////////////////////////////////////////////////////////////////// //
int VCvar::GetVarFlags (const char *var_name) {
  VCvar *var = FindVariable(var_name);
  if (!var) return -1;
  return var->getFlags();
}


// ////////////////////////////////////////////////////////////////////////// //
void VCvar::Set (const char *var_name, int value) {
  VCvar *var = FindVariable(var_name);
  if (!var) Sys_Error("Cvar_Set: variable %s not found\n", var_name);
  var->Set(value);
}


void VCvar::Set (const char *var_name, float value) {
  VCvar *var = FindVariable(var_name);
  if (!var) Sys_Error("Cvar_Set: variable %s not found\n", var_name);
  var->Set(value);
}


void VCvar::Set (const char *var_name, const VStr &value) {
  VCvar *var = FindVariable(var_name);
  if (!var) Sys_Error("Cvar_SetString: variable %s not found\n", var_name);
  var->Set(value);
}


// ////////////////////////////////////////////////////////////////////////// //
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
    GCon->Log(VStr(cvar->Name)+" is \""+cvar->StringValue+"\"");
    if ((cvar->Flags&CVAR_Latch) && cvar->LatchedString.IsNotEmpty()) {
      GCon->Log(VStr("Latched \"")+cvar->LatchedString+"\"");
    }
  } else if (needHelp) {
    GCon->Logf("%s: %s", cvar->GetName(), cvar->GetHelp());
  } else {
    if (cvar->Flags&CVAR_Rom) {
      GCon->Logf("%s is read-only", cvar->Name);
    } else if ((cvar->Flags&CVAR_Init) && host_initialised) {
      GCon->Logf("%s can be set only from command-line", cvar->Name);
    } else {
      cvar->Set(Args[1]);
    }
  }
  return true;
}


// ////////////////////////////////////////////////////////////////////////// //
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
    if (a->isModVar()) {
      if (!b->isModVar()) return 1;
    } else if (b->isModVar()) {
      if (!a->isModVar()) return -1;
    }
    return VStr::ICmp(a->GetName(), b->GetName());
  }
}


// contains `countCVars()` elements, must be `delete[]`d.
// can return `nullptr`.
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


// ////////////////////////////////////////////////////////////////////////// //
void VCvar::WriteVariablesToFile (FILE *f) {
  if (!f) return;
  vuint32 count = countCVars();
  VCvar **list = getSortedList();
  for (vuint32 n = 0; n < count; ++n) {
    VCvar *cvar = list[n];
    if (cvar->Flags&(CVAR_Archive|CVAR_AlwaysArchive)) {
      // do not write variables with default values
      if (!(cvar->Flags&CVAR_AlwaysArchive)) {
        if (cvar->StringValue.Cmp(cvar->DefaultString) == 0) continue;
      }
      if (cvar->Flags&CVAR_FromMod) {
        fprintf(f, "cvarinfovar");
        if (cvar->Flags&CVAR_ServerInfo) fprintf(f, " server"); else fprintf(f, " user");
        if (cvar->Flags&CVAR_Cheat) fprintf(f, " cheat");
        if (cvar->Flags&CVAR_Latch) fprintf(f, " latch");
        fprintf(f, " %s \"%s\"\n", cvar->Name, *cvar->StringValue.quote());
      } else {
        fprintf(f, "%s \"%s\"\n", cvar->Name, *cvar->StringValue.quote());
      }
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
#endif


//==========================================================================
//
//  cv_userInfoSet
//
//==========================================================================
static void cv_userInfoSet (VCvar *cvar) {
#ifdef CLIENT
  if (cvar->GetFlags()&CVAR_UserInfo) {
    Info_SetValueForKey(cls.userinfo, cvar->GetName(), *cvar->asStr());
    if (cl) {
      if (GGameInfo->NetMode == NM_TitleMap ||
          GGameInfo->NetMode == NM_Standalone ||
          GGameInfo->NetMode == NM_ListenServer)
      {
        VCommand::ExecuteString(VStr("setinfo \"")+cvar->GetName()+"\" \""+cvar->asStr()+"\"\n", VCommand::SRC_Client, cl);
      } else if (cl->Net) {
        cl->Net->SendCommand(VStr("setinfo \"")+cvar->GetName()+"\" \""+cvar->asStr()+"\"\n");
      }
    }
  }
#endif
}


//==========================================================================
//
//  cv_serverInfoSet
//
//==========================================================================
static void cv_serverInfoSet (VCvar *cvar) {
#ifdef SERVER
  if (cvar->GetFlags()&CVAR_ServerInfo) {
    Info_SetValueForKey(svs.serverinfo, cvar->GetName(), *cvar->asStr());
    if (GGameInfo && GGameInfo->NetMode != NM_None && GGameInfo->NetMode != NM_Client) {
      for (int i = 0; i < MAXPLAYERS; ++i) {
        if (GGameInfo->Players[i]) {
          GGameInfo->Players[i]->eventClientSetServerInfo(cvar->GetName(), cvar->asStr());
        }
      }
    }
  }
#endif
}


//==========================================================================
//
//  CVars_Init
//
//==========================================================================
void CVars_Init () {
  VCvar::Init();
  VCvar::AddAllVarsToAutocomplete(&VCommand::AddToAutoComplete);
  VCvar::UserInfoSetCB = &cv_userInfoSet;
  VCvar::ServerInfoSetCB = &cv_serverInfoSet;
}


// ////////////////////////////////////////////////////////////////////////// //
// COMMAND CvarList
//
// This is slightly more complicated, as we want nicely sorted list.
// It can be fairly slow, we don't care.
COMMAND(CvarList) {
  vuint32 count = VCvar::countCVars();
  VCvar **list = VCvar::getSortedList();
  bool showValues = (Args.length() > 1 && (Args[1].ICmp("values") == 0 || Args[1].ICmp("value") == 0));
  for (vuint32 n = 0; n < count; ++n) {
    VCvar *cvar = list[n];
    if (showValues) {
      GCon->Logf("%s = \"%s\"", cvar->GetName(), *VStr(cvar->asStr()).quote());
    } else {
      GCon->Logf("%s: %s", cvar->GetName(), cvar->GetHelp());
    }
  }
  GCon->Logf("%u variables.", count);
  delete[] list;
}


// ////////////////////////////////////////////////////////////////////////// //
// COMMAND whatis
//
// Show short description for a cvar.
COMMAND(WhatIs) {
  if (Args.Num() != 2) {
    GCon->Logf("Show short cvar description.");
    GCon->Logf("usage: whatis varname");
  } else {
    VCvar *cvar = VCvar::FindVariable(*(Args[1]));
    if (cvar) {
      GCon->Logf("%s: %s", cvar->GetName(), cvar->GetHelp());
    } else {
      GCon->Logf("Unknown cvar: '%s'", *(Args[1]));
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// COMMAND toggle
//
// Toggles boolean cvar
COMMAND(Toggle) {
  if (Args.Num() != 2) {
    GCon->Logf("Toggles boolean cvar.");
    GCon->Logf("usage: toggle varname");
  } else {
    VCvar *cvar = VCvar::FindVariable(*(Args[1]));
    if (cvar) {
      cvar->Set(cvar->asBool() ? 0 : 1);
    } else {
      GCon->Logf("Unknown cvar: '%s'", *(Args[1]));
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// COMMAND cvarinfovar
//
// create temp variable from user mod
COMMAND(CvarInfoVar) {
  if (Args.Num() < 3) {
    GCon->Logf("usage: cvarinfovar varname varvalue");
  } else {
    VStr vname = Args[Args.length()-2];
    VStr vvalue = Args[Args.length()-1];
    if (vname.length() == 0) return;
    VCvar *cvar = VCvar::FindVariable(*vname);
    if (cvar) {
      // just set value
      cvar->Set(vvalue);
      return;
    }
    int flags = CVAR_FromMod|CVAR_Archive|CVAR_AlwaysArchive;
    for (int f = 1; f < Args.length()-2; ++f) {
           if (Args[f].ICmp("noarchive") == 0) flags &= ~(CVAR_Archive|CVAR_AlwaysArchive);
      else if (Args[f].ICmp("cheat") == 0) flags |= CVAR_Cheat;
      else if (Args[f].ICmp("latch") == 0) flags |= CVAR_Latch;
      else if (Args[f].ICmp("server") == 0) flags |= CVAR_ServerInfo;
      else if (Args[f].ICmp("user") == 0) {}
      else { GCon->Logf("invalid cvarinfo flag '%s'", *Args[f]); return; }
    }
    //GCon->Logf("cvarinfo var '%s' (flags=0x%04x) val=\"%s\"", *vname, flags, *(vvalue.quote()));
    VCvar::CreateNew(*vname, vvalue, "cvarinfo variable", flags);
  }
}
