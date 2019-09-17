//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2019 Ketmar Dark
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "libs/core.h"


// ////////////////////////////////////////////////////////////////////////// //
// this is a very simplistic text parser
// its only purpose is to parse shitpp/vc code, and extract tokens
class SemParser {
public: // let's go wild, why not?
  VStr text;
  int currpos;
  VStr token;
  int tokpos;
  VStr srcfile;

public:
  struct SavedPos {
    int currpos;
    int tokpos;
    VStr token;

    SavedPos () : currpos(-1), tokpos(-1), token() {}
    SavedPos (const class SemParser &par) : currpos(par.currpos), tokpos(par.tokpos), token(par.token) {}

    inline void restore (class SemParser &par) const { par.currpos = currpos; par.tokpos = tokpos; par.token = token; }
  };

  inline SavedPos savePos () const { return SavedPos(*this); }
  inline void restorePos (const SavedPos &pos) { pos.restore(*this); }

protected:
  // to `token`
  void collectBasedNumber (int base);

public:
  // this will destroy a stream after reading
  SemParser (VStream *src);
  SemParser (const SemParser &src); // this can create copies
  ~SemParser ();

  SemParser &operator = (const SemParser &src);

  inline bool isEOF () const { return (currpos >= text.length()); }
  inline int getCurrLine () const { return getLineForPos(currpos); }
  inline int getTokenLine () const { return getLineForPos(tokpos); }

  int getLineForPos (int pos) const;

  // 0 means "current" (at currpos)
  inline char peekChar (int offs=0) const {
    //fuck off overflows, i don't care here
    offs += currpos;
    if (offs < 0 || offs >= text.length()) return 0;
    return (text[offs] ? text[offs] : ' ');
  }

  inline char getChar () {
    if (currpos >= text.length()) return 0;
    char ch = text[currpos++];
    if (!ch) ch = ' ';
    return ch;
  }

  inline void skipChar () { (void)getChar(); }

  inline void backoff () { if (currpos > 0) --currpos; }

  void skipBlanks (); // and comments
  void skipToken ();

  VStr expectId ();
  void expect (const VStr &s);
  int expectInt ();
  bool eat (const VStr &s);
};
