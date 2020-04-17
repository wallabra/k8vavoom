//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2018-2020 Ketmar Dark
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
//#include "rawtty.h"
#include "core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifndef NO_RAWTTY
# include <stdint.h>
# include <sys/ioctl.h>
# include <sys/select.h>
# include <sys/time.h>
# include <sys/types.h>
# include <termios.h>
#endif


static bool isAvailable = false;
enum TermType {
  other,
  rxvt,
  xterm,
  linuxcon, // linux console
  shitdoze,
};


#ifdef NO_RAWTTY
# define  isGood     false
# define  isRawMode  false
# define  isWaitKey  true
static TermType termType = TermType::shitdoze;
#else
static TermType termType = TermType::other;
static bool isGood = false;
static bool isRawMode = false;
static bool isWaitKey = false;
static termios origMode;
#endif


//==========================================================================
//
//  TTYEvent::getEventTypeName
//
//==========================================================================
const char *TTYEvent::getEventTypeName (const Type t) noexcept {
  switch (t) {
    case Type::None: return "None";
    case Type::Error: return "Error";
    case Type::Unknown: return "Unknown";
    case Type::Char: return "Char";
    case Type::PasteStart: return "PasteStart";
    case Type::PasteEnd: return "PasteEnd";
    case Type::ModChar: return "ModChar";
    case Type::Up: return "Up";
    case Type::Down: return "Down";
    case Type::Left: return "Left";
    case Type::Right: return "Right";
    case Type::Insert: return "Insert";
    case Type::Delete: return "Delete";
    case Type::PageUp: return "PageUp";
    case Type::PageDown: return "PageDown";
    case Type::Home: return "Home";
    case Type::End: return "End";
    case Type::Escape: return "Escape";
    case Type::Backspace: return "Backspace";
    case Type::Tab: return "Tab";
    case Type::Enter: return "Enter";
    case Type::Pad5: return "Pad5";
    case Type::F1: return "F1";
    case Type::F2: return "F2";
    case Type::F3: return "F3";
    case Type::F4: return "F4";
    case Type::F5: return "F5";
    case Type::F6: return "F6";
    case Type::F7: return "F7";
    case Type::F8: return "F8";
    case Type::F9: return "F9";
    case Type::F10: return "F10";
    case Type::F11: return "F11";
    case Type::F12: return "F12";
    case Type::MLeftDown: return "MLeftDown";
    case Type::MLeftUp: return "MLeftUp";
    case Type::MLeftMotion: return "MLeftMotion";
    case Type::MMiddleDown: return "MMiddleDown";
    case Type::MMiddleUp: return "MMiddleUp";
    case Type::MMiddleMotion: return "MMiddleMotion";
    case Type::MRightDown: return "MRightDown";
    case Type::MRightUp: return "MRightUp";
    case Type::MRightMotion: return "MRightMotion";
    case Type::MWheelUp: return "MWheelUp";
    case Type::MWheelDown: return "MWheelDown";
    case Type::MMotion: return "MMotion";
    case Type::MLeftClick: return "MLeftClick";
    case Type::MMiddleClick: return "MMiddleClick";
    case Type::MRightClick: return "MRightClick";
    case Type::MLeftDouble: return "MLeftDouble";
    case Type::MMiddleDouble: return "MMiddleDouble";
    case Type::MRightDouble: return "MRightDouble";
    case Type::FocusIn: return "FocusIn";
    case Type::FocusOut: return "FocusOut";
    default: return "Error";
  }
}


//==========================================================================
//
//  TTYEvent::toCharBuf
//
//==========================================================================
void TTYEvent::toCharBuf (char *dest, int destlen) const noexcept {
  #define TTYEvent_PutDestChar(ch_)  do { if (destlen > 1) { *dest++ = (ch_); --destlen; } } while (0)
  #define TTYEvent_PutDestStr(s_)  do { const char *sp_ = (s_); while (*sp_) TTYEvent_PutDestChar(*sp_++); } while (0)

  if (!dest || destlen < 1) return;
  const char *hexD = "0123456789abcdef";
  if (type == Type::ModChar) {
    if (isCtrlDown()) TTYEvent_PutDestStr("C-");
    if (isAltDown()) TTYEvent_PutDestStr("M-");
    if (isShiftDown()) TTYEvent_PutDestStr("S-");
  }
  if (type == Type::Char || type == Type::ModChar) {
    if (ch < ' ' || ch == 127) {
      TTYEvent_PutDestStr("x");
      TTYEvent_PutDestChar(hexD[(ch>>4)&0x0f]);
      TTYEvent_PutDestChar(hexD[ch&0x0f]);
    } else if (ch == ' ') {
      TTYEvent_PutDestStr("space");
    } else if (ch < 256) {
      TTYEvent_PutDestChar((char)(ch&0xff));
    } else if (ch <= 0xffff) {
      TTYEvent_PutDestStr("u");
      TTYEvent_PutDestChar(hexD[(ch>>12)&0x0f]);
      TTYEvent_PutDestChar(hexD[(ch>>8)&0x0f]);
      TTYEvent_PutDestChar(hexD[(ch>>4)&0x0f]);
      TTYEvent_PutDestChar(hexD[ch&0x0f]);
    } else {
      TTYEvent_PutDestStr("error");
    }
  } else {
         if (type == Type::None) TTYEvent_PutDestStr("none");
    else if (type == Type::Error || type > Type::FocusOut) TTYEvent_PutDestStr("error"); //KEEP IN SYNC WITH EVENT ENUM!
    else if (type == Type::Unknown) TTYEvent_PutDestStr("unknown");
    else {
      if (isCtrlDown()) TTYEvent_PutDestStr("C-");
      if (isAltDown()) TTYEvent_PutDestStr("M-");
      if (isShiftDown()) TTYEvent_PutDestStr("S-");
      TTYEvent_PutDestStr(getEventTypeName(type));
    }
  }
  if (destlen > 0) *dest = 0;

  #undef TTYEvent_PutDestChar
  #undef TTYEvent_PutDestStr
}


