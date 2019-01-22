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
//**
//**  Streams for loading and saving data.
//**
//**************************************************************************
class VStr;
class VNTValue;
class VStream;


// ////////////////////////////////////////////////////////////////////////// //
// variable-length integer codec

// returns number of bytes required to decode full number, in range [1..5]
int decodeVarIntLength (const vuint8 firstByte);

// returns decoded number; can consume up to 5 bytes
vuint32 decodeVarInt (const void *data);

// returns number of used bytes; can consume up to 5 bytes
int encodeVarInt (void *data, vuint32 n);


// ////////////////////////////////////////////////////////////////////////// //
// VObject knows how to serialize itself, others should inherit from this
class VSerialisable {
public:
  VSerialisable () {}
  virtual ~VSerialisable ();

  virtual void Serialise (VStream &) = 0;

  virtual VName GetClassName () = 0;
};


// ////////////////////////////////////////////////////////////////////////// //
// object i/o is using mappers internally, so let's make it explicit
class VStreamIOMapper {
public:
  VStreamIOMapper () {}
  virtual ~VStreamIOMapper ();

  // interface functions for objects and classes streams
  virtual void io (VName &) = 0;
  virtual void io (VStr &) = 0;
  virtual void io (VObject *&) = 0;
  virtual void io (VMemberBase *&) = 0;
  virtual void io (VSerialisable *&) = 0;

  virtual void SerialiseStructPointer (void *&Ptr, VStruct *Struct) = 0;
};


// ////////////////////////////////////////////////////////////////////////// //
// base class for various streams
class VStream {
protected:
  bool bLoading; // are we loading or saving?
  bool bError; // did we have any errors?

public:
  VStreamIOMapper *Mapper;

public:
  VStream () : bLoading(false), bError(false), Mapper(nullptr) {}
  virtual ~VStream ();

  // status requests
  inline bool IsLoading () const { return bLoading;}
  inline bool IsSaving () const { return !bLoading; }
  inline bool IsError () const { return bError; }

  inline void Serialize (void *buf, int len) { Serialise(buf, len); }
  inline void Serialize (const void *buf, int len) { Serialise(buf, len); }
  void Serialise (const void *buf, int len); // only write

  virtual vint16 GetVersion (); // stream version; usually purely informational

  // stream interface
  virtual const VStr &GetName () const;
  virtual void Serialise (void *Data, int Length);
  virtual void SerialiseBits (void *Data, int Length);
  virtual void SerialiseInt (vuint32 &Value, vuint32 Max);
  virtual void Seek (int);
  virtual int Tell ();
  virtual int TotalSize ();
  virtual bool AtEnd ();
  virtual void Flush ();
  virtual bool Close ();

  // interface functions for objects and classes streams
  virtual void io (VName &);
  virtual void io (VStr &);
  virtual void io (const VStr &);
  virtual void io (VObject *&);
  virtual void io (VMemberBase *&);
  virtual void io (VSerialisable *&);

  virtual void SerialiseStructPointer (void *&Ptr, VStruct *Struct);

  // serialise integers in particular byte order
  void SerialiseLittleEndian (void *, int);
  void SerialiseBigEndian (void *, int);
};

// stream serialisation operators
// it is fuckin' impossible to do template constraints in shit-plus-plus, so fuck it
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, VName &v) { Strm.io(v); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, VStr &v) { Strm.io(v); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, const VStr &v) { Strm.io(v); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, VObject *&v) { Strm.io(v); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, VMemberBase *&v) { Strm.io(v); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, VSerialisable *&v) { Strm.io(v); return Strm; }

static inline __attribute__((unused)) VStream &operator << (VStream &Strm, vint8 &Val) { Strm.Serialise(&Val, 1); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, vuint8 &Val) { Strm.Serialise(&Val, 1); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, vint16 &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, vuint16 &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, vint32 &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, vuint32 &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, float &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, double &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }


// ////////////////////////////////////////////////////////////////////////// //
// stream reader helper
template<class T> T Streamer (VStream &Strm) {
  T Val;
  Strm << Val;
  return Val;
}


// ////////////////////////////////////////////////////////////////////////// //
// class for serialising integer values in a compact way
// uses variable-int encoding
class VStreamCompactIndex {
public:
  vint32 Val;
  friend VStream &operator << (VStream &, VStreamCompactIndex &);
};
#define STRM_INDEX(val)  (*(VStreamCompactIndex *)&(val))

class VStreamCompactIndexU {
public:
  vuint32 Val;
  friend VStream &operator << (VStream &, VStreamCompactIndexU &);
};
#define STRM_INDEX_U(val)  (*(VStreamCompactIndexU *)&(val))
