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
//**
//**  Archiving: SaveGame I/O.
//**
//**************************************************************************
#include "../gamedefs.h"
#include "sv_local.h"

#include <time.h>
#include <sys/time.h>

#ifdef _WIN32
//# if __GNUC__ < 6
static inline struct tm *localtime_r (const time_t *_Time, struct tm *_Tm) {
  return (localtime_s(_Tm, _Time) ? NULL : _Tm);
}
//# endif
#endif


//#define VAVOOM_LOADER_CAN_SKIP_CLASSES

enum { NUM_AUTOSAVES = 9 };


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB r_dbg_save_on_level_exit("r_dbg_save_on_level_exit", false, "Save before exiting a level.\nNote that after loading this save you prolly won't be able to exit again.", CVAR_PreInit/*|CVAR_Archive*/);
static VCvarI save_compression_level("save_compression_level", "6", "Save file compression level [0..9]", CVAR_Archive);

static VCvarB dbg_save_ignore_wadlist("dbg_save_ignore_wadlist", false, "Ignore list of loaded wads in savegame when hash mated?", CVAR_PreInit/*|CVAR_Archive*/);

static VCvarB sv_new_map_autosave("sv_new_map_autosave", true, "Autosave when entering new map (except first one)?", CVAR_PreInit/*|CVAR_Archive*/);

static VCvarB sv_save_messages("sv_save_messages", true, "Show messages on save/load?", CVAR_Archive);

//static VCvarB loader_recalc_z("loader_recalc_z", true, "Recalculate Z on load (this should help with some edge cases)?", CVAR_Archive);
static VCvarB loader_ignore_kill_on_unarchive("loader_ignore_kill_on_unarchive", false, "Ignore 'Kill On Unarchive' flag when loading a game?", CVAR_PreInit/*|CVAR_Archive*/);

static VCvarI dbg_save_verbose("dbg_save_verbose", "0", "Slightly more verbose save. DO NOT USE, THIS IS FOR DEBUGGING!\n  0x01: register skips player\n  0x02: registered object\n  0x04: skipped actual player write\n  0x08: skipped unknown object\n  0x10: dump object data writing\b  0x20: dump checkpoints", CVAR_PreInit|CVAR_Archive);

static VCvarB dbg_checkpoints("dbg_checkpoints", false, "Checkpoint save/load debug dumps", 0);

extern VCvarB r_precalc_static_lights;
extern int r_precalc_static_lights_override; // <0: not set
extern VCvarB loader_cache_data;


// ////////////////////////////////////////////////////////////////////////// //
extern VCvarI Skill;
static VCvarB sv_autoenter_checkpoints("sv_autoenter_checkpoints", true, "Use checkpoints for autosaves when possible?", CVAR_Archive);

static VStr saveFileBase;


// ////////////////////////////////////////////////////////////////////////// //
#define QUICKSAVE_SLOT  (-666)

#define EMPTYSTRING  "Empty Slot"
#define MOBJ_NULL    (-1)

#define SAVE_DESCRIPTION_LENGTH    (24)
//#define SAVE_VERSION_TEXT_NO_DATE  "Version 1.34.4"
#define SAVE_VERSION_TEXT          "Version 1.34.12"
#define SAVE_VERSION_TEXT_LENGTH   (16)

static_assert(strlen(SAVE_VERSION_TEXT) <= SAVE_VERSION_TEXT_LENGTH, "oops");

#define SAVE_EXTDATA_ID_END      (0)
#define SAVE_EXTDATA_ID_DATEVAL  (1)
#define SAVE_EXTDATA_ID_DATESTR  (2)


// ////////////////////////////////////////////////////////////////////////// //
enum gameArchiveSegment_t {
  ASEG_MAP_HEADER = 101,
  ASEG_WORLD,
  ASEG_SCRIPTS,
  ASEG_SOUNDS,
  ASEG_END
};


class VSavedMap {
public:
  vuint8 Compressed;
  TArray<vuint8> Data;
  VName Name;
  vint32 DecompressedSize;
};


class VSavedCheckpoint {
public:
  struct EntityInfo {
    //vint32 index;
    VEntity *ent;
    VStr ClassName; // used only in loader
  };

public:
  TArray<QSValue> QSList;
  TArray<EntityInfo> EList;
  vint32 ReadyWeapon; // 0: none, otherwise entity index+1
  vint32 Skill; // -1 means "don't change"

  VSavedCheckpoint () : QSList(), EList(), ReadyWeapon(0), Skill(-1) {}
  ~VSavedCheckpoint () { Clear(); }

  void AddEntity (VEntity *ent) {
    vassert(ent);
    const int len = EList.length();
    for (int f = 0; f < len; ++f) {
      if (EList[f].ent == ent) return;
    }
    EntityInfo &ei = EList.alloc();
    //ei.index = EList.length()-1;
    ei.ent = ent;
    ei.ClassName = VStr(ent->GetClass()->GetName());
  }

  int FindEntity (VEntity *ent) const {
    if (!ent) return 0;
    const int len = EList.length();
    for (int f = 0; f < len; ++f) {
      if (EList[f].ent == ent) return f+1;
    }
    // the thing that should not be
    abort();
    return -1;
  }

  void Clear () {
    QSList.Clear();
    EList.Clear();
    ReadyWeapon = 0;
    Skill = -1; // this should be "no skill"
  }

  void Serialise (VStream *strm) {
    vassert(strm);
    // note that we cannot use `VNTValueIOEx` here, because names are already written!
    if (strm->IsLoading()) {
      // load players inventory
      Clear();
      // load ready weapon
      vint32 rw = 0;
      *strm << STRM_INDEX(rw);
      ReadyWeapon = rw;
      // load entity list
      vint32 entCount;
      *strm << STRM_INDEX(entCount);
      if (dbg_save_verbose&0x20) GCon->Logf("*** LOAD: rw=%d; entCount=%d", rw, entCount);
      if (entCount < 0 || entCount > 1024*1024) Host_Error("invalid entity count (%d)", entCount);
      for (int f = 0; f < entCount; ++f) {
        EntityInfo &ei = EList.alloc();
        ei.ent = nullptr;
        *strm << ei.ClassName;
        if (dbg_save_verbose&0x20) GCon->Logf("  ent #%d: '%s'", f+1, *ei.ClassName);
      }
      // load value list
      vint32 valueCount;
      *strm << STRM_INDEX(valueCount);
      if (dbg_save_verbose&0x20) GCon->Logf(" valueCount=%d", valueCount);
      for (int f = 0; f < valueCount; ++f) {
        QSValue &v = QSList.alloc();
        v.Serialise(*strm);
        // check for special values
        if (v.ent == nullptr) {
          if (v.name.strEqu("\x07 skill")) {
            if (v.type != QST_Int) Host_Error("invalid skill variable type in checkpoint save");
            Skill = v.ival;
            QSList.removeAt(QSList.length()-1);
            continue;
          }
        }
        if (dbg_save_verbose&0x20) GCon->Logf("  val #%d(%d): %s", f, v.objidx, *v.toString());
      }
      if (ReadyWeapon < 0 || ReadyWeapon > EList.length()) Host_Error("invalid ready weapon index (%d)", ReadyWeapon);
      if (dbg_checkpoints) GCon->Logf(NAME_Debug, "checkpoint data loaded (rw=%d; skill=%d)", ReadyWeapon, Skill);
    } else {
      // save players inventory
      // ready weapon
      vint32 rw = ReadyWeapon;
      *strm << STRM_INDEX(rw);
      // save entity list
      vint32 entCount = EList.length();
      *strm << STRM_INDEX(entCount);
      for (int f = 0; f < entCount; ++f) {
        EntityInfo &ei = EList[f];
        *strm << ei.ClassName;
      }
      const int addvalsCount = 1;
      // save value list
      vint32 valueCount = QSList.length()+addvalsCount; // one is reserved for skill
      *strm << STRM_INDEX(valueCount);
      // write special variables (only the skill for now)
      {
        QSValue v = QSValue::CreateInt(nullptr, "\x07 skill", Skill);
        v.Serialise(*strm);
      }
      for (auto &&v : QSList) v.Serialise(*strm);
      //GCon->Logf("*** SAVE: rw=%d; entCount=%d", rw, entCount);
    }
  }
};


class VSaveSlot {
public:
  VStr Description;
  VName CurrentMap;
  TArray<VSavedMap *> Maps; // if there are no maps, it is a checkpoint
  VSavedCheckpoint CheckPoint;

  ~VSaveSlot () { Clear(); }

  void Clear ();
  bool LoadSlot (int Slot);
  bool SaveToSlot (int Slot);
  VSavedMap *FindMap (VName Name);
};


// ////////////////////////////////////////////////////////////////////////// //
static VSaveSlot BaseSlot;


// ////////////////////////////////////////////////////////////////////////// //
class VSaveLoaderStream : public VStream {
private:
  VStream *Stream;

public:
  TArray<VName> NameRemap;
  TArray<VObject *> Exports;
  TArray<VLevelScriptThinker *> AcsExports;

public:
  VV_DISABLE_COPY(VSaveLoaderStream)

  VSaveLoaderStream (VStream *InStream) : Stream(InStream) { bLoading = true; }
  virtual ~VSaveLoaderStream () override { Close(); delete Stream; Stream = nullptr; }

  // stream interface
  virtual void Serialise (void *Data, int Len) override { Stream->Serialise(Data, Len); }
  virtual void Seek (int Pos) override { Stream->Seek(Pos); }
  virtual int Tell () override { return Stream->Tell(); }
  virtual int TotalSize () override { return Stream->TotalSize(); }
  virtual bool AtEnd () override { return Stream->AtEnd(); }
  virtual void Flush () override { Stream->Flush(); }
  virtual bool Close () override { return Stream->Close(); }

  virtual void io (VSerialisable *&Ref) override {
    vint32 scpIndex;
    *this << STRM_INDEX(scpIndex);
    if (scpIndex == 0) {
      Ref = nullptr;
    } else {
      Ref = AcsExports[scpIndex-1];
    }
    //GCon->Logf("LOADING: VSerialisable<%s>(%p); idx=%d", (Ref ? *Ref->GetClassName() : "[none]"), (void *)Ref, scpIndex);
  }

  virtual void io (VName &Name) override {
    vint32 NameIndex;
    *this << STRM_INDEX(NameIndex);
    if (NameIndex < 0 || NameIndex >= NameRemap.length()) {
      //GCon->Logf(NAME_Error, "SAVEGAME: invalid name index %d (max is %d)", NameIndex, NameRemap.length()-1);
      Host_Error("SAVEGAME: invalid name index %d (max is %d)", NameIndex, NameRemap.length()-1);
    }
    Name = NameRemap[NameIndex];
  }

