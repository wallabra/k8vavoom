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
#include "gamedefs.h"
#include "cl_local.h"
#include "drawer.h"


#define MAXHISTORY       (64)
#define MAX_LINES        (1024)
#define MAX_LINE_LENGTH  (80)


static const char *cli_LogFileName = nullptr;

/*static*/ bool cliRegister_con_args =
  VParsedArgs::RegisterStringOption("-logfile", "specify log file name", &cli_LogFileName) &&
  VParsedArgs::RegisterAlias("-log-file", "-logfile") &&
  VParsedArgs::RegisterAlias("--log-file", "-logfile");


enum cons_state_t {
  cons_closed,
  cons_opening,
  cons_open,
  cons_closing,
};


class FConsoleDevice : public FOutputDevice {
public:
  FConsoleDevice () noexcept;
  virtual void Serialise (const char *V, EName Event) noexcept override;
};


class FConsoleLog : public VLogListener {
public:
  virtual void Serialise (const char *V, EName Event) noexcept override;
};


static mythread_mutex conLogLock;
FConsoleDevice Console;
FOutputDevice *GCon = &Console;

FConsoleDevice::FConsoleDevice () noexcept {
  mythread_mutex_init(&conLogLock);
}


static TILine c_iline;

static cons_state_t consolestate = cons_closed;

struct ConLine {
  char *str;
  int len;
  int alloced;

  ConLine () : str(nullptr), len(0), alloced(0) {}
};

static ConLine clines[MAX_LINES];
static int num_lines = 0;
static int first_line = 0;
static int last_line = 0;


struct HistoryLine {
  char *str;
  int bufsize; // including trailing zero; NOT line length!

  HistoryLine () noexcept : str(nullptr), bufsize(0) {}
  ~HistoryLine () noexcept { Z_Free(str); str = nullptr; bufsize = 0; }

  HistoryLine &operator = (const HistoryLine &src) = delete;

  inline void swap (HistoryLine &src) noexcept {
    if (&src == this) return;
    { char *tmp = str; str = src.str; src.str = tmp; }
    { int tmp = bufsize; bufsize = src.bufsize; src.bufsize = tmp; }
  }

  inline const char *getCStr () const noexcept { return (str ? str : ""); }

  inline bool strEqu (const char *s, bool doStrip) const noexcept {
    if (!str) return false; // not initialized
    if (!s || !s[0]) return (str[0] == 0);
    if (doStrip) return strEqu(VStr(s), true);
    return (VStr::Cmp(str, s) == 0);
  }

  inline bool strEqu (VStr s, bool doStrip) const noexcept {
    if (!str) return false; // not initialized
    if (s.isEmpty()) return (str[0] == 0);
    if (!doStrip) return s.strEqu(str);
    return s.xstrip().strEqu(VStr(str).xstrip());
  }

  inline void putStr (const char *s) noexcept {
    if (!s) s = "";
    int slen = (int)strlen(s)+1;
    int newsz = (slen|0xff)+1;
    if (bufsize < newsz) {
      str = (char *)Z_Realloc(str, newsz);
      bufsize = newsz;
    }
    strcpy(str, s);
  }
};

static int c_history_size = 0;
static int c_history_current = -1;
static HistoryLine c_history[MAXHISTORY]; // 0 is oldest


static float cons_h = 0;

static VCvarF con_height("con_height", "240", "Console height.", CVAR_Archive);
static VCvarF con_speed("con_speed", "6666", "Console sliding speed.", CVAR_Archive);
static VCvarB con_clear_input_on_open("con_clear_input_on_open", true, "Clear input line when console opens?", CVAR_Archive);

static FConsoleLog ConsoleLog;

static FILE *logfout = nullptr;


//==========================================================================
//
//  onShowCompletionMatchCB
//
//==========================================================================
static void onShowCompletionMatchCB (bool isheader, VStr s) {
  if (isheader) {
    GCon->Logf("\034K%s", *s);
  } else {
    GCon->Logf("\034D  %s", *s);
  }
}


//==========================================================================
//
//  C_SysErrorCallback
//
//==========================================================================
static void C_SysErrorCallback (const char *msg) noexcept {
  if (logfout) {
    fprintf(logfout, "%s\n", (msg ? msg : ""));
    fclose(logfout);
    logfout = nullptr;
  }
}


