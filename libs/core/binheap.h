//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
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
//**
//**  Template for mapping kays to values.
//**
//**************************************************************************

#define TBinHeap_Class_Name  TBinHeap
#include "binheap_impl.h"
#undef TBinHeap_Class_Name

/*
#define TBinHeap_Class_Name  TBinHeapNC
#define TMAP_NO_CLEAR
#include "binheap_impl.h"
#undef TMAP_NO_CLEAR
#undef TBinHeap_Class_Name
*/

#define TBinHeap_Class_Name  TBinHeapNoDtor
#define VV_HEAP_SKIP_DTOR
#include "binheap_impl.h"
#undef VV_HEAP_SKIP_DTOR
#undef TBinHeap_Class_Name
