//**************************************************************************
//**
//**	##   ##    ##    ##   ##   ####     ####   ###     ###
//**	##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**	 ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**	 ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**	  ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**	   #    ##    ##    #      ####     ####   ##       ##
//**
//**	$Id$
//**
//**	Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**	This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**	This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#ifdef IN_VCC
#include "../utils/vcc/vcc.h"
#else
#include "gamedefs.h"
#include "progdefs.h"
#endif

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

#ifndef IN_VCC
class DummyClass1 : public VVirtualObjectBase
{
public:
	void*		Pointer;
	vuint8		Byte1;
	virtual void Dummy() = 0;
};

class DummyClass2 : public DummyClass1
{
public:
	vuint8		Byte2;
};
#endif

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

#ifndef IN_VCC
TArray<mobjinfo_t>		VClass::GMobjInfos;
TArray<mobjinfo_t>		VClass::GScriptIds;
TArray<VName>			VClass::GSpriteNames;
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
//	VClass::VClass
//
//==========================================================================

#ifndef IN_VCC
VClass::VClass(VName AName)
: VMemberBase(MEMBER_Class, AName)
, ObjectFlags(0)
, LinkNext(0)
, ParentClass(0)
, ClassSize(0)
, ClassUnalignedSize(0)
, ClassFlags(0)
, ClassVTable(0)
, ClassConstructor(0)
, ClassNumMethods(0)
, Fields(0)
, ReferenceFields(0)
, DestructorFields(0)
, NetFields(0)
, NetMethods(0)
, States(0)
, DefaultProperties(0)
, Defaults(0)
, NetId(-1)
, NetStates(0)
, NumNetFields(0)
, Replacement(NULL)
, Replacee(NULL)
{
	guard(VClass::VClass);
	LinkNext = GClasses;
	GClasses = this;
	unguard;
}
#else
VClass::VClass(VName InName, VMemberBase* InOuter, TLocation InLoc)
: VMemberBase(MEMBER_Class, InName, InOuter, InLoc)
, ParentClass(NULL)
, Fields(NULL)
, States(NULL)
, DefaultProperties(NULL)
, ParentClassName(NAME_None)
, Modifiers(0)
, GameExpr(NULL)
, MobjInfoExpr(NULL)
, ScriptIdExpr(NULL)
, Defined(true)
{
}
#endif

//==========================================================================
//
//	VClass::VClass
//
//==========================================================================

#ifndef IN_VCC
VClass::VClass(ENativeConstructor, size_t ASize, vuint32 AClassFlags, 
	VClass *AParent, EName AName, void(*ACtor)())
: VMemberBase(MEMBER_Class, AName)
, ObjectFlags(CLASSOF_Native)
, LinkNext(0)
, ParentClass(AParent)
, ClassSize(ASize)
, ClassUnalignedSize(ASize)
, ClassFlags(AClassFlags)
, ClassVTable(0)
, ClassConstructor(ACtor)
, ClassNumMethods(0)
, Fields(0)
, ReferenceFields(0)
, DestructorFields(0)
, NetFields(0)
, NetMethods(0)
, States(0)
, DefaultProperties(0)
, Defaults(0)
, NetId(-1)
, NetStates(0)
, NumNetFields(0)
, Replacement(NULL)
, Replacee(NULL)
{
	guard(native VClass::VClass);
	LinkNext = GClasses;
	GClasses = this;
	unguard;
}
#endif

//==========================================================================
//
//	VClass::~VClass
//
//==========================================================================

VClass::~VClass()
{
	guard(VClass::~VClass);
#ifndef IN_VCC
	if (ClassVTable)
	{
		delete[] ClassVTable;
	}
	if (Defaults)
	{
		DestructObject((VObject*)Defaults);
		delete[] Defaults;
	}

	if (!GObjInitialised)
	{
		return;
	}
	//	Unlink from classes list.
	if (GClasses == this)
	{
		GClasses = LinkNext;
	}
	else
	{
		VClass* Prev = GClasses;
		while (Prev && Prev->LinkNext != this)
		{
			Prev = Prev->LinkNext;
		}
		if (Prev)
		{
			Prev->LinkNext = LinkNext;
		}
		else
		{
			GCon->Log(NAME_Dev, "VClass Unlink: Class not in list");
		}
	}
#else
	if (MobjInfoExpr)
		delete MobjInfoExpr;
	if (ScriptIdExpr)
		delete ScriptIdExpr;
#endif
	unguard;
}

//==========================================================================
//
//	VClass::Serialise
//
//==========================================================================