//==========================================================================
//
//  C_Init
//
//  Console initialization
//
//==========================================================================
void C_Init () {
  VCommand::onShowCompletionMatch = &onShowCompletionMatchCB;

  if (cli_LogFileName && cli_LogFileName[0]) {
    logfout = fopen(cli_LogFileName, "w");
  }

#if defined(_WIN32)
  if (!logfout) logfout = fopen("conlog.log", "w");
#elif defined(__SWITCH__) && !defined(SWITCH_NXLINK)
  if (!logfout) logfout = fopen("/switch/k8vavoom/conlog.log", "w");
#endif

  SysErrorCB = &C_SysErrorCallback;

  memset((void *)&clines[0], 0, sizeof(clines));
  c_history_size = 0;
  c_history_current = -1;
  for (int f = 0; f < MAXHISTORY; ++f) {
    vassert(c_history[f].str == nullptr);
    vassert(c_history[f].bufsize == 0);
  }
  GLog.AddListener(&ConsoleLog);

  // prompt, cursor, and one char reserved
  c_iline.SetVisChars(MAX_LINE_LENGTH-3);
  c_iline.Init();
}


//==========================================================================
//
//  C_Shutdown
//
//==========================================================================
void C_Shutdown () {
  if (logfout) fclose(logfout);
  logfout = nullptr;
}


//==========================================================================
//
//  C_Start
//
//  Open console
//
//==========================================================================
void C_Start () {
  MN_DeactivateMenu();
  MyThreadLocker lock(&conLogLock);
  if (consolestate == cons_closed) {
    if (con_clear_input_on_open) c_iline.Init();
    last_line = num_lines;
  }
  consolestate = cons_opening;
  c_history_current = -1;
}


//==========================================================================
//
//  C_StartFull
//
//==========================================================================
void C_StartFull () {
  MN_DeactivateMenu();
  MyThreadLocker lock(&conLogLock);
  c_iline.Init();
  last_line = num_lines;
  consolestate = cons_open;
  c_history_current = -1;
  //cons_h = 480.0f;
  cons_h = fmax(con_height.asFloat(), 128.0f);
}


//==========================================================================
//
//  C_Stop
//
//  Close console
//
//==========================================================================
void C_Stop () {
  consolestate = cons_closing;
}


//==========================================================================
//
//  ToggleConsole
//
//==========================================================================
COMMAND(ToggleConsole) {
  //C_Start();
  if (consolestate == cons_closed) C_Start(); else C_Stop();
}


//==========================================================================
//
//  ShowConsole
//
//==========================================================================
COMMAND(ShowConsole) {
  C_Start();
}


//==========================================================================
//
//  HideConsole
//
//==========================================================================
COMMAND(HideConsole) {
  C_Stop();
}


//==========================================================================
//
//  C_Active
//
//==========================================================================
bool C_Active () {
  return (consolestate == cons_opening || consolestate == cons_open);
}


//==========================================================================
//
//  DrawInputLine
//
//  font and text mode should be already set
//
//==========================================================================
static void DrawInputLine (int y) {
  // input line
  T_DrawText(4, y, ">", CR_YELLOW);
  c_iline.DrawAt(12, y, CR_ORANGE, CR_FIRE);
}


//==========================================================================
//
//  C_Drawer
//
//  Draws console
//
//==========================================================================
void C_Drawer () {
  // scroll console up when closing
  if (consolestate == cons_closing) {
    cons_h -= con_speed*host_frametime;
    if (cons_h <= 0) {
      // closed
      cons_h = 0;
      consolestate = cons_closed;
    }
  }

  // scroll console down when opening
  if (consolestate == cons_opening) {
    cons_h += con_speed*host_frametime;
    if (cons_h >= con_height) {
      // open
      cons_h = con_height;
      consolestate = cons_open;
    }
  }

  if (!consolestate) return;

  // background
  Drawer->DrawConsoleBackground((int)(fScaleY*cons_h));

  T_SetFont(ConFont);
  T_SetAlign(hleft, vtop);

  // input line
  int y = (int)cons_h-10;
  DrawInputLine(y);
  y -= 10;

  // lines
  MyThreadLocker lock(&conLogLock);
  int i = last_line;
  while ((y+9 > 0) && i--) {
    int lidx = (i+first_line)%MAX_LINES;
    const ConLine &line = clines[lidx];
    int trans = CR_UNTRANSLATED;
    //if (line[0] == 1) { trans = line[1]; line += 2; }
    T_DrawText(4, y, (line.str ? line.str : ""), trans);
    y -= 9;
  }
}


