//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**  Copyright (C) 2018 Ketmar Dark
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
// ////////////////////////////////////////////////////////////////////////// //
// OPTIMIZER
//

// ////////////////////////////////////////////////////////////////////////// //
class VMCOptimiser; // well, `MC` stands for `machine code`, lol (k8)


// ////////////////////////////////////////////////////////////////////////// //
// this is used to hold original instruction, and various extra optimizer data
struct Instr {
  // optimizer will fill those
  VMCOptimiser *owner;
  Instr *prev, *next; // in main list
  Instr *jpprev, *jpnext; // in "jump" list
  // fields from the original
  vint32 Address;
  vint32 Opcode;
  vint32 Arg1;
  vint32 Arg2;
  VMemberBase *Member;
  VName NameArg;
  VFieldType TypeArg;
  TLocation loc;
  // copied from statement info list
  int opcArgType;
  // optimiser internal data
  bool meJumpTarget; // `true` if this instr is a jump target

  Instr (VMCOptimiser *aowner, const FInstruction &i)
    : owner(aowner)
    , prev(nullptr), next(nullptr)
    , jpprev(nullptr), jpnext(nullptr)
    , Address(i.Address)
    , Opcode(i.Opcode)
    , Arg1(i.Arg1)
    , Arg2(i.Arg2)
    , Member(i.Member)
    , NameArg(i.NameArg)
    , TypeArg(i.TypeArg)
    , loc(i.loc)
    , meJumpTarget(false)
  {
    opcArgType = StatementInfo[Opcode].Args;
  }

  inline void copyTo (FInstruction &dest) const {
    //dest.Address = Address; // no need to do this
    dest.Address = 0;
    dest.Opcode = Opcode;
    dest.Arg1 = Arg1;
    dest.Arg2 = Arg2;
    dest.Member = Member;
    dest.NameArg = NameArg;
    dest.TypeArg = TypeArg;
    dest.loc = loc;
  }

