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


//==========================================================================
//
//  TILine::Init
//
//==========================================================================
void TILine::Init () {
  len = 0;
  Data[0] = 0;
}


//==========================================================================
//
//  TILine::AddChar
//
//==========================================================================
void TILine::AddChar (char ch) {
  if (len < MAX_ILINE_LENGTH) {
    Data[len++] = ch;
    Data[len] = 0;
  }
}


//==========================================================================
//
//  TILine::DelChar
//
//==========================================================================
void TILine::DelChar () {
  if (len) Data[--len] = 0;
}


//==========================================================================
//
//  TILine::DelWord
//
//==========================================================================
void TILine::DelWord () {
  if (!len) return;
  if ((vuint8)Data[len-1] <= ' ') {
    while (len > 0 && (vuint8)Data[len-1] <= ' ') --len;
  } else {
    while (len > 0 && (vuint8)Data[len-1] > ' ') --len;
  }
  Data[len] = 0;
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
          GInput->SetClipboardText(VStr(Data));
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
                if (Data[len] != ' ' && Data[len] != '\t') AddChar(' ');
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

    // clear line
    case K_y:
      if (ev.modflags&bCtrl) {
        // clear line
        Init();
        return true;
      }
      break;

    // delete workd
    case K_w:
      if (ev.modflags&bCtrl) {
        DelWord();
        return true;
      }
      break;
  }

  int ch = ev.data1;
  if (ch >= ' ' && ch < 128) {
    ch = GInput->TranslateKey(ch);
    AddChar((char)ch);
  } else if (ch == K_BACKSPACE) {
    if (ev.modflags&bCtrl) {
      DelWord();
    } else {
      DelChar();
    }
  } else if (ch != K_ENTER && ch != K_PADENTER) {
    return false; // did not eat key
  }
  return true; // ate the key
}