  virtual void io (VObject *&Ref) override {
    vint32 TmpIdx;
    *this << STRM_INDEX(TmpIdx);
    if (TmpIdx == 0) {
      Ref = nullptr;
    } else if (TmpIdx > 0) {
      if (TmpIdx > Exports.Num()) Sys_Error("Bad index %d", TmpIdx);
      Ref = Exports[TmpIdx-1];
    } else {
      GCon->Logf("LOAD: playerbase %d", -TmpIdx-1);
      Ref = GPlayersBase[-TmpIdx-1];
    }
  }

  virtual void SerialiseStructPointer (void *&Ptr, VStruct *Struct) override {
    vint32 TmpIdx;
    *this << STRM_INDEX(TmpIdx);
    if (Struct->Name == "sector_t") {
      Ptr = (TmpIdx >= 0 ? &GLevel->Sectors[TmpIdx] : nullptr);
    } else if (Struct->Name == "line_t") {
      Ptr = (TmpIdx >= 0 ? &GLevel->Lines[TmpIdx] : nullptr);
    } else {
      devprintf("Don't know how to handle pointer to %s\n", *Struct->Name);
      Ptr = nullptr;
    }
  }
};


class VSaveWriterStream : public VStream {
private:
  VStream *Stream;

public:
  TArray<VName> Names;
  TArray<VObject *> Exports;
  TArray<vint32> NamesMap;
  TMapNC<vuint32, vint32> ObjectsMap; // key: object uid; value: internal index
  TArray</*VLevelScriptThinker*/VSerialisable *> AcsExports;
  bool skipPlayers;

public:
  VV_DISABLE_COPY(VSaveWriterStream)

  VSaveWriterStream (VStream *InStream) : Stream(InStream) {
    bLoading = false;
    NamesMap.SetNum(VName::GetNumNames());
    for (int i = 0; i < VName::GetNumNames(); ++i) NamesMap[i] = -1;
  }

  virtual ~VSaveWriterStream () override { Close(); delete Stream; Stream = nullptr; }

  // stream interface
  virtual void Serialise (void *Data, int Len) override { Stream->Serialise(Data, Len); }
  virtual void Seek (int Pos) override { Stream->Seek(Pos); }
  virtual int Tell () override { return Stream->Tell(); }
  virtual int TotalSize () override { return Stream->TotalSize(); }
  virtual bool AtEnd () override { return Stream->AtEnd(); }
  virtual void Flush () override { Stream->Flush(); }
  virtual bool Close () override { return Stream->Close(); }

  void RegisterObject (VObject *o) {
    if (!o) return;
    if (ObjectsMap.has(o->GetUniqueId())) return;
    if (skipPlayers) {
      VEntity *mobj = Cast<VEntity>(o);
      if (mobj != nullptr && (mobj->EntityFlags&VEntity::EF_IsPlayer)) {
        // skipping player mobjs
        if (dbg_save_verbose&0x01) GCon->Logf("*** SKIP(0) PLAYER MOBJ: <%s>", *mobj->GetClass()->GetFullName());
        return;
      }
    }
    if (dbg_save_verbose&0x02) GCon->Logf("*** unique object (%u : %s)", o->GetUniqueId(), *o->GetClass()->GetFullName());
    Exports.Append(o);
    ObjectsMap.put(o->GetUniqueId(), Exports.Num());
  }

  virtual void io (VSerialisable *&Ref) override {
    vint32 scpIndex = 0;
    if (Ref) {
      if (Ref->GetClassName() != "VAcs") Host_Error("trying to save unknown serialisable of class `%s`", *Ref->GetClassName());
      while (scpIndex < AcsExports.length() && AcsExports[scpIndex] != Ref) ++scpIndex;
      if (scpIndex >= AcsExports.length()) {
        scpIndex = AcsExports.length();
        AcsExports.append(Ref);
      }
      ++scpIndex;
    }
    //GCon->Logf("SAVING: VSerialisable<%s>(%p); idx=%d", (Ref ? *Ref->GetClassName() : "[none]"), (void *)Ref, scpIndex);
    *this << STRM_INDEX(scpIndex);
  }

  virtual void io (VName &Name) override {
    int nidx = Name.GetIndex();
    const int olen = NamesMap.length();
    if (olen <= nidx) {
      NamesMap.setLength(nidx+1);
      for (int f = olen; f <= nidx; ++f) NamesMap[f] = -1;
    }
    if (NamesMap[nidx] == -1) NamesMap[nidx] = Names.Append(Name);
    *this << STRM_INDEX(NamesMap[nidx]);
  }

  virtual void io (VObject *&Ref) override {
    vint32 TmpIdx;
    if (!Ref /*|| !Ref->IsGoingToDie()*/) {
      TmpIdx = 0;
    } else {
      //TmpIdx = ObjectsMap[Ref->GetObjectIndex()];
      auto ppp = ObjectsMap.get(Ref->GetUniqueId());
      if (!ppp) {
        if (skipPlayers) {
          VEntity *mobj = Cast<VEntity>(Ref);
          if (mobj != nullptr && (mobj->EntityFlags&VEntity::EF_IsPlayer)) {
            // skipping player mobjs
            if (dbg_save_verbose&0x04) {
              GCon->Logf("*** SKIP(1) PLAYER MOBJ: <%s> -- THIS IS HARMLESS", *mobj->GetClass()->GetFullName());
            }
            TmpIdx = 0;
            *this << STRM_INDEX(TmpIdx);
            return;
          }
        }
        if ((dbg_save_verbose&0x08) /*|| true*/) {
          GCon->Logf("*** unknown object (%u : %s) -- THIS IS HARMLESS", Ref->GetUniqueId(), *Ref->GetClass()->GetFullName());
        }
        TmpIdx = 0; // that is how it was done in previous version of the code
      } else {
        TmpIdx = *ppp;
      }
      //TmpIdx = *ObjectsMap.get(Ref->GetUniqueId());
    }
    *this << STRM_INDEX(TmpIdx);
  }

  virtual void SerialiseStructPointer (void *&Ptr, VStruct *Struct) override {
    vint32 TmpIdx;
    if (Struct->Name == "sector_t") {
      TmpIdx = (Ptr ? (int)((sector_t *)Ptr-GLevel->Sectors) : -1);
    } else if (Struct->Name == "line_t") {
      TmpIdx = (Ptr ? (int)((line_t *)Ptr-GLevel->Lines) : -1);
    } else {
      devprintf("Don't know how to handle pointer to %s\n", *Struct->Name);
      TmpIdx = -1;
    }
    *this << STRM_INDEX(TmpIdx);
  }
};


#ifdef CLIENT
static bool skipCallbackInited = false;


//==========================================================================
//
//  checkSkipClassCB
//
//==========================================================================
static bool checkSkipClassCB (VObject *self, VName clsname) {
  if (clsname == "Level_K8BDW" || clsname == "Level_K8Gore") {
    // allow any level descendant
    for (VClass *cls = self->GetClass(); cls; cls = cls->GetSuperClass()) {
      if (VStr::strEqu(cls->GetName(), "Level")) return true;
    }
  }
  return false;
}


//==========================================================================
//
//  SV_SetupSkipCallback
//
//==========================================================================
static void SV_SetupSkipCallback () {
  if (skipCallbackInited) return;
  skipCallbackInited = true;
  VObject::CanSkipReadingClassCBList.append(&checkSkipClassCB);
}
#endif


//==========================================================================
//
//  SV_GetSavesDir
//
//==========================================================================
static VStr SV_GetSavesDir () {
  return FL_GetSavesDir();
}


//==========================================================================
//
//  GetSaveSlotDirectoryPrefix
//
//==========================================================================
static VStr GetSaveSlotCommonDirectoryPrefix () {
  vuint32 hash = SV_GetModListHash();
  VStr pfx = VStr::buf2hex(&hash, 4);
  //GCon->Logf(NAME_Debug, "SAVE PFX: %s", *pfx);
  //pfx += "/";
  return pfx;
}


//==========================================================================
//
//  isBadSlotIndex
//
//  checking for "bad" index is more common
//
//==========================================================================
static inline bool isBadSlotIndex (int slot) {
  return (slot != QUICKSAVE_SLOT && (slot < -64 || slot > 63));
}


//==========================================================================
//
//  UpdateSaveDirWadList
//
//  writes text file with list of active wads.
//  this file is not used by the engine, and is written solely for user.
//
//==========================================================================
static void UpdateSaveDirWadList () {
  auto svpfx = SV_GetSavesDir().appendPath(GetSaveSlotCommonDirectoryPrefix());
  FL_CreatePath(svpfx); // just in case
  //GCon->Logf(NAME_Debug, "UpdateSaveDirWadList: svpfx=<%s>", *svpfx);
  svpfx = svpfx.appendPath("wadlist.txt");
  VStream *res = FL_OpenSysFileWrite(svpfx);
  if (res) {
    auto wadlist = FL_GetWadPk3List();
    for (auto &&wadname : wadlist) {
      //GCon->Logf(NAME_Debug, "  wad=<%s>", *wadname);
      res->writef("%s\n", *wadname);
    }
    res->Close();
    delete res;
  }
}


//==========================================================================
//
//  GetSaveSlotBaseFileName
//
//  if slot is < 0, this is autosave slot
//  QUICKSAVE_SLOT is quicksave slot
//  returns empty string for invalid slot
//
//==========================================================================
static VStr GetSaveSlotBaseFileName (int slot) {
  if (isBadSlotIndex(slot)) return VStr();
  VStr pfx = GetSaveSlotCommonDirectoryPrefix();
  if (slot == QUICKSAVE_SLOT) return pfx+"/quicksave";
  if (slot < 0) return pfx+va("/autosave_%02d", -slot);
  return pfx+va("/normsave_%02d", slot+1);
}


//==========================================================================
//
//  SV_OpenSlotFileRead
//
//  open savegame slot file if it exists
//  sets `saveFileBase`
//
//==========================================================================
static VStream *SV_OpenSlotFileRead (int slot) {
  saveFileBase.clear();
  if (isBadSlotIndex(slot)) return nullptr;
  // search save subdir
  auto svdir = SV_GetSavesDir()+"/"+GetSaveSlotCommonDirectoryPrefix();
  auto dir = Sys_OpenDir(svdir);
  if (dir) {
    auto svpfx = GetSaveSlotBaseFileName(slot).extractFileName();
    for (;;) {
      VStr fname = Sys_ReadDir(dir);
      if (fname.isEmpty()) break;
      if (fname.startsWithNoCase(svpfx) && fname.endsWithNoCase(".vsg")) {
        Sys_CloseDir(dir);
        saveFileBase = svdir+"/"+fname;
        return FL_OpenSysFileRead(saveFileBase);
      }
    }
    Sys_CloseDir(dir);
  }
  // for backwards compatibility, remove after several releases
  svdir = SV_GetSavesDir();
  dir = Sys_OpenDir(svdir);
  if (dir) {
    auto svpfx = GetSaveSlotCommonDirectoryPrefix();
    svpfx += "_";
    svpfx += GetSaveSlotBaseFileName(slot).extractFileName();
    for (;;) {
      VStr fname = Sys_ReadDir(dir);
      if (fname.isEmpty()) break;
      if (fname.startsWithNoCase(svpfx) && fname.endsWithNoCase(".vsg")) {
        Sys_CloseDir(dir);
        return FL_OpenSysFileRead(svdir+"/"+fname);
      }
    }
    Sys_CloseDir(dir);
  }
  return nullptr;
}


