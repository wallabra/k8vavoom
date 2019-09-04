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
#include "gamedefs.h"
#include "cl_local.h"

#define MAX_ILINE_LENGTH  (4096)


//==========================================================================
//
//  TILine::~TILine
//
//==========================================================================
TILine::~TILine () {
  Z_Free(data);
  data = nullptr;
  len = maxlen = 0;
  Z_Free(temp);
  temp = nullptr;
  tempsize = 0;
}


//==========================================================================
//
//  TILine::setup
//
//==========================================================================
void TILine::setup () {
       if (maxlen < 0) maxlen = 80;
  else if (maxlen == 0 || maxlen > MAX_ILINE_LENGTH) maxlen = MAX_ILINE_LENGTH;
  vassert(maxlen > 0);
  data = (char *)Z_Realloc(data, maxlen+16);
  vassert(data);
  data[0] = 0;
  len = 0;
  curpos = 0;
  visfirst = 0;
}


//==========================================================================
//
//  TILine::Init
//
//==========================================================================
void TILine::Init () {
  len = 0;
  curpos = 0;
  visfirst = 0;
  data[0] = 0;
}


//==========================================================================
//
//  TILine::SetVisChars
//
//==========================================================================
void TILine::SetVisChars (int vc) {
  if (vc < 8) vc = 8;
  if (vc == vischars) return;
  vischars = vc;
  ensureCursorVisible();
}


//==========================================================================
//
//  TILine::ensureCursorVisible
//
//==========================================================================
void TILine::ensureCursorVisible () {
  curpos = clampval(curpos, 0, len);
  if (curpos == len) {
    // special case
    visfirst = max2(0, len-(vischars-1));
    return;
  }
  if (curpos < visfirst) {
    // move left
    visfirst = max2(0, curpos-4);
  } else if (curpos-visfirst >= vischars-1) {
    visfirst = curpos-(vischars-4);
    if (visfirst+vischars > len) visfirst = len-(vischars-1);
    if (visfirst < 0) visfirst = 0;
  }
}


//==========================================================================
//
//  TILine::AddChar
//
//==========================================================================
void TILine::AddChar (char ch) {
  if (ch == '\t') ch = ' '; // why not?
  if ((vuint8)ch < ' ' || (vuint8)ch >= 127) return;
  if (len >= maxlen) return;
  if (curpos >= len) {
    data[len++] = ch;
    data[len] = 0;
    curpos = len;
  } else {
    if (curpos < 0) curpos = 0;
    for (int f = len; f > curpos; --f) data[f] = data[f-1];
    ++len;
    data[len] = 0; // just in case
    data[curpos] = ch;
    ++curpos;
  }
  ensureCursorVisible();
}


//==========================================================================
//
//  TILine::AddString
//
//==========================================================================
void TILine::AddString (const char *s) {
  if (!s || !s[0]) return;
  while (*s) AddChar(*s++);
}


//==========================================================================
//
//  TILine::AddString
//
//==========================================================================
void TILine::AddString (VStr s) {
  AddString(*s);
}


//==========================================================================
//
//  TILine::DelChar
//
//==========================================================================
void TILine::DelChar () {
  if (len == 0 || curpos < 1) return;
  --curpos;
  for (int f = curpos; f < len; ++f) data[f] = data[f+1];
  data[--len] = 0;
  ensureCursorVisible();
}


//==========================================================================
//
//  TILine::RemoveChar
//
//  this removes char at the current cursor position
//  and doesn't move cursor
//
//==========================================================================
void TILine::RemoveChar () {
  if (curpos >= len) return;
  for (int f = curpos; f < len; ++f) data[f] = data[f+1];
  data[--len] = 0;
  ensureCursorVisible();
}


//==========================================================================
//
//  TILine::DelWord
//
//==========================================================================
void TILine::DelWord () {
  ensureCursorVisible();
  if (curpos == 0) return;
  if ((vuint8)data[curpos-1] <= ' ') {
    // delete trailing spaces
    while (curpos > 0 && (vuint8)data[curpos-1] <= ' ') DelChar();
  } else {
    // delete text
    while (curpos > 0 && (vuint8)data[curpos-1] > ' ') DelChar();
  }
}


