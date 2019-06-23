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
//**  common VM API
//**
//**************************************************************************
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
# include "../gamedefs.h"
#else
# if defined(IN_VCC)
#  include "../../utils/vcc/vcc.h"
# elif defined(VCC_STANDALONE_EXECUTOR)
#  include "../../vccrun/vcc_run_vc.h"
#  include "vc_public.h"
# endif
#endif


// ////////////////////////////////////////////////////////////////////////// //
static inline VFieldType PR_ReadTypeFromPtr (const void *pp) {
  vuint8 *p = (vuint8 *)pp;
  VFieldType t = VFieldType::ReadTypeMem(p);
  return t;
}


//**************************************************************************
//
//  Vararg strings
//
//**************************************************************************

class PFFmtBuf {
public:
  char *buf;
  size_t bufalloted;
  size_t bufused;

private:
  void reserve (size_t add) {
    if (!add) return;
    ++add;
    while (bufalloted-bufused < add) bufalloted = (bufalloted|0x7ff)+1;
    buf = (char *)Z_Realloc(buf, bufalloted);
    if (!buf) { VObject::VMDumpCallStack(); Sys_Error("Out of memory"); }
  }

public:
  PFFmtBuf (size_t resv=0) : buf(nullptr), bufalloted(0), bufused(0) {
    reserve(resv);
  }
  ~PFFmtBuf () { if (buf) Z_Free(buf); }

  const char *getCStr () {
    if (!bufused) return "";
    reserve(1);
    buf[bufused] = 0;
    return buf;
  }

  void putStrInternal (const VStr &s, bool doQuote=false) {
    if (s.isEmpty()) return;
    int qlen = 0;
    if (doQuote) {
      // check if we need to quote string
      doQuote = false;
      qlen = s.length();
      for (int f = 0; f < s.length(); ++f) {
        vuint8 ch = (vuint8)s[f];
        if (ch == '\t' || ch == '\r' || ch == '\n') {
          doQuote = true;
          ++qlen;
        } else if (ch < ' ' || ch == 127) {
          doQuote = true;
          qlen += 3;
        } else if (ch == '"' || ch == '\\') {
          doQuote = true;
          ++qlen;
        }
      }
    }
    if (doQuote) {
      reserve((size_t)qlen);
      vuint8 *d = (vuint8 *)(buf+bufused);
      bufused += (size_t)qlen;
      for (int f = 0; f < s.length(); ++f) {
        vuint8 ch = (vuint8)s[f];
        if (ch == '\t') {
          *d++ = '\\';
          *d++ = 't';
        } else if (ch == '\n') {
          *d++ = '\\';
          *d++ = 'n';
        } else if (ch == '\r') {
          *d++ = '\\';
          *d++ = 'r';
        } else if (ch < ' ' || ch == 127) {
          snprintf((char *)d, 6, "\\x%02x", ch);
          d += 4;
        } else if (ch == '"' || ch == '\\') {
          *d++ = '\\';
          *d++ = ch;
        } else {
          *d++ = ch;
        }
      }
    } else {
      reserve((size_t)s.length());
      memcpy(buf+bufused, *s, (size_t)s.length());
      bufused += (size_t)s.length();
    }
  }

  void putStr (const VStr &s, int width, bool toRight, bool zeroFill, bool doQuote=false) {
    //if (doQuote) putChar('"');
    if (width > s.length() && toRight) {
      while (width-- > s.length()) putChar(' ');
    }
    putStrInternal(s, doQuote);
    if (width > s.length() && !toRight) {
      while (width-- > s.length()) putChar(' ');
    }
    //if (doQuote) putChar('"');
  }

  void putStr (const char *s, int sslen=-1, bool doQuote=false) {
    if (!s || !s[0]) return;
    if (doQuote) {
      VStr ss = VStr(s, sslen);
      putStrInternal(ss, doQuote);
    } else {
      size_t slen = (sslen < 0 ? strlen(s) : (size_t)sslen);
      reserve(slen);
      memcpy(buf+bufused, s, slen);
      bufused += slen;
    }
  }

  void putChar (char ch) {
    reserve(1);
    memcpy(buf+bufused, &ch, 1);
    bufused += 1;
  }