//==========================================================================
//
//  removeSlotSaveFiles
//
//  user can rename file to different case
//  kill 'em all!
//
//==========================================================================
static bool removeSlotSaveFiles (int slot, VStr fignore) {
  TArray<VStr> tokill;
  if (isBadSlotIndex(slot)) return false;
  VStr fignore2;
  if (fignore.length()) fignore2 = fignore+".lmap";
  // search save subdir
  auto svdir = SV_GetSavesDir()+"/"+GetSaveSlotCommonDirectoryPrefix();
  auto dir = Sys_OpenDir(svdir);
  if (dir) {
    auto svpfx = GetSaveSlotBaseFileName(slot).extractFileName();
    for (;;) {
      VStr fname = Sys_ReadDir(dir);
      if (fname.isEmpty()) break;
      if (fname.startsWithNoCase(svpfx) && (fname.endsWithNoCase(".vsg") || fname.endsWithNoCase(".vsg.lmap"))) {
        VStr fn = svdir+"/"+fname;
        if (fn != fignore && fn != fignore2) tokill.append(fn);
      }
    }
    Sys_CloseDir(dir);
  }
  // for backwards compatibility, remove after several releases
  svdir = SV_GetSavesDir();
  dir = Sys_OpenDir(svdir);
  if (dir) {
    auto svpfx = GetSaveSlotCommonDirectoryPrefix();
    svpfx += "_";
    svpfx += GetSaveSlotBaseFileName(slot).extractFileName();
    for (;;) {
      VStr fname = Sys_ReadDir(dir);
      if (fname.isEmpty()) break;
      if (fname.startsWithNoCase(svpfx) && (fname.endsWithNoCase(".vsg") || fname.endsWithNoCase(".vsg.lmap"))) {
        VStr fn = svdir+"/"+fname;
        if (fn != fignore && fn != fignore2) tokill.append(fn);
      }
    }
    Sys_CloseDir(dir);
  }
  if (tokill.length() > 4) return false; // something is VERY wrong here!
  for (int f = 0; f < tokill.length(); ++f) Sys_FileDelete(tokill[f]);
  return true;
}


//==========================================================================
//
//  SV_CreateSlotFileWrite
//
//  create new savegame slot file
//  also, removes any existing savegame file for the same slot
//  sets `saveFileBase`
//
//==========================================================================
static VStream *SV_CreateSlotFileWrite (int slot, VStr descr) {
  saveFileBase.clear();
  if (isBadSlotIndex(slot)) return nullptr;
  if (slot == QUICKSAVE_SLOT) descr = VStr();
  //removeSlotSaveFiles(slot);
  auto svpfx = SV_GetSavesDir()+"/"+GetSaveSlotBaseFileName(slot);
  FL_CreatePath(svpfx.extractFilePath()); // just in case
  // normalize description
  VStr newdesc;
  for (int f = 0; f < descr.length(); ++f) {
    char ch = descr[f];
    if (!ch) continue;
    if (ch >= '0' && ch <= '9') { newdesc += ch; continue; }
    if (ch >= 'A' && ch <= 'Z') { newdesc += ch-'A'+'a'; continue; } // poor man's tolower()
    if (ch >= 'a' && ch <= 'z') { newdesc += ch; continue; }
    // replace with underscore
    if (newdesc.length() == 0) continue;
    if (newdesc[newdesc.length()-1] == '_') continue;
    newdesc += "_";
  }
  while (newdesc.length() && newdesc[0] == '_') newdesc.chopLeft(1);
  while (newdesc.length() && newdesc[newdesc.length()-1] == '_') newdesc.chopRight(1);
  // finalize file name
  if (newdesc.length()) { svpfx += "_"; svpfx += newdesc; }
  svpfx += ".vsg";
  saveFileBase = svpfx;
  VStream *res = FL_OpenSysFileWrite(svpfx);
  if (res) {
    removeSlotSaveFiles(slot, svpfx);
    UpdateSaveDirWadList();
  }
  return res;
}


#ifdef CLIENT
//==========================================================================
//
//  SV_DeleteSlotFile
//
//==========================================================================
static bool SV_DeleteSlotFile (int slot) {
  if (isBadSlotIndex(slot)) return false;
  return removeSlotSaveFiles(slot, VStr::EmptyString);
}
#endif


// ////////////////////////////////////////////////////////////////////////// //
struct TTimeVal {
  int secs; // actually, unsigned
  int usecs;
  // for 2030+
  int secshi;

  inline bool operator < (const TTimeVal &tv) const {
    if (secshi < tv.secshi) return true;
    if (secshi > tv.secshi) return false;
    if (secs < tv.secs) return true;
    if (secs > tv.secs) return false;
    return false;
  }
};


//==========================================================================
//
//  GetTimeOfDay
//
//==========================================================================
static void GetTimeOfDay (TTimeVal *tvres) {
  if (!tvres) return;
  tvres->secshi = 0;
  timeval tv;
  if (gettimeofday(&tv, nullptr)) {
    tvres->secs = 0;
    tvres->usecs = 0;
  } else {
    tvres->secs = (int)(tv.tv_sec&0xffffffff);
    tvres->usecs = (int)tv.tv_usec;
    tvres->secshi = (int)(((uint64_t)tv.tv_sec)>>32);
  }
}


//==========================================================================
//
//  TimeVal2Str
//
//==========================================================================
static VStr TimeVal2Str (const TTimeVal *tvin, bool forAutosave=false) {
  timeval tv;
  tv.tv_sec = (((uint64_t)tvin->secs)&0xffffffff)|(((uint64_t)tvin->secshi)<<32);
  //tv.tv_usec = tvin->usecs;
  tm ctm;
  if (localtime_r(&tv.tv_sec, &ctm)) {
    if (forAutosave) {
      // for autosave
      return VStr(va("%02d:%02d", (int)ctm.tm_hour, (int)ctm.tm_min));
    } else {
      // full
      return VStr(va("%04d/%02d/%02d %02d:%02d:%02d",
        (int)(ctm.tm_year+1900),
        (int)ctm.tm_mon,
        (int)ctm.tm_mday,
        (int)ctm.tm_hour,
        (int)ctm.tm_min,
        (int)ctm.tm_sec));
    }
  } else {
    return VStr("unknown");
  }
}


//==========================================================================
//
//  VSaveSlot::Clear
//
//==========================================================================
void VSaveSlot::Clear () {
  Description.Clean();
  CurrentMap = NAME_None;
  for (int i = 0; i < Maps.Num(); ++i) { delete Maps[i]; Maps[i] = nullptr; }
  Maps.Clear();
  CheckPoint.Clear();
}


//==========================================================================
//
//  SkipExtData
//
//  skip extended data
//
//==========================================================================
static bool SkipExtData (VStream *Strm) {
  for (;;) {
    vint32 id, size;
    *Strm << STRM_INDEX(id);
    if (id == SAVE_EXTDATA_ID_END) break;
    *Strm << STRM_INDEX(size);
    if (size < 0 || size > 65536) return false;
    // skip data
    Strm->Seek(Strm->Tell()+size);
  }
  return true;
}


//==========================================================================
//
//  LoadDateStrExtData
//
//  get date string, or use timeval to build it
//  empty string means i/o error
//
//==========================================================================
static VStr LoadDateStrExtData (VStream *Strm) {
  bool tvvalid = false;
  TTimeVal tv;
  memset((void *)&tv, 0, sizeof(tv));
  VStr res;
  for (;;) {
    vint32 id, size;
    *Strm << STRM_INDEX(id);
    if (id == SAVE_EXTDATA_ID_END) break;
    *Strm << STRM_INDEX(size);
    if (size < 0 || size > 65536) return VStr();

    if (id == SAVE_EXTDATA_ID_DATEVAL && size == (vint32)sizeof(tv)) {
      tvvalid = true;
      Strm->Serialise(&tv, sizeof(tv));
      continue;
    }

    if (id == SAVE_EXTDATA_ID_DATESTR && size > 0 && size < 64) {
      char buf[65];
      memset(buf, 0, sizeof(buf));
      Strm->Serialise(buf, size);
      if (buf[0]) res = VStr(buf);
      continue;
    }

    // skip unknown data
    Strm->Seek(Strm->Tell()+size);
  }
  if (res.length() == 0) {
    if (tvvalid) {
      res = TimeVal2Str(&tv);
    } else {
      res = VStr("UNKNOWN");
    }
  }
  return res;
}


//==========================================================================
//
//  LoadDateTValExtData
//
//==========================================================================
static bool LoadDateTValExtData (VStream *Strm, TTimeVal *tv) {
  memset((void *)tv, 0, sizeof(*tv));
  for (;;) {
    vint32 id, size;
    *Strm << STRM_INDEX(id);
    if (id == SAVE_EXTDATA_ID_END) break;
    //fprintf(stderr, "   id=%d\n", id);
    *Strm << STRM_INDEX(size);
    if (size < 0 || size > 65536) break;

    if (id == SAVE_EXTDATA_ID_DATEVAL && size == (vint32)sizeof(*tv)) {
      Strm->Serialise(tv, sizeof(tv));
      //fprintf(stderr, "  found TV[%s] (%s)\n", *TimeVal2Str(tv), (Strm->IsError() ? "ERROR" : "OK"));
      return !Strm->IsError();
    }

    // skip unknown data
    Strm->Seek(Strm->Tell()+size);
  }
  return false;
}


