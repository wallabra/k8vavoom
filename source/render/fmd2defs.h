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
//**
//**  ALIAS MODELS
//**
//**************************************************************************

// little-endian "IDP2"
#define IDPOLY2HEADER  (((vuint32)'2'<<24)+((vuint32)'P'<<16)+((vuint32)'D'<<8)+(vuint32)'I')
#define ALIAS_VERSION  (8)

#define IDPOLY3HEADER  (((vuint32)'3'<<24)+((vuint32)'P'<<16)+((vuint32)'D'<<8)+(vuint32)'I')
#define MD3_VERSION    (15)

// TODO: tune this
#define MAXALIASVERTS    (16000)
#define MAXALIASSTVERTS  (16000)


#pragma pack(push, 1)

struct mmdl_t {
  vuint32 ident;
  vuint32 version;
  vuint32 skinwidth;
  vuint32 skinheight;
  vuint32 framesize;
  vuint32 numskins;
  vuint32 numverts;
  vuint32 numstverts;
  vuint32 numtris;
  vuint32 numcmds;
  vuint32 numframes;
  vuint32 ofsskins;
  vuint32 ofsstverts;
  vuint32 ofstris;
  vuint32 ofsframes;
  vuint32 ofscmds;
  vuint32 ofsend;
};

struct mskin_t {
  char name[64];
};

struct mstvert_t {
  vint16 s;
  vint16 t;
};

struct mtriangle_t {
  vuint16 vertindex[3];
  vuint16 stvertindex[3];
};

struct mframe_t {
  TVec scale;
  TVec scale_origin;
  char name[16];
};

struct trivertx_t {
  vuint8 v[3];
  vuint8 lightnormalindex;
};

#pragma pack(pop)