//==========================================================================
//
//  TTYEvent::parseEx
//
//==========================================================================
bool TTYEvent::parseEx (const char *&s) noexcept {
  #define TTYEvent_StrEquCI(str_)  do { \
    sequ = true; \
    const char *sp_ = (str_); \
    int cpos = pos; \
    while (*sp_ && cpos < epos) { \
      char sch = *sp_; \
      if (sch >= 'A' && sch <= 'Z') sch = sch-'A'+'a'; \
      char dch = str[cpos]; \
      if (dch >= 'A' && dch <= 'Z') dch = dch-'A'+'a'; \
      if (sch != dch) { sequ = false; break; } \
      ++sp_; \
      ++cpos; \
    } \
    if (sequ && *sp_) sequ = false; \
  } while (0)

  if (!s) return false;
  while ((unsigned)(s[0]&0xff) <= ' ') ++s;
  if (!s[0]) return false;
  // get space-delimited word
  int pos = 1; // 0 is always non-space here
  while (s[pos] && (unsigned)(s[pos]&0xff) > ' ') { if (++pos >= 1024) return false; }
  const char *str = s; // use this to parse
  int epos = pos; // parse until this
  pos = 0; // start from here
  // result
  Type restype = Type::None; // event type
  unsigned resmods = 0; // set of ModFlag
  unsigned resch = 0; // can be 0 for special key
  bool sequ; // working var
  // parse word
  while (pos < epos) {
    // modifier?
    if (epos-pos >= 2 && str[pos+1] == '-') {
      switch (str[pos]) {
        case 'C': case 'c': resmods |= ModFlag::Ctrl; break;
        case 'M': case 'm': resmods |= ModFlag::Alt; break;
        case 'S': case 's': resmods |= ModFlag::Shift; break;
        default: return false; // unknown modifier
      }
      pos += 2;
      continue;
    }
    // remove "<>"
    if (epos-pos > 2 && str[pos] == '<' && str[epos-1] == '>') {
      ++pos;
      --epos;
    }
    // ^key?
    if (epos-pos > 1 && str[pos] == '^') {
      // ^A means C-A
      resmods |= ModFlag::Ctrl;
      ++pos;
    }
    // remove "<>"
    if (epos-pos > 2 && str[pos] == '<' && str[epos-1] == '>') {
      ++pos;
      --epos;
    }
    if (pos >= epos) return false;
    // single char?
    if (epos-pos == 1) {
      resch = (unsigned)(str[pos]&0xff);
      // modchar?
      if (resmods&(ModFlag::Ctrl|ModFlag::Alt)) {
        restype = Type::ModChar;
        if (resch >= 'a' && resch <= 'z') resch -= 32; // toupper
      } else {
        // normal or shifted char
        restype = Type::Char;
        if (resmods&ModFlag::Shift) {
          if (resch >= 'a' && resch <= 'z') resch -= 32; // toupper
          else switch (resch) {
            case '`': resch = '~'; break;
            case '1': resch = '!'; break;
            case '2': resch = '@'; break;
            case '3': resch = '#'; break;
            case '4': resch = '$'; break;
            case '5': resch = '%'; break;
            case '6': resch = '^'; break;
            case '7': resch = '&'; break;
            case '8': resch = '*'; break;
            case '9': resch = '('; break;
            case '0': resch = ')'; break;
            case '-': resch = '_'; break;
            case '=': resch = '+'; break;
            case '[': resch = '{'; break;
            case ']': resch = '}'; break;
            case ';': resch = ':'; break;
            case '\'': resch = '"'; break;
            case '\\': resch = '|'; break;
            case ',': resch = '<'; break;
            case '.': resch = '>'; break;
            case '/': resch = '?'; break;
            default: break;
          }
          resmods &= ~(ModFlag::Shift);
        }
      }
    } else {
      // key name
      TTYEvent_StrEquCI("Up"); if (sequ) restype = Type::Up;
      TTYEvent_StrEquCI("Down"); if (sequ) restype = Type::Down;
      TTYEvent_StrEquCI("Left"); if (sequ) restype = Type::Left;
      TTYEvent_StrEquCI("Right"); if (sequ) restype = Type::Right;
      TTYEvent_StrEquCI("Insert"); if (sequ) restype = Type::Insert;
      TTYEvent_StrEquCI("Delete"); if (sequ) restype = Type::Delete;
      TTYEvent_StrEquCI("PageUp"); if (sequ) restype = Type::PageUp;
      TTYEvent_StrEquCI("PageDown"); if (sequ) restype = Type::PageDown;
      TTYEvent_StrEquCI("Home"); if (sequ) restype = Type::Home;
      TTYEvent_StrEquCI("End"); if (sequ) restype = Type::End;
      TTYEvent_StrEquCI("Escape"); if (sequ) restype = Type::Escape;
      TTYEvent_StrEquCI("Backspace"); if (sequ) restype = Type::Backspace;
      TTYEvent_StrEquCI("BS"); if (sequ) restype = Type::Backspace;
      TTYEvent_StrEquCI("Tab"); if (sequ) restype = Type::Tab;
      TTYEvent_StrEquCI("Enter"); if (sequ) restype = Type::Enter;
      TTYEvent_StrEquCI("Return"); if (sequ) restype = Type::Enter;
      TTYEvent_StrEquCI("Pad5"); if (sequ) restype = Type::Pad5;
      TTYEvent_StrEquCI("F1"); if (sequ) restype = Type::F1;
      TTYEvent_StrEquCI("F2"); if (sequ) restype = Type::F2;
      TTYEvent_StrEquCI("F3"); if (sequ) restype = Type::F3;
      TTYEvent_StrEquCI("F4"); if (sequ) restype = Type::F4;
      TTYEvent_StrEquCI("F5"); if (sequ) restype = Type::F5;
      TTYEvent_StrEquCI("F6"); if (sequ) restype = Type::F6;
      TTYEvent_StrEquCI("F7"); if (sequ) restype = Type::F7;
      TTYEvent_StrEquCI("F8"); if (sequ) restype = Type::F8;
      TTYEvent_StrEquCI("F9"); if (sequ) restype = Type::F9;
      TTYEvent_StrEquCI("F10"); if (sequ) restype = Type::F10;
      TTYEvent_StrEquCI("F11"); if (sequ) restype = Type::F11;
      TTYEvent_StrEquCI("F12"); if (sequ) restype = Type::F12;

      TTYEvent_StrEquCI("MLeftDown"); if (sequ) restype = Type::MLeftDown;
      TTYEvent_StrEquCI("MLeftUp"); if (sequ) restype = Type::MLeftUp;
      TTYEvent_StrEquCI("MLeftMotion"); if (sequ) restype = Type::MLeftMotion;
      TTYEvent_StrEquCI("MMiddleDown"); if (sequ) restype = Type::MMiddleDown;
      TTYEvent_StrEquCI("MMiddleUp"); if (sequ) restype = Type::MMiddleUp;
      TTYEvent_StrEquCI("MMiddleMotion"); if (sequ) restype = Type::MMiddleMotion;
      TTYEvent_StrEquCI("MRightDown"); if (sequ) restype = Type::MRightDown;
      TTYEvent_StrEquCI("MRightUp"); if (sequ) restype = Type::MRightUp;
      TTYEvent_StrEquCI("MRightMotion"); if (sequ) restype = Type::MRightMotion;
      TTYEvent_StrEquCI("MWheelUp"); if (sequ) restype = Type::MWheelUp;
      TTYEvent_StrEquCI("MWheelDown"); if (sequ) restype = Type::MWheelDown;
      TTYEvent_StrEquCI("MLeftClick"); if (sequ) restype = Type::MLeftClick;
      TTYEvent_StrEquCI("MMiddleClick"); if (sequ) restype = Type::MMiddleClick;
      TTYEvent_StrEquCI("MRightClick"); if (sequ) restype = Type::MRightClick;
      TTYEvent_StrEquCI("MLeftDouble"); if (sequ) restype = Type::MLeftDouble;
      TTYEvent_StrEquCI("MMiddleDouble"); if (sequ) restype = Type::MMiddleDouble;
      TTYEvent_StrEquCI("MRightDouble"); if (sequ) restype = Type::MRightDouble;

      TTYEvent_StrEquCI("PasteStart"); if (sequ) { restype = Type::PasteStart; resmods = 0; }
      TTYEvent_StrEquCI("PasteEnd"); if (sequ) { restype = Type::PasteEnd; resmods = 0; }
      TTYEvent_StrEquCI("Paste-Start"); if (sequ) { restype = Type::PasteStart; resmods = 0; }
      TTYEvent_StrEquCI("Paste-End"); if (sequ) { restype = Type::PasteEnd; resmods = 0; }

      TTYEvent_StrEquCI("space"); if (sequ) { restype = Type::Char; resch = 32; }
      TTYEvent_StrEquCI("spc"); if (sequ) { restype = Type::Char; resch = 32; }

           if (restype == Type::Enter) resch = 13;
      else if (restype == Type::Tab) resch = 9;
      else if (restype == Type::Escape) resch = 27;
      else if (restype == Type::Backspace) resch = 8;

      switch (restype) {
        case Type::Enter: resch = 13; break;
        case Type::Tab: resch = 9; break;
        case Type::Escape: resch = 27; break;
        case Type::Backspace: resch = 8; break;
        default: break;
      }
    }
    if (restype == Type::None) return false;
    s = str+pos;
    while (s[0] && (unsigned)(s[0]&0xff) <= ' ') ++s;
    type = restype;
    mods = resmods;
    ch = resch;
    return true;
  }
  return false;

  #undef TTYEvent_StrEquCI
}