//==========================================================================
//
//  VSaveSlot::LoadSlot
//
//==========================================================================
bool VSaveSlot::LoadSlot (int Slot) {
  Clear();
  saveFileBase.clear();

  VStream *Strm = SV_OpenSlotFileRead(Slot);
  if (!Strm) {
    saveFileBase.clear();
    GCon->Log("Savegame file doesn't exist");
    return false;
  }

  // check the version text
  char VersionText[SAVE_VERSION_TEXT_LENGTH+1];
  memset(VersionText, 0, sizeof(VersionText));
  Strm->Serialise(VersionText, SAVE_VERSION_TEXT_LENGTH);
  if (VStr::Cmp(VersionText, SAVE_VERSION_TEXT) /*&& VStr::Cmp(VersionText, SAVE_VERSION_TEXT_NO_DATE)*/) {
    saveFileBase.clear();
    // bad version
    Strm->Close();
    delete Strm;
    Strm = nullptr;
    GCon->Log("Savegame is from incompatible version");
    return false;
  }

  *Strm << Description;

  // skip extended data
  if (VStr::Cmp(VersionText, SAVE_VERSION_TEXT) == 0) {
    if (!SkipExtData(Strm) || Strm->IsError()) {
      saveFileBase.clear();
      // bad file
      Strm->Close();
      delete Strm;
      Strm = nullptr;
      GCon->Log("Savegame is corrupted");
      return false;
    }
  }

  // check list of loaded modules
  auto wadlist = FL_GetWadPk3List();
  vint32 wcount = wadlist.length();
  *Strm << wcount;

  if (wcount < 1 || wcount > 8192) {
    saveFileBase.clear();
    Strm->Close();
    delete Strm;
    Strm = nullptr;
    GCon->Log("Invalid savegame (bad number of mods)");
    return false;
  }

  if (!dbg_save_ignore_wadlist) {
    if (wcount != wadlist.length()) {
      saveFileBase.clear();
      Strm->Close();
      delete Strm;
      Strm = nullptr;
      GCon->Log("Invalid savegame (bad number of mods)");
      return false;
    }
  }

  for (int f = 0; f < wcount; ++f) {
    VStr s;
    *Strm << s;
    if (!dbg_save_ignore_wadlist) {
      if (s != wadlist[f]) {
        saveFileBase.clear();
        Strm->Close();
        delete Strm;
        Strm = nullptr;
        GCon->Log("Invalid savegame (bad mod)");
        return false;
      }
    }
  }

  VStr TmpName;
  *Strm << TmpName;
  CurrentMap = *TmpName;

  vint32 NumMaps;
  *Strm << STRM_INDEX(NumMaps);
  for (int i = 0; i < NumMaps; ++i) {
    VSavedMap *Map = new VSavedMap();
    Maps.Append(Map);
    vint32 DataLen;
    *Strm << TmpName << Map->Compressed << STRM_INDEX(Map->DecompressedSize) << STRM_INDEX(DataLen);
    Map->Name = *TmpName;
    Map->Data.SetNum(DataLen);
    Strm->Serialise(Map->Data.Ptr(), Map->Data.Num());
  }

  //HACK: if `NumMaps` is 0, we're loading checkpoint
  if (NumMaps == 0) {
    // load players inventory
    VSavedCheckpoint &cp = CheckPoint;
    cp.Serialise(Strm);
  } else {
    VSavedCheckpoint &cp = CheckPoint;
    cp.Clear();
  }

  bool err = Strm->IsError();

  Strm->Close();
  delete Strm;

  Host_ResetSkipFrames();

  if (err) {
    saveFileBase.clear();
    return false;
  }
  vassert(!saveFileBase.isEmpty());
  return true;
}


//==========================================================================
//
//  VSaveSlot::SaveToSlot
//
//==========================================================================
bool VSaveSlot::SaveToSlot (int Slot) {
  saveFileBase.clear();

  VStream *Strm = SV_CreateSlotFileWrite(Slot, Description);
  if (!Strm) {
    saveFileBase.clear();
    GCon->Logf("ERROR: cannot save to slot %d!", Slot);
    return false;
  }

  // write version info
  char VersionText[SAVE_VERSION_TEXT_LENGTH+1];
  memset(VersionText, 0, SAVE_VERSION_TEXT_LENGTH);
  VStr::Cpy(VersionText, SAVE_VERSION_TEXT);
  Strm->Serialise(VersionText, SAVE_VERSION_TEXT_LENGTH);

  // write game save description
  *Strm << Description;

  // extended data: date value and date string
  {
    // date value
    TTimeVal tv;
    GetTimeOfDay(&tv);
    vint32 id = SAVE_EXTDATA_ID_DATEVAL;
    *Strm << STRM_INDEX(id);
    vint32 size = (vint32)sizeof(tv);
    *Strm << STRM_INDEX(size);
    Strm->Serialise(&tv, sizeof(tv));

    // date string
    VStr dstr = TimeVal2Str(&tv);
    id = SAVE_EXTDATA_ID_DATESTR;
    *Strm << STRM_INDEX(id);
    size = dstr.length();
    *Strm << STRM_INDEX(size);
    Strm->Serialise(*dstr, size);

    // end of data marker
    id = SAVE_EXTDATA_ID_END;
    *Strm << STRM_INDEX(id);
  }

  // write list of loaded modules
  auto wadlist = FL_GetWadPk3List();
  //GCon->Logf("====================="); for (int f = 0; f < wadlist.length(); ++f) GCon->Logf("  %d: %s", f, *wadlist[f]);
  vint32 wcount = wadlist.length();
  *Strm << wcount;
  for (int f = 0; f < wadlist.length(); ++f) *Strm << wadlist[f];

  // write current map
  VStr TmpName(CurrentMap);
  *Strm << TmpName;

  vint32 NumMaps = Maps.Num();
  *Strm << STRM_INDEX(NumMaps);
  for (int i = 0; i < Maps.Num(); ++i) {
    TmpName = VStr(Maps[i]->Name);
    vint32 DataLen = Maps[i]->Data.Num();
    *Strm << TmpName << Maps[i]->Compressed << STRM_INDEX(Maps[i]->DecompressedSize) << STRM_INDEX(DataLen);
    Strm->Serialise(Maps[i]->Data.Ptr(), Maps[i]->Data.Num());
  }

  //HACK: if `NumMaps` is 0, we're saving checkpoint
  if (NumMaps == 0) {
    // save players inventory
    VSavedCheckpoint &cp = CheckPoint;
    cp.Serialise(Strm);
  }

  bool err = Strm->IsError();
  Strm->Close();
  err = err || Strm->IsError();
  delete Strm;

  Host_ResetSkipFrames();

  if (err) {
    saveFileBase.clear();
    GCon->Logf("ERROR: error saving to slot %d, savegame is corrupted!", Slot);
    return false;
  }

  vassert(!saveFileBase.isEmpty());
  return true;
}


//==========================================================================
//
//  VSaveSlot::FindMap
//
//==========================================================================
VSavedMap *VSaveSlot::FindMap (VName Name) {
  for (int i = 0; i < Maps.Num(); ++i) if (Maps[i]->Name == Name) return Maps[i];
  return nullptr;
}


//==========================================================================
//
//  SV_GetSaveString
//
//==========================================================================
bool SV_GetSaveString (int Slot, VStr &Desc) {
  VStream *Strm = SV_OpenSlotFileRead(Slot);
  if (Strm) {
    char VersionText[SAVE_VERSION_TEXT_LENGTH+1];
    memset(VersionText, 0, sizeof(VersionText));
    Strm->Serialise(VersionText, SAVE_VERSION_TEXT_LENGTH);
    bool goodSave = true;
    Desc = "???";
    if (VStr::Cmp(VersionText, SAVE_VERSION_TEXT) /*&& VStr::Cmp(VersionText, SAVE_VERSION_TEXT_NO_DATE)*/) {
      // bad version, put an asterisk in front of the description
      goodSave = false;
    } else {
      *Strm << Desc;
      // skip extended data
      if (VStr::Cmp(VersionText, SAVE_VERSION_TEXT) == 0) {
        if (!SkipExtData(Strm) || Strm->IsError()) goodSave = false;
      }
      if (goodSave) {
        // check list of loaded modules
        auto wadlist = FL_GetWadPk3List();
        vint32 wcount = wadlist.length();
        *Strm << wcount;
        if (wcount < 1 || wcount > 8192 || wcount != wadlist.length()) {
          goodSave = false;
        } else {
          for (int f = 0; f < wcount; ++f) {
            VStr s;
            *Strm << s;
            if (s != wadlist[f]) { goodSave = false; break; }
          }
        }
      }
    }
    if (!goodSave) Desc = "*"+Desc;
    delete Strm;
    return /*true*/goodSave;
  } else {
    Desc = EMPTYSTRING;
    return false;
  }
}



//==========================================================================
//
//  SV_GetSaveDateString
//
//==========================================================================
void SV_GetSaveDateString (int Slot, VStr &datestr) {
  VStream *Strm = SV_OpenSlotFileRead(Slot);
  if (Strm) {
    char VersionText[SAVE_VERSION_TEXT_LENGTH+1];
    memset(VersionText, 0, sizeof(VersionText));
    Strm->Serialise(VersionText, SAVE_VERSION_TEXT_LENGTH);
    datestr = "UNKNOWN";
    if (VStr::Cmp(VersionText, SAVE_VERSION_TEXT) == 0) {
      VStr Desc;
      *Strm << Desc;
      datestr = LoadDateStrExtData(Strm);
      if (datestr.length() == 0) datestr = "UNKNOWN";
    }
    delete Strm;
  } else {
    datestr = "UNKNOWN";
  }
}


//==========================================================================
//
//  SV_GetSaveDateTVal
//
//  false: slot is empty or invalid
//
//==========================================================================
static bool SV_GetSaveDateTVal (int Slot, TTimeVal *tv) {
  memset((void *)tv, 0, sizeof(*tv));
  VStream *Strm = SV_OpenSlotFileRead(Slot);
  if (Strm) {
    char VersionText[SAVE_VERSION_TEXT_LENGTH+1];
    memset(VersionText, 0, sizeof(VersionText));
    Strm->Serialise(VersionText, SAVE_VERSION_TEXT_LENGTH);
    //fprintf(stderr, "OPENED slot #%d\n", Slot);
    if (VStr::Cmp(VersionText, SAVE_VERSION_TEXT) != 0) { delete Strm; return false; }
    //fprintf(stderr, "  slot #%d has valid version\n", Slot);
    VStr Desc;
    *Strm << Desc;
    //fprintf(stderr, "  slot #%d description: [%s]\n", Slot, *Desc);
    if (!LoadDateTValExtData(Strm, tv)) { delete Strm; return false; }
    delete Strm;
    return true;
  } else {
    return false;
  }
}


//==========================================================================
//
//  SV_FindAutosaveSlot
//
//  returns 0 on error
//
//==========================================================================
static int SV_FindAutosaveSlot () {
  TTimeVal tv, besttv;
  int bestslot = 0;
  memset((void *)&tv, 0, sizeof(tv));
  memset((void *)&besttv, 0, sizeof(besttv));
  for (int slot = 1; slot <= NUM_AUTOSAVES; ++slot) {
    if (!SV_GetSaveDateTVal(-slot, &tv)) {
      //fprintf(stderr, "AUTOSAVE: free slot #%d found!\n", slot);
      bestslot = -slot;
      break;
    }
    if (!bestslot || tv < besttv) {
      //GCon->Logf("AUTOSAVE: better slot #%d found [%s] : old id #%d [%s]!", slot, *TimeVal2Str(&tv), -bestslot, (bestslot ? *TimeVal2Str(&besttv) : ""));
      bestslot = -slot;
      besttv = tv;
    } else {
      //GCon->Logf("AUTOSAVE: skipped slot #%d [%s] (%d,%d,%d)!", slot, *TimeVal2Str(&tv), tv.secshi, tv.secs, tv.usecs);
    }
  }
  return bestslot;
}


//==========================================================================
//
//  AssertSegment
//
//==========================================================================
static void AssertSegment (VStream &Strm, gameArchiveSegment_t segType) {
  if (Streamer<int>(Strm) != (int)segType) Host_Error("Corrupt save game: Segment [%d] failed alignment check", segType);
}


