//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
#include "gamedefs.h"
#include "cl_local.h"
#include "drawer.h"


#define MAXHISTORY       (32)
#define MAX_LINES        (1024)
#define MAX_LINE_LENGTH  (80)


enum cons_state_t {
  cons_closed,
  cons_opening,
  cons_open,
  cons_closing
};


class FConsoleDevice : public FOutputDevice {
public:
  virtual void Serialise (const char *V, EName Event) override;
};


class FConsoleLog : public VLogListener {
public:
  virtual void Serialise (const char *V, EName Event) override;
};


extern bool graphics_started;


FConsoleDevice Console;
FOutputDevice *GCon = &Console;


static TILine c_iline = {"", 0};

static cons_state_t consolestate = cons_closed;

static char clines[MAX_LINES][MAX_LINE_LENGTH];
static int num_lines = 0;
static int first_line = 0;
static int last_line = 0;

static char c_history_buf[MAXHISTORY][MAX_ILINE_LENGTH];
static int c_history_size = 0;
static int c_history_current = -1;
static char *c_history[MAXHISTORY] = {nullptr}; // 0 is oldest

static float cons_h = 0;

static VCvarF con_height("con_height", "240", "Console height.", CVAR_Archive);
static VCvarF con_speed("con_speed", "6666", "Console sliding speed.", CVAR_Archive);

// autocomplete
static int c_autocompleteIndex = -1;
static VStr c_autocompleteString;
static FConsoleLog ConsoleLog;


//==========================================================================
//
//  C_Init
//
//  Console initialization
//
//==========================================================================
void C_Init () {
  memset(clines, 0, sizeof(clines));
  /*
  c_history_last = 0;
  */
  memset(c_history_buf, 0, sizeof(c_history_buf));
  c_history_size = 0;
  c_history_current = -1;
  for (int f = 0; f < MAXHISTORY; ++f) c_history[f] = c_history_buf[f];
  GLog.AddListener(&ConsoleLog);
}


//==========================================================================
//
//  C_Shutdown
//
//==========================================================================
void C_Shutdown () {
  c_autocompleteString.Clean();
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
  if (consolestate == cons_closed) {
    c_iline.Init();
    last_line = num_lines;
  }
  consolestate = cons_opening;
  c_history_current = -1;
  c_autocompleteIndex = -1;
}


//==========================================================================
//
//  C_StartFull
//
//==========================================================================
void C_StartFull () {
  MN_DeactivateMenu();
  c_iline.Init();
  last_line = num_lines;
  consolestate = cons_open;
  c_history_current = -1;
  c_autocompleteIndex = -1;
  cons_h = 480.0;
}