  void putInt (int v, int width, bool toRight, bool zeroFill, bool asdec=true) {
    char tmp[64];
    int len;
    if (asdec) {
      len = (int)snprintf(tmp, sizeof(tmp), "%d", v);
    } else {
      len = (int)snprintf(tmp, sizeof(tmp), "%x", (unsigned)v);
    }
    if (len > 0) {
      if (width > len && toRight) {
        if (zeroFill) {
          if (tmp[0] == '-') {
            putChar('-');
            memmove(tmp, tmp+1, strlen(tmp));
          }
          while (width-- > len) putChar('0');
        } else {
          while (width-- > len) putChar(' ');
        }
      }
      putStr(tmp, len);
      if (width > len && !toRight) {
        while (width-- > len) putChar(' ');
      }
    }
  }

  void putHex (int v, int width, bool toRight, bool zeroFill) {
    putInt(v, width, toRight, zeroFill, false);
  }

  void putFloat (float v) {
    char tmp[VStr::FloatBufSize];
    int len = VStr::float2str(tmp, v);
    putStr(tmp, len);
    /*
    char tmp[512];
    int len = (int)snprintf(tmp, sizeof(tmp), "%f", v);
    if (len > 0) putStr(tmp, len);
    */
  }

  void putPtr (const void *v) {
    char tmp[128];
    int len = (int)snprintf(tmp, sizeof(tmp), "%p", v);
    if (len > 0) putStr(tmp, len);
  }
};