//==========================================================================
//
//  TILine::Key
//
//  Wrapper function for handling general keyed input.
//  Returns true if it ate the key
//
//==========================================================================
bool TILine::Key (const event_t &ev) {
  if (ev.type != ev_keydown) return false;

  switch (ev.data1) {
    // clipboard
    case K_INSERT:
      {
        vuint32 flg = ev.modflags&(bCtrl|bAlt|bShift|bHyper);
        // ctrl+insert: copy to clipboard
        if (flg == bCtrl) {
          GInput->SetClipboardText(VStr(data));
          return true;
        }
        // shift+insert: insert from clipboard
        if (flg == bShift) {
          VStr ntx = GInput->GetClipboardText();
          if (ntx.length()) {
            bool prevWasBlank = false;
            for (int f = 0; f < ntx.length(); ++f) {
              char ch = ntx[f];
              if (ch < 0) continue;
              if (ch == '\n') {
                if (data[len] != ' ' && data[len] != '\t') AddChar(' ');
                AddChar(';');
                prevWasBlank = false;
                continue;
              }
              if (ch <= 32) {
                if (!prevWasBlank) AddChar(' ');
                prevWasBlank = true;
                continue;
              }
              AddChar(ch);
              prevWasBlank = false;
            }
          }
        }
      }
      return true;

    case K_DELETE:
      {
        vuint32 flg = ev.modflags&(bCtrl|bAlt|bShift|bHyper);
        // ctrl+del: delete to the end of the line
        if (flg == bCtrl) {
          if (curpos < len) {
            data[curpos] = 0;
            len = curpos;
          }
          ensureCursorVisible();
          return true;
        }
        // del: delete char
        if (flg == 0) {
          RemoveChar();
          return true;
        }
      }
      break;

    // cursor movement
    case K_LEFTARROW:
      if (curpos > 0) {
        if (ev.isCtrlDown()) {
          // word left
          if ((vuint8)data[curpos-1] <= ' ') {
            // spaces
            while (curpos > 0 && (vuint8)data[curpos-1] <= ' ') --curpos;
          } else {
            // word
            while (curpos > 0 && (vuint8)data[curpos-1] > ' ') --curpos;
          }
        } else {
          --curpos;
        }
        ensureCursorVisible();
      }
      return true;
    case K_RIGHTARROW:
      if (curpos < len) {
        if (ev.isCtrlDown()) {
          // word right
          if ((vuint8)data[curpos] <= ' ') {
            // spaces
            while (curpos < len && (vuint8)data[curpos] <= ' ') ++curpos;
          } else {
            // word
            while (curpos < len && (vuint8)data[curpos] > ' ') ++curpos;
          }
        } else {
          ++curpos;
        }
        ensureCursorVisible();
      }
      return true;
    case K_HOME:
      curpos = 0;
      ensureCursorVisible();
      return true;
    case K_END:
      curpos = len;
      ensureCursorVisible();
      return true;

    // clear line
    case K_y:
      if (ev.isCtrlDown()) {
        // clear line
        Init();
        return true;
      }
      break;

    // to the start of the line
    case K_a:
      if (ev.isCtrlDown()) {
        curpos = 0;
        ensureCursorVisible();
        return true;
      }
      break;

    // to the end of the line
    case K_e:
      if (ev.isCtrlDown()) {
        curpos = len;
        ensureCursorVisible();
        return true;
      }
      break;

    // delete workd
    case K_w:
      if (ev.isCtrlDown()) {
        DelWord();
        return true;
      }
      break;
  }

  if (ev.keycode == K_BACKSPACE) {
    if (ev.isCtrlDown()) DelWord(); else DelChar();
  } else if (ev.keycode == K_ENTER || ev.keycode == K_PADENTER) {
    return true;
  } else {
    int ch = GInput->TranslateKey(ev.keycode);
    if (ch >= ' ' && ch < 128) {
      AddChar((char)ch);
      return true; // ate the key
    }
  }

  return false; // did not eat key
}


//==========================================================================
//
//  TILine::DrawAt
//
//==========================================================================
void TILine::DrawAt (int x0, int y0, int clrNormal, int clrLR) {
  if (!data) return; // just in case
  ensureCursorVisible();
  // ensure that our temporary buffer is ok
  if (tempsize < vischars+8) {
    tempsize = vischars+16;
    temp = (char *)Z_Realloc(temp, tempsize);
  }
  // draw left "arrow"
  if (visfirst > 0) { T_DrawText(x0, y0, "<", CR_FIRE); x0 = T_GetCursorX(); }
  // draw text before cursor
  int cpos = visfirst;
  int tpos = 0;
  while (cpos < curpos) {
    vassert(tpos < tempsize+4);
    temp[tpos++] = data[cpos++];
  }
  if (tpos > 0) {
    temp[tpos] = 0;
    T_DrawText(x0, y0, temp, clrNormal);
  }
  // remember cursor position
  x0 = T_GetCursorX();
  y0 = T_GetCursorY();
  // draw text after cursor
  tpos = 0;
  while (cpos < len && cpos-visfirst < vischars) {
    vassert(tpos < tempsize+4);
    temp[tpos++] = data[cpos++];
  }
  if (tpos > 0) {
    if (cpos < len) --tpos;
    temp[tpos] = 0;
    T_DrawText(x0, y0, temp, clrNormal);
    // draw right "arrow"
    if (cpos < len) T_DrawText(T_GetCursorX(), y0, ">", CR_FIRE);
  }
  // draw cursor
  T_DrawCursorAt(x0, y0);
}
