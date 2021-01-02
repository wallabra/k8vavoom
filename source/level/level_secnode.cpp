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
//**  Copyright (C) 2018-2021 Ketmar Dark
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
#include "../gamedefs.h"


//=============================================================================
//
//  VLevel::AddSecnode
//
//  phares 3/16/98
//
//  Searches the current list to see if this sector is already there.
//  If not, it adds a sector node at the head of the list of sectors this
//  object appears in. This is called when creating a list of nodes that
//  will get linked in later. Returns a pointer to the new node.
//
//=============================================================================
msecnode_t *VLevel::AddSecnode (sector_t *Sec, VEntity *Thing, msecnode_t *NextNode) {
  if (!Sec) Sys_Error("AddSecnode of 0 for %s\n", Thing->GetClass()->GetName());

  msecnode_t *Node = NextNode;
  while (Node) {
    // already have a node for this sector?
    if (Node->Sector == Sec) {
      // yes: setting m_thing says 'keep it'
      Node->Thing = Thing;
      return NextNode;
    }
    Node = Node->TNext;
  }

  // couldn't find an existing node for this sector: add one at the head of the list

  // retrieve a node from the freelist
  if (HeadSecNode) {
    Node = HeadSecNode;
    HeadSecNode = HeadSecNode->SNext;
  } else {
    Node = new msecnode_t;
  }

  // killough 4/4/98, 4/7/98: mark new nodes unvisited
  Node->Visited = 0;

  Node->Sector = Sec; // sector
  Node->Thing = Thing; // mobj
  Node->TPrev = nullptr; // prev node on Thing thread
  Node->TNext = NextNode; // next node on Thing thread
  if (NextNode) NextNode->TPrev = Node; // set back link on Thing

  // add new node at head of sector thread starting at Sec->TouchingThingList
  Node->SPrev = nullptr; // prev node on sector thread
  Node->SNext = Sec->TouchingThingList; // next node on sector thread
  if (Sec->TouchingThingList) Node->SNext->SPrev = Node;
  Sec->TouchingThingList = Node;
  return Node;
}


//=============================================================================
//
//  VLevel::DelSecnode
//
//  Deletes a sector node from the list of sectors this object appears in.
//  Returns a pointer to the next node on the linked list, or nullptr.
//
//=============================================================================
msecnode_t *VLevel::DelSecnode (msecnode_t *Node) {
  msecnode_t *tp; // prev node on thing thread
  msecnode_t *tn; // next node on thing thread
  msecnode_t *sp; // prev node on sector thread
  msecnode_t *sn; // next node on sector thread

  if (Node) {
    // unlink from the Thing thread. The Thing thread begins at
    // sector_list and not from VEntity->TouchingSectorList
    tp = Node->TPrev;
    tn = Node->TNext;
    if (tp) tp->TNext = tn;
    if (tn) tn->TPrev = tp;

    // unlink from the sector thread. This thread begins at
    // sector_t->TouchingThingList
    sp = Node->SPrev;
    sn = Node->SNext;
    if (sp) sp->SNext = sn; else Node->Sector->TouchingThingList = sn;
    if (sn) sn->SPrev = sp;

    // return this node to the freelist
    Node->SNext = HeadSecNode;
    HeadSecNode = Node;
    return tn;
  }
  return nullptr;
}
// phares 3/13/98


//=============================================================================
//
//  VLevel::DelSectorList
//
//  Deletes the sector_list and NULLs it.
//
//=============================================================================
void VLevel::DelSectorList () {
  if (SectorList) {
    msecnode_t *Node = SectorList;
    while (Node) Node = DelSecnode(Node);
    SectorList = nullptr;
  }
}
