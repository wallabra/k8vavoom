//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
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
template<typename T, typename C> class Property {
public:
  using GetterType = T (C::*) () const;
  using SetterType = void (C::*) (const T &);

private:
  C *const mObject;
  GetterType const mGetter;
  SetterType const mSetter;

public:
  Property (C *aobject, GetterType agetter, SetterType asetter) : mObject(aobject), mGetter(agetter), mSetter(asetter) {}

  operator T () const { return (mObject->*mGetter)(); }

  C &operator = (const T &avalue) { (mObject->*mSetter)(avalue); return *mObject; }
};


template<typename T, typename C> class PropertyRO {
public:
  using GetterType = T (C::*) () const;

private:
  C *const mObject;
  GetterType const mGetter;

public:
  PropertyRO (C *aobject, GetterType agetter) : mObject(aobject), mGetter(agetter) {}

  operator T () const { return (mObject->*mGetter)(); }
};