//==========================================================================
//
//  ttyIsAvailable
//
//  returns `false` if TTY is not available at all
//
//==========================================================================
bool ttyIsAvailable () noexcept {
  return isAvailable;
}


//==========================================================================
//
//  ttyIsGood
//
//  returns `true` if TTY is good and supports fancy features
//  if TTY is not good, no other API will work, and calling 'em is UB
//
//==========================================================================
bool ttyIsGood () noexcept {
  return isGood;
}


//==========================================================================
//
//  ttyIsInRawMode
//
//  returns current TTY mode as was previously set by `ttySetRawMode()`
//
//==========================================================================
bool ttyIsInRawMode () noexcept {
  return isRawMode;
}


//==========================================================================
//
//  ttyGetWidth
//
//==========================================================================
int ttyGetWidth () noexcept {
  #ifdef NO_RAWTTY
  return 80;
  #else
  if (!isGood) return 80;
  winsize sz;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &sz) != -1) return sz.ws_col;
  return 80;
  #endif
}


//==========================================================================
//
//  ttyGetHeight
//
//==========================================================================
int ttyGetHeight () noexcept {
  #ifdef NO_RAWTTY
  return 25;
  #else
  if (!isGood) return 25;
  winsize sz;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &sz) != -1) return sz.ws_row;
  return 80;
  #endif
}