//==========================================================================
//
//  ArchiveNames
//
//==========================================================================
static void ArchiveNames (VSaveWriterStream *Saver) {
  // write offset to the names in the beginning of the file
  vint32 NamesOffset = Saver->Tell();
  Saver->Seek(0);
  *Saver << NamesOffset;
  Saver->Seek(NamesOffset);

  // serialise names
  vint32 Count = Saver->Names.Num();
  *Saver << STRM_INDEX(Count);
  for (int i = 0; i < Count; ++i) {
    //*Saver << *VName::GetEntry(Saver->Names[i].GetIndex());
    const char *EName = *Saver->Names[i];
    vuint8 len = (vuint8)VStr::Length(EName);
    *Saver << len;
    if (len) Saver->Serialise((void *)EName, len);
  }

  // serialise number of ACS exports
  vint32 numScripts = Saver->AcsExports.length();
  *Saver << STRM_INDEX(numScripts);
}


//==========================================================================
//
//  UnarchiveNames
//
//==========================================================================
static void UnarchiveNames (VSaveLoaderStream *Loader) {
  vint32 NamesOffset;
  *Loader << NamesOffset;

  vint32 TmpOffset = Loader->Tell();
  Loader->Seek(NamesOffset);
  vint32 Count;
  *Loader << STRM_INDEX(Count);
  Loader->NameRemap.SetNum(Count);
  for (int i = 0; i < Count; ++i) {
    /*
    VNameEntry E;
    *Loader << E;
    */
    char EName[NAME_SIZE+1];
    vuint8 len = 0;
    *Loader << len;
    vassert(len <= NAME_SIZE);
    if (len) Loader->Serialise(EName, len);
    EName[len] = 0;
    Loader->NameRemap[i] = VName(EName);
  }

  // unserialise number of ACS exports
  vint32 numScripts = -1;
  *Loader << STRM_INDEX(numScripts);
  if (numScripts < 0 || numScripts >= 1024*1024*2) Host_Error("invalid number of ACS scripts (%d)", numScripts);
  Loader->AcsExports.setLength(numScripts);

  // create empty script objects
  for (vint32 f = 0; f < numScripts; ++f) Loader->AcsExports[f] = AcsCreateEmptyThinker();

  Loader->Seek(TmpOffset);
}


//==========================================================================
//
//  ArchiveThinkers
//
//==========================================================================
static void ArchiveThinkers (VSaveWriterStream *Saver, bool SavingPlayers) {
  vint32 Seg = ASEG_WORLD;
  *Saver << Seg;

  Saver->skipPlayers = !SavingPlayers;

  // add level
  Saver->RegisterObject(GLevel);

  // add world info
  vuint8 WorldInfoSaved = (vuint8)SavingPlayers;
  *Saver << WorldInfoSaved;
  if (WorldInfoSaved) Saver->RegisterObject(GGameInfo->WorldInfo);

  // add players
  {
    vassert(MAXPLAYERS >= 0 && MAXPLAYERS <= 254);
    vuint8 mpl = MAXPLAYERS;
    *Saver << mpl;
  }
  for (int i = 0; i < MAXPLAYERS; ++i) {
    vuint8 Active = (vuint8)(SavingPlayers && GGameInfo->Players[i]);
    *Saver << Active;
    if (!Active) continue;
    Saver->RegisterObject(GGameInfo->Players[i]);
  }

  // add thinkers
  int ThinkersStart = Saver->Exports.Num();
  for (TThinkerIterator<VThinker> Th(GLevel); Th; ++Th) {
    // players will be skipped by `Saver`
    Saver->RegisterObject(*Th);
    /*
    if ((*Th)->IsA(VEntity::StaticClass())) {
      VEntity *e = (VEntity *)(*Th);
      if (e->EntityFlags&VEntity::EF_IsPlayer) GCon->Logf("PLRSAV: FloorZ=%f; CeilingZ=%f; Floor=%p; Ceiling=%p", e->FloorZ, e->CeilingZ, e->Floor, e->Ceiling);
    }
    */
  }

  // write exported object names
  vint32 NumObjects = Saver->Exports.Num()-ThinkersStart;
  *Saver << STRM_INDEX(NumObjects);
  for (int i = ThinkersStart; i < Saver->Exports.Num(); ++i) {
    VName CName = Saver->Exports[i]->GetClass()->GetVName();
    *Saver << CName;
  }

  // serialise objects
  for (int i = 0; i < Saver->Exports.Num(); ++i) {
    if (dbg_save_verbose&0x10) GCon->Logf("** SR #%d: <%s>", i, *Saver->Exports[i]->GetClass()->GetFullName());
    Saver->Exports[i]->Serialise(*Saver);
  }

  //GCon->Logf("dbg_save_verbose=0x%04x (%s) %d", dbg_save_verbose.asInt(), *dbg_save_verbose.asStr(), dbg_save_verbose.asInt());

  // collect acs scripts, serialize acs level
  GLevel->Acs->Serialise(*Saver);

  // save collected VAcs objects contents
  for (vint32 f = 0; f < Saver->AcsExports.length(); ++f) Saver->AcsExports[f]->Serialise(*Saver);
}


//==========================================================================
//
//  UnarchiveThinkers
//
//==========================================================================
static void UnarchiveThinkers (VSaveLoaderStream *Loader) {
  VObject *Obj = nullptr;

  AssertSegment(*Loader, ASEG_WORLD);

  // add level
  Loader->Exports.Append(GLevel);

  // add world info
  vuint8 WorldInfoSaved;
  *Loader << WorldInfoSaved;
  if (WorldInfoSaved) Loader->Exports.Append(GGameInfo->WorldInfo);

  // add players
  {
    vuint8 mpl = 255;
    *Loader << mpl;
    if (mpl != MAXPLAYERS) Host_Error("Invalid number of players in save");
  }
  sv_load_num_players = 0;
  for (int i = 0; i < MAXPLAYERS; ++i) {
    vuint8 Active;
    *Loader << Active;
    if (Active) {
      ++sv_load_num_players;
      Loader->Exports.Append(GPlayersBase[i]);
    }
  }

  TArray<VEntity *> elist;
#ifdef VAVOOM_LOADER_CAN_SKIP_CLASSES
  TMapNC<VObject *, bool> deadThinkers;
#endif

  bool hasSomethingToRemove = false;

  vint32 NumObjects;
  *Loader << STRM_INDEX(NumObjects);
  if (NumObjects < 0) Host_Error("invalid number of VM objects");
  for (int i = 0; i < NumObjects; ++i) {
    // get params
    VName CName;
    *Loader << CName;
    VClass *Class = VClass::FindClass(*CName);
    if (!Class) {
      #ifdef VAVOOM_LOADER_CAN_SKIP_CLASSES
      GCon->Logf("I/O WARNING: No such class '%s'", *CName);
      //Loader->Exports.Append(nullptr);
      Class = VThinker::StaticClass();
      Obj = VObject::StaticSpawnNoReplace(Class);
      //deadThinkers.append((VThinker *)Obj);
      deadThinkers.put(Obj, false);
      #else
      Sys_Error("I/O ERROR: No such class '%s'", *CName);
      #endif
    } else {
      // allocate object and copy data
      Obj = VObject::StaticSpawnNoReplace(Class);
    }
    // reassign server uids
    if (Obj && Obj->IsA(VThinker::StaticClass())) ((VThinker *)Obj)->ServerUId = Obj->GetUniqueId();

    // handle level info
    if (Obj->IsA(VLevelInfo::StaticClass())) {
      GLevelInfo = (VLevelInfo *)Obj;
      GLevelInfo->Game = GGameInfo;
      GLevelInfo->World = GGameInfo->WorldInfo;
      GLevel->LevelInfo = GLevelInfo;
    } else if (Obj->IsA(VEntity::StaticClass())) {
      VEntity *e = (VEntity *)Obj;
      if (!hasSomethingToRemove && (e->EntityFlags&VEntity::EF_KillOnUnarchive) != 0) hasSomethingToRemove = true;
      elist.append(e);
    }

    Loader->Exports.Append(Obj);
  }

  GLevelInfo->Game = GGameInfo;
  GLevelInfo->World = GGameInfo->WorldInfo;

  for (int i = 0; i < Loader->Exports.Num(); ++i) {
    vassert(Loader->Exports[i]);
#ifdef VAVOOM_LOADER_CAN_SKIP_CLASSES
    auto dpp = deadThinkers.find(Loader->Exports[i]);
    if (dpp) {
      //GCon->Logf("!!! %d: %s", i, Loader->Exports[i]->GetClass()->GetName());
      Loader->Exports[i]->Serialise(*Loader);
    } else
#endif
    {
      Loader->Exports[i]->Serialise(*Loader);
    }
  }
#ifdef VAVOOM_LOADER_CAN_SKIP_CLASSES
  //for (int i = 0; i < deadThinkers.length(); ++i) deadThinkers[i]->DestroyThinker();
  for (auto it = deadThinkers.first(); it; ++it) ((VThinker *)it.getValue())->DestroyThinker();
#endif

  // unserialise acs script
  GLevel->Acs->Serialise(*Loader);

  // load collected VAcs objects contents
  for (vint32 f = 0; f < Loader->AcsExports.length(); ++f) Loader->AcsExports[f]->Serialise(*Loader);

  // `LinkToWorld()` in `VEntity::SerialiseOther()` will find the correct floor

  /*
  for (int i = 0; i < elist.length(); ++i) {
    VEntity *e = elist[i];
    GCon->Logf("ENTITY <%s>: org=(%f,%f,%f); flags=0x%08x", *e->GetClass()->GetFullName(), e->Origin.x, e->Origin.y, e->Origin.z, e->EntityFlags);
  }
  */

  // remove unnecessary entities
  if (hasSomethingToRemove && !loader_ignore_kill_on_unarchive) {
    for (int i = 0; i < elist.length(); ++i) if (elist[i]->EntityFlags&VEntity::EF_KillOnUnarchive) elist[i]->DestroyThinker();
  }

  GLevelInfo->eventAfterUnarchiveThinkers();
  GLevel->eventAfterUnarchiveThinkers();
}


//==========================================================================
//
//  ArchiveSounds
//
//==========================================================================
static void ArchiveSounds (VStream &Strm) {
  vint32 Seg = ASEG_SOUNDS;
  Strm << Seg;
#ifdef CLIENT
  GAudio->SerialiseSounds(Strm);
#else
  vint32 Dummy = 0;
  Strm << Dummy;
#endif
}