//==========================================================================
//
//  C_Responder
//
//  Handles the events
//
//==========================================================================
bool C_Responder (event_t *ev) {
  const char *cp;
  VStr str;

  // respond to events only when console is active
  if (!C_Active()) return false;

  // we are iterested only in key down events
  if (ev->type != ev_keydown) return false;
  // k8: nope, eat all keyboard events
  //     oops, console (de)activation is processed down the chain
  //if (ev->type != ev_keydown && ev->type != ev_keyup) return false;

  // shit; i have to perform more fine-grained locking
  switch (ev->data1) {
    // close console
    case K_ESCAPE:
      if (consolestate != cons_open) return false;
      /* fallthrough */
    case '`':
    case K_BACKQUOTE:
      if (consolestate == cons_closing) C_Start(); else C_Stop();
      return true;

    // execute entered command
    case K_ENTER:
    case K_PADENTER:
      {
        VStr ccmds = VStr(c_iline.getCStr()).xstrip();
        if (ccmds.length() != 0) {
          VStr ccmdfull = VStr(c_iline.getCStr()).trimLeft();
          // leave only one trailing space
          if (!ccmdfull.isEmpty() && (vuint8)ccmdfull[ccmdfull.length()-1] <= ' ') ccmdfull = ccmdfull.xstrip()+" ";
          // print it
          GCon->Logf(">%s", *ccmds);
          MyThreadLocker lock(&conLogLock);

          // add to history (but if it is a duplicate, move it to history top)
          int dupidx = -1;
          for (int f = 0; f < c_history_size; ++f) {
            if (c_history[f].strEqu(ccmds, true)) {
              dupidx = f;
              // if we have a space at the end, replace
              if (ccmdfull.length() > ccmds.length()) c_history[f].putStr(*ccmdfull);
              break;
            }
          }
          if (dupidx >= 0) {
            // duplicate found
            // move to history bottom (or top, it depends of your PoV)
            for (int f = dupidx+1; f <= c_history_size-1; ++f) c_history[f-1].swap(c_history[f]);
          } else {
            // no duplicate, append to history buffer
            if (c_history_size == MAXHISTORY) {
              // move oldest line to bottom, and reuse it
              for (int f = 1; f < MAXHISTORY; ++f) c_history[f-1].swap(c_history[f]);
            } else {
              ++c_history_size;
            }
            c_history[c_history_size-1].putStr(*ccmdfull);
          }
          c_history_current = -1;

          // add to command buffer
          GCmdBuf << ccmds << "\n";
        }
      }

      // clear line
      c_iline.Init();
      return true;

    // scroll lines up
    case K_PAGEUP:
      {
        MyThreadLocker lock(&conLogLock);
        for (int i = 0; i < (ev->isShiftDown() ? 1 : max2(2, (int)con_height/9-2)); ++i) {
          if (last_line > 1) --last_line;
        }
      }
      return true;

    // scroll lines down
    case K_PAGEDOWN:
      {
        MyThreadLocker lock(&conLogLock);
        for (int i = 0; i < (ev->isShiftDown() ? 1 : max2(2, (int)con_height/9-2)); ++i) {
          if (last_line < num_lines) ++last_line;
        }
      }
      return true;

    // go to first line
    case K_HOME:
      if (ev->isShiftDown()) {
        MyThreadLocker lock(&conLogLock);
        last_line = 1;
        return true;
      }
      return c_iline.Key(*ev);

    // go to last line
    case K_END:
      if (ev->isShiftDown()) {
        MyThreadLocker lock(&conLogLock);
        last_line = num_lines;
        return true;
      }
      return c_iline.Key(*ev);

    // command history up
    case K_UPARROW:
      if (c_history_size > 0 && c_history_current < c_history_size-1) {
        ++c_history_current;
        c_iline.Init();
        cp = c_history[c_history_size-c_history_current-1].getCStr();
        while (*cp) c_iline.AddChar(*cp++);
      }
      return true;

    // command history down
    case K_DOWNARROW:
      if (c_history_size > 0 && c_history_current >= 0) {
        --c_history_current;
        c_iline.Init();
        if (c_history_current >= 0) {
          cp = c_history[c_history_size-c_history_current-1].getCStr();
          while (*cp) c_iline.AddChar(*cp++);
        }
      }
      return true;

    // auto complete
    case K_TAB:
      //TODO: autocompletion with moved cursor
      if (c_iline.length() != 0) {
        VStr clineRest = c_iline.getCStr();
        VStr cline = clineRest.left(c_iline.getCurPos());
        clineRest.chopLeft(c_iline.getCurPos());
        // find last command
        int cmdstart = cline.findNextCommand();
        for (;;) {
          const int cmdstnext = cline.findNextCommand(cmdstart);
          if (cmdstnext == cmdstart) break;
          cmdstart = cmdstnext;
        }
        VStr oldpfx = cline;
        oldpfx.chopLeft(cmdstart); // remove completed commands
        VStr newpfx = VCommand::GetAutoComplete(oldpfx);
        if (oldpfx != newpfx) {
          c_iline.Init();
          c_iline.AddString(cline.left(cmdstart));
          c_iline.AddString(newpfx);
          // append rest of cline
          if (clineRest.length()) {
            int cpos = c_iline.getCurPos();
            c_iline.AddString(clineRest);
            c_iline.setCurPos(cpos);
          }
        }
      }
      return true;

    // add character to input line
    default:
      return c_iline.Key(*ev);
  }
}