//==========================================================================
//
//  ttySetRawMode
//
//  switch TTY to raw or to normal mode
//
//==========================================================================
bool ttySetRawMode (bool enable) noexcept {
  #ifdef NO_RAWTTY
  return false;
  #else
  if (!isGood) return false; // oops
  if (isRawMode == enable) return true; // already done
  if (enable) {
    // go to raw mode
    //enum { IUCLC = 512 }; //0001000
    //termios raw = origMode; // modify the original mode
    termios raw;
    memset((void *)&raw, 0, sizeof(raw));
    //tcflush(STDIN_FILENO, TCIOFLUSH);
    raw.c_iflag = IGNBRK;
    // output modes: disable post processing
    raw.c_oflag = OPOST|ONLCR;
    // control modes: set 8 bit chars
    raw.c_cflag = CS8|CLOCAL;
    // control chars: set return condition: min number of bytes and timer; we want read to return every single byte, without timeout
    raw.c_cc[VMIN] = (isWaitKey ? 1 : 0); // wait/poll mode
    raw.c_cc[VTIME] = 0; // no timer
    // put terminal in raw mode after flushing
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) return false;
    // G0 is ASCII, G1 is graphics
    const char *setupStr = "\x1b(B\x1b)0\x0f";
    write(STDOUT_FILENO, setupStr, strlen(setupStr));
  } else {
    // go to cooked mode
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &origMode) != 0) return false;
  }
  isRawMode = enable;
  return true;
  #endif
}


//==========================================================================
//
//  ttyIsWaitKey
//
//==========================================================================
bool ttyIsWaitKey () noexcept {
  return isWaitKey;
}


//==========================================================================
//
//  ttySetWaitKey
//
//  set wait/poll mode
//
//==========================================================================
bool ttySetWaitKey (bool doWait) noexcept {
  #ifdef NO_RAWTTY
  return false;
  #else
  if (!isGood || !isRawMode) return false;
  if (isWaitKey == doWait) return true;
  termios raw;
  if (tcgetattr(STDOUT_FILENO, &raw) != 0) return false; //redirected = false;
  raw.c_cc[VMIN] = (doWait ? 1 : 0); // wait/poll mode
  if (tcsetattr(STDOUT_FILENO, TCSAFLUSH, &raw) != 0) return false;
  isWaitKey = doWait;
  return true;
  #endif
}


static bool xtermMetaSendsEscape = true; /// you should add `XTerm*metaSendsEscape: true` to "~/.Xdefaults"
static bool ttyIsFuckedFlag = false;


//==========================================================================
//
//  ttyRawWrite
//
//==========================================================================
void ttyRawWrite (const char *str...) noexcept {
  #ifndef NO_RAWTTY
  if (str && str[0]) write(STDOUT_FILENO, str, strlen(str));
  #endif
}


//==========================================================================
//
//  ttyBeep
//
//==========================================================================
void ttyBeep () noexcept {
  if (isAvailable) {
    const char *str = "\x07";
    ttyRawWrite(str);
  }
}


//==========================================================================
//
//  ttyEnableBracketedPaste
//
//==========================================================================
void ttyEnableBracketedPaste () noexcept {
  if (isGood) {
    const char *str = "\x1b[?2004h";
    ttyRawWrite(str);
  }
}