void VClass::Serialise(VStream& Strm)
{
	guard(VClass::Serialise);
	VMemberBase::Serialise(Strm);
#ifndef IN_VCC
	VClass* PrevParent = ParentClass;
#endif
	Strm << ParentClass
		<< Fields
		<< States
		<< DefaultProperties;
#ifndef IN_VCC
	if ((ObjectFlags & CLASSOF_Native) && ParentClass != PrevParent)
	{
		Sys_Error("Bad parent class, class %s, C++ %s, VavoomC %s)",
			GetName(), PrevParent ? PrevParent->GetName() : "(none)",
			ParentClass ? ParentClass->GetName() : "(none)");
	}
	if (Strm.IsLoading())
	{
		NetStates = States;
	}
#endif

	int NumRepInfos = RepInfos.Num();
	Strm << STRM_INDEX(NumRepInfos);
	if (Strm.IsLoading())
	{
		RepInfos.SetNum(NumRepInfos);
	}
	for (int i = 0; i < RepInfos.Num(); i++)
	{
		Strm << RepInfos[i].Cond;
#ifndef IN_VCC
		int NumRepMembers = RepInfos[i].RepMembers.Num();
		Strm << STRM_INDEX(NumRepMembers);
		if (Strm.IsLoading())
		{
			RepInfos[i].RepMembers.SetNum(NumRepMembers);
		}
		for (int j = 0; j < RepInfos[i].RepMembers.Num(); j++)
		{
			Strm << RepInfos[i].RepMembers[j];
		}
#else
		int NumRepFields = RepInfos[i].RepFields.Num();
		Strm << STRM_INDEX(NumRepFields);
		if (Strm.IsLoading())
		{
			RepInfos[i].RepFields.SetNum(NumRepFields);
		}
		for (int j = 0; j < RepInfos[i].RepFields.Num(); j++)
		{
			Strm << RepInfos[i].RepFields[j].Member;
		}
#endif
	}

	int NumStateLabels = StateLabels.Num();
	Strm << STRM_INDEX(NumStateLabels);
	if (Strm.IsLoading())
	{
		StateLabels.SetNum(NumStateLabels);
	}
	for (int i = 0; i < StateLabels.Num(); i++)
	{
		Strm << StateLabels[i];
	}
	unguard;
}

#ifdef IN_VCC

//==========================================================================
//
//	VClass::AddConstant
//
//==========================================================================

void VClass::AddConstant(VConstant* c)
{
	Constants.Append(c);
}

//==========================================================================
//
//	VClass::AddField
//
//==========================================================================

void VClass::AddField(VField* f)
{
	if (!Fields)
		Fields = f;
	else
	{
		VField* Prev = Fields;
		while (Prev->Next)
			Prev = Prev->Next;
		Prev->Next = f;
	}
	f->Next = NULL;
}

//==========================================================================
//
//	VClass::AddProperty
//
//==========================================================================

void VClass::AddProperty(VProperty* p)
{
	Properties.Append(p);
}

//==========================================================================
//
//	VClass::AddState
//
//==========================================================================

void VClass::AddState(VState* s)
{
	if (!States)
		States = s;
	else
	{
		VState* Prev = States;
		while (Prev->Next)
			Prev = Prev->Next;
		Prev->Next = s;
	}
	s->Next = NULL;
}

//==========================================================================
//
//	VClass::AddMethod
//
//==========================================================================

void VClass::AddMethod(VMethod* m)
{
	Methods.Append(m);
}

//==========================================================================
//
//	CheckForFunction
//
//==========================================================================

VMethod* VClass::CheckForFunction(VName Name)
{
	if (Name == NAME_None)
	{
		return NULL;
	}
	return (VMethod*)StaticFindMember(Name, this, MEMBER_Method);
}

//==========================================================================
//
//	VClass::CheckForMethod
//
//==========================================================================

VMethod* VClass::CheckForMethod(VName Name)
{
	if (Name == NAME_None)
	{
		return NULL;
	}
	VMethod* M = (VMethod*)StaticFindMember(Name, this, MEMBER_Method);
	if (M)
	{
		return M;
	}
	if (ParentClass)
	{
		return ParentClass->CheckForMethod(Name);
	}
	return NULL;
}

//==========================================================================
//
//	VClass::CheckForConstant
//
//==========================================================================

VConstant* VClass::CheckForConstant(VName Name)
{
	VMemberBase* m = StaticFindMember(Name, this, MEMBER_Const);
	if (m)
	{
		return (VConstant*)m;
	}
	if (ParentClass)
	{
		return ParentClass->CheckForConstant(Name);
	}
	return NULL;
}

//==========================================================================
//
//	VClass::CheckForField
//
//==========================================================================

VField* VClass::CheckForField(TLocation l, VName Name, VClass* SelfClass, bool CheckPrivate)
{
	if (Name == NAME_None)
	{
		return NULL;
	}
	for (VField *fi = Fields; fi; fi = fi->Next)
	{
		if (Name == fi->Name)
		{
			if (CheckPrivate && fi->Flags & FIELD_Private &&
				this != SelfClass)
			{
				ParseError(l, "Field %s is private", *fi->Name);
			}
			return fi;
		}
	}
	if (ParentClass)
	{
		return ParentClass->CheckForField(l, Name, SelfClass, CheckPrivate);
	}
	return NULL;
}

//==========================================================================
//
//	VClass::CheckForProperty
//
//==========================================================================

