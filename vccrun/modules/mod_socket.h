/**************************************************************************
 *
 * Coded by Ketmar Dark, 2018
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 **************************************************************************/
#ifndef VCCMOD_SOCKET_HEADER_FILE
#define VCCMOD_SOCKET_HEADER_FILE
// event-based socket i/o

#include "../vcc_run.h"


// callback should be thread-safe (it may be called from several different threads)
// if `wantAck` is `true`, and event wasn't eaten or cancelled in dispatcher,
// the next callback will be called
extern void (*sockmodPostEventCB) (int code, int sockid, int data, bool wantAck);

// this callback will be called... ah, see above
void sockmodAckEvent (int code, int sockid, int data, bool eaten, bool cancelled);


#endif