//==========================================================================
//
//  ttyDisableBracketedPaste
//
//==========================================================================
void ttyDisableBracketedPaste () noexcept {
  if (isGood) {
    const char *str = "\x1b[?2004l";
    ttyRawWrite(str);
  }
}


//==========================================================================
//
//  ttyEnableFocusReports
//
//==========================================================================
void ttyEnableFocusReports () noexcept {
  if (isGood) {
    const char *str = "\x1b[?1004h";
    ttyRawWrite(str);
  }
}


//==========================================================================
//
//  ttyDisableFocusReports
//
//==========================================================================
void ttyDisableFocusReports () noexcept {
  if (isGood) {
    const char *str = "\x1b[?1004l";
    ttyRawWrite(str);
  }
}


//==========================================================================
//
//  ttyEnableMouseReports
//
//==========================================================================
void ttyEnableMouseReports () noexcept {
  if (isGood) {
    const char *str = "\x1b[?1000h\x1b[?1006h\x1b[?1002h";
    ttyRawWrite(str);
  }
}


//==========================================================================
//
//  ttyDisableMouseReports
//
//==========================================================================
void ttyDisableMouseReports () noexcept {
  if (isGood) {
    const char *str = "\x1b[?1002l\x1b[?1006l\x1b[?1000l";
    ttyRawWrite(str);
  }
}


/**
 * Wait for keypress.
 *
 * Params:
 *  toMSec = timeout in milliseconds; <0: infinite; 0: don't wait; default is -1
 *
 * Returns:
 *  true if key was pressed, false if no key was pressed in the given time
 */
bool ttyWaitKey (int toMSec) noexcept {
  #ifdef NO_RAWTTY
  return false;
  #else
  if (!isGood || !isRawMode) return false;
  timeval tv;
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(STDIN_FILENO, &fds); //STDIN_FILENO is 0
  if (toMSec <= 0) {
    tv.tv_sec = 0;
    tv.tv_usec = 0;
  } else {
    tv.tv_sec = (int)(toMSec/1000);
    tv.tv_usec = (toMSec%1000)*1000;
  }
  select(STDIN_FILENO+1, &fds, nullptr, nullptr, (toMSec < 0 ? nullptr : &tv));
  return FD_ISSET(STDIN_FILENO, &fds);
  #endif
}


/**
 * Check if key was pressed. Don't block.
 *
 * Returns:
 *  true if key was pressed, false if no key was pressed
 */
bool ttyIsKeyHit () noexcept {
  return ttyWaitKey(0);
}


/**
 * Read one byte from stdin.
 *
 * Params:
 *  toMSec = timeout in milliseconds; <0: infinite; 0: don't wait; default is -1
 *
 * Returns:
 *  read byte or -1 on error/timeout
 */
int ttyReadKeyByte (int toMSec) noexcept {
  #ifdef NO_RAWTTY
  return -1;
  #else
  if (!isGood || !isRawMode) return -1;
  if (!ttyWaitKey(toMSec)) return -1;
  uint8_t res;
  if (read(STDIN_FILENO, &res, 1) != 1) return -1;
  return res;
  #endif
}


//==========================================================================
//
//  skipCSI
//
//==========================================================================
static void skipCSI (TTYEvent &type, int toEscMSec) noexcept {
  type.type = TTYEvent::Type::Unknown;
  for (;;) {
    int ch = ttyReadKeyByte(toEscMSec);
    if (ch < 0 || ch == 27) { type.type = TTYEvent::Type::Escape; type.ch = 27; break; }
    if (ch != ';' && (ch < '0' || ch > '9')) break;
  }
}


//==========================================================================
//
//  badCSI
//
//==========================================================================
static void badCSI (TTYEvent &type) noexcept {
  const double tsv = type.time;
  memset((void *)&type, 0, sizeof(type));
  type.type = TTYEvent::Type::Unknown;
  type.time = tsv;
}


//==========================================================================
//
//  xtermMods
//
//==========================================================================
static bool xtermMods (TTYEvent &type, unsigned mci) noexcept {
  switch (mci) {
    case 2: type.setShiftDown(true); return true;
    case 3: type.setAltDown(true); return true;
    case 4: type.setAltDown(true); type.setShiftDown(true); return true;
    case 5: type.setCtrlDown(true); return true;
    case 6: type.setCtrlDown(true); type.setShiftDown(true); return true;
    case 7: type.setAltDown(true); type.setCtrlDown(true); return true;
    case 8: type.setAltDown(true); type.setCtrlDown(true); type.setShiftDown(true); return true;
    default: break;
  }
  return false;
}


//==========================================================================
//
//  xtermSpecial
//
//==========================================================================
static void xtermSpecial (TTYEvent &type, char ch) noexcept {
  switch (ch) {
    case 'A': type.type = TTYEvent::Type::Up; break;
    case 'B': type.type = TTYEvent::Type::Down; break;
    case 'C': type.type = TTYEvent::Type::Right; break;
    case 'D': type.type = TTYEvent::Type::Left; break;
    case 'E': type.type = TTYEvent::Type::Pad5; break;
    case 'H': type.type = TTYEvent::Type::Home; break;
    case 'F': type.type = TTYEvent::Type::End; break;
    case 'P': type.type = TTYEvent::Type::F1; break;
    case 'Q': type.type = TTYEvent::Type::F2; break;
    case 'R': type.type = TTYEvent::Type::F3; break;
    case 'S': type.type = TTYEvent::Type::F4; break;
    case 'Z': type.type = TTYEvent::Type::Tab; type.ch = 9; if (!type.isShiftDown() && !type.isAltDown() && !type.isCtrlDown()) type.setShiftDown(true); break;
    default: badCSI(type); break;
  }
}


