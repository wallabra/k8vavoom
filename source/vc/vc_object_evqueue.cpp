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
static event_t *events = nullptr;
static int eventMax = 2048; // default size; why not?
static int eventLast = 0; // points past the last stored event
static int eventFirst = 0; // first stored event
// that is, new event will be posted at [head+1], and
// to read events, get [tail] while (tail != head)


//==========================================================================
//
//  VObject::ClearEventQueue
//
//  unconditionally clears event queue
//
//==========================================================================
void VObject::ClearEventQueue () {
  eventLast = eventFirst = 0;
}


//==========================================================================
//
//  VObject::CountQueuedEvents
//
//  check if event queue has any unprocessed events
//  returns number of events in queue or 0
//  it is unspecified if unprocessed events will be processed in the current
//  frame, or in the next one
//
//==========================================================================
int VObject::CountQueuedEvents () {
  return eventLast+(eventLast < eventFirst ? eventMax : 0)-eventFirst;
}


//==========================================================================
//
//  VObject::GetEventQueueSize
//
//  returns maximum size of event queue
//  note that event queue may be longer that the returned value
//
//==========================================================================
int VObject::GetEventQueueSize () {
  return eventMax;
}


//==========================================================================
//
//  VObject::SetEventQueueSize
//
//  invalid newsize values will be ignored
//  if event queue currently contanis more than `newsize` events, the API is noop
//  returns success flag
//
//==========================================================================
bool VObject::SetEventQueueSize (int newsize) {
  if (newsize < 1) return false; // oops
  if (newsize < CountQueuedEvents()) return false;
  // allocate new buffer, copy events
  event_t *newbuf = (event_t *)Z_Malloc(newsize*sizeof(event_t));
  int npos = 0;
  while (eventFirst != eventLast) {
    newbuf[npos++] = events[eventFirst++];
    eventFirst %= eventMax;
  }
  Z_Free(events);
  events = newbuf;
  eventFirst = 0;
  eventLast = npos;
  return true;
}


//==========================================================================
//
//  VObject::PostEvent
//
//  returns `false` if queue is full
//  add event to the bottom of the current queue
//  it is unspecified if posted event will be processed in the current
//  frame, or in the next one
//
//==========================================================================
bool VObject::PostEvent (const event_t &ev) {
  int nextFree = (eventLast+1)%eventMax;
  if (nextFree == eventFirst) return false; // queue overflow
  // if this is first ever event, allocate queue
  if (!events) events = (event_t *)Z_Malloc(eventMax*sizeof(event_t));
  events[eventLast] = ev;
  eventLast = nextFree;
  return true;
}


//==========================================================================
//
//  VObject::InsertEvent
//
//  returns `false` if queue is full
//  add event to the top of the current queue
//  it is unspecified if posted event will be processed in the current
//  frame, or in the next one
//
//==========================================================================
bool VObject::InsertEvent (const event_t &ev) {
  int prevFirst = (eventFirst+eventMax-1)%eventMax;
  if (prevFirst == eventLast) return false; // queue overflow
  // if this is first ever event, allocate queue
  if (!events) events = (event_t *)Z_Malloc(eventMax*sizeof(event_t));
  events[eventFirst] = ev;
  eventFirst = prevFirst;
  return true;
}


//==========================================================================
//
//  VObject::PeekEvent
//
//  peek event from queue
//  event with index 0 is the top one
//  it is safe to call this with `nullptr`
//
//==========================================================================
bool VObject::PeekEvent (int idx, event_t *ev) {
  if (idx < 0 || idx > CountQueuedEvents()) {
    if (ev) memset((void *)ev, 0, sizeof(event_t));
    return false;
  }
  if (ev) *ev = events[(idx+eventFirst)%eventMax];
  return true;
}


// get top event from queue
// returns `false` if there are no more events
// it is safe to call this with `nullptr` (in this case event will be removed)
bool VObject::GetEvent (event_t *ev) {
  if (eventFirst == eventLast) {
    if (ev) memset((void *)ev, 0, sizeof(event_t));
    return false;
  }
  if (ev) *ev = events[eventFirst++];
  eventFirst %= eventMax;
  return true;
}