//==========================================================================
//
//  Cls_f
//
//==========================================================================
COMMAND(Cls) {
  MyThreadLocker lock(&conLogLock);
  num_lines = 0;
  first_line = 0;
  last_line = 0;
}


//==========================================================================
//
//  AddLine
//
//  Ads a line to console strings
//
//==========================================================================
static void AddLine (const char *Data) noexcept {
  if (!Data) Data = "";
  //MyThreadLocker lock(&conLogLock);
  if (num_lines >= MAX_LINES) {
    --num_lines;
    ++first_line;
  }
  int len = VStr::length(Data);
  int lidx = (num_lines+first_line)%MAX_LINES;
  ConLine &line = clines[lidx];
  if (len+1 > line.alloced) {
    int newsz = ((len+1)|0xff)+1;
    line.str = (char *)Z_Realloc(line.str, newsz);
    line.alloced = newsz;
  }
  line.len = len;
  memcpy(line.str, Data, len+1);
  /*
  VStr::NCpy(clines[(num_lines+first_line)%MAX_LINES], Data, MAX_LINE_LENGTH);
  clines[(num_lines+first_line)%MAX_LINES][MAX_LINE_LENGTH-1] = 0;
  */
  ++num_lines;
  if (last_line == num_lines-1) last_line = num_lines;
}


//==========================================================================
//
//  DoPrint
//
//==========================================================================
static ConLine cpbuf;
static int cpCurrLineLen = 0;
static VStr cpLastColor;
static bool cpLogFileNeedName = true;


static void cpAppendChar (char ch) noexcept {
  if (ch == 0) ch = ' ';
  int nlen = cpbuf.len+1;
  if (nlen+1 > cpbuf.alloced) {
    int newsz = ((nlen+1)|0xff)+1;
    cpbuf.str = (char *)Z_Realloc(cpbuf.str, newsz);
    cpbuf.alloced = newsz;
  }
  cpbuf.str[cpbuf.len++] = ch;
  cpbuf.str[cpbuf.len] = 0;
}


static void cpPrintCurrColor () noexcept {
  for (const char *s = cpLastColor.getCStr(); *s; ++s) cpAppendChar(*s);
}


static void cpFlushCurrent (bool asNewline) noexcept {
  AddLine(cpbuf.str);
  cpbuf.len = 0;
  if (cpbuf.str) cpbuf.str[0] = 0;
  cpCurrLineLen = 0;
  if (asNewline) {
    cpLastColor.clear();
  } else {
    cpPrintCurrColor();
  }
}


// *ch should be TEXT_COLOR_ESCAPE
static const char *cpProcessColorEscape (const char *ch) noexcept {
  vassert(*ch == TEXT_COLOR_ESCAPE);
  cpLastColor.clear();
  ++ch; // skip TEXT_COLOR_ESCAPE
  if (!ch[0]) {
    // reset
    cpAppendChar(TEXT_COLOR_ESCAPE);
    cpAppendChar('L'); // untranslated
    return ch;
  }
  cpLastColor += TEXT_COLOR_ESCAPE;
  if (*ch == '[') {
    cpLastColor += *ch++;
    while (*ch && *ch != ']') cpLastColor += *ch++;
    if (*ch) cpLastColor += *ch++; else cpLastColor += ']';
  } else {
    cpLastColor += *ch++;
  }
  cpPrintCurrColor();
  return ch;
}


// *ch should be TEXT_COLOR_ESCAPE
static const char *cpSkipColorEscape (const char *ch) noexcept {
  vassert(*ch == TEXT_COLOR_ESCAPE);
  ++ch; // skip TEXT_COLOR_ESCAPE
  if (!ch[0]) return ch;
  if (*ch++ == '[') {
    while (*ch && *ch != ']') ++ch;
    if (*ch) ++ch;
  }
  return ch;
}