//==========================================================================
//
//  linconSpecial
//
//==========================================================================
static void linconSpecial (TTYEvent &type, char ch) noexcept {
  switch (ch) {
    case 'A': type.type = TTYEvent::Type::F1; break;
    case 'B': type.type = TTYEvent::Type::F2; break;
    case 'C': type.type = TTYEvent::Type::F3; break;
    case 'D': type.type = TTYEvent::Type::F4; break;
    default: badCSI(type); break;
  }
}


//==========================================================================
//
//  csiSpecial
//
//==========================================================================
static void csiSpecial (TTYEvent &type, unsigned n) noexcept {
  switch (n) {
    case 1: type.type = TTYEvent::Type::Home; return; // xterm
    case 2: type.type = TTYEvent::Type::Insert; return;
    case 3: type.type = TTYEvent::Type::Delete; return;
    case 4: type.type = TTYEvent::Type::End; return;
    case 5: type.type = TTYEvent::Type::PageUp; return;
    case 6: type.type = TTYEvent::Type::PageDown; return;
    case 7: type.type = TTYEvent::Type::Home; return; // rxvt
    case 8: type.type = TTYEvent::Type::End; return;
    case 1+10: type.type = TTYEvent::Type::F1; return;
    case 2+10: type.type = TTYEvent::Type::F2; return;
    case 3+10: type.type = TTYEvent::Type::F3; return;
    case 4+10: type.type = TTYEvent::Type::F4; return;
    case 5+10: type.type = TTYEvent::Type::F5; return;
    case 6+11: type.type = TTYEvent::Type::F6; return;
    case 7+11: type.type = TTYEvent::Type::F7; return;
    case 8+11: type.type = TTYEvent::Type::F8; return;
    case 9+11: type.type = TTYEvent::Type::F9; return;
    case 10+11: type.type = TTYEvent::Type::F10; return;
    case 11+12: type.type = TTYEvent::Type::F11; return;
    case 12+12: type.type = TTYEvent::Type::F12; return;
    default: badCSI(type); break;
  }
}


//==========================================================================
//
//  parseMouse
//
//  {\e}[<0;58;32M (button;x;y;[Mm])
//
//==========================================================================
static void parseMouse (TTYEvent &type, int toEscMSec) noexcept {
  unsigned nn[3] = {0,0,0};
  unsigned nc = 0;
  bool press = false;
  for (;;) {
    auto ch = ttyReadKeyByte(toEscMSec);
    if (ch < 0 || ch == 27) { type.type = TTYEvent::Type::Escape; type.ch = 27; return; }
    if (ch == ';') {
      ++nc;
    } else if (ch >= '0' && ch <= '9') {
      if (nc < 3) nn[nc] = nn[nc]*10+ch-'0';
    } else {
           if (ch == 'M') press = true;
      else if (ch == 'm') press = false;
      else { type.type = TTYEvent::Type::Unknown; return; }
      break;
    }
  }
  if (nn[1] > 0) --nn[1];
  if (nn[2] > 0) --nn[2];
  if (nn[1] < 0) nn[1] = 1;
  if (nn[1] > 0x7fffu) nn[1] = 0x7fffu;
  if (nn[2] < 0) nn[2] = 1;
  if (nn[2] > 0x7fffu) nn[2] = 0x7fffu;
  switch (nn[0]) {
    case 0: type.type = (press ? TTYEvent::Type::MLeftDown : TTYEvent::Type::MLeftUp); break;
    case 1: type.type = (press ? TTYEvent::Type::MMiddleDown : TTYEvent::Type::MMiddleUp); break;
    case 2: type.type = (press ? TTYEvent::Type::MRightDown : TTYEvent::Type::MRightUp); break;
    case 32: if (!press) { type.type = TTYEvent::Type::Unknown; return; } type.type = TTYEvent::Type::MLeftMotion; break;
    case 33: if (!press) { type.type = TTYEvent::Type::Unknown; return; } type.type = TTYEvent::Type::MMiddleMotion; break;
    case 34: if (!press) { type.type = TTYEvent::Type::Unknown; return; } type.type = TTYEvent::Type::MRightMotion; break;
    case 64: if (!press) { type.type = TTYEvent::Type::Unknown; return; } type.type = TTYEvent::Type::MWheelUp; break;
    case 65: if (!press) { type.type = TTYEvent::Type::Unknown; return; } type.type = TTYEvent::Type::MWheelDown; break;
    default: type.type = TTYEvent::Type::Unknown; return;
  }
  type.x = nn[1];
  type.y = nn[2];
}


