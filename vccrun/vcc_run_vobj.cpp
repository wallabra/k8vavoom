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

//native static final int fsysAppendDir (string path, optional string pfx);
IMPLEMENT_FREE_FUNCTION(VObject, fsysAppendDir) {
  VOptParamStr pfx;
  VStr fname;
  vobjGetParam(fname, pfx);
  //fprintf(stderr, "pakid(%d)=%d; fname=<%s>\n", (int)specified_pakid, pakid, *fname);
  if (pfx.specified) {
    RET_INT(fsysAppendDir(fname, pfx));
  } else {
    RET_INT(fsysAppendDir(fname));
  }
}


// append archive to the list of archives
// it will be searched in the current dir, and then in `fsysBaseDir`
// returns pack id or 0
//native static final int fsysAppendPak (string fname, optional int pakid);
IMPLEMENT_FREE_FUNCTION(VObject, fsysAppendPak) {
  VStr fname;
  VOptParamInt pakid(-1);
  vobjGetParam(fname, pakid);
  //fprintf(stderr, "pakid(%d)=%d; fname=<%s>\n", (int)specified_pakid, pakid, *fname);
  if (pakid.specified) {
    RET_INT(fsysAppendPak(fname, pakid));
  } else {
    RET_INT(fsysAppendPak(fname));
  }
}

// remove given pack from pack list
//native static final void fsysRemovePak (int pakid);
IMPLEMENT_FREE_FUNCTION(VObject, fsysRemovePak) {
  int pakid;
  vobjGetParam(pakid);
  fsysRemovePak(pakid);
}

// remove all packs from pakid and later
//native static final void fsysRemovePaksFrom (int pakid);
IMPLEMENT_FREE_FUNCTION(VObject, fsysRemovePaksFrom) {
  int pakid;
  vobjGetParam(pakid);
  fsysRemovePaksFrom(pakid);
}

// 0: no such pack
//native static final int fsysFindPakByPrefix (string pfx);
IMPLEMENT_FREE_FUNCTION(VObject, fsysFindPakByPrefix) {
  VStr pfx;
  vobjGetParam(pfx);
  RET_BOOL(fsysFindPakByPrefix(pfx));
}

//native static final bool fsysFileExists (string fname, optional int pakid);
IMPLEMENT_FREE_FUNCTION(VObject, fsysFileExists) {
  VStr fname;
  VOptParamInt pakid(-1);
  vobjGetParam(fname, pakid);
  if (pakid.specified) {
    RET_BOOL(fsysFileExists(fname, pakid));
  } else {
    RET_BOOL(fsysFileExists(fname));
  }
}

// find file with any extension
//native static final string fsysFileFindAnyExt (string fname, optional int pakid);
IMPLEMENT_FREE_FUNCTION(VObject, fsysFileFindAnyExt) {
  VStr fname;
  VOptParamInt pakid(-1);
  vobjGetParam(fname, pakid);
  if (pakid.specified) {
    RET_STR(fsysFileFindAnyExt(fname, pakid));
  } else {
    RET_STR(fsysFileFindAnyExt(fname));
  }
}


// return pack file path for the given pack id (or empty string)
//native static final string fsysGetPakPath (int pakid);
IMPLEMENT_FREE_FUNCTION(VObject, fsysGetPakPath) {
  int pakid;
  vobjGetParam(pakid);
  RET_STR(fsysGetPakPath(pakid));
}

// return pack prefix for the given pack id (or empty string)
//native static final string fsysGetPakPrefix (int pakid);
IMPLEMENT_FREE_FUNCTION(VObject, fsysGetPakPrefix) {
  int pakid;
  vobjGetParam(pakid);
  RET_STR(fsysGetPakPrefix(pakid));
}


//native static final int fsysGetLastPakId ();
IMPLEMENT_FREE_FUNCTION(VObject, fsysGetLastPakId) {
  RET_INT(fsysGetLastPakId());
}


IMPLEMENT_FREE_FUNCTION(VObject, get_fsysKillCommonZipPrefix) {
  RET_BOOL(fsysKillCommonZipPrefix);
}

IMPLEMENT_FREE_FUNCTION(VObject, set_fsysKillCommonZipPrefix) {
  bool v;
  vobjGetParam(v);
  fsysKillCommonZipPrefix = v;
}


// native final void appSetName (string appname);
IMPLEMENT_FREE_FUNCTION(VObject, appSetName) {
  VStr aname;
  vobjGetParam(aname);
  appName = aname;
}


#ifdef SERIALIZER_USE_LIBHA
static const char *LHABinStorageSignature = "VaVoom C Binary Data Storage V0"; // 32 bytes

struct LHAIOData {
  VStream *sin;
  VStream *sout;
};

extern "C" {
  // return number of bytes read; 0: EOF; <0: error; can be less than buf_len
  static int lhaRead (void *buf, int buf_len, void *udata) {
    LHAIOData *dd = (LHAIOData *)udata;
    if (dd->sin->IsError()) return -1;
    auto left = dd->sin->TotalSize()-dd->sin->Tell();
    if (left == 0) return 0;
    if (buf_len > left) buf_len = left;
    dd->sin->Serialise(buf, buf_len);
    if (dd->sin->IsError()) return -1;
    return buf_len;
  }

  // result != buf_len: error
  static int lhaWrite (const void *buf, int buf_len, void *udata) {
    LHAIOData *dd = (LHAIOData *)udata;
    if (dd->sout->IsError()) return -1;
    dd->sout->Serialise(buf, buf_len);
    if (dd->sout->IsError()) return -1;
    return buf_len;
  }

  static libha_io_t lhaiostrm {
    .bread = &lhaRead,
    .bwrite = &lhaWrite,
  };
}
#endif