VProperty* VClass::CheckForProperty(VName Name)
{
	if (Name == NAME_None)
	{
		return NULL;
	}
	VProperty* P = (VProperty*)StaticFindMember(Name, this, MEMBER_Property);
	if (P)
	{
		return P;
	}
	if (ParentClass)
	{
		return ParentClass->CheckForProperty(Name);
	}
	return NULL;
}

//==========================================================================
//
//	VClass::Define
//
//==========================================================================

bool VClass::Define()
{
	Modifiers = TModifiers::Check(Modifiers, AllowedModifiers, Loc);
	int ClassAttr = TModifiers::ClassAttr(Modifiers);

	if (ParentClassName != NAME_None)
	{
		ParentClass = CheckForClass(ParentClassName);
		if (!ParentClass)
		{
			ParseError(ParentClassLoc, "No such class %s", *ParentClassName);
		}
		else if (!ParentClass->Defined)
		{
			ParseError(ParentClassLoc, "Parent class must be defined before");
		}
	}

	for (int i = 0; i < Structs.Num(); i++)
	{
		if (!Structs[i]->Define())
		{
			return false;
		}
	}

	int GameFilter = 0;
	if (GameExpr)
	{
		VEmitContext ec(this);
		GameExpr = GameExpr->Resolve(ec);
		if (!GameExpr)
		{
			return false;
		}
		if (!GameExpr->IsIntConst())
		{
			ParseError(GameExpr->Loc, "Integer constant expected");
			return false;
		}
		GameFilter = GameExpr->GetIntConst();
	}

	if (MobjInfoExpr)
	{
		VEmitContext ec(this);
		MobjInfoExpr = MobjInfoExpr->Resolve(ec);
		if (!MobjInfoExpr)
		{
			return false;
		}
		if (!MobjInfoExpr->IsIntConst())
		{
			ParseError(MobjInfoExpr->Loc, "Integer constant expected");
			return false;
		}
		mobjinfo_t& mi = ec.Package->MobjInfo.Alloc();
		mi.DoomEdNum = MobjInfoExpr->GetIntConst();
		mi.GameFilter = GameFilter;
		mi.Class = this;
	}

	if (ScriptIdExpr)
	{
		VEmitContext ec(this);
		ScriptIdExpr = ScriptIdExpr->Resolve(ec);
		if (!ScriptIdExpr)
		{
			return false;
		}
		if (!ScriptIdExpr->IsIntConst())
		{
			ParseError(ScriptIdExpr->Loc, "Integer constant expected");
			return false;
		}
		mobjinfo_t& mi = ec.Package->ScriptIds.Alloc();
		mi.DoomEdNum = ScriptIdExpr->GetIntConst();
		mi.GameFilter = GameFilter;
		mi.Class = this;
	}

	Defined = true;
	return true;
}

//==========================================================================
//
//	VClass::DefineMembers
//
//==========================================================================

bool VClass::DefineMembers()
{
	bool Ret = true;

	for (int i = 0; i < Constants.Num(); i++)
	{
		if (!Constants[i]->Define())
		{
			Ret = false;
		}
	}

	for (int i = 0; i < Structs.Num(); i++)
	{
		Structs[i]->DefineMembers();
	}

	VField* PrevBool = NULL;
	for (VField* fi = Fields; fi; fi = fi->Next)
	{
		if (!fi->Define())
		{
			Ret = false;
		}
		if (fi->Type.Type == TYPE_Bool && PrevBool && PrevBool->Type.BitMask != 0x80000000)
		{
			fi->Type.BitMask = PrevBool->Type.BitMask << 1;
		}
		PrevBool = fi->Type.Type == TYPE_Bool ? fi : NULL;
	}

	for (int i = 0; i < Properties.Num(); i++)
	{
		if (!Properties[i]->Define())
		{
			Ret = false;
		}
	}

	for (int i = 0; i < Methods.Num(); i++)
	{
		if (!Methods[i]->Define())
		{
			Ret = false;
		}
	}

	if (!DefaultProperties->Define())
	{
		Ret = false;
	}

	for (VState* s = States; s; s = s->Next)
	{
		if (!s->Define())
		{
			Ret = false;
		}
	}

	for (int ri = 0; ri < RepInfos.Num(); ri++)
	{
		if (!RepInfos[ri].Cond->Define())
		{
			Ret = false;
		}
		TArray<VRepField>& RepFields = RepInfos[ri].RepFields;
		for (int i = 0; i < RepFields.Num(); i++)
		{
			VField* RepField = NULL;
			for (VField* F = Fields; F; F = F->Next)
			{
				if (F->Name == RepFields[i].Name)
				{
					RepField = F;
					break;
				}
			}
			if (RepField)
			{
				if (RepField->Flags & FIELD_Net)
				{
					ParseError(RepFields[i].Loc, "Field %s has multiple replication statements",
						*RepFields[i].Name);
					continue;
				}
				RepField->Flags |= FIELD_Net;
				RepField->ReplCond = RepInfos[ri].Cond;
				RepFields[i].Member = RepField;
				continue;
			}

			VMethod* RepMethod = NULL;
			for (int mi = 0; mi < Methods.Num(); mi++)
			{
				if (Methods[mi]->Name == RepFields[i].Name)
				{
					RepMethod = Methods[mi];
					break;
				}
			}
			if (RepMethod)
			{
				if (RepMethod->SuperMethod)
				{
					ParseError(RepFields[i].Loc, "Method %s is overloaded in this class",
						*RepFields[i].Name);
					continue;
				}
				if (RepMethod->Flags & FUNC_Net)
				{
					ParseError(RepFields[i].Loc, "Method %s has multiple replication statements",
						*RepFields[i].Name);
					continue;
				}
				RepMethod->Flags |= FUNC_Net;
				RepMethod->ReplCond = RepInfos[ri].Cond;
				if (RepInfos[ri].Reliable)
					RepMethod->Flags |= FUNC_NetReliable;
				RepFields[i].Member = RepMethod;
				continue;
			}

			ParseError(RepFields[i].Loc, "No such field or method %s", *RepFields[i].Name);
		}
	}

	return Ret;
}