/**
 * Read key from stdin.
 *
 * WARNING! no utf-8 support yet!
 *
 * Params:
 *  toMSec = timeout in milliseconds; <0: infinite; 0: don't wait; default is -1
 *  toEscMSec = timeout in milliseconds for escape sequences
 *
 * Returns:
 *  null on error or keyname
 */
TTYEvent ttyReadKey (int toMSec, int toEscMSec) noexcept {
  TTYEvent type;
  memset((void *)&type, 0, 0);
  type.time = Sys_Time();

  int ch = ttyReadKeyByte(toMSec);
  if (ch < 0) { type.type = TTYEvent::Type::Error; return type; } // error
  if (ch == 0) { type.type = TTYEvent::Type::ModChar; type.setCtrlDown(true); type.ch = ' '; return type; }
  if (ch == 8 || ch == 127) { type.type = TTYEvent::Type::Backspace; type.ch = 8; return type; }
  if (ch == 9) { type.type = TTYEvent::Type::Tab; type.ch = 9; return type; }
  //if (ch == 10) { key.key = TtyEvent.Type::Enter; key.ch = 13; return key; }
  if (ch == 13) { type.type = TTYEvent::Type::Enter; type.ch = 13; return type; }

  type.type = TTYEvent::Type::Unknown;

  // escape?
  if (ch == 27) {
    ch = ttyReadKeyByte(toEscMSec);
    if (ch < 0 || ch == 27) { type.type = TTYEvent::Type::Escape; type.ch = 27; return type; }
    // xterm stupidity
    if (termType != TermType::rxvt && ch == 'O') {
      ch = ttyReadKeyByte(toEscMSec);
      if (ch < 0 || ch == 27) { type.type = TTYEvent::Type::Escape; type.ch = 27; return type; }
      if (ch >= 'A' && ch <= 'Z') xtermSpecial(type, (char)ch);
      if (ch >= 'a' && ch <= 'z') { type.setShiftDown(true); xtermSpecial(type, (char)(ch-32)); }
      return type;
    }
    // csi
    if (ch == '[') {
      unsigned nn[2] = {0,0};
      unsigned nc = 0;
      bool wasDigit = false;
      bool firstChar = true;
      bool linuxCon = false;
      // parse csi
      for (;;) {
        ch = ttyReadKeyByte(toEscMSec);
        if (firstChar && ch == '<') { parseMouse(type, toEscMSec); return type; }
        if (firstChar && ch == 'I') { type.type = TTYEvent::Type::FocusIn; return type; }
        if (firstChar && ch == 'O') { type.type = TTYEvent::Type::FocusOut; return type; }
        if (firstChar && ch == '[') { linuxCon = true; firstChar = false; continue; }
        firstChar = false;
        if (ch < 0 || ch == 27) { type.type = TTYEvent::Type::Escape; type.ch = 27; return type; }
        if (ch == ';') {
          ++nc;
          if (nc > 2) { skipCSI(type, toEscMSec); return type; }
        } else if (ch >= '0' && ch <= '9') {
          if (nc >= 2) { skipCSI(type, toEscMSec); return type; }
          nn[nc] = nn[nc]*10+ch-'0';
          wasDigit = true;
        } else {
          if (wasDigit) ++nc;
          break;
        }
      }
      //debug(rawtty_show_csi) { import core.stdc.stdio : printf; printf("nc=%u", nc); foreach (unsigned idx; 0..nc) printf("; n%u=%u", idx, nn.ptr[idx]); printf("; ch=%c\n", ch); }
      // process specials
      if (nc == 0) {
             if (linuxCon) linconSpecial(type, (char)ch);
        else if (ch >= 'A' && ch <= 'Z') xtermSpecial(type, (char)ch);
      } else if (nc == 1) {
        if (ch == '~' && nn[0] == 200) { type.type = TTYEvent::Type::PasteStart; return type; }
        if (ch == '~' && nn[0] == 201) { type.type = TTYEvent::Type::PasteEnd; return type; }
        switch (ch) {
          case '~':
            switch (nn[0]) {
              case 23: type.setShiftDown(true); type.type = TTYEvent::Type::F1; return type;
              case 24: type.setShiftDown(true); type.type = TTYEvent::Type::F2; return type;
              case 25: type.setShiftDown(true); type.type = TTYEvent::Type::F3; return type;
              case 26: type.setShiftDown(true); type.type = TTYEvent::Type::F4; return type;
              case 28: type.setShiftDown(true); type.type = TTYEvent::Type::F5; return type;
              case 29: type.setShiftDown(true); type.type = TTYEvent::Type::F6; return type;
              case 31: type.setShiftDown(true); type.type = TTYEvent::Type::F7; return type;
              case 32: type.setShiftDown(true); type.type = TTYEvent::Type::F8; return type;
              case 33: type.setShiftDown(true); type.type = TTYEvent::Type::F9; return type;
              case 34: type.setShiftDown(true); type.type = TTYEvent::Type::F10; return type;
              default: break;
            }
            break;
          case '^': type.setCtrlDown(true); break;
          case '$': type.setShiftDown(true); break;
          case '@': type.setCtrlDown(true); type.setShiftDown(true); break;
          case 'A' ... 'Z': xtermMods(type, nn[0]); xtermSpecial(type, (char)ch); return type;
          default: badCSI(type); return type;
        }
        csiSpecial(type, nn[0]);
      } else if (nc == 2 && xtermMods(type, nn[1])) {
        if (nn[0] == 1 && ch >= 'A' && ch <= 'Z') {
          xtermSpecial(type, (char)ch);
        } else if (ch == '~') {
          csiSpecial(type, nn[0]);
        }
      } else {
        badCSI(type);
      }
      return type;
    }
    if (ch == 9) {
      type.type = TTYEvent::Type::Tab;
      type.setAltDown(true);
      type.ch = 9;
      return type;
    }
    if (ch >= 1 && ch <= 26) {
      type.type = TTYEvent::Type::ModChar;
      type.setAltDown(true);
      type.setCtrlDown(true);
      type.ch = (unsigned)(ch+64);
           if (type.ch == 'H') { type.type = TTYEvent::Type::Backspace; type.setCtrlDown(false); type.ch = 8; }
      else if (type.ch == 'J') { type.type = TTYEvent::Type::Enter; type.setCtrlDown(false); type.ch = 13; }
      return type;
    }
    if (/*(ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '`'*/true) {
      type.setAltDown(true);
      type.type = TTYEvent::Type::ModChar;
      type.setShiftDown(ch >= 'A' && ch <= 'Z'); // ignore capslock
      if (ch >= 'a' && ch <= 'z') ch -= 32;
      type.ch = (unsigned)ch;
      return type;
    }
    return type;
  }

  if (ch == 9) {
    type.type = TTYEvent::Type::Tab;
    type.ch = 9;
  } else if (ch < 32) {
    // ctrl+letter
    type.type = TTYEvent::Type::ModChar;
    type.setCtrlDown(true);
    type.ch = (unsigned)(ch+64);
         if (type.ch == 'H') { type.type = TTYEvent::Type::Backspace; type.setCtrlDown(false); type.ch = 8; }
    else if (type.ch == 'J') { type.type = TTYEvent::Type::Enter; type.setCtrlDown(false); type.ch = 13; }
  } else {
    type.type = TTYEvent::Type::Char;
    type.ch = (unsigned)ch;
    if (ttyIsFuckedFlag && ch >= 0x80) {
      VUtf8DecoderFast udc;
      for (;;) {
        if (udc.put(ch&0xff)) {
          // done
          type.ch = (udc.invalid() ? '?' : udc.codepoint);
          break;
        }
        // want more shit!
        ch = ttyReadKeyByte(toEscMSec);
        if (ch < 0) {
          type.ch = '?';
          break;
        }
      }
    } else {
      // xterm does alt+letter with 7th bit set
      if (!xtermMetaSendsEscape && termType == TermType::xterm && ch >= 0x80 && ch <= 0xff) {
        ch -= 0x80;
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_') {
          type.setAltDown(true);
          type.type = TTYEvent::Type::ModChar;
          type.setShiftDown(ch >= 'A' && ch <= 'Z'); // ignore capslock
          if (ch >= 'a' && ch <= 'z') ch -= 32;
          type.ch = (unsigned)ch;
          return type;
        }
      }
    }
  }
  return type;
}


