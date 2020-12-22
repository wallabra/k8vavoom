//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2019-2020 Ketmar Dark
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
#include "sempai.h"


//==========================================================================
//
//  SemLocation::toStringNoCol
//
//==========================================================================
VStr SemLocation::toStringNoCol () const {
  if (!file.isEmpty()) return va("%s:%d", *file, line);
  return va("%d", line);
}


//==========================================================================
//
//  SemLocation::toString
//
//==========================================================================
VStr SemLocation::toString () const {
  if (file.isEmpty()) return va("%d:%d", line, col);
  return va("%s:%d:%d", *file, line, col);
}


//==========================================================================
//
//  SemParser::SemParser
//
//  this will destroy a stream after reading
//
//==========================================================================
SemParser::SemParser (VStream *src)
  : text()
  , currpos(0)
  , token()
  , tokpos(0)
  , srcfile(src->GetName())
{
  src->Seek(0);
  text.setLength(src->TotalSize());
  src->Serialise(text.getMutableCStr(), text.length());
  delete src;
}


//==========================================================================
//
//  SemParser::SemParser
//
//==========================================================================
SemParser::SemParser (const SemParser &src)
  : text()
  , currpos(0)
  , token()
  , tokpos(0)
  , srcfile()
{
  *this = src;
}


//==========================================================================
//
//  SemParser::~SemParser
//
//==========================================================================
SemParser::~SemParser () {
}


//==========================================================================
//
//  SemParser::operator =
//
//==========================================================================
SemParser &SemParser::operator = (const SemParser &src) {
  if (&src != this) {
    text = src.text;
    currpos = src.currpos;
    token = src.token;
    tokpos = src.tokpos;
    srcfile = src.srcfile;
  }
  return *this;
}


//==========================================================================
//
//  SemParser::getLineForPos
//
//==========================================================================
int SemParser::getLineForPos (int pos) const {
  SemLocation loc;
  calcLocation(loc, pos);
  return loc.line;
}


//==========================================================================
//
//  SemParser::skipBlanks
//
//  ...and comments
//
//==========================================================================
void SemParser::skipBlanks () {
  for (;;) {
    char ch = peekChar();
    if (!ch) break;
    if ((vuint8)ch <= ' ') { skipChar(); continue; }
    // line join?
    if (ch == '\\') {
      int ofs = 1;
      for (;;) {
        ch = peekChar(ofs);
        if (!ch) return;
        if (ch == '\n') {
          // yeah, join
          skipChar(); // slash
          break;
        }
        if ((vuint8)ch > ' ') return; // oops
      }
      continue;
    }
    // comment?
    if (ch != '/') break;
    ch = peekChar(1);
    // one-line comment?
    if (ch == '/') {
      for (;;) {
        ch = getChar();
        if (!ch || ch == '\n') break;
      }
      continue;
    }
    // multiline?
    if (ch == '*') {
      skipChar();
      skipChar();
      for (;;) {
        ch = getChar();
        if (!ch) break;
        if (ch != '*') continue;
        ch = getChar();
        if (ch == '/') break;
      }
      continue;
    }
    // nested?
    if (ch == '+') {
      skipChar();
      skipChar();
      int level = 1;
      for (;;) {
        ch = getChar();
        if (!ch) break;
        if (ch == '+' && peekChar() == '/') {
          skipChar();
          if (--level == 0) break;
        } else if (ch == '/' && peekChar() == '+') {
          skipChar();
          ++level;
        }
      }
      continue;
    }
    // not a comment, not a blank
    break;
  }
}


//==========================================================================
//
//  SemParser::collectBasedNumber
//
//==========================================================================
void SemParser::collectBasedNumber (int base) {
  for (;;) {
    char ch = peekChar();
    if (!ch) return;
    if (ch != '_' && VStr::digitInBase(ch, base) < 0) break;
    token += getChar();
  }
}


//==========================================================================
//
//  SemParser::skipToken
//
//==========================================================================
void SemParser::skipToken () {
  skipBlanks();

  token.clear();
  tokpos = currpos;
  char ch = getChar();
  if (!ch) return;
  token += ch;

  // quoted?
  if (ch == '\'' || ch == '"') {
    char ech = ch;
    for (;;) {
      ch = getChar();
      if (!ch) break;
      token += ch;
      if (ch == ech) break;
      if (ch == '\\') {
        ch = getChar();
        if (!ch) break;
        token += ch;
      }
    }
    return;
  }

  // number?
  // actually, numbers can start with signs, but i don't care here
  if (VStr::digitInBase(ch, 10) >= 0) {
    int base = 10;
    // hex number?
    if (ch == '0' && peekChar() == 'x') {
      token += getChar(); // eat 'x'
      base = 16;
    } else if (ch == '0' && peekChar() == 'b') {
      token += getChar(); // eat 'b'
      base = 2;
    }
    // integral part
    collectBasedNumber(base);
    ch = peekChar();
    // fractional part?
    if (ch == '.') {
      token += getChar(); // eat the dot
      collectBasedNumber(base);
      ch = peekChar();
    }
    // exponent?
    if ((base == 10 && (ch == 'e' || ch == 'E')) || (base == 16 && (ch == 'p' || ch == 'P'))) {
      token += getChar(); // so eat the 'e'/'p'
      ch = peekChar();
      if (ch == '+' || ch == '-') token += getChar();
      collectBasedNumber(base);
    }
    return;
  }

  // identifier?
  if (ch == '_' || VStr::isAlphaAscii(ch)) {
    for (;;) {
      ch = peekChar();
      if (!ch) return;
      if (ch != '_' && !VStr::isAlphaAscii(ch) && VStr::digitInBase(ch, 10) < 0) break;
      token += getChar();
    }
    return;
  }

  // here we should parse multichar tokens, but meh...
  if (ch == ':' && peekChar() == ':') {
    token += getChar();
  }
}


