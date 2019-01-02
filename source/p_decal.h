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
// decal, decalgroup, decal animator


// ////////////////////////////////////////////////////////////////////////// //
class VDecalDef;
class VDecalAnim;
class VDecalGroup;


// ////////////////////////////////////////////////////////////////////////// //
// linked list of all known decals
class VDecalDef {
public:
  enum {
    FlipNone = 0,
    FlipAlways = 1,
    FlipRandom = 2,
  };

private:
  VDecalDef *next; // in decalDefHead
  VName animname;

private:
  static void addToList (VDecalDef *dc);
  static void removeFromList (VDecalDef *dc);

  void fixup ();

public:
  // name is not parsed yet
  bool parse (VScriptParser *sc);

public:
  // decaldef properties
  VName name;
  //VName pic;
  int texid;
  int id;
  float scaleX, scaleY;
  int flipX, flipY; // FlipXXX constant
  float alpha; // decal alpha
  float addAlpha; // alpha for additive translucency (not supported yet)
  bool fuzzy; // draw decal with "fuzzy" effect (not supported yet)
  bool fullbright;
  VName lowername;
  VDecalAnim *animator; // decal animator (can be nullptr)
  // this is used in animators
  //float ofsX, ofsY;

public:
  VDecalDef () : next(nullptr), animname(NAME_None), name(NAME_None), texid(-1)/*pic(NAME_None)*/, id(-1), scaleX(1), scaleY(1), flipX(FlipNone), flipY(FlipNone), alpha(1), addAlpha(0), fuzzy(false), fullbright(false), lowername(NAME_None), animator(nullptr) {}
  ~VDecalDef ();

public:
  static VDecalDef *find (const VStr &aname);
  static VDecalDef *find (const VName &aname);
  static VDecalDef *findById (int id);

  static VDecalDef *getDecal (const VStr &aname);
  static VDecalDef *getDecal (const VName &aname);
  static VDecalDef *getDecalById (int id);

  static bool hasDecal (const VName &aname);

private:
  static VDecalDef *listHead;

  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
  friend class VDecalGroup;
};


// ////////////////////////////////////////////////////////////////////////// //
// will choose a random decal
class VDecalGroup {
private:
  VDecalGroup *next; // in decalDefHead

public:
  struct NameListItem {
    VName name;
    vuint16 weight;

    NameListItem () : name(NAME_None), weight(0) {}
    NameListItem (const VName &aname, vuint16 aweight) : name(aname), weight(aweight) {}
  };

  struct ListItem {
    VDecalDef *dd;
    VDecalGroup *dg;

    ListItem () : dd(nullptr), dg(nullptr) {}
    ListItem (VDecalDef *add, VDecalGroup *adg) : dd(add), dg(adg) {}
  };

private:
  static void addToList (VDecalGroup *dg);
  static void removeFromList (VDecalGroup *dg);

  void fixup ();

public:
  // name is not parsed yet
  bool parse (VScriptParser *sc);

public:
  // decaldef properties
  VName name;
  TArray<NameListItem> nameList; // can be empty in cloned/loaded object
  //FIXME: it can refer another decal group
  TWeightedList</*VDecalDef*/ListItem*> list; // can contain less items than `nameList`

public:
  VDecalGroup () : next(nullptr), name(NAME_None), nameList(), list() {}
  ~VDecalGroup () {}

  VDecalDef *chooseDecal (int reclevel=0);

public:
  static VDecalGroup *find (const VStr &aname);
  static VDecalGroup *find (const VName &aname);

private:
  static VDecalGroup *listHead;

  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
  friend class VDecalDef;
};


// ////////////////////////////////////////////////////////////////////////// //
// base decal animator class
class VDecalAnim {
public:
  enum { TypeId = 0 };

private:
  VDecalAnim *next; // animDefHead

private:
  static void addToList (VDecalAnim *anim);
  static void removeFromList (VDecalAnim *anim);

protected:
  // working data
  float timePassed;

protected:
  virtual vuint8 getTypeId () const { return VDecalAnim::TypeId; }
  virtual void doIO (VStream &Strm) = 0;
  virtual void fixup ();

public:
  virtual bool parse (VScriptParser *sc) = 0;

public:
  // decaldef properties
  VName name;

public:
  VDecalAnim () : next(nullptr), timePassed(0), name(NAME_None) {}
  virtual ~VDecalAnim ();

  // this does deep clone, so we can attach it to the actual decal object
  virtual VDecalAnim *clone () = 0;

  // return `false` to stop continue animation; set decal alpha to 0 (or negative) to remove decal on next cleanup
  virtual bool animate (decal_t *decal, float timeDelta) = 0;