//==========================================================================
//
//	VClass::Emit
//
//==========================================================================

void VClass::Emit()
{
	//	Emit method code.
	for (int i = 0; i < Methods.Num(); i++)
	{
		Methods[i]->Emit();
	}

	//	Emit code of the state methods.
	for (VState* s = States; s; s = s->Next)
	{
		s->Emit();
	}

	for (int i = 0; i < StateLabels.Num(); i++)
	{
		VStateLabel& Lbl = StateLabels[i];
		if (Lbl.GotoLabel != NAME_None)
		{
			Lbl.State = ResolveStateLabel(Lbl.Loc, Lbl.GotoLabel, Lbl.GotoOffset);
		}
	}

	//	Emit code of the network replication conditions.
	for (int ri = 0; ri < RepInfos.Num(); ri++)
	{
		RepInfos[ri].Cond->Emit();
	}

	DefaultProperties->Emit();
}

//==========================================================================
//
//	VClass::CheckForStateLabel
//
//==========================================================================

VStateLabel* VClass::CheckForStateLabel(VName LblName, bool CheckParent)
{
	for (int i = 0; i < StateLabels.Num(); i++)
	{
		if (StateLabels[i].Name == LblName)
		{
			return &StateLabels[i];
		}
	}
	if (CheckParent && ParentClass)
	{
		return ParentClass->CheckForStateLabel(LblName);
	}
	return NULL;
}

//==========================================================================
//
//	VClass::ResolveStateLabel
//
//==========================================================================

VState* VClass::ResolveStateLabel(TLocation Loc, VName LabelName, int Offset)
{
	VClass* CheckClass = this;
	VName CheckName = LabelName;

	const char* DCol = strstr(*LabelName, "::");
	if (DCol)
	{
		char ClassNameBuf[NAME_SIZE];
		VStr::Cpy(ClassNameBuf, *LabelName);
		ClassNameBuf[DCol - *LabelName] = 0;
		VName ClassName(ClassNameBuf);
		if (ClassName == NAME_Super)
		{
			CheckClass = ParentClass;
		}
		else
		{
			CheckClass = CheckForClass(ClassName);
			if (!CheckClass)
			{
				ParseError(Loc, "No such class %s", *ClassName);
				return NULL;
			}
		}
		CheckName = DCol + 2;
	}

	VStateLabel* Lbl = CheckClass->CheckForStateLabel(CheckName, false);
	if (!Lbl)
	{
		ParseError(Loc, "No such state %s", *LabelName);
		return NULL;
	}

	VState* State = Lbl->State;
	int Count = Offset;
	while (Count--)
	{
		if (!State || !State->Next)
		{
			ParseError(Loc, "Bad jump offset");
			return NULL;
		}
		State = State->Next;
	}
	return State;
}

#else

//==========================================================================
//
//	VClass::Shutdown
//
//==========================================================================

void VClass::Shutdown()
{
	guard(VClass::Shutdown);
	if (ClassVTable)
	{
		delete[] ClassVTable;
		ClassVTable = NULL;
	}
	if (Defaults)
	{
		DestructObject((VObject*)Defaults);
		delete[] Defaults;
		Defaults = NULL;
	}
	StatesLookup.Clear();
	RepInfos.Clear();
	unguard;
}

//==========================================================================
//
//	VClass::FindClass
//
//==========================================================================

VClass *VClass::FindClass(const char *AName)
{
	VName TempName(AName, VName::Find);
	if (TempName == NAME_None)
	{
		// No such name, no chance to find a class
		return NULL;
	}
	for (VClass* Cls = GClasses; Cls; Cls = Cls->LinkNext)
	{
		if (Cls->GetVName() == TempName)
		{
			return Cls;
		}
	}
	return NULL;
}

//==========================================================================
//
//	VClass::FindSprite
//
//==========================================================================

int VClass::FindSprite(VName Name)
{
	guard(VClass::FindSprite);
	for (int i = 0; i < GSpriteNames.Num(); i++)
		if (GSpriteNames[i] == Name)
			return i;
	return GSpriteNames.Append(Name);
	unguard;
}