// caller should be tagged with `[printf]` attribute
VStr PF_FormatString () {
  const int MAX_PARAMS = 256;

  //VStr Ret;
  VStack params[MAX_PARAMS];
  VFieldType ptypes[MAX_PARAMS];
  int pi = MAX_PARAMS-1;

  P_GET_INT(count);
  if (count < 0) { VObject::VMDumpCallStack(); Sys_Error("Invalid number of arguments to string formatting function (%d)", count); }
  if (count >= MAX_PARAMS) { VObject::VMDumpCallStack(); Sys_Error("Too many arguments to string formatting function (%d)", count); }
  //fprintf(stderr, "***COUNT=%d\n", count);
  for (int pcnt = 0; pcnt < count; ++pcnt) {
    if (pi < 0) { VObject::VMDumpCallStack(); Sys_Error("Too many arguments to string formatting function (%d)", count); }
    ptypes[pi] = PR_ReadTypeFromPtr((--pr_stackPtr)->p);
    //fprintf(stderr, "  %d: <%s>\n", pcnt, *ptypes[pi].GetName());
    params[pi] = *(--pr_stackPtr);
    --pi;
    if (ptypes[pi+1].Type == TYPE_Vector) {
      if (pi < 2) { VObject::VMDumpCallStack(); Sys_Error("Too many arguments to string formatting function (%d)", count); }
      ptypes[pi] = ptypes[pi+1];
      params[pi] = *(--pr_stackPtr);
      --pi;
      ptypes[pi] = ptypes[pi+1];
      params[pi] = *(--pr_stackPtr);
      --pi;
    } else if (ptypes[pi+1].Type == TYPE_Delegate) {
      if (pi < 1) { VObject::VMDumpCallStack(); Sys_Error("Too many arguments to string formatting function (%d)", count); }
      ptypes[pi] = ptypes[pi+1];
      params[pi] = *(--pr_stackPtr);
      --pi;
    }
  }
  pi += 1;
  //int pistart = pi;
  P_GET_STR(str);
  //fprintf(stderr, "<%s>\n", *str);

  PFFmtBuf pbuf((size_t)str.length());
  int spos = 0;
  while (spos < str.length()) {
    if (str[spos] == '%') {
      auto savedpos = spos;
      ++spos;
      if (spos >= str.length()) { pbuf.putChar('%'); break; }
      if (str[spos] == '%') { pbuf.putChar('%'); ++spos; continue; }
      bool toRight = true;
      bool zeroFill = false;
      int width = -1;
      if (str[spos] == '-') {
        toRight = false;
        if (++spos == str.length()) { pbuf.putStr("%-"); break; }
      }
      if (str[spos] >= '0' && str[spos] <= '9') {
        zeroFill = (str[spos] == '0');
        width = 0;
        while (spos <= str.length() && str[spos] >= '0' && str[spos] <= '9') {
          width = width*10+str[spos++]-'0';
        }
        if (spos >= str.length()) {
          while (savedpos < str.length()) pbuf.putChar(str[savedpos++]);
          break;
        }
      }
      char fspec = str[spos++];
      switch (fspec) {
        case 'i':
        case 'd':
          if (pi >= MAX_PARAMS) { VObject::VMDumpCallStack(); Sys_Error("Out of arguments to string formatting function"); }
          switch (ptypes[pi].Type) {
            case TYPE_Int: case TYPE_Byte: case TYPE_Bool: pbuf.putInt(params[pi].i, width, toRight, zeroFill); break;
            case TYPE_Float: pbuf.putInt((int)params[pi].f, width, toRight, zeroFill); break;
            case TYPE_Name: pbuf.putInt(params[pi].i, width, toRight, zeroFill); break; // hack
            default: VObject::VMDumpCallStack(); Sys_Error("Invalid argument to format specifier '%c'", fspec);
          }
          ++pi;
          break;
        case 'x':
          if (pi >= MAX_PARAMS) { VObject::VMDumpCallStack(); Sys_Error("Out of arguments to string formatting function"); }
          switch (ptypes[pi].Type) {
            case TYPE_Int: case TYPE_Byte: case TYPE_Bool: pbuf.putHex(params[pi].i, width, toRight, zeroFill); break;
            case TYPE_Float: pbuf.putHex((int)params[pi].f, width, toRight, zeroFill); break; //FIXME! HACK!
            case TYPE_Name: pbuf.putHex(params[pi].i, width, toRight, zeroFill); break; // hack
            default: VObject::VMDumpCallStack(); Sys_Error("Invalid argument to format specifier '%c'", fspec);
          }
          ++pi;
          break;
        case 'f':
          if (pi >= MAX_PARAMS) { VObject::VMDumpCallStack(); Sys_Error("Out of arguments to string formatting function"); }
          switch (ptypes[pi].Type) {
            case TYPE_Int: case TYPE_Byte: case TYPE_Bool: pbuf.putFloat(params[pi].i); break;
            case TYPE_Float: pbuf.putFloat(params[pi].f); break;
            default: VObject::VMDumpCallStack(); Sys_Error("Invalid argument to format specifier '%c'", fspec);
          }
          ++pi;
          break;
        case 'n':
          if (pi >= MAX_PARAMS) { VObject::VMDumpCallStack(); Sys_Error("Out of arguments to string formatting function"); }
          switch (ptypes[pi].Type) {
            case TYPE_Name:
              {
                VName n = *(VName *)&params[pi].i;
                if (!n.isValid()) { VObject::VMDumpCallStack(); Sys_Error("garbage name"); }
                if (n == NAME_None) pbuf.putStr(VStr("<none>"), width, toRight, zeroFill); else pbuf.putStr(VStr(*n), width, toRight, zeroFill);
              }
              break;
            case TYPE_String:
              pbuf.putStr(*(VStr *)&params[pi].p, width, toRight, zeroFill);
              ((VStr *)&params[pi].p)->Clean();
              break;
            default: VObject::VMDumpCallStack(); Sys_Error("Invalid argument to format specifier '%c'", fspec);
          }
          ++pi;
          break;
        case 'p':
          if (pi >= MAX_PARAMS) { VObject::VMDumpCallStack(); Sys_Error("Out of arguments to string formatting function"); }
          switch (ptypes[pi].Type) {
            case TYPE_Pointer: pbuf.putPtr(params[pi].p); break;
            default: VObject::VMDumpCallStack(); Sys_Error("Invalid argument to format specifier '%c'", fspec);
          }
          ++pi;
          break;
        case 'v':
          if (MAX_PARAMS-pi < 3) { VObject::VMDumpCallStack(); Sys_Error("Out of arguments to string formatting function"); }
          if (ptypes[pi].Type == TYPE_Vector) {
            pbuf.putChar('(');
            pbuf.putFloat(params[pi].f);
            pbuf.putChar(',');
            pbuf.putFloat(params[pi+1].f);
            pbuf.putChar(',');
            pbuf.putFloat(params[pi+2].f);
            pbuf.putChar(')');
          } else {
            VObject::VMDumpCallStack(); Sys_Error("Invalid argument to format specifier '%c'", fspec);
          }
          pi += 3;
          break;
        case 'B': // boolean
          if (pi >= MAX_PARAMS) { VObject::VMDumpCallStack(); Sys_Error("Out of arguments to string formatting function"); }
          switch (ptypes[pi].Type) {
            case TYPE_Int: case TYPE_Byte: case TYPE_Bool: pbuf.putStr(VStr(params[pi].i ? "true" : "false"), width, toRight, zeroFill); break;
            case TYPE_Float: pbuf.putStr(VStr(params[pi].f ? "true" : "false"), width, toRight, zeroFill); break;
            //TODO
            default: VObject::VMDumpCallStack(); Sys_Error("Invalid argument to format specifier '%c'", fspec);
          }
          ++pi;
          break;
        case 'C': // class name
          if (pi >= MAX_PARAMS) { VObject::VMDumpCallStack(); Sys_Error("Out of arguments to string formatting function"); }
          switch (ptypes[pi].Type) {
            case TYPE_Class: pbuf.putStr(VStr(params[pi].p ? ((VClass *)params[pi].p)->GetName() : "<none>"), width, toRight, zeroFill); break;
            case TYPE_Reference: pbuf.putStr(VStr(params[pi].p ? ((VObject *)params[pi].p)->GetClass()->GetName() : "<none>"), width, toRight, zeroFill); break;
            default: VObject::VMDumpCallStack(); Sys_Error("Invalid argument to format specifier '%c'", fspec);
          }
          ++pi;
          break;
        case 's': // this can convert most of the types to string
        case 'q': // this can convert most of the types to string
          if (pi >= MAX_PARAMS) { VObject::VMDumpCallStack(); Sys_Error("Out of arguments to string formatting function"); }
          switch (ptypes[pi].Type) {
            case TYPE_Void: pbuf.putStr(VStr("<void>"), width, toRight, zeroFill); break;
            case TYPE_Int: case TYPE_Byte: case TYPE_Bool: pbuf.putInt(params[pi].i, width, toRight, zeroFill); break;
            case TYPE_Float: pbuf.putFloat(params[pi].f); break;
            case TYPE_Name:
              {
                VName n = *(VName*)&params[pi].i;
                if (n == NAME_None) pbuf.putStr(VStr("<none>"), width, toRight, zeroFill); else pbuf.putStr(VStr(*n), width, toRight, zeroFill, (fspec == 'q'));
              }
              break;
            case TYPE_String:
              pbuf.putStr(*(VStr*)&params[pi].p, width, toRight, zeroFill, (fspec == 'q'));
              ((VStr*)&params[pi].p)->Clean();
              break;
            case TYPE_Pointer: pbuf.putPtr(params[pi].p); break;
            case TYPE_Reference:
              if (params[pi].p) {
                if (ptypes[pi].Class) {
                  pbuf.putStr(va("(%s)", *ptypes[pi].Class->Name), width, toRight, zeroFill, (fspec == 'q'));
                } else {
                  pbuf.putStr(va("(none:%s)", *ptypes[pi].GetName()), width, toRight, zeroFill, (fspec == 'q'));
                }
                /*
                VStr ss = "(";
                ss += (ptypes[pi].Class ? *ptypes[pi].Class->Name : "none");
                ss += ")";
                pbuf.putStr(ss, width, toRight, zeroFill, (fspec == 'q'));
                */
                /*
                pbuf.putChar(':');
                pbuf.putPtr(params[pi].p);
                */
              } else {
                //pbuf.putStr(VStr("none"), width, toRight, zeroFill);
                pbuf.putStr(va("(none:%s)", *ptypes[pi].GetName()), width, toRight, zeroFill, (fspec == 'q'));
              }
              break;
            case TYPE_Class:
              if (params[pi].p) {
                pbuf.putStr(((VMemberBase *)params[pi].p)->GetFullName(), width, toRight, zeroFill, (fspec == 'q'));
              } else {
                pbuf.putStr(va("(none:%s)", *ptypes[pi].GetName()), width, toRight, zeroFill);
              }
              break;
            case TYPE_State:
              if (params[pi].p) {
                if (fspec == 'q') {
                  pbuf.putStr(((VMemberBase *)params[pi].p)->GetFullName(), width, toRight, zeroFill, (fspec == 'q'));
                } else {
                  pbuf.putStr(((VMemberBase *)params[pi].p)->GetFullName()+"["+((VMemberBase *)params[pi].p)->Loc.toStringNoCol()+"]", width, toRight, zeroFill, (fspec == 'q'));
                }
              } else {
                pbuf.putStr(VStr("(none)"), width, toRight, zeroFill);
              }
              break;
            case TYPE_Delegate:
              // [pi+0]: self
              // [pi+1]: method
              if (params[pi+1].p) {
                if (!params[pi].p) {
                  pbuf.putStr("(invalid delegate)", width, toRight, zeroFill);
                } else {
                  pbuf.putStr(va("delegate<%s/%s>", *((VObject *)params[pi].p)->GetClass()->GetFullName(), *((VMemberBase *)params[pi+1].p)->GetFullName()), width, toRight, zeroFill, (fspec == 'q'));
                }
              } else {
                pbuf.putStr("(empty delegate)", width, toRight, zeroFill);
              }
              pi += 1;
              break;
            //case TYPE_Struct:
            //case TYPE_Array:
            //case TYPE_DynamicArray:
            //case TYPE_SliceArray:
            //case TYPE_Unknown:
            //case TYPE_Automatic:
            //  pbuf.putStr(VStr(ptypes[pi].GetName()), width, toRight, zeroFill, (fspec == 'q'));
            //  break;
            case TYPE_Vector:
              {
                char tx[VStr::FloatBufSize];
                char ty[VStr::FloatBufSize];
                char tz[VStr::FloatBufSize];
                VStr::float2str(tx, params[pi+0].f);
                VStr::float2str(ty, params[pi+1].f);
                VStr::float2str(tz, params[pi+2].f);
                pbuf.putStr(va("(%s,%s,%s)", tx, ty, tz), width, toRight, zeroFill);
              }
              pi += 2;
              break;
            case TYPE_Dictionary:
              {
                VScriptDict *dc = (VScriptDict *)params[pi].p;
                if (dc && dc->length()) {
                  pbuf.putStr(va("dictionary!(%s,%s)", *dc->getKeyType().GetName(), *dc->getValueType().GetName()), width, toRight, zeroFill);
                } else {
                  pbuf.putStr("<empty dicitionary>", width, toRight, zeroFill);
                }
              }
              break;
            case TYPE_DynamicArray:
              {
                VScriptArray *arr = (VScriptArray *)params[pi].p;
                pbuf.putStr(va("%s(%d)", *ptypes[pi].GetName(), arr->length()), width, toRight, zeroFill);
              }
              break;
            case TYPE_Array:
            case TYPE_SliceArray:
              pbuf.putStr(*ptypes[pi].GetName(), width, toRight, zeroFill);
              break;
            default: VObject::VMDumpCallStack(); Sys_Error("Invalid argument to format specifier '%c'", fspec);
          }
          ++pi;
          break;
        default:
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
          GCon->Logf(NAME_Dev, "PF_FormatString: Unknown format identifier '%c'", fspec);
#else
          fprintf(stderr, "PF_FormatString: Unknown format identifier '%c'\n", fspec);
#endif
          pbuf.putChar(fspec);
          break;
      }
    } else {
      pbuf.putChar(str[spos++]);
    }
  }

#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
  if (pi < MAX_PARAMS) GCon->Log(NAME_Dev, "PF_FormatString: Not all params were used");
  //if (pi > count) GCon->Log(NAME_Dev, "PF_FormatString: Param count overflow");
#else
  if (pi < MAX_PARAMS) fprintf(stderr, "PF_FormatString: Not all params were used (%d, %d)\n", pi, count);
  //if (pi > count) fprintf(stderr, "PF_FormatString: Param count overflow\n");
#endif

  VStr res(pbuf.getCStr());
  return res;
}


