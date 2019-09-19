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
#ifndef VAVOOM_FSYS_FORMATS_LOCAL_HEADER
#define VAVOOM_FSYS_FORMATS_LOCAL_HEADER

// this is included from "fsys_local.h"


//==========================================================================
//  VWadFile
//==========================================================================
class VWadFile : public VPakFileBase {
private:
  void InitNamespaces ();
  void FixVoiceNamespaces ();
  void InitNamespace (EWadNamespace NS, VName Start, VName End, VName AltStart=NAME_None, VName AltEnd=NAME_None, bool flatNS=false);

public:
  VWadFile ();

  void Open (VStr FileName, bool FixVoices, VStream *InStream);
  void OpenSingleLumpStream (VStream *strm, VStr FileName);
  virtual VStream *CreateLumpReaderNum (int) override;
  virtual int CheckNumForName (VName LumpName, EWadNamespace InNS, bool wantFirst=true) override;
  virtual int IterateNS (int, EWadNamespace, bool allowEmptyName8=false) override;
};


//==========================================================================
//  VZipFile
//==========================================================================
class VZipFile : public VPakFileBase {
private:
  vuint32 BytesBeforeZipFile; // byte before the zipfile, (>0 for sfx)

  // you can pass central dir offset here
  void OpenArchive (VStream *fstream, vuint32 cdofs=0);

public:
  VZipFile (VStr zipfile); // only disk files
  VZipFile (VStream *fstream); // takes ownership
  // you can pass central dir offset here
  VZipFile (VStream *fstream, VStr name, vuint32 cdofs=0); // takes ownership

  virtual VStream *CreateLumpReaderNum (int) override;

public: // fuck shitpp friend idiocity
  // returns 0 if not found
  static vuint32 SearchCentralDir (VStream *strm);
};


//==========================================================================
//  VQuakePakFile
//==========================================================================
class VQuakePakFile : public VPakFileBase {
private:
  void OpenArchive (VStream *fstream, int signtype=0);

public:
  VQuakePakFile (VStr);
  VQuakePakFile (VStream *fstream); // takes ownership
  VQuakePakFile (VStream *fstream, VStr name, int signtype=0); // takes ownership

  virtual VStream *CreateLumpReaderNum (int) override;
};


//==========================================================================
//  VDFWadFile
//==========================================================================
class VDFWadFile : public VPakFileBase {
private:
  void OpenArchive (VStream *fstream);

public:
  VDFWadFile (VStr);
  VDFWadFile (VStream *fstream); // takes ownership
  VDFWadFile (VStream *fstream, VStr name); // takes ownership

  virtual VStream *CreateLumpReaderNum (int) override;
};


//==========================================================================
//  VDirPakFile
//==========================================================================
class VDirPakFile : public VPakFileBase {
private:
  // relative to PakFileName
  void ScanDirectory (VStr relpath, int depth);

  void ScanAllDirs ();

public:
  VDirPakFile (VStr);

  //inline bool hasFiles () const { return (files.length() > 0); }

  virtual VStream *OpenFileRead (VStr)  override;
  virtual VStream *CreateLumpReaderNum (int) override;
  //virtual int LumpLength (int) override;
};


#endif