//==========================================================================
//
//	VClass::GetSpriteNames
//
//==========================================================================

void VClass::GetSpriteNames(TArray<FReplacedString>& List)
{
	guard(VClass::GetSpriteNames);
	for (int i = 0; i < GSpriteNames.Num(); i++)
	{
		FReplacedString&R = List.Alloc();
		R.Index = i;
		R.Replaced = false;
		R.Old = VStr(*GSpriteNames[i]).ToUpper();
	}
	unguard;
}

//==========================================================================
//
//	VClass::ReplaceSpriteNames
//
//==========================================================================

void VClass::ReplaceSpriteNames(TArray<FReplacedString>& List)
{
	guard(VClass::ReplaceSpriteNames);
	for (int i = 0; i < List.Num(); i++)
	{
		if (!List[i].Replaced)
		{
			continue;
		}
		GSpriteNames[List[i].Index] = *List[i].New.ToLower();
	}

	//	Update sprite names in states.
	for (int i = 0; i < VMemberBase::GMembers.Num(); i++)
	{
		if (GMembers[i] && GMembers[i]->MemberType == MEMBER_State)
		{
			VState* S = (VState*)GMembers[i];
			S->SpriteName = GSpriteNames[S->SpriteIndex];
		}
	}
	unguard;
}

//==========================================================================
//
//	VClass::StaticReinitStatesLookup
//
//==========================================================================

void VClass::StaticReinitStatesLookup()
{
	guard(VClass::StaticReinitStatesLookup);
	//	Clear states lookup tables.
	for (VClass* C = GClasses; C; C = C->LinkNext)
	{
		C->StatesLookup.Clear();
	}

	//	Now init states lookup tables again.
	for (VClass* C = GClasses; C; C = C->LinkNext)
	{
		C->InitStatesLookup();
	}
	unguard;
}

//==========================================================================
//
//	VClass::FindField
//
//==========================================================================

VField* VClass::FindField(VName AName)
{
	guard(VClass::FindField);
	for (VField* F = Fields; F; F = F->Next)
	{
		if (F->Name == AName)
		{
			return F;
		}
	}
	if (ParentClass)
	{
		return ParentClass->FindField(AName);
	}
	return NULL;
	unguard;
}

//==========================================================================
//
//	VClass::FindFieldChecked
//
//==========================================================================

VField* VClass::FindFieldChecked(VName AName)
{
	guard(VClass::FindFieldChecked);
	VField* F = FindField(AName);
	if (!F)
	{
		Sys_Error("Field %s not found", *AName);
	}
	return F;
	unguard;
}

//==========================================================================
//
//	VClass::FindFunction
//
//==========================================================================

VMethod *VClass::FindFunction(VName AName)
{
	guard(VClass::FindFunction);
	VMethod* M = (VMethod*)StaticFindMember(AName, this, MEMBER_Method);
	if (M)
	{
		return M;
	}
	if (ParentClass)
	{
		return ParentClass->FindFunction(AName);
	}
	return NULL;
	unguard;
}

//==========================================================================
//
//	VClass::FindFunctionChecked
//
//==========================================================================

VMethod *VClass::FindFunctionChecked(VName AName)
{
	guard(VClass::FindFunctionChecked);
	VMethod *func = FindFunction(AName);
	if (!func)
	{
		Sys_Error("Function %s not found", *AName);
	}
	return func;
	unguard;
}

//==========================================================================
//
//	VClass::GetFunctionIndex
//
//==========================================================================

int VClass::GetFunctionIndex(VName AName)
{
	guard(VClass::GetFunctionIndex);
	for (int i = 0; i < ClassNumMethods; i++)
	{
		if (ClassVTable[i]->Name == AName)
		{
			return i;
		}
	}
	return -1;
	unguard;
}

//==========================================================================
//
//	VClass::FindState
//
//==========================================================================

VState* VClass::FindState(VName AName)
{
	guard(VClass::FindState);
	for (VState* s = States; s; s = s->Next)
	{
		if (s->Name == AName)
		{
			return s;
		}
	}
	if (ParentClass)
	{
		return ParentClass->FindState(AName);
	}
	return NULL;
	unguard;
}

//==========================================================================
//
//	VClass::FindStateChecked
//
//==========================================================================

VState* VClass::FindStateChecked(VName AName)
{
	guard(VClass::FindStateChecked);
	VState* s = FindState(AName);
	if (!s)
	{
		Sys_Error("State %s not found", *AName);
	}
	return s;
	unguard;
}

//==========================================================================
//
//	VClass::FindStateLabel
//
//==========================================================================

VState* VClass::FindStateLabel(VName AName)
{
	guard(VClass::FindStateLabel);
	for (int i = 0; i < StateLabels.Num(); i++)
	{
		if (StateLabels[i].Name == AName)
		{
			return StateLabels[i].State;
		}
	}
	if (ParentClass)
	{
		return ParentClass->FindStateLabel(AName);
	}
	return NULL;
	unguard;
}

