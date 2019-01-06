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

#define MAX_ILINE_LENGTH  (1024)

// input text line widget
class TILine {
public:
  char Data[MAX_ILINE_LENGTH+1]; // line of text
  int len; // current line length
  int maxlen;

public:
  TILine () { Data[0] = 0; len = 0; maxlen = MAX_ILINE_LENGTH; }
  TILine (int amaxlen) { Data[0] = 0; len = 0; if (amaxlen < 1 || amaxlen > MAX_ILINE_LENGTH) amaxlen = MAX_ILINE_LENGTH; maxlen = amaxlen; }

  void Init ();
  void AddChar (char ch);
  void DelChar ();
  void DelWord ();
  bool Key (const event_t &ev); // whether eaten
};
