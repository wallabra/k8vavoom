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

//**************************************************************************
//**
//**  MESSAGE IO FUNCTIONS
//**
//**    Handles byte ordering and avoids alignment errors
//**
//**************************************************************************

// WARNING! KEEP THIS IN SYNC WITH BITINT FORMAT IN `Write[U]Int()`/`Read[U]Int()`!
VVA_OKUNUSED static inline constexpr int BitStreamCalcIntBits (vint32 val) noexcept {
  int res = 1; // sign bit
  if (val&0x80000000u) val ^= 0xffffffffu;
  while (val) {
    res += 5; // continute bit, 4 data bits
    val >>= 4;
  }
  return res+1; // and stop bit
}


// WARNING! KEEP THIS IN SYNC WITH BITINT FORMAT IN `Write[U]Int()`/`Read[U]Int()`!
VVA_OKUNUSED static inline constexpr int BitStreamCalcUIntBits (vuint32 val) noexcept {
  int res = 0; // no sign bit
  while (val) {
    res += 5; // continute bit, 4 data bits
    val >>= 4;
  }
  return res+1; // and stop bit
}


// ////////////////////////////////////////////////////////////////////////// //
class VBitStreamWriter : public VStream {
  friend class VBitStreamReader;
protected:
  TArray<vuint8> Data;
  vint32 Max;
  vint32 Pos;
  bool bAllowExpand;

protected:
  bool Expand () noexcept;

  inline bool ReadBitInternal () noexcept {
    if (Pos >= Data.length()*8) {
      bError = true;
      return false;
    }
    bool Ret = !!(Data.ptr()[Pos>>3]&(1<<(Pos&7)));
    ++Pos;
    return Ret;
  }

public:
  VV_DISABLE_COPY(VBitStreamWriter)

  VBitStreamWriter (vint32 AMax, bool allowExpand=false);
  virtual ~VBitStreamWriter () override;

  void cloneFrom (const VBitStreamWriter *wr);

  void Reinit (vint32 AMax, bool allowExpand=false);

  virtual void Serialise (void *Data, int Length) override;
  virtual void SerialiseBits (void *Data, int Length) override;
  virtual void SerialiseInt (vuint32 &Value) override;
  void WriteInt (vint32 Val);
  void WriteUInt (vuint32 Val);
  inline vuint8 *GetData () noexcept { return Data.Ptr(); }
  inline int GetNumBits () const noexcept { return Pos; }
  inline int GetNumBytes () const noexcept { return (Pos+7)>>3; }

  inline void Clear () noexcept { bError = false; Pos = 0; if (Data.length()) memset(Data.ptr(), 0, Data.length()); }

  inline bool IsExpanded () const noexcept { return (Pos > Max); }

  // won't modify `strm`
  // actually, this appends data from `strm`
  void CopyFromWS (const VBitStreamWriter &strm) noexcept;

  inline void WriteBit (bool Bit) noexcept {
    if (bError) return;
    if (Pos+1 > Max) {
      if (!bAllowExpand) { bError = true; return; }
      if ((Pos+7)/8+1 > Data.length()) {
        if (!Expand()) { bError = true; return; }
      }
    }
    if (Bit) Data.ptr()[((unsigned)Pos)>>3] |= 1u<<(Pos&7); else Data.ptr()[((unsigned)Pos)>>3] &= ~(1u<<(Pos&7));
    ++Pos;
  }

  // this forces bit at the given position without adjusting the actual position
  inline void ForceBitAt (int bitpos, bool Bit) noexcept {
    if (bError) return;
    if (bitpos < 0 || bitpos >= Data.length()*8) { bError = true; return; }
    if (Bit) Data.ptr()[((unsigned)bitpos)>>3] |= 1u<<(bitpos&7); else Data.ptr()[((unsigned)bitpos)>>3] &= ~(1u<<(bitpos&7));
  }

  // add trailing bit so we can find out how many bits the message has
  inline void WriteTrailingBit () noexcept {
    WriteBit(true);
    // pad it with zero bits until the byte boundary
    while (Pos&7) WriteBit(false);
  }

  static inline int CalcIntBits (vint32 val) noexcept { return BitStreamCalcIntBits(val); }
  static inline int CalcUIntBits (vuint32 val) noexcept { return BitStreamCalcUIntBits(val); }

  inline int GetPos () const noexcept { return Pos; }
  inline int GetNum () const noexcept { return Max; }
};


// ////////////////////////////////////////////////////////////////////////// //
class VBitStreamReader : public VStream {
  friend class VBitStreamWriter;
protected:
  TArray<vuint8> Data;
  vint32 Num;
  vint32 Pos;

public:
  VV_DISABLE_COPY(VBitStreamReader)

  // `Length` is in bits, not in bytes!
  VBitStreamReader (vuint8 *Src=nullptr, vint32 Length=0);
  virtual ~VBitStreamReader () override;

  void cloneFrom (const VBitStreamReader *rd);

  // this reads bits from `Src`
  void SetData (VBitStreamReader &Src, int Length) noexcept;

  virtual void Serialise (void *Data, int Length) override;
  virtual void SerialiseBits (void *Data, int Length) override;
  virtual void SerialiseInt (vuint32 &Value/*, vuint32 Max*/) override;
  vint32 ReadInt ();
  vuint32 ReadUInt ();
  virtual bool AtEnd () override;
  inline vuint8 *GetData () noexcept { return Data.Ptr(); }
  inline int GetNumBits () const noexcept { return Num; }
  inline int GetNumBytes () const noexcept { return (Num+7)>>3; }
  inline int GetPosBits () const noexcept { return Pos; }

  inline void Clear (bool freeData=false) noexcept { bError = false; Num = 0; Pos = 0; if (freeData) Data.clear(); else Data.reset(); }

  // if `FixWithTrailingBit` is true, shrink with the last trailing bit (including it)
  void SetupFrom (const vuint8 *data, vint32 len, bool FixWithTrailingBit=false) noexcept;

  // actually, this appends data from `buf`
  void CopyFromBuffer (const vuint8 *buf, int bitLength) noexcept;

  inline bool ReadBit () noexcept {
    if (Pos+1 > Num) {
      bError = true;
      return false;
    }
    const bool Ret = !!(Data.ptr()[((unsigned)Pos)>>3]&(1u<<(Pos&7)));
    ++Pos;
    return Ret;
  }

  // this returns bit at the given position without adjusting the actual position
  inline bool GetBitAt (int bitpos) const noexcept {
    if (bError) return false;
    if (bitpos < 0 || bitpos >= Num) return false; // ignore invalid offsets, why not
    return !!(Data.ptr()[((unsigned)bitpos)>>3]&(1u<<(bitpos&7)));
  }

  static inline int CalcIntBits (vint32 val) noexcept { return BitStreamCalcIntBits(val); }
  static inline int CalcUIntBits (vuint32 val) noexcept { return BitStreamCalcUIntBits(val); }

  inline int GetPos () const noexcept { return Pos; }
  inline int GetNum () const noexcept { return Num; }
};