//==========================================================================
//
//	VClass::FindStateLabelChecked
//
//==========================================================================

VState* VClass::FindStateLabelChecked(VName AName)
{
	guard(VClass::FindStateLabelChecked);
	VState* s = FindStateLabel(AName);
	if (!s)
	{
		Sys_Error("State %s not found", *AName);
	}
	return s;
	unguard;
}

//==========================================================================
//
//	VClass::SetStateLabel
//
//==========================================================================

void VClass::SetStateLabel(VName AName, VState* State)
{
	guard(VClass::SetStateLabel);
	for (int i = 0; i < StateLabels.Num(); i++)
	{
		if (StateLabels[i].Name == AName)
		{
			StateLabels[i].State = State;
			return;
		}
	}
	VStateLabel& L = StateLabels.Alloc();
	L.Name = AName;
	L.State = State;
	unguard;
}

//==========================================================================
//
//	VClass::PostLoad
//
//==========================================================================

void VClass::PostLoad()
{
	if (ObjectFlags & CLASSOF_PostLoaded)
	{
		//	Already set up.
		return;
	}

	//	Make sure parent class has been set up.
	if (GetSuperClass())
	{
		GetSuperClass()->PostLoad();
	}

	//	Calculate field offsets and class size.
	CalcFieldOffsets();

	//	Initialise reference fields.
	InitReferences();

	//	Initialise destructor fields.
	InitDestructorFields();

	//	Initialise net fields.
	InitNetFields();

	//	Create virtual table.
	CreateVTable();

	//	Set up states lookup table.
	InitStatesLookup();

	//	Set state in-class indexes.
	int CurrIndex = 0;
	for (VState* S = NetStates; S; S = S->NetNext)
	{
		S->InClassIndex = CurrIndex++;
	}

	ObjectFlags |= CLASSOF_PostLoaded;
}

//==========================================================================
//
//	VClass::CalcFieldOffsets
//
//==========================================================================

void VClass::CalcFieldOffsets()
{
	guard(VClass::CalcFieldOffsets);
	//	Skip this for C++ only classes.
	if (!Outer && (ObjectFlags & CLASSOF_Native))
	{
		ClassNumMethods = ParentClass ? ParentClass->ClassNumMethods : 0;
		return;
	}

	int numMethods = ParentClass ? ParentClass->ClassNumMethods : 0;
	for (int i = 0; i < GMembers.Num(); i++)
	{
		if (GMembers[i]->MemberType != MEMBER_Method ||
			GMembers[i]->Outer != this)
		{
			continue;
		}
		VMethod* M = (VMethod*)GMembers[i];
		if (M == DefaultProperties)
		{
			M->VTableIndex = -1;
			continue;
		}
		int MOfs = -1;
		if (ParentClass)
		{
			MOfs = ParentClass->GetFunctionIndex(M->Name);
		}
		if (MOfs == -1 && !(M->Flags & FUNC_Final))
		{
			MOfs = numMethods++;
		}
		M->VTableIndex = MOfs;
	}

	VField* PrevField = NULL;
	int PrevSize = ClassSize;
	int size = 0;
	if (ParentClass)
	{
		//	GCC has a strange behavior of starting to add fields in subclasses
		// in a class that has virtual methods on unaligned parent size offset.
		// In other cases and in other compilers it starts on aligned parent
		// class size offset.
		if (sizeof(DummyClass1) == sizeof(DummyClass2))
			size = ParentClass->ClassUnalignedSize;
		else
			size = ParentClass->ClassSize;
	}
	for (VField* fi = Fields; fi; fi = fi->Next)
	{
		if (fi->Type.Type == TYPE_Bool && PrevField &&
			PrevField->Type.Type == TYPE_Bool &&
			PrevField->Type.BitMask != 0x80000000)
		{
			vuint32 bit_mask = PrevField->Type.BitMask << 1;
			if (fi->Type.BitMask != bit_mask)
				Sys_Error("Wrong bit mask");
			fi->Type.BitMask = bit_mask;
			fi->Ofs = PrevField->Ofs;
		}
		else
		{
			if (fi->Type.Type == TYPE_Struct ||
				(fi->Type.Type == TYPE_Array && fi->Type.ArrayInnerType == TYPE_Struct))
			{
				//	Make sure struct size has been calculated.
				fi->Type.Struct->PostLoad();
			}
			int FldAlign = fi->Type.GetAlignment();
			size = (size + FldAlign - 1) & ~(FldAlign - 1);
			fi->Ofs = size;
			size += fi->Type.GetSize();
		}
		PrevField = fi;
	}
	ClassUnalignedSize = size;
	size = (size + sizeof(void*) - 1) & ~(sizeof(void*) - 1);
	ClassSize = size;
	ClassNumMethods = numMethods;
	if ((ObjectFlags & CLASSOF_Native) && ClassSize != PrevSize)
	{
		Sys_Error("Bad class size, class %s, C++ %d, VavoomC %d)",
			GetName(), PrevSize, ClassSize);
	}
	unguard;
}

