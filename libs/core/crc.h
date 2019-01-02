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

// ////////////////////////////////////////////////////////////////////////// //
class TCRC16 {
private:
  enum {
    CRC_INIT_VALUE = 0xffffU,
    CRC_XOR_VALUE  = 0x0000U,
  };

private:
  static const vuint32 crc16Table[256];
  vuint16 curr;

public:
  TCRC16 () : curr(CRC_INIT_VALUE) {}
  TCRC16 (const TCRC16 &o) : curr(o.curr) {}
  inline void Init () { curr = CRC_INIT_VALUE; }
  inline void reset () { curr = CRC_INIT_VALUE; }
  inline TCRC16 &operator += (vuint8 v) { curr = (curr<<8)^crc16Table[(curr>>8)^v]; return *this; }
  TCRC16 &put (const void *buf, size_t len) {
    const vuint8 *b = (const vuint8 *)buf;
    while (len--) {
      curr = (curr<<8)^crc16Table[(curr>>8)^(*b++)];
    }
    return *this;
  }
  inline operator vuint16 () const { return curr^CRC_XOR_VALUE; }
};



// ////////////////////////////////////////////////////////////////////////// //
class TCRC32 {
private:
  static const vuint32 crc32Table[16];
  vuint32 curr;

public:
  TCRC32 () : curr(0xffffffffU) {}
  TCRC32 (const TCRC32 &o) : curr(o.curr) {}
  inline void Init () { curr = 0; }
  inline void reset () { curr = 0; }
  inline TCRC32 &operator += (vuint8 b) { curr ^= b; curr = crc32Table[curr&0x0f]^(curr>>4); curr = crc32Table[curr&0x0f]^(curr>>4); return *this; }
  TCRC32 &put (const void *buf, size_t len) {
    const vuint8 *b = (const vuint8 *)buf;
    while (len--) {
      curr ^= *b++;
      curr = crc32Table[curr&0x0f]^(curr>>4);
      curr = crc32Table[curr&0x0f]^(curr>>4);
    }
    return *this;
  }
  inline operator vuint32 () const { return curr^0xffffffffU; }
};
