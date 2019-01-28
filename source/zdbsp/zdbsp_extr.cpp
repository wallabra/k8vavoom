#include "zdbsp_extr.h"
//#include "../console.h"
#include "../misc.h"
extern FOutputDevice *GCon;


//==========================================================================
//
//  ZDWarn
//
//==========================================================================
void ZDWarn (const char *format, ...) {
  if (!ZDBSP::ShowWarnings) return;

  va_list marker;
  va_start(marker, format);
  //GLog.doWrite(NAME_Log, format, marker, false); // don't add EOL
  char *buf = vavarg(format, marker);
  va_end(marker);
  char *ebuf = buf+strlen(buf);
  while (ebuf > buf && (ebuf[-1] == '\n' || ebuf[-1] == '\r')) {
    --ebuf;
    *ebuf = 0;
  }
  GCon->Log(buf);
}


// total==-1: complete
/*
void ZDProgress (int curr, int total) {
  GCon->Logf("BSP: %d/%d", curr, total);
}
*/