//==========================================================================
//
//	VClass::InitNetFields
//
//==========================================================================

void VClass::InitNetFields()
{
	guard(VClass::InitNetFields);
	if (ParentClass)
	{
		NetFields = ParentClass->NetFields;
		NetMethods = ParentClass->NetMethods;
		NumNetFields = ParentClass->NumNetFields;
	}

	for (VField* fi = Fields; fi; fi = fi->Next)
	{
		if (!(fi->Flags & FIELD_Net))
		{
			continue;
		}
		fi->NetIndex = NumNetFields++;
		fi->NextNetField = NetFields;
		NetFields = fi;
	}

	for (int i = 0; i < GMembers.Num(); i++)
	{
		if (GMembers[i]->MemberType != MEMBER_Method ||
			GMembers[i]->Outer != this)
		{
			continue;
		}
		VMethod* M = (VMethod*)GMembers[i];
		if (!(M->Flags & FUNC_Net))
		{
			continue;
		}
		VMethod* MPrev = NULL;
		if (ParentClass)
		{
			MPrev = ParentClass->FindFunction(M->Name);
		}
		if (MPrev)
		{
			M->NetIndex = MPrev->NetIndex;
		}
		else
		{
			M->NetIndex = NumNetFields++;
		}
		M->NextNetMethod = NetMethods;
		NetMethods = M;
	}
	unguard;
}

//==========================================================================
//
//	VClass::InitReferences
//
//==========================================================================

void VClass::InitReferences()
{
	guard(VClass::InitReferences);
	ReferenceFields = NULL;
	if (GetSuperClass())
	{
		ReferenceFields = GetSuperClass()->ReferenceFields;
	}

	for (VField* F = Fields; F; F = F->Next)
	{
		switch (F->Type.Type)
		{
		case TYPE_Reference:
		case TYPE_Delegate:
			F->NextReference = ReferenceFields;
			ReferenceFields = F;
			break;
		
		case TYPE_Struct:
			F->Type.Struct->PostLoad();
			if (F->Type.Struct->ReferenceFields)
			{
				F->NextReference = ReferenceFields;
				ReferenceFields = F;
			}
			break;

		case TYPE_Array:
		case TYPE_DynamicArray:
			if (F->Type.ArrayInnerType == TYPE_Reference)
			{
				F->NextReference = ReferenceFields;
				ReferenceFields = F;
			}
			else if (F->Type.ArrayInnerType == TYPE_Struct)
			{
				F->Type.Struct->PostLoad();
				if (F->Type.Struct->ReferenceFields)
				{
					F->NextReference = ReferenceFields;
					ReferenceFields = F;
				}
			}
			break;
		}
	}
	unguard;
}

//==========================================================================
//
//	VClass::InitDestructorFields
//
//==========================================================================

void VClass::InitDestructorFields()
{
	guard(VClass::InitDestructorFields);
	DestructorFields = NULL;
	if (GetSuperClass())
	{
		DestructorFields = GetSuperClass()->DestructorFields;
	}

	for (VField* F = Fields; F; F = F->Next)
	{
		switch (F->Type.Type)
		{
		case TYPE_String:
			F->DestructorLink = DestructorFields;
			DestructorFields = F;
			break;

		case TYPE_Struct:
			F->Type.Struct->PostLoad();
			if (F->Type.Struct->DestructorFields)
			{
				F->DestructorLink = DestructorFields;
				DestructorFields = F;
			}
			break;

		case TYPE_Array:
			if (F->Type.ArrayInnerType == TYPE_String)
			{
				F->DestructorLink = DestructorFields;
				DestructorFields = F;
			}
			else if (F->Type.ArrayInnerType == TYPE_Struct)
			{
				F->Type.Struct->PostLoad();
				if (F->Type.Struct->DestructorFields)
				{
					F->DestructorLink = DestructorFields;
					DestructorFields = F;
				}
			}
			break;

		case TYPE_DynamicArray:
			F->DestructorLink = DestructorFields;
			DestructorFields = F;
			break;
		}
	}
	unguard;
}

//==========================================================================
//
//	VClass::CreateVTable
//
//==========================================================================

void VClass::CreateVTable()
{
	guard(VClass::CreateVTable);
	ClassVTable = new VMethod*[ClassNumMethods];
	memset(ClassVTable, 0, ClassNumMethods * sizeof(VMethod*));
	if (ParentClass)
	{
		memcpy(ClassVTable, ParentClass->ClassVTable,
			ParentClass->ClassNumMethods * sizeof(VMethod*));
	}
	for (int i = 0; i < GMembers.Num(); i++)
	{
		if (GMembers[i]->MemberType != MEMBER_Method ||
			GMembers[i]->Outer != this)
		{
			continue;
		}
		VMethod* M = (VMethod*)GMembers[i];
		if (M->VTableIndex == -1)
		{
			continue;
		}
		ClassVTable[M->VTableIndex] = M;
	}
	unguard;
}