  static void Serialise (VStream &Strm, VDecalAnim *&aptr);

public:
  static VDecalAnim *find (const VStr &aname);
  static VDecalAnim *find (const VName &aname);

private:
  static VDecalAnim *listHead;

  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
};


// ////////////////////////////////////////////////////////////////////////// //
class VDecalAnimFader : public VDecalAnim {
public:
  enum { TypeId = 1 };

public:
  // animator properties
  float startTime, actionTime; // in seconds

protected:
  virtual vuint8 getTypeId () const override { return VDecalAnimFader::TypeId; }
  virtual void doIO (VStream &Strm) override;

public:
  virtual bool parse (VScriptParser *sc) override;

public:
  VDecalAnimFader () : VDecalAnim(), startTime(0), actionTime(0) {}
  virtual ~VDecalAnimFader ();

  // this does deep clone, so we can attach it to the actual decal object
  virtual VDecalAnim *clone () override;

  virtual bool animate (decal_t *decal, float timeDelta) override;

  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
};


// ////////////////////////////////////////////////////////////////////////// //
class VDecalAnimStretcher : public VDecalAnim {
public:
  enum { TypeId = 2 };

public:
  // animator properties
  float goalX, goalY;
  float startTime, actionTime; // in seconds

protected:
  virtual vuint8 getTypeId () const override { return VDecalAnimStretcher::TypeId; }
  virtual void doIO (VStream &Strm) override;

public:
  virtual bool parse (VScriptParser *sc) override;

public:
  VDecalAnimStretcher () : VDecalAnim(), goalX(1), goalY(1), startTime(0), actionTime(0) {}
  virtual ~VDecalAnimStretcher ();

  // this does deep clone, so we can attach it to the actual decal object
  virtual VDecalAnim *clone () override;

  virtual bool animate (decal_t *decal, float timeDelta) override;

  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
};


// ////////////////////////////////////////////////////////////////////////// //
class VDecalAnimSlider : public VDecalAnim {
public:
  enum { TypeId = 3 };

public:
  // animator properties
  float distX, distY;
  float startTime, actionTime; // in seconds

protected:
  virtual vuint8 getTypeId () const override { return VDecalAnimSlider::TypeId; }
  virtual void doIO (VStream &Strm) override;

public:
  virtual bool parse (VScriptParser *sc) override;

public:
  VDecalAnimSlider () : VDecalAnim(), distX(0), distY(0), startTime(0), actionTime(0) {}
  virtual ~VDecalAnimSlider ();

  // this does deep clone, so we can attach it to the actual decal object
  virtual VDecalAnim *clone () override;

  virtual bool animate (decal_t *decal, float timeDelta) override;

  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
};


// ////////////////////////////////////////////////////////////////////////// //
class VDecalAnimColorChanger : public VDecalAnim {
public:
  enum { TypeId = 4 };

public:
  // animator properties
  float dest[3];
  float startTime, actionTime; // in seconds

protected:
  virtual vuint8 getTypeId () const override { return VDecalAnimColorChanger::TypeId; }
  virtual void doIO (VStream &Strm) override;

public:
  virtual bool parse (VScriptParser *sc) override;

public:
  VDecalAnimColorChanger () : VDecalAnim(), startTime(0), actionTime(0) { dest[0] = dest[1] = dest[2] = 0; }
  virtual ~VDecalAnimColorChanger ();

  // this does deep clone, so we can attach it to the actual decal object
  virtual VDecalAnim *clone () override;

  virtual bool animate (decal_t *decal, float timeDelta) override;

  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
};


// ////////////////////////////////////////////////////////////////////////// //
class VDecalAnimCombiner : public VDecalAnim {
public:
  enum { TypeId = 5 };

private:
  bool mIsCloned;

protected:
  virtual vuint8 getTypeId () const override { return VDecalAnimCombiner::TypeId; }
  virtual void doIO (VStream &Strm) override;

public:
  // animator properties
  TArray<VName> nameList; // can be empty in cloned/loaded object
  TArray<VDecalAnim *> list; // can contain less items than `nameList`

protected:
  virtual void fixup () override;

public:
  virtual bool parse (VScriptParser *sc) override;

public:
  VDecalAnimCombiner () : VDecalAnim(), mIsCloned(false), nameList(), list() {}
  virtual ~VDecalAnimCombiner ();

  // this does deep clone, so we can attach it to the actual decal object
  virtual VDecalAnim *clone () override;

  virtual bool animate (decal_t *decal, float timeDelta) override;

  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
};


// ////////////////////////////////////////////////////////////////////////// //
void ParseDecalDef (VScriptParser *sc);

void ProcessDecalDefs ();
