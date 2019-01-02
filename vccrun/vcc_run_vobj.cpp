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

//native static final int fsysAppendDir (string path, optional string pfx);
IMPLEMENT_FUNCTION(VObject, fsysAppendDir) {
  P_GET_STR_OPT(pfx, VStr());
  P_GET_STR(fname);
  //fprintf(stderr, "pakid(%d)=%d; fname=<%s>\n", (int)specified_pakid, pakid, *fname);
  if (specified_pfx) {
    RET_INT(fsysAppendDir(fname, pfx));
  } else {
    RET_INT(fsysAppendDir(fname));
  }
}


// append archive to the list of archives
// it will be searched in the current dir, and then in `fsysBaseDir`
// returns pack id or 0
//native static final int fsysAppendPak (string fname, optional int pakid);
IMPLEMENT_FUNCTION(VObject, fsysAppendPak) {
  P_GET_INT_OPT(pakid, -1);
  P_GET_STR(fname);
  //fprintf(stderr, "pakid(%d)=%d; fname=<%s>\n", (int)specified_pakid, pakid, *fname);
  if (specified_pakid) {
    RET_INT(fsysAppendPak(fname, pakid));
  } else {
    RET_INT(fsysAppendPak(fname));
  }
}

// remove given pack from pack list
//native static final void fsysRemovePak (int pakid);
IMPLEMENT_FUNCTION(VObject, fsysRemovePak) {
  P_GET_INT(pakid);
  fsysRemovePak(pakid);
}

// remove all packs from pakid and later
//native static final void fsysRemovePaksFrom (int pakid);
IMPLEMENT_FUNCTION(VObject, fsysRemovePaksFrom) {
  P_GET_INT(pakid);
  fsysRemovePaksFrom(pakid);
}

// 0: no such pack
//native static final int fsysFindPakByPrefix (string pfx);
IMPLEMENT_FUNCTION(VObject, fsysFindPakByPrefix) {
  P_GET_STR(pfx);
  RET_BOOL(fsysFindPakByPrefix(pfx));
}

//native static final bool fsysFileExists (string fname, optional int pakid);
IMPLEMENT_FUNCTION(VObject, fsysFileExists) {
  P_GET_INT_OPT(pakid, -1);
  P_GET_STR(fname);
  if (specified_pakid) {
    RET_BOOL(fsysFileExists(fname, pakid));
  } else {
    RET_BOOL(fsysFileExists(fname));
  }
}

// find file with any extension
//native static final string fsysFileFindAnyExt (string fname, optional int pakid);
IMPLEMENT_FUNCTION(VObject, fsysFileFindAnyExt) {
  P_GET_INT_OPT(pakid, -1);
  P_GET_STR(fname);
  if (specified_pakid) {
    RET_STR(fsysFileFindAnyExt(fname, pakid));
  } else {
    RET_STR(fsysFileFindAnyExt(fname));
  }
}


// return pack file path for the given pack id (or empty string)
//native static final string fsysGetPakPath (int pakid);
IMPLEMENT_FUNCTION(VObject, fsysGetPakPath) {
  P_GET_INT(pakid);
  RET_STR(fsysGetPakPath(pakid));
}

// return pack prefix for the given pack id (or empty string)
//native static final string fsysGetPakPrefix (int pakid);
IMPLEMENT_FUNCTION(VObject, fsysGetPakPrefix) {
  P_GET_INT(pakid);
  RET_STR(fsysGetPakPrefix(pakid));
}


//native static final int fsysGetLastPakId ();
IMPLEMENT_FUNCTION(VObject, fsysGetLastPakId) {
  RET_INT(fsysGetLastPakId());
}


IMPLEMENT_FUNCTION(VObject, get_fsysKillCommonZipPrefix) {
  RET_BOOL(fsysKillCommonZipPrefix);
}

IMPLEMENT_FUNCTION(VObject, set_fsysKillCommonZipPrefix) {
  P_GET_BOOL(v);
  fsysKillCommonZipPrefix = v;
}


// native final void appSetName (string appname);
IMPLEMENT_FUNCTION(VObject, appSetName) {
  P_GET_STR(aname);
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


//native final bool appSaveOptions (Object optobj, optional string optfile, optional bool packit);
IMPLEMENT_FUNCTION(VObject, appSaveOptions) {
  P_GET_BOOL_OPT(packit, true);
  (void)packit;
  (void)specified_packit;
  P_GET_STR_OPT(optfile, VStr());
  P_GET_REF(VObject, optobj);
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
    delete strm;
    RET_BOOL(res && !saver.IsError());
  }
#else
  ObjectSaver saver(*strm, svmap);
  bool res = saver.saveAll();
  delete strm;
  RET_BOOL(res && !saver.IsError());
#endif
}


//native final spawner Object appLoadOptions (class cls, optional string optfile);
IMPLEMENT_FUNCTION(VObject, appLoadOptions) {
  P_GET_STR_OPT(optfile, VStr());
  P_GET_PTR(VClass, cls);
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
