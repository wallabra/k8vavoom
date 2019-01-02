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

#define VERSION_MAJOR 1
#define VERSION_MINOR 34
#define VERSION_RELEASE 9
#define VERSION_TEXT  "1.34.9"

// The version as seen in the Windows resource
#define RC_FILEVERSION VERSION_MAJOR,VERSION_MINOR,VERSION_RELEASE,666
#define RC_PRODUCTVERSION VERSION_MAJOR,VERSION_MINOR,VERSION_RELEASE,666
#define RC_FILEVERSION2 VERSION_TEXT
#define RC_PRODUCTVERSION2 VERSION_TEXT

#if !defined CLIENT && !defined SERVER
# define CLIENT
# define SERVER
#endif

// if rangecheck is undefined, most parameter validation debugging code
// will not be compiled
//#define PARANOID    1

//#define DEVELOPER   1
