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
#ifndef VCCMOD_CONSOLE_HEADER_FILE
#define VCCMOD_CONSOLE_HEADER_FILE

#include "../vcc_run.h"


// ////////////////////////////////////////////////////////////////////////// //
class VConsole : public VObject {
  DECLARE_CLASS(VConsole, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VConsole)

private:
  static char* cbuf;
  static vuint32 cbufHead;
  static vuint32 cbufTail;
  static bool cbufLastWasCR;
  static vuint32 cbufcursize;
  static vuint32 cbufmaxsize;

  static inline vuint32 textsize () {
    return
      cbufTail == cbufHead ? cbufcursize :
      cbufTail > cbufHead ? cbufTail-cbufHead :
      cbufcursize-cbufHead+cbufTail;
  }

  static char* cbufPos (vuint32 pos) {
    return
      pos >= textsize() ? nullptr :
      pos < cbufcursize-cbufHead ? cbuf+cbufHead+pos :
      cbuf+(pos-(cbufcursize-cbufHead));
  }

  static char cbufAt (vuint32 pos) {
    return
      pos >= textsize() ? '\0' :
      *(pos < cbufcursize-cbufHead ? cbuf+cbufHead+pos :
        cbuf+(pos-(cbufcursize-cbufHead)));
  }

  static vuint32 getLastLineStart ();

  static void PutCharInternal (char ch, bool doDump);

public:
  //virtual void Destroy () override;

  static inline void PutChar (char ch) { PutCharInternal(ch, true); }

  static void WriteStr (const VStr &str);
  static void WriteStr (const char *str, size_t strlen);

  static void PutCmd (const VStr &str);

public:
  //DECLARE_FUNCTION(WriteStr)
  //DECLARE_FUNCTION(PutCmd)
};


#endif
