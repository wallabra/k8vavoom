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
//**************************************************************************

class VXmlAttribute {
public:
  VStr Name;
  VStr Value;
};


class VXmlNode {
public:
  VStr Name;
  VStr Value;
  VXmlNode *Parent;
  VXmlNode *FirstChild;
  VXmlNode *LastChild;
  VXmlNode *PrevSibling;
  VXmlNode *NextSibling;
  TArray<VXmlAttribute> Attributes;

public:
  VXmlNode ();
  ~VXmlNode ();

  VXmlNode *FindChild (const char *AName) const;
  VXmlNode *FindChild (const VStr &AName) const;
  VXmlNode *GetChild (const char *AName) const;
  VXmlNode *GetChild (const VStr &AName) const;
  VXmlNode *FindNext (const char *AName) const;
  VXmlNode *FindNext (const VStr &AName) const;
  VXmlNode *FindNext () const; // with the same name as the current one
  bool HasAttribute (const char *AttrName) const;
  bool HasAttribute (const VStr &AttrName) const;
  const VStr &GetAttribute (const char *AttrName, bool Required=true) const;
  const VStr &GetAttribute (const VStr &AttrName, bool Required=true) const;
};


class VXmlDocument {
private:
  enum { UTF8, WIN1251, KOI8 };

private:
  char *Buf;
  int CurPos;
  int EndPos;
  int Encoding;

public:
  VStr Name;
  VXmlNode Root;

public:
  void Parse (VStream &Strm, const VStr &AName);

private:
  vuint32 GetChar (); // with correct encoding

  void SkipWhitespace ();
  bool SkipComment ();
  void Error (const char*);
  VStr ParseName ();
  VStr ParseAttrValue (char);
  bool ParseAttribute (VStr &, VStr &);
  void ParseNode (VXmlNode *);
  VStr HandleReferences (const VStr &);
};
