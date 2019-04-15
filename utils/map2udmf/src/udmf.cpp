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
#include <math.h>
#include <string.h>

#include "udmf.h"
#include "exceptions.h"


#define todbl()  \
  double a, b; \
  \
  switch (valtype) { \
    case BVT_BOOLEAN: a = val.boolval; break; \
    case BVT_FLOAT: a = val.floatval; break; \
    case BVT_INTEGER: a = val.intval; break; \
    default: a = 0; break; \
  } \
  \
  switch (other.valtype) { \
    case BVT_BOOLEAN: b = other.val.boolval; break; \
    case BVT_FLOAT: b = other.val.floatval; break; \
    case BVT_INTEGER: b = other.val.intval; break; \
    default: b = 0; break; \
  }


namespace udmf {


//==========================================================================
//
//  strEquCI
//
//==========================================================================
static inline bool strEquCI (const char *s0, const char *s1) {
  if (!s0) s0 = "";
  if (!s1) s1 = "";
  while (*s0 && *s1) {
    char c0 = *s0++;
    char c1 = *s1++;
     // poor mans tolower
    if (c0 >= 'A' && c0 <= 'Z') c0 += 32;
    if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
    if (c0 != c1) return false;
  }
  return (s0[0] == 0 && s1[0] == 0);
}


//==========================================================================
//
//  block::addNode
//
//==========================================================================
block::Node *block::addNode (const char *name) {
  if (!name || !name[0]) abort();
  Node *last = nullptr;
  for (Node *n = nodes; n; last = n, n = n->next) {
    if (strEquCI(name, n->name)) return n;
  }
  Node *n = new Node(name);
  if (last) last->next = n; else nodes = n;
  return n;
}


//==========================================================================
//
//  block::findNode
//
//==========================================================================
const block::Node *block::findNode (const char *name) const {
  if (!name || !name[0]) return nullptr;
  for (const Node *n = nodes; n; n = n->next) {
    if (strEquCI(name, n->name)) return n;
  }
  return nullptr;
}


//==========================================================================
//
//  block::del
//
//==========================================================================
void block::del (const char *name) {
  if (!name || !name[0]) return;
  Node *prev = nullptr;
  for (Node *n = nodes; n; prev = n, n = n->next) {
    if (strEquCI(name, n->name)) {
      if (prev) prev->next = n->next; else nodes = n->next;
      delete n;
      return;
    }
  }
}


//==========================================================================
//
//  block::clear
//
//==========================================================================
void block::clear () {
  while (nodes) {
    Node *n = nodes;
    nodes = n->next;
    delete n;
  }
}


//==========================================================================
//
//  block::copyFrom
//
//==========================================================================
void block::copyFrom (const block &src) {
  Node *last = nodes;
  if (last) { while (last->next) last = last->next; }
  for (const Node *sn = src.nodes; sn; sn = sn->next) {
    Node *n = new Node(*sn);
    if (last) last->next = n; else nodes = n;
    last = n;
  }
}


//==========================================================================
//
//  variant::variant
//
//==========================================================================
variant::variant (const udmf::block &newval) : valtype(BVT_SUBBLOCK) {
  val.subval = new udmf::block;
  *(val.subval) = newval;
}


//==========================================================================
//
//  variant::operator =
//
//==========================================================================
variant &variant::operator = (const variant &other) {
  if (this == &other) return *this; // oops
  nullify();
  valtype = other.valtype;
  switch (other.valtype) {
    case BVT_STRING:
      val.stringval = new char[strlen(other.val.stringval)+1];
      strcpy(val.stringval, other.val.stringval);
      break;
    case BVT_SUBBLOCK:
      val.subval = new udmf::block;
      *(val.subval) = *(other.val.subval);
      break;
    default:
      this->val = other.val;
      break;
  }
  return *this;
}


//==========================================================================
//
//  variant::operator >
//
//==========================================================================
bool variant::operator > (const variant &other) const {
  todbl();
  return (a > b);
}


//==========================================================================
//
//  variant::operator <
//
//==========================================================================
bool variant::operator < (const variant &other) const {
  todbl();
  return (a < b);
}


//==========================================================================
//
//  variant::operator >=
//
//==========================================================================
bool variant::operator >= (const variant &other) const {
  todbl();
  return (a >= b);
}


//==========================================================================
//
//  variant::operator <=
//
//==========================================================================
bool variant::operator <= (const variant &other) const {
  todbl();
  return (a <= b);
}


//==========================================================================
//
//  variant::operator ==
//
//==========================================================================
bool variant::operator == (const variant &other) const {
  if (valtype == BVT_STRING && other.valtype == BVT_STRING) {
    return (strcmp(val.stringval, other.val.stringval) == 0);
  }
  todbl();
  return (a == b);
}


//==========================================================================
//
//  variant::operator!=
//
//==========================================================================
bool variant::operator != (const variant &other) const {
  if (valtype == BVT_STRING && other.valtype == BVT_STRING) {
    return (strcmp(val.stringval, other.val.stringval) != 0);
  }
  todbl();
  return (a != b);
}


//==========================================================================
//
//  variant::nullify
//
//==========================================================================
void variant::nullify () {
  switch (valtype) {
    case BVT_STRING: delete [] val.stringval; break;
    case BVT_SUBBLOCK: delete val.subval; break;
    default: break;
  }
  memset((void *)&val, 0, sizeof(val));
  valtype = BVT_NULL;
}


//==========================================================================
//
//  variant::setint
//
//==========================================================================
void variant::setint (int newval) {
  nullify();
  valtype = BVT_INTEGER;
  val.intval = newval;
}


//==========================================================================
//
//  variant::setfloat
//
//==========================================================================
void variant::setfloat (double newval) {
  nullify();
  valtype = BVT_FLOAT;
  val.floatval = newval;
}


//==========================================================================
//
//  variant::setbool
//
//==========================================================================
void variant::setbool (bool newval) {
  nullify();
  valtype = BVT_BOOLEAN;
  val.boolval = newval;
}


//==========================================================================
//
//  variant::setstr
//
//==========================================================================
void variant::setstr (const char *newval) {
  if (!newval) newval = "";
  char *nv = new char[strlen(newval)+1];
  strcpy(nv, newval);
  nullify();
  valtype = BVT_STRING;
  val.stringval = nv;
}


//==========================================================================
//
//  variant::setstr
//
//==========================================================================
void variant::setstr (const char *newval, int len) {
  if (len < 0) len = 0;
  if (!newval) newval = "";
  char *nv = new char[len+1];
  memset(nv, 0, len+1);
  if (len) strncpy(nv, newval, len);
  nullify();
  valtype = BVT_STRING;
  val.stringval = nv;
}


//==========================================================================
//
//  variant::setsub
//
//==========================================================================
void variant::setsub (const udmf::block &other) {
  udmf::block *nv = new udmf::block;
  *nv = other;
  nullify();
  valtype = BVT_SUBBLOCK;
  val.subval = nv;
}


//==========================================================================
//
//  writeIndent
//
//==========================================================================
static void writeIndent (std::ofstream &out, int indent) {
  while (indent-- > 0) out << ' ';
}


//==========================================================================
//
//  writeblock
//
//==========================================================================
void writeblock (int counter, const char *name, const block &b, std::ofstream &out, int indent) {
  if (!name) abort();
  if (counter > 0) out << "\n";
  writeIndent(out, indent);
  out << name;
  if (counter >= 0) out << " // " << counter;
  out << "\n";
  writeIndent(out, indent);
  out << "{\n";
  indent += 2;
  for (const block::Node *n = b.nodes; n; n = n->next) {
    if (n->v.valtype == variant::BVT_NULL) continue;
    writeIndent(out, indent);
    out << n->name << " = ";
    switch (n->v.valtype) {
      case variant::BVT_BOOLEAN:
        out << (n->v.val.boolval ? "true" : "false");
        break;
      case variant::BVT_FLOAT:
        if (n->v.val.floatval == floor(n->v.val.floatval)) {
          out << n->v.val.floatval << ".0";
        } else {
          out << n->v.val.floatval;
        }
        break;
      case variant::BVT_INTEGER:
        out << n->v.val.intval;
        break;
      case variant::BVT_STRING:
        out << '\"' << n->v.val.stringval << '\"';
        //Does not escape strings and backslashes like it should
        break;
      case variant::BVT_SUBBLOCK:
        writeblock(-1, n->name, *(n->v.val.subval), out, indent);
        break;
      default:
        throw exception::root("Unknown variant type detected");
    }
    out << ";\n";
  }
  indent -= 2;
  writeIndent(out, indent);
  out << "}\n";
}


}