// ////////////////////////////////////////////////////////////////////////// //
class RawTTYInternalInitClass {
  static bool isStartsWithCI (const char *s, const char *pat) noexcept {
    if (!s || !pat || !s[0] || !pat[0]) return false;
    while (*s && *pat) {
      char cs = *s++;
      if (cs >= 'A' && cs <= 'Z') cs = cs-'A'+'a';
      char cp = *pat++;
      if (cp >= 'A' && cp <= 'Z') cp = cp-'A'+'a';
      if (cs != cp) return false;
    }
    return (pat[0] == 0);
  }

public:
  RawTTYInternalInitClass () {
    isAvailable = (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO));
    #ifndef NO_RAWTTY
    ttyIsFuckedFlag = false;

    const char *lang = getenv("LANG");
    if (!lang) return;

    while (*lang) {
      if ((lang[0] == 'U' || lang[0] == 'u') &&
          (lang[1] == 'T' || lang[1] == 't') &&
          (lang[2] == 'F' || lang[2] == 'f'))
      {
        ttyIsFuckedFlag = true;
        break;
      }
      ++lang;
    }

    if (isAvailable) {
      isGood = (tcgetattr(STDOUT_FILENO, &origMode) == 0);
    } else {
      isGood = false;
    }

    const char *tt = getenv("TERM");
    if (tt) {
           if (isStartsWithCI(tt, "rxvt")) termType = TermType::rxvt;
      else if (isStartsWithCI(tt, "xterm")) termType = TermType::xterm;
      else if (isStartsWithCI(tt, "linux")) termType = TermType::linuxcon;
    }
    #endif
  }

  ~RawTTYInternalInitClass () {
    ttySetRawMode(false);
    if (isGood) ttyRawWrite("\x1b[0m"); // reset color
  }
};

RawTTYInternalInitClass rawTTYInternalInitClassInitVar_;
