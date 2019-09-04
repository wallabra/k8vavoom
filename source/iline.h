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
//**
//**  input line library
//**
//**************************************************************************

// input text line widget
class TILine {
protected:
  char *data; // line of text (zero-terminated)
  int len; // current line length
  int maxlen;
  int curpos;
  int vischars; // number of on-screen visible chars
  int visfirst; // first visible char in current string
  // temp buffer for renderer
  char *temp;
  int tempsize;

protected:
  void setup ();

  void ensureCursorVisible ();

public:
  TILine () : data(nullptr), len(0), maxlen(0), curpos(0), vischars(80), visfirst(0), temp(nullptr), tempsize(0) { setup(); }
  TILine (int amaxlen) : data(nullptr), len(0), maxlen(amaxlen), curpos(0), vischars(80), visfirst(0), temp(nullptr), tempsize(0) { setup(); }
  ~TILine ();

  inline int length () const { return len; }
  inline int maxLength () const { return maxlen; }
  inline const char *getCStr () const { return data; }
  inline const char *operator * () const { return data; }

  inline int getCurPos () const { return clampval(curpos, 0, len); }
  inline void setCurPos (int cpos) { curpos = cpos; ensureCursorVisible(); }

  void SetVisChars (int vc);

  void Init ();
  void AddString (VStr s);
  void AddString (const char *s);
  void AddChar (char ch);
  void DelChar (); // this does "backspace"
  void RemoveChar (); // this removes char at the current cursor position, and doesn't move cursor
  void DelWord ();
  bool Key (const event_t &ev); // whether eaten

  // font and align should be already set
  void DrawAt (int x0, int y0, int clrNormal=CR_ORANGE, int clrLR=CR_FIRE);
};