static void DoPrint (const char *buf) noexcept {
  const char *ch = buf;
  while (*ch) {
    if (*ch == '\n') {
      cpFlushCurrent(true);
      ++ch;
    } else if (*ch == TEXT_COLOR_ESCAPE) {
      // new color sequence
      ch = cpProcessColorEscape(ch);
    } else if (*(const vuint8 *)ch > ' ') {
      // count word length
      const char *p = ch;
      int wlen = 0;
      while (*(const vuint8 *)p > ' ') {
        if (*p == TEXT_COLOR_ESCAPE) {
          p = cpSkipColorEscape(p);
        } else {
          ++wlen;
          ++p;
        }
      }

      if (cpCurrLineLen+wlen >= MAX_LINE_LENGTH) {
        if (cpCurrLineLen) {
          // word too long and it is not a first word
          // add current buffer and try again
          cpFlushCurrent(false); // don't clear current color
        } else {
          // a very long first word, add partially
          while (*(const vuint8 *)ch > ' ' && cpCurrLineLen < MAX_LINE_LENGTH) {
            if (*ch == TEXT_COLOR_ESCAPE) {
              ch = cpProcessColorEscape(ch);
            } else {
              cpAppendChar(*ch++);
              ++cpCurrLineLen;
            }
          }
          cpFlushCurrent(false); // don't clear current color
        }
      } else {
        // add word to buffer
        while (*(const vuint8 *)ch > ' ') {
          if (*ch == TEXT_COLOR_ESCAPE) {
            ch = cpProcessColorEscape(ch);
          } else {
            cpAppendChar(*ch++);
            ++cpCurrLineLen;
          }
        }
      }
    } else {
      // whitespace symbol
      if (cpCurrLineLen < MAX_LINE_LENGTH) {
        int count;
        if (*ch == '\t') {
          // tab
          count = 8-cpCurrLineLen%8;
        } else {
          count = 1;
        }
        while (count--) {
          cpAppendChar(' ');
          ++cpCurrLineLen;
        }
      }
      ++ch;
    }
  }
}


//==========================================================================
//
//  ConSerialise
//
//  tty output is done by standard logger
//
//==========================================================================
static void ConSerialise (const char *str, EName Event, bool fromGLog) noexcept {
  //devprintf("%s: %s\n", VName::SafeString(Event), *rc);
  if (Event == NAME_Dev && !developer) return;
  if (!fromGLog) { GLog.WriteLine(Event, "%s", str); return; }
  if (!str) str = "";
  MyThreadLocker lock(&conLogLock);
  //fprintf(stderr, "<<<%s>>>", str);
  //HACK! if string starts with "Sys_Error:", print it, and close log file
  if (VStr::NCmp(str, "Sys_Error:", 10) == 0) {
    if (logfout) { fflush(logfout); fprintf(logfout, "*** %s\n", str); fclose(logfout); logfout = nullptr; }
  }
  if (Event == NAME_Warning) {
    cpLastColor = VStr(TEXT_COLOR_ESCAPE_STR "X");
    cpPrintCurrColor();
  } else if (Event == NAME_Error) {
    cpLastColor = VStr(TEXT_COLOR_ESCAPE_STR "T"); //R T
    cpPrintCurrColor();
  } else if (Event == NAME_Init) {
    cpLastColor = VStr(TEXT_COLOR_ESCAPE_STR "C");
    cpPrintCurrColor();
  }
  DoPrint(str);
  // write to log
  if (logfout) {
    VStr rc = VStr(str).RemoveColors();
    const char *rstr = *rc;
    while (rstr && *rstr) {
      const char *eol = strchr(rstr, '\n');
      if (!eol) eol = rstr+strlen(rstr);
      if (cpLogFileNeedName) {
        fprintf(logfout, "%s:%s", VName::SafeString(Event), (rstr == eol ? "" : " "));
        cpLogFileNeedName = false;
      }
      if (eol != rstr) fwrite(rstr, (ptrdiff_t)(eol-rstr), 1, logfout);
      rstr = eol;
      if (!rstr[0]) break;
      vassert(rstr[0] == '\n');
      fprintf(logfout, "\n");
      cpLogFileNeedName = true;
      ++rstr;
    }
    //fprintf(logfout, "%s: %s", VName::SafeString(Event), *rc);
  }
}


//==========================================================================
//
//  FConsoleDevice::Serialise
//
//==========================================================================
void FConsoleDevice::Serialise (const char *V, EName Event) noexcept {
  ConSerialise(V, Event, false);
}


//==========================================================================
//
//  FConsoleLog::Serialise
//
//==========================================================================
void FConsoleLog::Serialise (const char *Text, EName Event) noexcept {
  ConSerialise(Text, Event, true);
}