//==========================================================================
//
//  SemParser::expectId
//
//==========================================================================
VStr SemParser::expectId () {
  int savepos = currpos;
  skipBlanks();
  char ch = peekChar();
  if (ch == '_' || VStr::isAlphaAscii(ch)) {
    skipToken();
    return token;
  }
  currpos = savepos;
  Sys_Error("%s:%d: identifier expected", *srcfile, getCurrLine());
}


//==========================================================================
//
//  SemParser::expect
//
//==========================================================================
void SemParser::expect (const VStr &s) {
  auto pos = savePos();
  skipToken();
  if (token.strEqu(s)) return;
  VStr tk = token;
  restorePos(pos);
  Sys_Error("%s:%d: `%s` expected, got `%s`", *srcfile, getCurrLine(), *s, *tk);
}


//==========================================================================
//
//  SemParser::eat
//
//==========================================================================
bool SemParser::eat (const VStr &s) {
  auto pos = savePos();
  skipToken();
  if (token.strEqu(s)) return true;
  restorePos(pos);
  return false;
}


//==========================================================================
//
//  SemParser::eatStartsWith
//
//==========================================================================
bool SemParser::eatStartsWith (const VStr &s) {
  auto pos = savePos();
  skipToken();
  if (token.startsWith(s)) return true;
  restorePos(pos);
  return false;
}


//==========================================================================
//
//  SemParser::check
//
//  same as eat, but doesn't eat
//
//==========================================================================
bool SemParser::check (const VStr &s) {
  auto pos = savePos();
  skipToken();
  bool res = token.strEqu(s);
  restorePos(pos);
  return res;
}


//==========================================================================
//
//  SemParser::expectInt
//
//==========================================================================
int SemParser::expectInt () {
  auto pos = savePos();
  skipToken();
  int res = 0;
  if (!VStr::convertInt(*token, &res)) {
    if (token == "MAX_ACS_MAP_VARS") return 666;
    if (token == "MAX_LOCAL_VARS") return 666;
    if (token == "NUM_BLOCK_SURFS") return 666;
    if (token == "BLOCK_WIDTH") return 666;
    if (token == "BLOCK_HEIGHT") return 666;
    if (token == "MAXPLAYERS") return 8;
    if (token == "TID_HASH_SIZE") return 128; //FIXME!
    if (token == "NUMPSPRITES") return 3; //FIXME!

    if (token == "BODYQUESIZE") return 32; //FIXME!
    if (token == "CORPSEQUEUESIZE") return 64; //FIXME!
    if (token == "NUM_NOTIFY_LINES") return 8; //FIXME!
    if (token == "NUM_CHAT_LINES") return 15; //FIXME!
    if (token == "CastCount") return 17; //FIXME!
    if (token == "MaxWeaponSlots") return 10; //FIXME!
    if (token == "MaxWeaponsInSlot") return 8; //FIXME!
    if (token == "NUM_END_MESSAGES") return 15; //FIXME!

    if (token == "ST_NUMFACES") return 666; //FIXME!

    if (token == "NUM_SPECIALS") return 666;
    if (token == "NUMTOTALBOTS") return 666;

    if (token == "ISAAC_RAND_SIZE") return 256;
    if (token == "MaxDepthMaskStack") return 16;

    if (token == "MaxStackSlots") return 1024;

    if (token == "MaxShadowCubes") return 4;

    if (token == "VMethod") {
      skipToken();
      if (token == "::") {
        skipToken();
        if (token == "MAX_PARAMS") return 32;
      }
    }

    if (token == "DEFAULT_BODYQUESIZE") return 4096;
    if (token == "DEFAULT_CORPSEQUEUESIZE") return 4096;

    restorePos(pos);
    Sys_Error("%s:%d: integer expected", *srcfile, getCurrLine());
  }
  return res;
}


//==========================================================================
//
//  SemParser::calcLocation
//
//==========================================================================
void SemParser::calcLocation (SemLocation &loc, int pos) const {
  loc.line = 1;
  loc.col = 1;
  if (pos <= 0) return;
  const int len = text.length();
  const char *s = *text;
  for (int cpos = 0; cpos < len; ++cpos, ++s) {
    if (cpos == pos) break;
    if (*s == '\n') {
      ++loc.line;
      loc.col = 1;
    } else {
      ++loc.col;
    }
  }
}


//==========================================================================
//
//  SemParser::getLoc
//
//==========================================================================
SemLocation SemParser::getLoc () const {
  SemLocation loc;
  loc.file = srcfile;
  calcLocation(loc, currpos);
  return loc;
}


//==========================================================================
//
//  SemParser::getTokenLoc
//
//==========================================================================
SemLocation SemParser::getTokenLoc () const {
  SemLocation loc;
  loc.file = srcfile;
  calcLocation(loc, tokpos);
  return loc;
}


//==========================================================================
//
//  SemParser::getSavedLoc
//
//==========================================================================
SemLocation SemParser::getSavedLoc (const SavedPos &pos) const {
  SemLocation loc;
  loc.file = srcfile;
  calcLocation(loc, pos.currpos);
  return loc;
}



//==========================================================================
//
//  SemParser::getSavedTokenLoc
//
//==========================================================================
SemLocation SemParser::getSavedTokenLoc (const SavedPos &pos) const {
  SemLocation loc;
  loc.file = srcfile;
  calcLocation(loc, pos.tokpos);
  return loc;
}