//==========================================================================
//
//  UnarchiveSounds
//
//==========================================================================
static void UnarchiveSounds (VStream &Strm) {
  AssertSegment(Strm, ASEG_SOUNDS);
#ifdef CLIENT
  GAudio->SerialiseSounds(Strm);
#else
  vint32 count = 0;
  Strm << count;
  //FIXME: keep this in sync with VAudio
  //Strm.Seek(Strm.Tell()+Dummy*36); //FIXME!
  if (count < 0) Sys_Error("invalid sound sequence data");
  while (count-- > 0) {
    vuint8 xver = 0; // current version is 0
    Strm << xver;
    if (xver != 0) Sys_Error("invalid sound sequence data");
    vint32 Sequence;
    vint32 OriginId;
    TVec Origin;
    vint32 CurrentSoundID;
    float DelayTime;
    vuint32 DidDelayOnce;
    float Volume;
    float Attenuation;
    vint32 ModeNum;
    Strm << STRM_INDEX(Sequence)
      << STRM_INDEX(OriginId)
      << Origin
      << STRM_INDEX(CurrentSoundID)
      << DelayTime
      << STRM_INDEX(DidDelayOnce)
      << Volume
      << Attenuation
      << STRM_INDEX(ModeNum);

    vint32 Offset;
    Strm << STRM_INDEX(Offset);

    vint32 Count;
    Strm << STRM_INDEX(Count);
    if (Count < 0) Sys_Error("invalid sound sequence data");
    for (int i = 0; i < Count; ++i) {
      VName SeqName;
      Strm << SeqName;
    }

    vint32 ParentSeqIdx;
    vint32 ChildSeqIdx;
    Strm << STRM_INDEX(ParentSeqIdx) << STRM_INDEX(ChildSeqIdx);
  }
#endif
}


//==========================================================================
//
// SV_SaveMap
//
//==========================================================================
static void SV_SaveMap (bool savePlayers) {
  // make sure we don't have any garbage
  Host_CollectGarbage(true);

  // open the output file
  VMemoryStream *InStrm = new VMemoryStream();
  VSaveWriterStream *Saver = new VSaveWriterStream(InStrm);

  int NamesOffset = 0;
  *Saver << NamesOffset;

  // place a header marker
  vint32 Seg = ASEG_MAP_HEADER;
  *Saver << Seg;

  // write the level timer
  *Saver << GLevel->Time << GLevel->TicTime;

  ArchiveThinkers(Saver, savePlayers);
  ArchiveSounds(*Saver);

  // place a termination marker
  Seg = ASEG_END;
  *Saver << Seg;

  ArchiveNames(Saver);

  // close the output file
  Saver->Close();

  TArray<vuint8> &Buf = InStrm->GetArray();

  VSavedMap *Map = BaseSlot.FindMap(GLevel->MapName);
  if (!Map) {
    Map = new VSavedMap();
    BaseSlot.Maps.Append(Map);
    Map->Name = GLevel->MapName;
  }

  // compress map data
  Map->DecompressedSize = Buf.Num();
  Map->Data.Clear();
  if (save_compression_level <= 0) {
    Map->Compressed = 0;
    Map->Data.setLength(Buf.length());
    if (Buf.length()) memcpy(Map->Data.ptr(), Buf.ptr(), Buf.length());
  } else {
    Map->Compressed = 1;
    VArrayStream *ArrStrm = new VArrayStream("<savemap>", Map->Data);
    ArrStrm->BeginWrite();
    VZLibStreamWriter *ZipStrm = new VZLibStreamWriter(ArrStrm, (int)save_compression_level);
    ZipStrm->Serialise(Buf.Ptr(), Buf.Num());
    delete ZipStrm;
    delete ArrStrm;
  }

  delete Saver;
}


//==========================================================================
//
//  SV_SaveCheckpoint
//
//==========================================================================
static bool SV_SaveCheckpoint () {
  if (!GGameInfo) return false;
  if (GGameInfo->NetMode != NM_Standalone) return false; // oops

  VBasePlayer *plr = nullptr;
  // check if checkpoints are possible
  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (GGameInfo->Players[i]) {
      if (!GGameInfo->Players[i]->IsCheckpointPossible()) return false;
      if (plr) return false;
      plr = GGameInfo->Players[i];
    }
  }
  if (!plr || !plr->MO) return false;

  QS_StartPhase(QSPhase::QSP_Save);
  VSavedCheckpoint &cp = BaseSlot.CheckPoint;
  cp.Clear();
  cp.Skill = GGameInfo->WorldInfo->GameSkill;
  VEntity *rwe = plr->eventGetReadyWeapon();

  if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: === creating ===");
  for (VEntity *invFirst = plr->MO->QS_GetEntityInventory();
       invFirst;
       invFirst = invFirst->QS_GetEntityInventory())
  {
    cp.AddEntity(invFirst);
    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: inventory item '%s'", invFirst->GetClass()->GetName());
  }
  if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: getting properties");

  plr->QS_Save();
  for (auto &&qse : cp.EList) qse.ent->QS_Save();
  cp.QSList = QS_GetCurrentArray();

  // count entities, build entity list
  for (int f = 0; f < cp.QSList.length(); ++f) {
    QSValue &qv = cp.QSList[f];
    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: property #%d of '%s': %s", f, (qv.ent ? qv.ent->GetClass()->GetName() : "player"), *qv.toString());
    if (!qv.ent) {
      qv.objidx = 0;
    } else {
      qv.objidx = cp.FindEntity(qv.ent);
      if (rwe == qv.ent) cp.ReadyWeapon = cp.FindEntity(rwe);
    }
  }

  QS_StartPhase(QSPhase::QSP_None);

  if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: game skill is %d, cp skill is %d", GGameInfo->WorldInfo->GameSkill, cp.Skill);
  if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: === complete ===");

  return true;
}


//==========================================================================
//
//  SV_LoadMap
//
//  returns `true` if checkpoint was loaded
//
//==========================================================================
static bool SV_LoadMap (VName MapName, bool allowCheckpoints, bool hubTeleport) {
  bool isCheckpoint = (BaseSlot.Maps.length() == 0);
  if (isCheckpoint && !allowCheckpoints) {
    Host_Error("Trying to load checkpoint in hub game!");
  }
#ifdef CLIENT
  if (isCheckpoint && svs.max_clients != 1) {
    Host_Error("Checkpoints aren't supported in networked games!");
  }
  // if we are loading a checkpoint, simulate normal map start
  if (isCheckpoint) sv_loading = false;
#else
  // standalone server
  if (isCheckpoint) {
    Host_Error("Checkpoints aren't supported on dedicated servers!");
  }
#endif

  // load a base level (spawn thinkers if this is checkpoint save)
  if (!hubTeleport) SV_ResetPlayers();
  try {
    VBasePlayer::isCheckpointSpawn = isCheckpoint;
#ifdef CLIENT
    // setup skill for server here, so checkpoints will start with a right one
    if (isCheckpoint) {
      VSavedCheckpoint &cp = BaseSlot.CheckPoint;
      if (dbg_checkpoints) GCon->Logf(NAME_Debug, "*** CP SKILL: %d", cp.Skill);
      if (cp.Skill >= 0) {
        if (dbg_checkpoints) GCon->Logf(NAME_Debug, "*** setting skill from a checkpoint: %d", cp.Skill);
        Skill = cp.Skill;
      }
    }
#endif
    SV_SpawnServer(*MapName, isCheckpoint/*spawn thinkers*/);
  } catch (...) {
    VBasePlayer::isCheckpointSpawn = false;
    throw;
  }

#ifdef CLIENT
  if (isCheckpoint) {
    sv_loading = false; // just in case
    try {
      VBasePlayer::isCheckpointSpawn = true;
      CL_SetupLocalPlayer();
    } catch (...) {
      VBasePlayer::isCheckpointSpawn = false;
      throw;
    }
    VBasePlayer::isCheckpointSpawn = false;

    Host_ResetSkipFrames();

    // do this here so that clients have loaded info, not initial one
    SV_SendServerInfoToClients();

    VSavedCheckpoint &cp = BaseSlot.CheckPoint;

    // put inventory
    VBasePlayer *plr = nullptr;
    for (int i = 0; i < MAXPLAYERS; ++i) {
      if (!GGameInfo->Players[i] || !GGameInfo->Players[i]->MO) continue;
      plr = GGameInfo->Players[i];
      break;
    }
    if (!plr) Host_Error("active player not found");
    VEntity *rwe = nullptr; // ready weapon

    QS_StartPhase(QSPhase::QSP_Load);

    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: === loading ===");
    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: --- (starting inventory)");
    if (dbg_checkpoints) plr->CallDumpInventory();
    plr->MO->QS_ClearEntityInventory();
    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: --- (cleared inventory)");
    if (dbg_checkpoints) plr->CallDumpInventory();
    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: ---");

    // create inventory items
    // have to do it backwards due to the way `AttachToOwner()` works
    for (int f = cp.EList.length()-1; f >= 0; --f) {
      VSavedCheckpoint::EntityInfo &ei = cp.EList[f];
      VEntity *inv = plr->MO->QS_SpawnEntityInventory(VName(*ei.ClassName));
      if (!inv) Host_Error("cannot spawn inventory item '%s'", *ei.ClassName);
      if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: spawned '%s'", inv->GetClass()->GetName());
      ei.ent = inv;
      if (cp.ReadyWeapon == f+1) rwe = inv;
    }
    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: --- (spawned inventory)");
    if (dbg_checkpoints) plr->CallDumpInventory();
    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: ---");

    for (int f = 0; f < cp.QSList.length(); ++f) {
      QSValue &qv = cp.QSList[f];
      if (qv.objidx == 0) {
        qv.ent = nullptr;
        if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS:  #%d:player: %s", f, *qv.toString());
      } else {
        qv.ent = cp.EList[qv.objidx-1].ent;
        vassert(qv.ent);
        if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS:  #%d:%s: %s", f, qv.ent->GetClass()->GetName(), *qv.toString());
      }
      QS_EnterValue(qv);
    }

    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: --- (inventory before setting properties)");
    if (dbg_checkpoints) plr->CallDumpInventory();
    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: ---");
    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: calling loaders");
    // call player loader, then entity loaders
    plr->QS_Load();
    for (int f = 0; f < cp.EList.length(); ++f) {
      VSavedCheckpoint::EntityInfo &ei = cp.EList[f];
      ei.ent->QS_Load();
    }

    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: --- (final inventory)");
    if (dbg_checkpoints) plr->CallDumpInventory();
    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: === done ===");
    QS_StartPhase(QSPhase::QSP_None);

    plr->eventAfterUnarchiveThinkers();

    plr->PlayerState = PST_LIVE;
    if (rwe) plr->eventSetReadyWeapon(rwe, true); // instant

    Host_ResetSkipFrames();
    return true;
  }
#endif

  Host_ResetSkipFrames();

  VSavedMap *Map = BaseSlot.FindMap(MapName);
  vassert(Map);

  // decompress map data
  TArray<vuint8> DecompressedData;
  if (!Map->Compressed) {
    DecompressedData.setLength(Map->Data.length());
    if (Map->Data.length()) memcpy(DecompressedData.ptr(), Map->Data.ptr(), Map->Data.length());
  } else {
    VArrayStream *ArrStrm = new VArrayStream("<savemap:mapdata>", Map->Data);
    VZLibStreamReader *ZipStrm = new VZLibStreamReader(ArrStrm, VZLibStreamReader::UNKNOWN_SIZE, Map->DecompressedSize);
    DecompressedData.SetNum(Map->DecompressedSize);
    ZipStrm->Serialise(DecompressedData.Ptr(), DecompressedData.Num());
    delete ZipStrm;
    delete ArrStrm;
  }

  VSaveLoaderStream *Loader = new VSaveLoaderStream(new VArrayStream("<savemap:mapdata>", DecompressedData));

  // load names
  UnarchiveNames(Loader);

  AssertSegment(*Loader, ASEG_MAP_HEADER);

  // read the level timer
  *Loader << GLevel->Time << GLevel->TicTime;

  UnarchiveThinkers(Loader);
  UnarchiveSounds(*Loader);

  AssertSegment(*Loader, ASEG_END);

  // free save buffer
  Loader->Close();
  delete Loader;
  Loader = nullptr;

  Host_ResetSkipFrames();

  // do this here so that clients have loaded info, not initial one
  SV_SendServerInfoToClients();

  Host_ResetSkipFrames();

  return false;
}


