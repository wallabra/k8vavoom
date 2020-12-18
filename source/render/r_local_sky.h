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
#ifndef VAVOOM_R_LOCAL_HEADER_SKY
#define VAVOOM_R_LOCAL_HEADER_SKY


// ////////////////////////////////////////////////////////////////////////// //
class VSky {
public:
  enum {
    VDIVS = 8,
    HDIVS = 16
  };

  sky_t sky[HDIVS*VDIVS];
  int NumSkySurfs;
  int SideTex;
  bool bIsSkyBox;
  bool SideFlip;

  void InitOldSky (int, int, float, float, bool, bool, bool);
  void InitSkyBox (VName, VName);
  void Init (int, int, float, float, bool, bool, bool, bool);
  void Draw (int);
};


// ////////////////////////////////////////////////////////////////////////// //
class VSkyPortal : public VPortal {
public:
  VSky *Sky;

  inline VSkyPortal (VRenderLevelShared *ARLev, VSky *ASky) : VPortal(ARLev), Sky(ASky) {}
  virtual bool NeedsDepthBuffer () const override;
  virtual bool IsSky () const override;
  virtual bool MatchSky (VSky *) const override;
  virtual void DrawContents () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VSkyBoxPortal : public VPortal {
public:
  VEntity *Viewport;

  inline VSkyBoxPortal (VRenderLevelShared *ARLev, VEntity *AViewport) : VPortal(ARLev), Viewport(AViewport) {}
  virtual bool IsSky () const override;
  virtual bool IsSkyBox () const override;
  virtual bool MatchSkyBox (VEntity *) const override;
  virtual void DrawContents () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VSectorStackPortal : public VPortal {
public:
  VEntity *Viewport;

  inline VSectorStackPortal (VRenderLevelShared *ARLev, VEntity *AViewport) : VPortal(ARLev), Viewport(AViewport) {}
  virtual bool IsStack () const override;
  virtual bool MatchSkyBox (VEntity *) const override;
  virtual void DrawContents () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VMirrorPortal : public VPortal {
public:
  TPlane *Plane;

  inline VMirrorPortal (VRenderLevelShared *ARLev, TPlane *APlane) : VPortal(ARLev), Plane(APlane) {}
  virtual bool IsMirror () const override;
  virtual bool MatchMirror (TPlane *) const override;
  virtual void DrawContents () override;
};


#endif
