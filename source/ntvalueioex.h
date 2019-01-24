//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2019 Ketmar Dark
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
//**  extended NTValue-based I/O
//**
//**************************************************************************

class VNTValueIOEx : public VNTValueIO {
public:
  VNTValueIOEx (VStream *astrm) : VNTValueIO(astrm) {}

  // fuck you, shitplusplus!
  virtual void io (VName vname, vint32 &v) override { VNTValueIO::io(vname, v); }
  virtual void io (VName vname, vuint32 &v) override { VNTValueIO::io(vname, v); }
  virtual void io (VName vname, float &v) override { VNTValueIO::io(vname, v); }
  virtual void io (VName vname, TVec &v) override { VNTValueIO::io(vname, v); }
  virtual void io (VName vname, VName &v) override { VNTValueIO::io(vname, v); }
  virtual void io (VName vname, VStr &v) override { VNTValueIO::io(vname, v); }
  virtual void io (VName vname, VClass *&v) override { VNTValueIO::io(vname, v); }
  virtual void io (VName vname, VObject *&v) override { VNTValueIO::io(vname, v); }
  virtual void io (VName vname, VSerialisable *&v) override { VNTValueIO::io(vname, v); }

  virtual void io (VName vname, VTextureID &v);
};
