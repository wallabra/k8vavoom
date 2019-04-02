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
#define MAXALIASVERTS    (65536)
#define MAXALIASSTVERTS  (65536)


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


// ////////////////////////////////////////////////////////////////////////// //
// md3
struct MD3Header {
  char sign[4];
  vuint32 ver;
  char name[64];
  vuint32 flags;
  vuint32 frameNum;
  vuint32 tagNum;
  vuint32 surfaceNum;
  vuint32 skinNum;
  vuint32 frameOfs;
  vuint32 tagOfs;
  vuint32 surfaceOfs;
  vuint32 eofOfs;
};

struct MD3Frame {
  float bmin[3];
  float bmax[3];
  float origin[3];
  float radius;
  char name[16];
};

struct MD3Tag {
  char name[64];
  float origin[3];
  float axes[3][3];
};

struct MD3Shader {
  char name[64];
  vuint32 index;
};

struct MD3Tri {
  vuint32 v0, v1, v2;
};

struct MD3ST {
  float s, t;
};

struct MD3Vertex {
  vint16 x, y, z;
  vuint16 normal;
};

struct MD3Surface {
  char sign[4];
  char name[64];
  vuint32 flags;
  vuint32 frameNum;
  vuint32 shaderNum;
  vuint32 vertNum;
  vuint32 triNum;
  vuint32 triOfs;
  vuint32 shaderOfs;
  vuint32 stOfs;
  vuint32 vertOfs;
  vuint32 endOfs;
};


static inline __attribute__((unused)) TVec md3vert (const MD3Vertex *v) { return TVec(v->x/64.0f, v->y/64.0f, v->z/64.0f); }

static inline __attribute__((unused)) TVec md3vertNormal (const MD3Vertex *v) {
  const float lat = ((v->normal>>8)&0xff)*(2*M_PI)/255.0f;
  const float lng = (v->normal&0xff)*(2*M_PI)/255.0f;
  return TVec(cosf(lat)*sinf(lng), sinf(lat)*sinf(lng), cosf(lng));
}


#pragma pack(pop)
