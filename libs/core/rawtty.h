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
#ifndef VAVOOM_RAWTTY_HEADER
#define VAVOOM_RAWTTY_HEADER


// TTY event returned by `ttyReadEvent()`
struct TTYEvent {
  // event type
  enum Type /*: ubyte*/ {
    None,
    Error, // error reading key
    Unknown, // can't interpret escape code

    Char,

    // for bracketed paste mode
    PasteStart,
    PasteEnd,

    ModChar, // char with some modifier

    Up,
    Down,
    Left,
    Right,
    Insert,
    Delete,
    PageUp,
    PageDown,
    Home,
    End,

    Escape,
    Backspace,
    Tab,
    Enter,

    Pad5, // xterm can return this

    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,

    //A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    //N0, N1, N2, N3, N4, N5, N6, N7, N8, N9,

    MLeftDown,
    MLeftUp,
    MLeftMotion,
    MMiddleDown,
    MMiddleUp,
    MMiddleMotion,
    MRightDown,
    MRightUp,
    MRightMotion,

    MWheelUp,
    MWheelDown,

    MMotion, // mouse motion without buttons, not read from tty by now, but can be useful for other backends

    // synthesized events, used in tui
    MLeftClick,
    MMiddleClick,
    MRightClick,

    MLeftDouble,
    MMiddleDouble,
    MRightDouble,

    FocusIn,
    FocusOut,
  };

  enum ModFlag /*: ubyte*/ {
    Ctrl  = 1u<<0,
    Alt   = 1u<<1,
    Shift = 1u<<2,
  };

  // returned by getter
  enum MButton /*: int*/ {
    MBNone = 0,
    MBLeft = 1,
    MBMiddle = 2,
    MBRight = 3,
    MBWheelUp = 4,
    MBWheelDown = 5,
    MBFirst = MBLeft,
  };

public:
  Type type = Type::None; // event type
  unsigned mods = 0; // set of ModFlag
  unsigned ch = 0; // can be 0 for special key
  int x = 0, y = 0; // for mouse reports
  double time = 0; // when the event was read (alas); set with `Sys_Time()`

public:
  static const char *getEventTypeName (const Type t) noexcept;

public:
  inline TTYEvent () noexcept { memset((void *)this, 0, sizeof(*this)); }

  inline TTYEvent (const char *s) noexcept {
    if (!parseEx(s)) {
      type = Type::Error;
      mods = 0;
      ch = 0;
    }
  }

  inline MButton getMouseButton () const noexcept {
    return
      type == Type::MLeftDown || type == Type::MLeftUp || type == Type::MLeftMotion || type == Type::MLeftClick || type == Type::MLeftDouble ? MButton::MBLeft :
      type == Type::MRightDown || type == Type::MRightUp || type == Type::MRightMotion || type == Type::MRightClick || type == Type::MRightDouble ? MButton::MBRight :
      type == Type::MMiddleDown || type == Type::MMiddleUp || type == Type::MMiddleMotion || type == Type::MMiddleClick || type == Type::MMiddleDouble ? MButton::MBMiddle :
      type == Type::MWheelUp ? MButton::MBWheelUp :
      type == Type::MWheelDown ? MButton::MBWheelDown :
      MButton::MBNone;
  }

  inline bool isMouse () const noexcept { return (type >= Type::MLeftDown && type <= Type::MRightDouble); }
  inline bool isMPress () const noexcept { return (type == Type::MLeftDown || type == Type::MRightDown || type == Type::MMiddleDown); }
  inline bool isMRelease () const noexcept { return (type == Type::MLeftUp || type == Type::MRightUp || type == Type::MMiddleUp); }
  inline bool isMClick () const noexcept { return (type == Type::MLeftClick || type == Type::MRightClick || type == Type::MMiddleClick); }
  inline bool isMDouble () const noexcept { return (type == Type::MLeftDouble || type == Type::MRightDouble || type == Type::MMiddleDouble); }
  inline bool isMMotion () const noexcept { return (type == Type::MLeftMotion || type == Type::MRightMotion || type == Type::MMiddleMotion || type == Type::MMotion); }
  inline bool isMWheel () const noexcept { return (type == Type::MWheelUp || type == Type::MWheelDown); }
  inline bool isFocusOn () const noexcept { return (type == Type::FocusIn); }
  inline bool isFocusOut () const noexcept { return (type == Type::FocusOut); }
  inline bool isCtrlDown () const noexcept { return ((mods&ModFlag::Ctrl) != 0); }
  inline bool isAltDown () const noexcept { return ((mods&ModFlag::Alt) != 0); }
  inline bool isShiftDown () const noexcept { return ((mods&ModFlag::Shift) != 0); }