//native final static bool appSaveOptions (Object optobj, optional string optfile, optional bool packit);
IMPLEMENT_FREE_FUNCTION(VObject, appSaveOptions) {
  VObject *optobj;
  VOptParamStr optfile;
  VOptParamBool packit(true);
  vobjGetParam(optobj, optfile, packit);
  (void)packit;
  if (appName.isEmpty() || !optobj) { RET_BOOL(false); return; }
  if (optobj->GetClass()->Name == NAME_Object) { RET_BOOL(false); return; }
  ObjectSaveMap svmap(optobj);
  if (svmap.wasError) { RET_BOOL(false); return; }
  auto fname = buildConfigName(optfile);
  if (fname.isEmpty()) { RET_BOOL(false); return; }
  auto strm = fsysOpenDiskFileWrite(fname);
  if (!strm) { RET_BOOL(false); return; }
#ifdef SERIALIZER_USE_LIBHA
  if (packit) {
    auto wrs = new VStreamMemWrite();
    ObjectSaver saver(*wrs, svmap);
    bool res = saver.saveAll();
    if (res && !saver.IsError()) {
      strm->Serialise(LHABinStorageSignature, 32);
      auto rds = new VStreamMemRead(wrs->getData(), wrs->Tell());
      LHAIOData data;
      data.sin = rds;
      data.sout = strm;
      libha_t lha = libha_alloc(&lhaiostrm, &data);
      if (!lha) {
        delete rds;
        delete wrs;
        delete strm;
        RET_BOOL(false);
        return;
      }
      auto herr = libha_pack(lha);
      libha_free(lha);
      delete rds;
      delete wrs;
      delete strm;
      RET_BOOL(herr == LIBHA_ERR_OK);
    } else {
      delete wrs;
      delete strm;
      RET_BOOL(false);
    }
  } else {
    ObjectSaver saver(*strm, svmap);
    bool res = saver.saveAll();
    bool err = saver.IsError();
    delete strm;
    RET_BOOL(res && !err);
  }
#else
  ObjectSaver saver(*strm, svmap);
  bool res = saver.saveAll();
  bool err = saver.IsError();
  delete strm;
  RET_BOOL(res && !err);
#endif
}


//native final static spawner Object appLoadOptions (class cls, optional string optfile);
IMPLEMENT_FREE_FUNCTION(VObject, appLoadOptions) {
  VClass *cls;
  VOptParamStr optfile;
  vobjGetParam(cls, optfile);
  if (!cls) { RET_REF(nullptr); return; }
  if (cls->Name == NAME_Object) { RET_REF(nullptr); return; }
  if (appName.isEmpty()) { RET_REF(nullptr); return; }
  auto fname = buildConfigName(optfile);
  if (fname.isEmpty()) { RET_REF(nullptr); return; }
  auto strm = fsysOpenDiskFile(fname);
  if (!strm) { RET_REF(nullptr); return; }
#ifdef SERIALIZER_USE_LIBHA
  char sign[32];
  strm->Serialise(sign, 32);
  if (!strm->IsError() && memcmp(sign, LHABinStorageSignature, 32) == 0) {
    // packed data
    auto sz = strm->TotalSize()-strm->Tell();
    auto xbuf = new vuint8[sz];
    strm->Serialise(xbuf, sz);
    bool rerr = strm->IsError();
    delete strm;
    if (rerr) {
      delete xbuf;
      RET_REF(nullptr);
      return;
    }
    auto rds = new VStreamMemRead(xbuf, sz);
    auto wrs = new VStreamMemWrite();
    LHAIOData data;
    data.sin = rds;
    data.sout = wrs;
    libha_t lha = libha_alloc(&lhaiostrm, &data);
    if (!lha) {
      delete rds;
      delete wrs;
      delete xbuf;
      RET_REF(nullptr);
      return;
    }
    auto herr = libha_unpack(lha);
    libha_free(lha);
    if (herr != LIBHA_ERR_OK) {
      delete rds;
      delete wrs;
      delete xbuf;
      RET_REF(nullptr);
      return;
    }
    delete rds;
    delete [] xbuf;
    rds = new VStreamMemRead(wrs->getData(), wrs->Tell());
    ObjectLoader ldr(*rds, cls);
    bool uerr = !ldr.loadAll();
    delete rds;
    delete wrs;
    if (uerr) {
      ldr.clear();
      RET_REF(nullptr);
      return;
    }
    RET_REF(ldr.objarr[1]); // 0 is `none`
  } else if (!strm->IsError() && memcmp(sign, BinStorageSignature, 32) == 0) {
    // unpacked data
    strm->Seek(0);
    ObjectLoader ldr(*strm, cls);
    if (!ldr.loadAll()) {
      delete strm;
      ldr.clear();
      RET_REF(nullptr);
      return;
    }
    delete strm;
    RET_REF(ldr.objarr[1]); // 0 is `none`
  } else {
    // wutafuck?
    delete strm;
    RET_REF(nullptr);
  }
#else
  ObjectLoader ldr(*strm, cls);
  if (!ldr.loadAll()) {
    delete strm;
    ldr.clear();
    RET_REF(nullptr);
    return;
  }
  delete strm;
  //fprintf(stderr, "%p\n", ldr.objarr[1]);
  RET_REF(ldr.objarr[1]); // 0 is `none`
#endif
}