  inline bool isAnyBranch () const {
    switch (opcArgType) {
      case OPCARGS_BranchTargetB:
      case OPCARGS_BranchTargetNB:
      case OPCARGS_BranchTargetS:
      case OPCARGS_BranchTarget:
      case OPCARGS_ByteBranchTarget:
      case OPCARGS_ShortBranchTarget:
      case OPCARGS_IntBranchTarget:
       return true;
    }
    return false;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
// main optimizer class
class VMCOptimiser {
public:
  TArray<FInstruction> *origInstrList;
  // instructions list
  Instr *ilistHead, *ilistTail;
  // all known jump instructions
  Instr *jplistHead, *jplistTail;

private:
  int countInstrs () const;

  inline void appendToList (Instr *i) {
    if (!i) return; // jist in case
    i->next = nullptr;
    i->prev = ilistTail;
    if (ilistTail) ilistTail->next = i; else ilistHead = i;
    ilistTail = i;
  }

  inline void appendToJPList (Instr *i) {
    if (!i) return; // jist in case
    i->jpnext = nullptr;
    i->jpprev = jplistTail;
    if (jplistTail) jplistTail->jpnext = i; else jplistHead = i;
    jplistTail = i;
  }

  /*
  void removeToList (Instr *&head, Instr *&tail) {
    prev = tail;
    if (tail) tail->next = this; else head = this;
    tail = this;
  }
  */

public:
  VMCOptimiser (TArray<FInstruction> &aorig);
  ~VMCOptimiser ();

  void clear ();

  void setupFrom (TArray<FInstruction> *aorig);

  // this will copy result back to `aorig`, and will clear everything
  void finish ();

  void optimiseAll ();

protected:
  void optimiseLoads ();
  void optimiseJumps ();
};


// ////////////////////////////////////////////////////////////////////////// //
VMCOptimiser::VMCOptimiser (TArray<FInstruction> &aorig)
  : origInstrList(nullptr)
  , ilistHead(nullptr), ilistTail(nullptr)
  , jplistHead(nullptr), jplistTail(nullptr)
{
  setupFrom(&aorig);
}


VMCOptimiser::~VMCOptimiser () {
  clear();
}


void VMCOptimiser::clear () {
  origInstrList = nullptr;
  Instr *c = ilistHead;
  while (c) {
    Instr *x = c;
    c = c->next;
    delete x;
  }
  ilistHead = ilistTail = nullptr;
  jplistHead = jplistTail = nullptr;
}


void VMCOptimiser::setupFrom (TArray<FInstruction> *aorig) {
  clear();
  origInstrList = aorig;
  TArray<FInstruction> &olist = *aorig;
  // build ilist
  for (int f = 0; f < olist.length(); ++f) {
    // done must be the last one, and we aren't interested in it at all
    if (olist[f].Opcode == OPC_Done) {
      if (f != olist.length()-1) FatalError("VCOPT: OPC_Done is not a last one");
      break;
    }
    auto i = new Instr(this, olist[f]);
    appendToList(i);
    // append to jplist (if necessary)
    if (i->isAnyBranch()) appendToJPList(i);
  }
}


int VMCOptimiser::countInstrs () const {
  int res = 0;
  for (const Instr *it = ilistHead; it; it = it->next) ++res;
  return res;
}


void VMCOptimiser::finish () {
  TArray<FInstruction> &olist = *origInstrList;
  olist.setLength(countInstrs()+1); // one for `Done`
  int iofs = 0;
  for (Instr *it = ilistHead; it; it = it->next, ++iofs) it->copyTo(olist[iofs]);
  // append `Done`
  olist[iofs].Address = 0;
  olist[iofs].Opcode = OPC_Done;
  olist[iofs].Arg1 = 0;
  olist[iofs].Arg2 = 0;
  olist[iofs].Member = nullptr;
  olist[iofs].NameArg = NAME_None;
  olist[iofs].TypeArg = VFieldType(TYPE_Void);
  if (iofs > 0) {
    olist[iofs].loc = olist[iofs-1].loc;
  } else {
    olist[iofs].loc = TLocation();
  }
  // done
  clear();
}


// ////////////////////////////////////////////////////////////////////////// //
void VMCOptimiser::optimiseAll () {
  optimiseLoads(); // should be last
  optimiseJumps();
}


// ////////////////////////////////////////////////////////////////////////// //
// optimise various load/call instructions
void VMCOptimiser::optimiseLoads () {
  for (Instr *it = ilistHead; it; it = it->next) {
    Instr &insn = *it;
    switch (insn.Opcode) {
      case OPC_PushVFunc:
        // make sure class virtual table has been calculated
        insn.Member->Outer->PostLoad();
        if (((VMethod *)insn.Member)->VTableIndex < 256) insn.Opcode = OPC_PushVFuncB;
        break;
      case OPC_VCall:
        // make sure class virtual table has been calculated
        insn.Member->Outer->PostLoad();
        if (((VMethod *)insn.Member)->VTableIndex < 256) insn.Opcode = OPC_VCallB;
        break;
      case OPC_DelegateCall:
        // make sure struct / class field offsets have been calculated
        insn.Member->Outer->PostLoad();
             if (((VField *)insn.Member)->Ofs < 256) insn.Opcode = OPC_DelegateCallB;
        else if (((VField *)insn.Member)->Ofs <= MAX_VINT16) insn.Opcode = OPC_DelegateCallS;
        break;
      case OPC_Offset:
      case OPC_FieldValue:
      case OPC_VFieldValue:
      case OPC_PtrFieldValue:
      case OPC_StrFieldValue:
      case OPC_SliceFieldValue:
      case OPC_ByteFieldValue:
      case OPC_Bool0FieldValue:
      case OPC_Bool1FieldValue:
      case OPC_Bool2FieldValue:
      case OPC_Bool3FieldValue:
        // no short form for slices
        if (insn.Opcode != OPC_SliceFieldValue) {
          // make sure struct / class field offsets have been calculated
          insn.Member->Outer->PostLoad();
               if (((VField *)insn.Member)->Ofs < 256) insn.Opcode += 2;
          else if (((VField *)insn.Member)->Ofs <= MAX_VINT16) ++insn.Opcode;
        }
        break;
      case OPC_ArrayElement:
             if (insn.TypeArg.GetSize() < 256) insn.Opcode = OPC_ArrayElementB;
        else if (insn.TypeArg.GetSize() < MAX_VINT16) insn.Opcode = OPC_ArrayElementS;
        break;
      case OPC_PushName:
             if (insn.NameArg.GetIndex() < 256) insn.Opcode = OPC_PushNameB;
        else if (insn.NameArg.GetIndex() < MAX_VINT16) insn.Opcode = OPC_PushNameS;
        break;
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// optimise various branch instructions (convert to a short form, if we can)
void VMCOptimiser::optimiseJumps () {
  // calculate approximate addresses for jump instructions
  int addr = 0;
  TArray<int> iaddrs; // to avoid constant list searches
  for (Instr *it = ilistHead; it; it = it->next) {
    iaddrs.append(addr);
    Instr &insn = *it;
    insn.Address = addr;
    addr += 1; // opcode itself
    switch (insn.opcArgType) {
      case OPCARGS_Member:
      case OPCARGS_String:
        addr += sizeof(void *);
        break;
      case OPCARGS_BranchTargetB:
      case OPCARGS_BranchTargetNB:
      case OPCARGS_Byte:
      case OPCARGS_NameB:
      case OPCARGS_FieldOffsetB:
      case OPCARGS_VTableIndexB:
      case OPCARGS_TypeSizeB:
        addr += 1;
        break;
      case OPCARGS_BranchTargetS:
      case OPCARGS_Short:
      case OPCARGS_NameS:
      case OPCARGS_FieldOffsetS:
      case OPCARGS_VTableIndex:
      case OPCARGS_VTableIndexB_Byte:
      case OPCARGS_FieldOffsetB_Byte:
      case OPCARGS_TypeSizeS:
        addr += 2;
        break;
      case OPCARGS_ByteBranchTarget:
      case OPCARGS_VTableIndex_Byte:
      case OPCARGS_FieldOffsetS_Byte:
        addr += 3;
        break;
      case OPCARGS_BranchTarget:
      case OPCARGS_ShortBranchTarget:
      case OPCARGS_Int:
      case OPCARGS_Name:
      case OPCARGS_FieldOffset:
      case OPCARGS_TypeSize:
        addr += 4;
        break;
      case OPCARGS_FieldOffset_Byte:
        addr += 5;
        break;
      case OPCARGS_IntBranchTarget:
        addr += 6;
        break;
      case OPCARGS_Type:
        addr += 8+sizeof(void*);
        break;
      case OPCARGS_Builtin:
        addr += 1;
        break;
    }
  }

  // now optimize jump instructions
  for (Instr *it = ilistHead; it; it = it->next) {
    Instr &insn = *it;
    if (insn.Opcode != OPC_IteratorDtorAt && insn.opcArgType == OPCARGS_BranchTarget && insn.Arg1 < iaddrs.length()) {
      vint32 ofs = iaddrs[insn.Arg1]-insn.Address;
           if (ofs >= 0 && ofs < 256) insn.Opcode -= 3;
      else if (ofs < 0 && ofs > -256) insn.Opcode -= 2;
      else if (ofs >= MIN_VINT16 && ofs <= MAX_VINT16) insn.Opcode -= 1;
    }
  }
}