  inline void setCtrlDown (const bool v) noexcept { if (v) mods |= ModFlag::Ctrl; else mods &= ~(ModFlag::Ctrl); }
  inline void setAltDown (const bool v) noexcept { if (v) mods |= ModFlag::Alt; else mods &= ~(ModFlag::Alt); }
  inline void setShiftDown (const bool v) noexcept { if (v) mods |= ModFlag::Shift; else mods &= ~(ModFlag::Shift); }

  inline bool operator == (const TTYEvent &k) const noexcept {
    return
      (type == k.type ?
       (type == Type::Char ? (ch == k.ch) :
        type == Type::ModChar ? (mods == k.mods && ch == k.ch) :
        //key >= Type::MLeftDown && key <= MWheelDown ? true :
        type > Type::ModChar ? (mods == k.mods) :
        true
       ) : false
      );
  }

  inline bool operator == (const char *s) const noexcept {
    TTYEvent k;
    if (!k.parseEx(s)) return false;
    return operator == (k);
  }

  inline VStr toString () const noexcept {
    char buf[128];
    toCharBuf(buf, 128);
    return VStr(buf);
  }

  void toCharBuf (char *dest, int destlen) const noexcept;

  /** parse key name. get first word, return rest of the string (with trailing spaces removed)
   *
   * "C-<home>" (emacs-like syntax is recognized)
   *
   * "C-M-x"
   *
   * mods: C(trl), M(eta:alt), S(hift)
   *
   * returns `false` on error, and event contents are undefined in this case
   *
   * skips all leading `s` blanks, and all trailing `s` spaces on success
   * on failure, skips only leading blanks
   */
  bool parseEx (const char *&s) noexcept;
};


// returns `false` if TTY is not available at all
bool ttyIsAvailable () noexcept;

// returns `true` if TTY is good and supports fancy features
// if TTY is not good, no other API will work, and calling 'em is UB
bool ttyIsGood () noexcept;

// switch TTY to raw or to normal mode
// returns `false` if failed
// WARNING! calls are not counted! i.e. two disables and then one enable will enable
bool ttySetRawMode (bool enable) noexcept;

bool ttyIsWaitKey () noexcept;

// set wait/poll mode
bool ttySetWaitKey (bool doWait) noexcept;


// returns current TTY mode as was previously set by `ttySetRawMode()`
bool ttyIsInRawMode () noexcept;

// returns TTY size
int ttyGetWidth () noexcept;
int ttyGetHeight () noexcept;


/**
 * Wait for keypress.
 *
 * Params:
 *  toMSec = timeout in milliseconds; <0: infinite; 0: don't wait; default is -1
 *
 * Returns:
 *  true if key was pressed, false if no key was pressed in the given time
 */
bool ttyWaitKey (int toMSec=-1) noexcept;

/**
 * Check if key was pressed. Don't block.
 *
 * Returns:
 *  true if key was pressed, false if no key was pressed
 */
bool ttyIsKeyHit () noexcept;


/**
 * Read one byte from stdin.
 *
 * Params:
 *  toMSec = timeout in milliseconds; <0: infinite; 0: don't wait; default is -1
 *
 * Returns:
 *  read byte or -1 on error/timeout
 */
int ttyReadKeyByte (int toMSec=-1) noexcept;


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
TTYEvent ttyReadKey (int toMSec=-1, int toEscMSec=300) noexcept;


void ttyRawWrite (const char *str...) noexcept;

void ttyBeep () noexcept;

void ttyEnableBracketedPaste () noexcept;
void ttyDisableBracketedPaste () noexcept;

void ttyEnableFocusReports () noexcept;
void ttyDisableFocusReports () noexcept;

void ttyEnableMouseReports () noexcept;
void ttyDisableMouseReports () noexcept;


#endif
