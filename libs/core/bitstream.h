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
//**
//**  MESSAGE IO FUNCTIONS
//**
//**    Handles byte ordering and avoids alignment errors
//**
//**************************************************************************

class VBitStreamWriter : public VStream {
protected:
  TArray<vuint8> Data;
  vint32 Max;
  vint32 Pos;

public:
  VBitStreamWriter (vint32);
  virtual void Serialise (void*, int) override;
  virtual void SerialiseBits (void*, int) override;
  virtual void SerialiseInt (vuint32&, vuint32) override;
  void WriteBit (bool);
  void WriteInt (vuint32, vuint32);
  inline vuint8 *GetData () { return Data.Ptr(); }
  inline int GetNumBits () const { return Pos; }
  inline int GetNumBytes () const { return (Pos+7)>>3; }
};


class VBitStreamReader : public VStream {
protected:
  TArray<vuint8> Data;
  vint32 Num;
  vint32 Pos;

public:
  VBitStreamReader (vuint8* = nullptr, vint32 = 0);
  void SetData (VBitStreamReader&, int);
  virtual void Serialise (void*, int) override;
  virtual void SerialiseBits (void*, int) override;
  virtual void SerialiseInt (vuint32&, vuint32) override;
  bool ReadBit ();
  vuint32 ReadInt (vuint32);
  virtual bool AtEnd () override;
  inline vuint8 *GetData () { return Data.Ptr(); }
  inline int GetNumBits () const { return Num; }
  inline int GetNumBytes () const { return (Num+7)>>3; }
  inline int GetPosBits () const { return Pos; }
};
