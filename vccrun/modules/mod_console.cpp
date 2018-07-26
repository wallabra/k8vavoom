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
#include "mod_console.h"

// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_CLASS(V, Console);


// ////////////////////////////////////////////////////////////////////////// //
/*
void VTextReader::Destroy () {
  delete fstream; fstream = nullptr;
  Super::Destroy();
}
*/


// ////////////////////////////////////////////////////////////////////////// //
char* VConsole::cbuf = nullptr;
vuint32 VConsole::cbufHead = 0;
vuint32 VConsole::cbufTail = 0; // `cbuftail` points *at* last char
bool VConsole::cbufLastWasCR = false;
vuint32 VConsole::cbufcursize = 0;
vuint32 VConsole::cbufmaxsize = 256*1024;


//==========================================================================
//
//  VConsole::getLastLineStart
//
//==========================================================================
vuint32 VConsole::getLastLineStart () {
  vuint32 sz = textsize();
  while (sz--) {
    if (cbufAt(sz) == '\n') return sz+1;
  }
  return 0;
}


//==========================================================================
//
//  VConsole::PutCharInternal
//
//==========================================================================
void VConsole::PutCharInternal (char ch, bool doDump) {
  if (ch == '\r') return;

  if (cbufcursize != cbufmaxsize) {
    cbuf = (char *)realloc(cbuf, cbufmaxsize);
    if (!cbuf) FatalError("Out of memory for console buffer");
    //FIXME
    cbufHead = 0;
    cbufTail = 1;
    cbuf[0] = '\n';
    cbufcursize = cbufmaxsize;
  }

  if (ch == '\t') {
    if (doDump) putc(ch, stdout);
    auto lslen = (textsize()-getLastLineStart())%8;
    for (; lslen < 8; ++lslen) PutCharInternal(' ', doDump);
    return;
  }

  if (ch != '\n' && (vuint8)ch < 32) ch = ' ';

  // has room?
  if (cbufTail == cbufHead) {
    // oops, no room; remove top line then
    /*
    bool foundNL = false;
    vuint32 cbsz = cbufcursize;
    vuint32 cbh = cbufHead;
    cbh = (cbh+cbsz-1)%cbsz; // one char back, unconditionally
    for (vuint32 cnt = cbsz-1; cnt > 0; --cnt) {
      cbh = (cbh+cbsz-1)%cbsz;
      if (cncbuf[cbh] == '\n') {
        cbufHead = cbh;
        foundNL = true;
        break;
      }
    }
    if (!foundNL) {
      //FIXME: no newline, just clear it all
      cbufHead = 0;
      cbufTail = 1;
      cbuf[0] = ch;
      return;
    }
    */
    cbufHead = (cbufHead+1)%cbufcursize;
  }
  cbuf[cbufTail++] = ch;
  cbufTail %= cbufcursize;

  if (doDump) putc(ch, stdout);
}


//==========================================================================
//
//  VConsole::WriteStr
//
//==========================================================================
void VConsole::WriteStr (const VStr &str) {
  if (!str.isEmpty()) {
    fprintf(stdout, "%.*s", (vuint32)str.length(), *str);
    const char *strd = *str;
    int slen = str.length();
    while (slen--) PutCharInternal(*strd++, false);
  }
}


//==========================================================================
//
//  VConsole::WriteStr
//
//==========================================================================
void VConsole::WriteStr (const char *str, size_t strlen) {
  if (strlen) fprintf(stdout, "%.*s", (vuint32)strlen, str);
  while (strlen--) PutCharInternal(*str++, false);
}


//==========================================================================
//
//  VConsole::PutCmd
//
//==========================================================================
void VConsole::PutCmd (const VStr &str) {
}