//==========================================================================
//
//  ToggleConsole
//
//==========================================================================
COMMAND(ToggleConsole) {
  C_Start();
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
//  C_Stop
//
//  Close console
//
//==========================================================================
COMMAND(HideConsole) {
  consolestate = cons_closing;
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
  T_DrawText(4, y, ">", CR_UNTRANSLATED);
  int i = VStr::Length(c_iline.Data)-37;
  if (i < 0) i = 0;
  T_DrawText(12, y, c_iline.Data+i, CR_UNTRANSLATED);
  T_DrawCursor();
  y -= 10;

  // lines
  i = last_line;
  while ((y+9 > 0) && i--) {
    T_DrawText(4, y, clines[(i+first_line)%MAX_LINES], CR_UNTRANSLATED);
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
  bool eat;

  // respond to events only when console is active
  if (!C_Active()) return false;

  // we are iterested only in key down events
  if (ev->type != ev_keydown) return false;

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
      if (c_iline.Data && c_iline.Data[0]) {
        // print it
        GCon->Logf(">%s", c_iline.Data);

        // add to history (but if it is a duplicate, move it to history top)
        int dupidx = -1;
        for (int f = 0; f < c_history_size; ++f) {
          if (VStr::Cmp(c_iline.Data, c_history[f]) == 0) {
            dupidx = f;
            break;
          }
        }
        if (dupidx >= 0) {
          if (dupidx != c_history_size-1) {
            // move to history bottom (or top, it depends of your PoV)
            char *cplp = c_history[dupidx];
            for (int f = dupidx+1; f < c_history_size; ++f) c_history[f-1] = c_history[f];
            c_history[c_history_size-1] = cplp;
          }
        } else {
          if (c_history_size == MAXHISTORY) {
            // move oldest line to bottom, and reuse it
            char *cplp = c_history[0];
            for (int f = 1; f < MAXHISTORY; ++f) c_history[f-1] = c_history[f];
            c_history[MAXHISTORY-1] = cplp;
          } else {
            ++c_history_size;
          }
          char *dest = c_history[c_history_size-1];
          const char *newstr = c_iline.Data;
          if (!newstr) newstr = "";
          auto cldlen = (int)strlen(newstr);
          if (cldlen < MAX_LINE_LENGTH) {
            memcpy(dest, newstr, cldlen+1);
          } else {
            memcpy(dest, newstr, MAX_LINE_LENGTH-1);
          }
          dest[MAX_LINE_LENGTH-1] = 0;
        }
        c_history_current = -1;

        // add to command buffer
        GCmdBuf << c_iline.Data << "\n";
      }

      // clear line
      c_iline.Init();
      c_autocompleteIndex = -1;
      return true;

    // scroll lines up
    case K_PAGEUP:
      for (int i = 0; i < (GInput->ShiftDown ? 1 : 5); ++i) {
        if (last_line > 1) --last_line;
      }
      return true;

    // scroll lines down
    case K_PAGEDOWN:
      for (int i = 0; i < (GInput->ShiftDown ? 1 : 5); ++i) {
        if (last_line < num_lines) ++last_line;
      }
      return true;

    // go to first line
    case K_HOME:
      last_line = 1;
      return true;

    // go to last line
    case K_END:
      last_line = num_lines;
      return true;

    // command history up
    case K_UPARROW:
      if (c_history_size > 0 && c_history_current < c_history_size-1) {
        ++c_history_current;
        c_iline.Init();
        cp = c_history[c_history_size-c_history_current-1];
        while (*cp) c_iline.AddChar(*cp++);
        c_autocompleteIndex = -1;
      }
      return true;

    // command history down
    case K_DOWNARROW:
      if (c_history_size > 0 && c_history_current >= 0) {
        --c_history_current;
        c_iline.Init();
        if (c_history_current >= 0) {
          cp = c_history[c_history_size-c_history_current-1];
          while (*cp) c_iline.AddChar(*cp++);
        }
        c_autocompleteIndex = -1;
      }
      return true;

    // auto complete
    case K_TAB:
      if (!c_iline.Data[0]) return true;

      if (c_autocompleteIndex == -1) c_autocompleteString = c_iline.Data;
      str = VCommand::GetAutoComplete(c_autocompleteString, c_autocompleteIndex, (GInput->ShiftDown ? true : false));
      if (str.length() != 0) {
        c_iline.Init();
        for (int i = 0; i < (int)str.Length(); ++i) c_iline.AddChar(str[i]);
        c_iline.AddChar(' ');
      }
      return true;

    // add character to input line
    default:
      eat = c_iline.Key((byte)ev->data1);
      if (eat) c_autocompleteIndex = -1;
      return eat;
  }
}


//==========================================================================
//
//  Cls_f
//
//==========================================================================
COMMAND(Cls) {
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
static void AddLine (const char *Data) {
  if (num_lines >= MAX_LINES) {
    --num_lines;
    ++first_line;
  }
  VStr::NCpy(clines[(num_lines+first_line)%MAX_LINES], Data, MAX_LINE_LENGTH);
  clines[(num_lines+first_line)%MAX_LINES][MAX_LINE_LENGTH-1] = 0;
  ++num_lines;
  if (last_line == num_lines-1) last_line = num_lines;
}


//==========================================================================
//
//  DoPrint
//
//==========================================================================
static char cpbuf[MAX_LINE_LENGTH];
static int  cpbuflen = 0;

static void DoPrint (const char *buf) {
  const char *ch;
  const char *p;
  int wlen;

#ifndef _WIN32
  //k8: done in `Serialize()` if (!graphics_started) printf("%s", buf);
#endif

  ch = buf;
  while (*ch) {
    if (*ch == '\n') {
      cpbuf[cpbuflen] = 0;
      AddLine(cpbuf);
      cpbuflen = 0;
      ++ch;
    } else if (*(const vuint8 *)ch > ' ') {
      // count word length
      p = ch;
      wlen = 0;
      while (*(const vuint8 *)p > ' ') {
        ++wlen;
        ++p;
      }

      if (cpbuflen+wlen >= MAX_LINE_LENGTH) {
        if (cpbuflen) {
          // word too long and it is not a first word
          // add current buffer and try again
          cpbuf[cpbuflen] = 0;
          AddLine(cpbuf);
          cpbuflen = 0;
        } else {
          // a very long word
          VStr::NCpy(cpbuf, ch, MAX_LINE_LENGTH-1);
          cpbuf[MAX_LINE_LENGTH-1] = 0;
          AddLine(cpbuf);
          ch += MAX_LINE_LENGTH-1;
        }
      } else {
        // add word to buffer
        while (*(const vuint8 *)ch > ' ') cpbuf[cpbuflen++] = *ch++;
      }
    } else {
      // whitespace symbol
      cpbuf[cpbuflen++] = *ch;
      if (cpbuflen >= MAX_LINE_LENGTH) {
        cpbuf[MAX_LINE_LENGTH-1] = 0;
        AddLine(cpbuf);
        cpbuflen = 0;
      }
      ++ch;
    }
  }
}


//==========================================================================
//
//  FConsoleDevice::Serialise
//
//==========================================================================
void FConsoleDevice::Serialise (const char *V, EName Event) {
  dprintf("%s: %s\n", VName::SafeString(Event), *VStr(V).RemoveColours());
  if (Event == NAME_Dev && !developer) return;
  printf("%s: %s\n", VName::SafeString(Event), *VStr(V).RemoveColours()); //k8
  DoPrint(V);
  DoPrint("\n");
}


//==========================================================================
//
//  FConsoleLog::Serialise
//
//==========================================================================
void FConsoleLog::Serialise (const char *Text, EName Event) {
  if (Event == NAME_Dev && !developer) return;
  DoPrint(Text);
}