// if `buf` is `nullptr`, it means "flush"
void (*PR_WriterCB) (const char *buf, bool debugPrint, VName wrname) = nullptr;

static char wrbuffer[16384] = {0};


void PR_DoWriteBuf (const char *buf, bool debugPrint, VName wrname) {
  if (PR_WriterCB) {
    PR_WriterCB(buf, debugPrint, wrname);
    return;
  }
  if (!buf) {
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
         if (debugPrint) GCon->Log(NAME_Dev, wrbuffer);
    else if (wrname == NAME_None) GCon->Log(wrbuffer);
    else GCon->Log((EName)wrname.GetIndex(), wrbuffer);
#endif
    wrbuffer[0] = 0;
  } else if (buf[0]) {
    char *sptr = (char *)memchr(wrbuffer, 0, sizeof(wrbuffer));
    if (!sptr) {
      wrbuffer[sizeof(wrbuffer)-1] = 0; // just in case
    } else {
      size_t maxlen = (size_t)(wrbuffer+sizeof(wrbuffer)-sptr);
      if (maxlen > 1) snprintf(sptr, maxlen, "%s", buf);
    }
  }
}


void PR_WriteOne (const VFieldType &type) {
  char buf[256];
  buf[0] = 0;
  switch (type.Type) {
    case TYPE_Int: case TYPE_Byte: snprintf(buf, sizeof(buf), "%d", PR_Pop()); break;
    case TYPE_Bool: snprintf(buf, sizeof(buf), "%s", (PR_Pop() ? "true" : "false")); break;
    case TYPE_Float: /*snprintf(buf, sizeof(buf), "%f", PR_Popf());*/ (void)VStr::float2str(buf, PR_Popf()); break;
    case TYPE_Name:
      //snprintf(buf, sizeof(buf), "%s", *PR_PopName()); break;
      {
        VName n = PR_PopName();
        if (n == NAME_None) {
          PR_DoWriteBuf("");
        } else {
          PR_DoWriteBuf(*n);
        }
      }
      return;
    case TYPE_String:
      //snprintf(buf, sizeof(buf), "%s", *PR_PopStr()); break;
      {
        VStr s = PR_PopStr();
        if (s.length() == 0) {
          PR_DoWriteBuf("");
        } else {
          PR_DoWriteBuf(*s);
        }
      }
      return;
    case TYPE_Vector:
      {
        TVec v = PR_Popv();
        //snprintf(buf, sizeof(buf), "(%f,%f,%f)", v.x, v.y, v.z);
        buf[0] = '(';
        int bpos = 1;
        int xlen = VStr::float2str(buf+bpos, v.x);
        bpos += xlen;
        buf[bpos++] = ',';
        xlen = VStr::float2str(buf+bpos, v.y);
        bpos += xlen;
        buf[bpos++] = ',';
        xlen = VStr::float2str(buf+bpos, v.z);
        bpos += xlen;
        buf[bpos++] = ')';
        buf[bpos] = 0;
      }
      break;
    case TYPE_Pointer: snprintf(buf, sizeof(buf), "<%s>(%p)", *type.GetName(), PR_PopPtr()); break;
    case TYPE_Class:
      {
        VClass *cls = (VClass *)PR_PopPtr();
        if (cls) {
          snprintf(buf, sizeof(buf), "(class!%s)", cls->GetName());
        } else {
          snprintf(buf, sizeof(buf), "<%s>", *type.GetName());
        }
      }
      break;
    case TYPE_State:
      {
        VState *st = (VState *)PR_PopPtr();
        if (st) {
          snprintf(buf, sizeof(buf), "<state:%s %d %f>", (st->SpriteName != NAME_None ? *st->SpriteName : "<####>"), st->Frame, st->Time);
        } else {
          snprintf(buf, sizeof(buf), "<state>");
        }
      }
      break;
    case TYPE_Reference:
      {
        VObject *o = PR_PopRef();
        if (o) {
          snprintf(buf, sizeof(buf), "(%s)", *o->GetClass()->Name);
        } else {
          snprintf(buf, sizeof(buf), "<none:%s>", *type.GetName());
        }
      }
      break;
    case TYPE_Delegate:
      //snprintf(buf, sizeof(buf), "<%s:%p:%p>", *type.GetName(), PR_PopPtr(), PR_PopPtr());
      {
        VMethod *m = (VMethod *)PR_PopPtr();
        VObject *o = (VObject *)PR_PopPtr();
        if (m) {
          if (!o) {
            snprintf(buf, sizeof(buf), "(invalid delegate)");
          } else {
            snprintf(buf, sizeof(buf), "delegate<%s/%s>", *o->GetClass()->GetFullName(), *m->GetFullName());
          }
        } else {
          snprintf(buf, sizeof(buf), "(empty delegate)");
        }
      }
      break;
    case TYPE_Struct: PR_PopPtr(); snprintf(buf, sizeof(buf), "<%s>", *type.Struct->Name); break;
    case TYPE_Array: PR_PopPtr(); snprintf(buf, sizeof(buf), "<%s>", *type.GetName()); break;
    case TYPE_SliceArray: snprintf(buf, sizeof(buf), "<%s:%d>", *type.GetName(), PR_Pop()); PR_PopPtr(); break;
    case TYPE_DynamicArray:
      {
        VScriptArray *a = (VScriptArray *)PR_PopPtr();
        snprintf(buf, sizeof(buf), "%s(%d)", *type.GetName(), a->Num());
      }
      break;
    case TYPE_Dictionary:
      {
        VScriptDict *dc = (VScriptDict *)PR_PopPtr();
        if (dc && dc->length()) {
          snprintf(buf, sizeof(buf), "dictionary!(%s,%s)", *dc->getKeyType().GetName(), *dc->getValueType().GetName());
        } else {
          snprintf(buf, sizeof(buf), "<empty dictionary>");
        }
      }
      break;
    default: Sys_Error("Tried to print something strange...");
  }
  PR_DoWriteBuf(buf);
}


void PR_WriteFlush () {
  PR_DoWriteBuf(nullptr);
}