//==========================================================================
//
//  SV_SaveGame
//
//==========================================================================
static void SV_SaveGame (int slot, VStr Description, bool checkpoint, bool isAutosave) {
  BaseSlot.Description = Description;
  BaseSlot.CurrentMap = GLevel->MapName;

  // save out the current map
  if (checkpoint) {
    // if we have no maps in our base slot, checkpoints are enabled
    if (BaseSlot.Maps.length() != 0) {
      GCon->Logf("AUTOSAVE: cannot create checkpoint, perform a full save sequence");
      checkpoint = false;
    } else {
      GCon->Logf("AUTOSAVE: checkpoints allowed");
    }
  }

  // perform full update, so lightmap cache will be valid
  if (!checkpoint && GLevel->Renderer && GLevel->Renderer->isNeedLightmapCache()) {
    GLevel->Renderer->FullWorldUpdate(true);
  }

  SV_SendBeforeSaveEvent(isAutosave, checkpoint);

  if (checkpoint) {
    // player state save
    if (!SV_SaveCheckpoint()) {
      GCon->Logf("AUTOSAVE: checkpoint creation failed, perform a full save sequence");
      checkpoint = false;
      SV_SaveMap(true); // true = save player info
    }
  } else {
    // full save
    SV_SaveMap(true); // true = save player info
  }

  // write data to destination slot
  if (BaseSlot.SaveToSlot(slot)) {
    // checkpoints using normal map cache
    if (!checkpoint && !saveFileBase.isEmpty()) {
      VStr ccfname = saveFileBase+".lmap";
      #ifdef CLIENT
      bool doPrecalc = (r_precalc_static_lights_override >= 0 ? !!r_precalc_static_lights_override : r_precalc_static_lights);
      #else
      enum { doPrecalc = false };
      #endif
      if (!GLevel->Renderer || !GLevel->Renderer->isNeedLightmapCache() || !loader_cache_data || !doPrecalc) {
        // no rendered usually means that this is some kind of server (the thing that should not be, but...)
        Sys_FileDelete(ccfname);
      } else {
        GLevel->cacheFileBase = saveFileBase;
        GLevel->cacheFlags &= ~VLevel::CacheFlag_Ignore;
        VStream *lmc = FL_OpenSysFileWrite(ccfname);
        if (lmc) {
          GCon->Logf("writing lightmap cache to '%s'", *ccfname);
          GLevel->Renderer->saveLightmaps(lmc);
          bool err = lmc->IsError();
          lmc->Close();
          err = (err || lmc->IsError());
          delete lmc;
          if (err) {
            GCon->Logf(NAME_Warning, "removed broken lightmap cache '%s'", *ccfname);
            Sys_FileDelete(ccfname);
          }
        } else {
          GCon->Logf(NAME_Warning, "cannot create lightmap cache file '%s'", *ccfname);
        }
      }
    }
    SV_SendAfterSaveEvent(isAutosave, checkpoint);
  }

  Host_ResetSkipFrames();
}


#ifdef CLIENT
//==========================================================================
//
//  SV_LoadGame
//
//==========================================================================
static void SV_LoadGame (int slot) {
  SV_ShutdownGame();

  // temp hack
  SV_SetupSkipCallback();

  if (!BaseSlot.LoadSlot(slot)) return;

  sv_loading = true;

  // load the current map
  if (!SV_LoadMap(BaseSlot.CurrentMap, true/*allowCheckpoints*/, false/*hubTeleport*/)) {
    // not a checkpoint
    GLevel->cacheFileBase = saveFileBase;
    GLevel->cacheFlags &= ~VLevel::CacheFlag_Ignore;
    //GCon->Logf(NAME_Debug, "**********************: <%s>", *GLevel->cacheFileBase);
    #ifdef CLIENT
    if (GGameInfo->NetMode != NM_DedicatedServer) CL_SetupLocalPlayer();
    #endif
    // launch waiting scripts
    if (!svs.deathmatch) GLevel->Acs->CheckAcsStore();

    //GCon->Logf(NAME_Debug, "************************** (%d)", svs.max_clients);
    for (int i = 0; i < MAXPLAYERS; ++i) {
      VBasePlayer *Player = GGameInfo->Players[i];
      if (!Player) {
        //GCon->Logf(NAME_Debug, "*** no player #%d", i);
        continue;
      }
      Player->eventAfterUnarchiveThinkers();
    }
  } else {
    // checkpoint
    GLevel->cacheFlags &= ~VLevel::CacheFlag_Ignore;
  }

  SV_SendLoadedEvent();
}
#endif


//==========================================================================
//
//  SV_InitBaseSlot
//
//==========================================================================
void SV_InitBaseSlot () {
  BaseSlot.Clear();
}


//==========================================================================
//
// SV_MapTeleport
//
//==========================================================================
/*
  CHANGELEVEL_KEEPFACING      = 0x00000001,
  CHANGELEVEL_RESETINVENTORY  = 0x00000002,
  CHANGELEVEL_NOMONSTERS      = 0x00000004,
  CHANGELEVEL_CHANGESKILL     = 0x00000008,
  CHANGELEVEL_NOINTERMISSION  = 0x00000010,
  CHANGELEVEL_RESETHEALTH     = 0x00000020,
  CHANGELEVEL_PRERAISEWEAPON  = 0x00000040,
*/
void SV_MapTeleport (VName mapname, int flags, int newskill) {
  TArray<VThinker *> TravelObjs;

  if (newskill >= 0 && (flags&CHANGELEVEL_CHANGESKILL) != 0) {
    GCon->Logf("SV_MapTeleport: new skill is %d", newskill);
    Skill = newskill;
    flags &= ~CHANGELEVEL_CHANGESKILL; // clear flag
  }

  // we won't show intermission anyway, so remove this flag
  flags &= ~CHANGELEVEL_NOINTERMISSION;

  if (flags&~(CHANGELEVEL_KEEPFACING|CHANGELEVEL_RESETINVENTORY|CHANGELEVEL_RESETHEALTH|CHANGELEVEL_PRERAISEWEAPON|CHANGELEVEL_REMOVEKEYS)) {
    GCon->Logf("SV_MapTeleport: unimplemented flag set: 0x%04x", (unsigned)flags);
  }

  TAVec plrAngles[MAXPLAYERS];
  memset((void *)plrAngles, 0, sizeof(plrAngles));

  // call PreTravel event
  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (!GGameInfo->Players[i]) continue;
    plrAngles[i] = GGameInfo->Players[i]->ViewAngles;
    GGameInfo->Players[i]->eventPreTravel();
  }

  // collect list of thinkers that will go to the new level (player inventory)
  for (VThinker *Th = GLevel->ThinkerHead; Th; Th = Th->Next) {
    VEntity *vent = Cast<VEntity>(Th);
    if (vent && vent->Owner && vent->Owner->IsPlayer()) {
      TravelObjs.Append(vent);
      GLevel->RemoveThinker(vent);
      vent->UnlinkFromWorld();
      GLevel->DelSectorList();
      vent->StopSound(0);
      //GCon->Logf(NAME_Debug, "SV_MapTeleport: saved player inventory item '%s'", vent->GetClass()->GetName());
      continue;
    }
    if (Th->IsA(VPlayerReplicationInfo::StaticClass())) {
      TravelObjs.Append(Th);
      GLevel->RemoveThinker(Th);
    }
  }

  if (!svs.deathmatch) {
    const VMapInfo &old_info = P_GetMapInfo(GLevel->MapName);
    const VMapInfo &new_info = P_GetMapInfo(mapname);
    // all maps in cluster 0 are treated as in different clusters
    if (old_info.Cluster && old_info.Cluster == new_info.Cluster &&
        (P_GetClusterDef(old_info.Cluster)->Flags&CLUSTERF_Hub))
    {
      // same cluster: save map without saving player mobjs
      SV_SaveMap(false);
    } else {
      // entering new cluster: clear base slot
      if (dbg_save_verbose&0x20) GCon->Logf("**** NEW CLUSTER ****");
      BaseSlot.Clear();
    }
  }

  vuint8 oldNoMonsters = GGameInfo->nomonsters;
  if (flags&CHANGELEVEL_NOMONSTERS) GGameInfo->nomonsters = 1;

  sv_map_travel = true;
  if (!svs.deathmatch && BaseSlot.FindMap(mapname)) {
    // unarchive map
    SV_LoadMap(mapname, false/*allowCheckpoints*/, true/*hubTeleport*/); // don't allow checkpoints
  } else {
    // new map
    SV_SpawnServer(*mapname, true/*spawn thinkers*/);
    // if we spawned a new server, there is no need to reset inventory, health or keys
    flags &= ~(CHANGELEVEL_RESETINVENTORY|CHANGELEVEL_RESETHEALTH|CHANGELEVEL_REMOVEKEYS);
  }

  if (flags&CHANGELEVEL_NOMONSTERS) GGameInfo->nomonsters = oldNoMonsters;

  // add traveling thinkers to the new level
  for (int i = 0; i < TravelObjs.Num(); ++i) {
    //GCon->Logf(NAME_Debug, "SV_MapTeleport: adding back player inventory item '%s'", TravelObjs[i]->GetClass()->GetName());
    GLevel->AddThinker(TravelObjs[i]);
    VEntity *Ent = Cast<VEntity>(TravelObjs[i]);
    if (Ent) Ent->LinkToWorld(true);
  }

  Host_ResetSkipFrames();

