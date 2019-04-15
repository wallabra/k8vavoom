//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2009, 2010 Brendon Duncan
//**  Copyright (C) 2019 Ketmar Dark
//**  Based on https://github.com/CO2/UDMF-Convert
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
#ifndef UDMF_DATA_H
#define UDMF_DATA_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>


namespace udmf {


class variant;
class block;


// ////////////////////////////////////////////////////////////////////////// //
class variant {
  friend void writeblock (int counter, const char *name, const block &b, std::ofstream &out, int indent);

public:
  enum valtypes {
    BVT_NULL,
    BVT_INTEGER,
    BVT_FLOAT,
    BVT_BOOLEAN,
    BVT_STRING,
    BVT_SUBBLOCK
  };


private:
  int valtype;
  union {
    int intval;
    double floatval;
    bool boolval;
    char *stringval;
    block *subval;
  } val;

public:
  variant () : valtype(BVT_NULL) { memset((void *)&val, 0, sizeof(val)); }
  variant (const variant &src) : valtype(src.valtype), val(src.val) {}
  variant (int newval) : valtype(BVT_INTEGER) { val.intval = newval; }
  variant (double newval) : valtype(BVT_FLOAT) { val.floatval = newval; }
  variant (bool newval) : valtype(BVT_BOOLEAN) { val.boolval = newval; }

  variant (const char *newval) : valtype(BVT_STRING) {
    if (!newval) newval = "";
    val.stringval = new char[strlen(newval)+1];
    strcpy(val.stringval, newval);
  }

  variant (const udmf::block &newval);

  ~variant () {
    nullify();
  }

  variant &operator = (const variant &other);
  bool operator > (const variant &other) const;
  bool operator < (const variant &other) const;
  bool operator >= (const variant &other) const;
  bool operator <= (const variant &other) const;
  bool operator == (const variant &other) const;
  bool operator != (const variant &other) const;

  void nullify ();
  void setint (int newval);
  void setfloat (double newval);
  void setbool (bool newval);
  void setstr (const char *newval);
  void setstr (const char *newval, int len);
  void setsub (const block &newval);
};


// ////////////////////////////////////////////////////////////////////////// //
// k8: i don't like stl
class block {
  friend void writeblock (int counter, const char *name, const block &b, std::ofstream &out, int indent);

private:
  struct Node {
    char *name;
    variant v;
    Node *next;

    Node () : name(nullptr), v(), next(nullptr) {}
    Node (const char *aname) : name(nullptr), v(), next(nullptr) {
      if (!aname) aname = "";
      name = new char[strlen(aname)+1];
      strcpy(name, aname);
    }
    Node (const Node &src) : name(nullptr), v(), next(nullptr) {
      name = new char[strlen(src.name)+1];
      strcpy(name, src.name);
      v = src.v;
    }
    ~Node () { delete [] name; v.nullify(); name = nullptr; next = nullptr; }
    Node &operator = (const Node &src) {
      if (this != &src) {
        delete [] name;
        name = new char[strlen(src.name)+1];
        strcpy(name, src.name);
        v = src.v;
      }
      return *this;
    }
  };

private:
  Node *nodes;

private:
  Node *addNode (const char *name);
  const Node *findNode (const char *name) const;

  void copyFrom (const block &src);

public:
  block () : nodes(nullptr) {}
  block (const block &src) : nodes(nullptr) { copyFrom(src); }
  block &operator = (const block &src) { if (this != &src) { clear(); copyFrom(src); } return *this; }

  ~block () { clear (); }

  void clear ();

  inline void put (const char *name, int value) { Node *n = addNode(name); n->v.setint(value); }
  inline void put (const char *name, double value) { Node *n = addNode(name); n->v.setfloat(value); }
  inline void put (const char *name, bool value) { Node *n = addNode(name); n->v.setbool(value); }
  inline void put (const char *name, const char *value) { Node *n = addNode(name); n->v.setstr(value); }
  inline void put (const char *name, const char *value, int len) { Node *n = addNode(name); n->v.setstr(value, len); }
  inline void put (const char *name, const variant &value) { Node *n = addNode(name); n->v = value; }

  inline variant get (const char *name, int defval=0) const {
    const Node *n = findNode(name);
    return (n ? n->v : variant());
  }

  void del (const char *name);
};


// ////////////////////////////////////////////////////////////////////////// //
extern void writeblock (int counter, const char *name, const block &b, std::ofstream &out, int indent=0);


}

#endif
