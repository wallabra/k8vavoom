/**************************************************************************
 *
 * Coded by Ketmar Dark, 2018
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 **************************************************************************/
#ifndef VCCMOD_FILERIDER_HEADER_FILE
#define VCCMOD_FILERIDER_HEADER_FILE

#include "../vcc_run.h"


// ////////////////////////////////////////////////////////////////////////// //
class VFileReader : public VObject {
  DECLARE_CLASS(VFileReader, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VFileReader)

public:
  enum {
    SeekStart,
    SeekCur,
    SeekEnd,
  };

private:
  VStream *fstream;

public:
  virtual void Destroy () override;

public:
  DECLARE_FUNCTION(Destroy)

  DECLARE_FUNCTION(Open)
  DECLARE_FUNCTION(close)
  DECLARE_FUNCTION(seek)
  DECLARE_FUNCTION(getch)
  DECLARE_FUNCTION(readBuf)

  DECLARE_FUNCTION(get_fileName)
  DECLARE_FUNCTION(get_isOpen)
  DECLARE_FUNCTION(get_error)
  DECLARE_FUNCTION(get_size)
  DECLARE_FUNCTION(get_position)
  DECLARE_FUNCTION(set_position)

  // convenient functions
  DECLARE_FUNCTION(readU8)
  DECLARE_FUNCTION(readU16)
  DECLARE_FUNCTION(readU32)
  DECLARE_FUNCTION(readI8)
  DECLARE_FUNCTION(readI16)
  DECLARE_FUNCTION(readI32)

  DECLARE_FUNCTION(readU16BE)
  DECLARE_FUNCTION(readU32BE)
  DECLARE_FUNCTION(readI16BE)
  DECLARE_FUNCTION(readI32BE)
};


#endif