#ifdef CLIENT
  bool doSaveGame = false;
  if (GGameInfo->NetMode == NM_TitleMap ||
      GGameInfo->NetMode == NM_Standalone ||
      GGameInfo->NetMode == NM_ListenServer)
  {
    CL_SetupStandaloneClient();
    doSaveGame = sv_new_map_autosave;
  }

  if (doSaveGame && fsys_hasMapPwads && fsys_PWadMaps.length()) {
    // do not autosave on iwad maps
    doSaveGame = false;
    for (auto &&lmp : fsys_PWadMaps) {
      if (lmp.mapname.strEquCI(*mapname)) {
        doSaveGame = true;
        break;
      }
    }
    if (!doSaveGame) GCon->Logf(NAME_Warning, "autosave skipped due to iwad map");
  }
#else
  const bool doSaveGame = false;
#endif

  if (flags&(CHANGELEVEL_KEEPFACING|CHANGELEVEL_RESETINVENTORY|CHANGELEVEL_RESETHEALTH|CHANGELEVEL_PRERAISEWEAPON|CHANGELEVEL_REMOVEKEYS)) {
    for (int i = 0; i < MAXPLAYERS; ++i) {
      VBasePlayer *plr = GGameInfo->Players[i];
      if (!plr) continue;

      if (flags&CHANGELEVEL_KEEPFACING) {
        plr->ViewAngles = plrAngles[i];
        plr->eventClientSetAngles(plrAngles[i]);
        plr->PlayerFlags &= ~VBasePlayer::PF_FixAngle;
      }
      if (flags&CHANGELEVEL_RESETINVENTORY) plr->eventResetInventory();
      if (flags&CHANGELEVEL_RESETHEALTH) plr->eventResetHealth();
      if (flags&CHANGELEVEL_PRERAISEWEAPON) plr->eventPreraiseWeapon();
      if (flags&CHANGELEVEL_REMOVEKEYS) plr->eventRemoveKeys();
    }
  }

  // launch waiting scripts
  if (!svs.deathmatch) GLevel->Acs->CheckAcsStore();

  if (doSaveGame) GCmdBuf << "AutoSaveEnter\n";

  /*
  for (int i = 0; i < MAXPLAYERS; ++i) {
    VBasePlayer *plr = GGameInfo->Players[i];
    if (!plr) continue;
    VEntity *sve = plr->ehGetSavedInventory();
    if (!sve) continue;
    GCon->Logf(NAME_Debug, "+++ player #%d saved inventory +++", i);
    sve->DebugDumpInventory(true);
  }
  */
}


#ifdef CLIENT
void Draw_SaveIcon ();
void Draw_LoadIcon ();


static bool CheckIfLoadIsAllowed () {
  if (svs.deathmatch) {
    GCon->Log("Can't load in deathmatch game");
    return false;
  }

  return true;
}
#endif


static bool CheckIfSaveIsAllowed () {
  if (svs.deathmatch) {
    GCon->Log("Can't save in deathmatch game");
    return false;
  }

  if (GGameInfo->NetMode == NM_None || GGameInfo->NetMode == NM_TitleMap || GGameInfo->NetMode == NM_Client) {
    GCon->Log("You can't save if you aren't playing!");
    return false;
  }

  if (sv.intermission) {
    GCon->Log("You can't save while in intermission!");
    return false;
  }

  return true;
}


static void BroadcastSaveText (const char *msg) {
  if (!msg || !msg[0]) return;
  if (sv_save_messages) {
    for (int i = 0; i < MAXPLAYERS; ++i) {
      VBasePlayer *plr = GGameInfo->Players[i];
      if (!plr) continue;
      if ((plr->PlayerFlags&VBasePlayer::PF_Spawned) == 0) continue;
      plr->eventClientPrint(msg);
    }
  } else {
    GCon->Log(msg);
  }
}


void SV_AutoSave (bool checkpoint) {
  if (!CheckIfSaveIsAllowed()) return;

  int aslot = SV_FindAutosaveSlot();
  if (!aslot) {
    BroadcastSaveText("Cannot find autosave slot (this should not happen)!");
    return;
  }

#ifdef CLIENT
  Draw_SaveIcon();
#endif

  TTimeVal tv;
  GetTimeOfDay(&tv);
  VStr svname = TimeVal2Str(&tv, true)+": "+VStr("AUTO: ")+(*GLevel->MapName);

  SV_SaveGame(aslot, svname, checkpoint, true);
  Host_ResetSkipFrames();

  BroadcastSaveText(va("Game autosaved to slot #%d", -aslot));
}


#ifdef CLIENT
void SV_AutoSaveOnLevelExit () {
  if (!r_dbg_save_on_level_exit) return;

  if (!CheckIfSaveIsAllowed()) return;

  int aslot = SV_FindAutosaveSlot();
  if (!aslot) {
    BroadcastSaveText("Cannot find autosave slot (this should not happen)!");
    return;
  }

  Draw_SaveIcon();

  TTimeVal tv;
  GetTimeOfDay(&tv);
  VStr svname = TimeVal2Str(&tv, true)+": "+VStr("OUT: ")+(*GLevel->MapName);

  SV_SaveGame(aslot, svname, false, true); // not a checkpoint, obviously
  Host_ResetSkipFrames();

  BroadcastSaveText(va("Game autosaved to slot #%d", -aslot));
}


//==========================================================================
//
//  COMMAND Save
//
//  Called by the menu task. Description is a 24 byte text string
//
//==========================================================================
COMMAND(Save) {
  if (Args.Num() != 3) {
    GCon->Log("usage: save slotindex description");
    return;
  }

  if (!CheckIfSaveIsAllowed()) return;

  if (Args[2].Length() >= 32) {
    BroadcastSaveText("Description too long!");
    return;
  }

  Draw_SaveIcon();

  SV_SaveGame(VStr::atoi(*Args[1]), Args[2], false, false); // not a checkpoint
  Host_ResetSkipFrames();

  BroadcastSaveText("Game saved.");
}


//==========================================================================
//
//  COMMAND DeleteSavedGame <slotidx|quick>
//
//==========================================================================
COMMAND(DeleteSavedGame) {
  //GCon->Logf("DeleteSavedGame: argc=%d", Args.length());

  if (Args.Num() != 2) return;

  if (!CheckIfLoadIsAllowed()) return;

  VStr numstr = Args[1].xstrip();
  if (numstr.isEmpty()) return;

  //GCon->Logf("DeleteSavedGame: <%s>", *numstr);

  if (/*numstr.ICmp("quick") == 0*/numstr.startsWithCI("q")) {
    if (SV_DeleteSlotFile(QUICKSAVE_SLOT)) BroadcastSaveText("Quicksave deleted.");
    return;
  }

  int pos = 0;
  while (pos < numstr.length() && (vuint8)numstr[pos] <= ' ') ++pos;
  if (pos >= numstr.length()) return;

  bool neg = false;
  if (numstr[pos] == '-') {
    neg = true;
    ++pos;
    if (pos >= numstr.length()) return;
  }

  int slot = 0;
  while (pos < numstr.length()) {
    char ch = numstr[pos++];
    if (ch < '0' || ch > '9') return;
    slot = slot*10+ch-'0';
  }
  //GCon->Logf("DeleteSavedGame: slot=%d (neg=%d)", slot, (neg ? 1 : 0));
  if (neg && slot == abs(QUICKSAVE_SLOT)) {
    slot = QUICKSAVE_SLOT;
  } else {
    if (slot < 0 || slot > 99) return;
    if (neg) slot = -slot;
  }

  if (SV_DeleteSlotFile(slot)) {
         if (slot == QUICKSAVE_SLOT) BroadcastSaveText("Quicksave deleted.");
    else if (slot < 0) BroadcastSaveText(va("Autosave #%d deleted", -slot));
    else BroadcastSaveText(va("Savegame #%d deleted", slot));
  }
}


//==========================================================================
//
//  COMMAND Load
//
//==========================================================================
COMMAND(Load) {
  if (Args.Num() != 2) return;

  if (!CheckIfLoadIsAllowed()) return;

  int slot = VStr::atoi(*Args[1]);
  VStr desc;
  if (!SV_GetSaveString(slot, desc)) {
    BroadcastSaveText("Empty slot!");
    return;
  }
  GCon->Logf("Loading \"%s\"", *desc);

  Draw_LoadIcon();
  SV_LoadGame(slot);
  Host_ResetSkipFrames();

  //if (GGameInfo->NetMode == NM_Standalone) SV_UpdateRebornSlot(); // copy the base slot to the reborn slot
  BroadcastSaveText(va("Loaded save \"%s\".", *desc));
}


//==========================================================================
//
//  COMMAND QuickSave
//
//==========================================================================
COMMAND(QuickSave) {
  if (!CheckIfSaveIsAllowed()) return;

  Draw_SaveIcon();

  SV_SaveGame(QUICKSAVE_SLOT, "quicksave", false, false); // not a checkpoint
  Host_ResetSkipFrames();

  BroadcastSaveText("Game quicksaved.");
}


//==========================================================================
//
//  COMMAND QuickLoad
//
//==========================================================================
COMMAND(QuickLoad) {
  if (!CheckIfLoadIsAllowed()) return;

  VStr desc;
  if (!SV_GetSaveString(QUICKSAVE_SLOT, desc)) {
    BroadcastSaveText("Empty quicksave slot");
    return;
  }
  GCon->Log("Loading quicksave");

  Draw_LoadIcon();
  SV_LoadGame(QUICKSAVE_SLOT);
  Host_ResetSkipFrames();
  // don't copy to reborn slot -- this is quickload after all!

  BroadcastSaveText("Quicksave loaded.");
}


//==========================================================================
//
//  COMMAND AutoSaveEnter
//
//==========================================================================
COMMAND(AutoSaveEnter) {
  // there is no reason to autosave on standard maps when we have pwads
  if (!CheckIfSaveIsAllowed()) return;

  int aslot = SV_FindAutosaveSlot();
  if (!aslot) {
    BroadcastSaveText("Cannot find autosave slot (this should not happen)!");
    return;
  }

  Draw_SaveIcon();

  TTimeVal tv;
  GetTimeOfDay(&tv);
  VStr svname = TimeVal2Str(&tv, true)+": "+(*GLevel->MapName);

  SV_SaveGame(aslot, svname, sv_autoenter_checkpoints, true);
  Host_ResetSkipFrames();

  BroadcastSaveText(va("Game autosaved to slot #%d", -aslot));
}


//==========================================================================
//
//  COMMAND AutoSaveLeave
//
//==========================================================================
COMMAND(AutoSaveLeave) {
  if (GGameInfo->NetMode == NM_None || GGameInfo->NetMode == NM_Client) return;
  SV_AutoSaveOnLevelExit();
  Host_ResetSkipFrames();
}


//==========================================================================
//
//  COMMAND ShowSavePrefix
//
//==========================================================================
COMMAND(ShowSavePrefix) {
  auto wadlist = FL_GetWadPk3List();
  GCon->Log("==== MODS ====");
  for (auto &&mname: wadlist) GCon->Logf("  %s", *mname);
  GCon->Log("----");
  vuint32 hash = SV_GetModListHash();
  VStr pfx = VStr::buf2hex(&hash, 4);
  GCon->Logf("save prefix: %s", *pfx);
}
#endif