//==========================================================================
//
//	VClass::InitStatesLookup
//
//==========================================================================

void VClass::InitStatesLookup()
{
	guard(VClass::InitStatesLookup);
	//	This is also called from dehacked parser, so we must do this check.
	if (StatesLookup.Num())
	{
		return;
	}

	//	Create states lookup table.
	if (GetSuperClass())
	{
		GetSuperClass()->InitStatesLookup();
		for (int i = 0; i < GetSuperClass()->StatesLookup.Num(); i++)
		{
			StatesLookup.Append(GetSuperClass()->StatesLookup[i]);
		}
	}
	for (VState* S = NetStates; S; S = S->NetNext)
	{
		S->NetId = StatesLookup.Num();
		StatesLookup.Append(S);
	}
	unguard;
}

//==========================================================================
//
//	VClass::CreateDefaults
//
//==========================================================================

void VClass::CreateDefaults()
{
	guard(VClass::CreateDefaults);
	if (Defaults)
	{
		return;
	}

	if (ParentClass && !ParentClass->Defaults)
	{
		ParentClass->CreateDefaults();
	}

	//	Allocate memory.
	Defaults = new vuint8[ClassSize];
	memset(Defaults, 0, ClassSize);

	//	Copy default properties from the parent class.
	if (ParentClass)
	{
		ParentClass->CopyObject(ParentClass->Defaults, Defaults);
	}

	//	Call default properties method.
	if (DefaultProperties)
	{
		P_PASS_REF((VObject*)Defaults);
		VObject::ExecuteFunction(DefaultProperties);
	}
	unguard;
}

//==========================================================================
//
//	VClass::CopyObject
//
//==========================================================================

void VClass::CopyObject(const vuint8* Src, vuint8* Dst)
{
	guard(VClass::CopyObject);
	//	Copy parent class fields.
	if (GetSuperClass())
	{
		GetSuperClass()->CopyObject(Src, Dst);
	}
	//	Copy fields.
	for (VField* F = Fields; F; F = F->Next)
	{
		VField::CopyFieldValue(Src + F->Ofs, Dst + F->Ofs, F->Type);
	}
	unguardf(("(%s)", GetName()));
}

//==========================================================================
//
//	VClass::SerialiseObject
//
//==========================================================================

void VClass::SerialiseObject(VStream& Strm, VObject* Obj)
{
	guard(SerialiseObject);
	//	Serialise parent class fields.
	if (GetSuperClass())
	{
		GetSuperClass()->SerialiseObject(Strm, Obj);
	}
	//	Serialise fields.
	for (VField* F = Fields; F; F = F->Next)
	{
		//	Skip native and transient fields.
		if (F->Flags & (FIELD_Native | FIELD_Transient))
		{
			continue;
		}
		VField::SerialiseFieldValue(Strm, (vuint8*)Obj + F->Ofs, F->Type);
	}
	unguardf(("(%s)", GetName()));
}

//==========================================================================
//
//	VClass::CleanObject
//
//==========================================================================

void VClass::CleanObject(VObject* Obj)
{
	guard(VClass::CleanObject);
	for (VField* F = ReferenceFields; F; F = F->NextReference)
	{
		VField::CleanField((vuint8*)Obj + F->Ofs, F->Type);
	}
	unguardf(("(%s)", GetName()));
}

//==========================================================================
//
//	VClass::DestructObject
//
//==========================================================================

void VClass::DestructObject(VObject* Obj)
{
	guard(VClass::DestructObject);
	for (VField* F = DestructorFields; F; F = F->DestructorLink)
	{
		VField::DestructField((vuint8*)Obj + F->Ofs, F->Type);
	}
	unguardf(("(%s)", GetName()));
}

//==========================================================================
//
//	VClass::CreateDerivedClass
//
//==========================================================================

VClass* VClass::CreateDerivedClass(VName AName)
{
	guard(VClass::CreateDerivedClass);
	VClass* NewClass = new VClass(AName);
	NewClass->ParentClass = this;
	NewClass->PostLoad();
	NewClass->CreateDefaults();
	return NewClass;
	unguard;
}

//==========================================================================
//
//	VClass::GetReplacement
//
//==========================================================================

VClass* VClass::GetReplacement()
{
	guard(VClass::GetReplacement);
	if (!Replacement)
	{
		return this;
	}
	//	Avoid looping recursion by temporarely NULL-ing the field
	VClass* Temp = Replacement;
	Replacement = NULL;
	VClass* Ret = Temp->GetReplacement();
	Replacement = Temp;
	return Ret;
	unguard;
}

//==========================================================================
//
//	VClass::GetReplacee
//
//==========================================================================

VClass* VClass::GetReplacee()
{
	guard(VClass::GetReplacee);
	if (!Replacee)
	{
		return this;
	}
	//	Avoid looping recursion by temporarely NULL-ing the field
	VClass* Temp = Replacee;
	Replacee = NULL;
	VClass* Ret = Temp->GetReplacee();
	Replacee = Temp;
	return Ret;
	unguard;
}

#endif
