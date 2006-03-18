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
//**	Copyright (C) 1999-2002 J�nis Legzdi��
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

#include "gamedefs.h"
#include "progdefs.h"

bool					VMemberBase::GObjInitialized;
VClass*					VMemberBase::GClasses;
TArray<VMemberBase*>	VMemberBase::GMembers;

TArray<mobjinfo_t>		VClass::GMobjInfos;
TArray<mobjinfo_t>		VClass::GScriptIds;
TArray<VName>			VClass::GSpriteNames;
TArray<VName>			VClass::GModelNames;

//==========================================================================
//
//	VMemberBase::VMemberBase
//
//==========================================================================

VMemberBase::VMemberBase(vuint8 InMemberType, VName AName)
: MemberType(InMemberType)
, Name(AName)
{
	if (GObjInitialized)
	{
		GMembers.AddItem(this);
	}
}

//==========================================================================
//
//	VMemberBase::~VMemberBase
//
//==========================================================================

VMemberBase::~VMemberBase()
{
}

//==========================================================================
//
//	VMemberBase::Serialise
//
//==========================================================================

void VMemberBase::Serialise(VStream&)
{
}

//==========================================================================
//
//	VMemberBase::StaticInit
//
//==========================================================================

void VMemberBase::StaticInit()
{
	guard(VMemberBase::StaticInit);
	VClass::GModelNames.AddItem(NAME_None);
	for (VClass* C = GClasses; C; C = C->LinkNext)
		GMembers.AddItem(C);
	GObjInitialized = true;
	unguard;
}

//==========================================================================
//
//	VMemberBase::StaticExit
//
//==========================================================================

void VMemberBase::StaticExit()
{
	GObjInitialized = false;
}

//==========================================================================
//
//	VPackage::VPackage
//
//==========================================================================

VPackage::VPackage(VName AName)
: VMemberBase(MEMBER_Package, AName)
{
}

//==========================================================================
//
//	VPackage::Serialise
//
//==========================================================================

void VPackage::Serialise(VStream& Strm)
{
	guard(VPackage::Serialise);
	VMemberBase::Serialise(Strm);
	unguard;
}

//==========================================================================
//
//	VField::VField
//
//==========================================================================

VField::VField(VName AName)
: VMemberBase(MEMBER_Field, AName)
{
}

//==========================================================================
//
//	VField::Serialise
//
//==========================================================================

void VField::Serialise(VStream& Strm)
{
	guard(VField::Serialise);
	VMemberBase::Serialise(Strm);
	VMethod* func;
	Strm << Next
		<< STRM_INDEX(Ofs)
		<< Type
		<< func
		<< STRM_INDEX(Flags);
	unguard;
}

//==========================================================================
//
//	operator VStream << FType
//
//==========================================================================

VStream& operator<<(VStream& Strm, VField::FType& T)
{
	guard(operator VStream << FType);
	Strm << T.Type;
	byte RealType = T.Type;
	if (RealType == ev_array)
	{
		Strm << T.ArrayInnerType
			<< STRM_INDEX(T.ArrayDim);
		RealType = T.ArrayInnerType;
	}
	if (RealType == ev_pointer)
	{
		Strm << T.InnerType
			<< T.PtrLevel;
		RealType = T.InnerType;
	}
	if (RealType == ev_reference)
		Strm << T.Class;
	else if (RealType == ev_struct || RealType == ev_vector)
		Strm << T.Struct;
	else if (RealType == ev_delegate)
		Strm << T.Function;
	else if (RealType == ev_bool)
		Strm << T.BitMask;
	return Strm;
	unguard;
}

//==========================================================================
//
//	VField::SerialiseFieldValue
//
//==========================================================================

void VField::SerialiseFieldValue(VStream& Strm, byte* Data, const VField::FType& Type)
{
	guard(SerialiseFieldValue);
	VField::FType IntType;
	int InnerSize;
	switch (Type.Type)
	{
	case ev_int:
		Strm << *(int*)Data;
		break;

	case ev_float:
		Strm << *(float*)Data;
		break;

	case ev_bool:
		if (Type.BitMask == 1)
			Strm << *(int*)Data;
		break;

	case ev_vector:
		Strm << *(TVec*)Data;
		break;

	case ev_name:
		Strm << *(VName*)Data;
		break;

	case ev_string:
		if (Strm.IsLoading())
		{
			int TmpIdx;
			Strm << TmpIdx;
			if (TmpIdx)
			{
				*(int*)Data = (int)svpr.StrAtOffs(TmpIdx);
			}
			else
			{
				*(int*)Data = 0;
			}
		}
		else
		{
			int TmpIdx = 0;
			if (*(int*)Data)
			{
				TmpIdx = svpr.GetStringOffs(*(char**)Data);
			}
			Strm << TmpIdx;
		}
		break;

	case ev_pointer:
		if (Type.InnerType == ev_struct)
			Strm.SerialiseStructPointer(*(void**)Data, Type.Struct);
		else
		{
			dprintf("Don't know how to serialise pointer type %d\n", Type.InnerType);
			Strm << *(int*)Data;
		}
		break;

	case ev_reference:
		Strm.SerialiseReference(*(VObject**)Data, Type.Class);
		break;

	case ev_classid:
		if (Strm.IsLoading())
		{
			VName CName;
			Strm << CName;
			if (CName != NAME_None)
			{
				*(VClass**)Data = VClass::FindClass(*CName);
			}
			else
			{
				*(VClass**)Data = NULL;
			}
		}
		else
		{
			VName CName = NAME_None;
			if (*(VClass**)Data)
			{
				CName = (*(VClass**)Data)->GetVName();
			}
			Strm << CName;
		}
		break;

	case ev_state:
		if (Strm.IsLoading())
		{
			VName CName;
			VName SName;
			Strm << CName << SName;
			if (SName != NAME_None)
			{
				*(VState**)Data = VClass::FindClass(*CName)->FindStateChecked(SName);
			}
			else
			{
				*(VState**)Data = NULL;
			}
		}
		else
		{
			VName CName = NAME_None;
			VName SName = NAME_None;
			if (*(VState**)Data)
			{
				CName = (*(VState**)Data)->OuterClass->GetVName();
				SName = (*(VState**)Data)->Name;
			}
			Strm << CName << SName;
		}
		break;

	case ev_delegate:
		Strm.SerialiseReference(*(VObject**)Data, Type.Class);
		if (Strm.IsLoading())
		{
			VName FuncName;
			Strm << FuncName;
			if (*(VObject**)Data)
				((VMethod**)Data)[1] = (*(VObject**)Data)->GetVFunction(FuncName);
		}
		else
		{
			VName FuncName = NAME_None;
			if (*(VObject**)Data)
				FuncName = ((VMethod**)Data)[1]->Name;
			Strm << FuncName;
		}
		break;

	case ev_struct:
		Type.Struct->SerialiseObject(Strm, Data);
		break;

	case ev_array:
		IntType = Type;
		IntType.Type = Type.ArrayInnerType;
		InnerSize = IntType.Type == ev_vector ? 12 : IntType.Type == ev_struct ? IntType.Struct->Size : 4;
		for (int i = 0; i < Type.ArrayDim; i++)
		{
			SerialiseFieldValue(Strm, Data + i * InnerSize, IntType);
		}
		break;
	}
	unguard;
}

//==========================================================================
//
//	VField::CleanField
//
//==========================================================================

void VField::CleanField(byte* Data, const VField::FType& Type)
{
	guard(CleanField);
	VField::FType IntType;
	int InnerSize;
	switch (Type.Type)
	{
	case ev_reference:
		if (*(VObject**)Data && (*(VObject**)Data)->GetFlags() & _OF_CleanupRef)
		{
			*(VObject**)Data = NULL;
		}
		break;

	case ev_delegate:
		if (*(VObject**)Data && (*(VObject**)Data)->GetFlags() & _OF_CleanupRef)
		{
			*(VObject**)Data = NULL;
			((VMethod**)Data)[1] = NULL;
		}
		break;

	case ev_struct:
		Type.Struct->CleanObject(Data);
		break;

	case ev_array:
		IntType = Type;
		IntType.Type = Type.ArrayInnerType;
		InnerSize = IntType.Type == ev_struct ? IntType.Struct->Size : 4;
		for (int i = 0; i < Type.ArrayDim; i++)
		{
			CleanField(Data + i * InnerSize, IntType);
		}
		break;
	}
	unguard;
}

//==========================================================================
//
//	VMethod::VMethod
//
//==========================================================================

VMethod::VMethod(VName AName)
: VMemberBase(MEMBER_Method, AName)
{
}

//==========================================================================
//
//  PF_Fixme
//
//==========================================================================

static void PF_Fixme()
{
	Sys_Error("unimplemented bulitin");
}

//==========================================================================
//
//	VMethod::Serialise
//
//==========================================================================

void VMethod::Serialise(VStream& Strm)
{
	guard(VMethod::Serialise);
	VMemberBase::Serialise(Strm);
	VField::FType TmpType;
	vint32 TmpNumParams;
	vint32 TmpNumLocals;
	vint32 TmpFlags;
	vint32 ParamsSize;
	Strm << OuterClass
		<< STRM_INDEX(FirstStatement)
		<< STRM_INDEX(TmpNumLocals)
		<< STRM_INDEX(TmpFlags)
		<< TmpType
		<< STRM_INDEX(TmpNumParams)
		<< STRM_INDEX(ParamsSize);
	Type = TmpType.Type;
	NumParms = ParamsSize;
	NumLocals = TmpNumLocals;
	Flags = TmpFlags;
	for (int i = 0; i < TmpNumParams; i++)
		Strm << TmpType;

	//	Set up builtins
	if (NumParms > 16)
		Sys_Error("Function has more than 16 params");
	for (FBuiltinInfo* B = FBuiltinInfo::Builtins; B; B = B->Next)
	{
		if (OuterClass == B->OuterClass && !strcmp(*Name, B->Name))
		{
			if (Flags & FUNC_Native)
			{
				FirstStatement = (int)B->Func;
				break;
			}
			else
			{
				Sys_Error("PR_LoadProgs: Builtin %s redefined", B->Name);
			}
		}
	}
	if (!FirstStatement && Flags & FUNC_Native)
	{
		//	Default builtin
		FirstStatement = (int)PF_Fixme;
#if defined CLIENT && defined SERVER
		//	Don't abort with error, because it will be done, when this
		// function will be called (if it will be called).
		GCon->Logf(NAME_Dev, "WARNING: Builtin %s.%s not found!",
			OuterClass->GetName(), *Name);
#endif
	}
	unguard;
}

//==========================================================================
//
//	VState::VState
//
//==========================================================================

VState::VState(VName AName)
: VMemberBase(MEMBER_State, AName)
{
}

//==========================================================================
//
//	VState::Serialise
//
//==========================================================================

void VState::Serialise(VStream& Strm)
{
	guard(VState::Serialise);
	VMemberBase::Serialise(Strm);
	Strm << SpriteName
		<< STRM_INDEX(frame)
		<< ModelName
		<< STRM_INDEX(model_frame)
		<< time
		<< nextstate
		<< function
		<< OuterClass
		<< Next;
	if (Strm.IsLoading())
	{
		SpriteIndex = VClass::FindSprite(SpriteName);
		ModelIndex = VClass::FindModel(ModelName);
	}
	unguard;
}

//==========================================================================
//
//	VState::IsInRange
//
//==========================================================================

bool VState::IsInRange(VState* Start, VState* End, int MaxDepth)
{
	guard(VState::IsInRange);
	int Depth = 0;
	VState* check = Start;
	do
	{
		if (check == this)
			return true;
		if (check)
			check = check->Next;
		Depth++;
	}
	while (Depth < MaxDepth && check != End);
	return false;
	unguard;
}

//==========================================================================
//
//	VConstant::VConstant
//
//==========================================================================

VConstant::VConstant(VName AName)
: VMemberBase(MEMBER_Const, AName)
{
}

//==========================================================================
//
//	VConstant::Serialise
//
//==========================================================================

void VConstant::Serialise(VStream& Strm)
{
	VMemberBase::Serialise(Strm);
	Strm << OuterClass
		<< Type;
	switch (Type)
	{
	case ev_float:
		Strm << *(float*)&Value;
		break;

	case ev_name:
		Strm << *(VName*)&Value;
		break;

	default:
		Strm << STRM_INDEX(Value);
		break;
	}
}

//==========================================================================
//
//	VStruct::VStruct
//
//==========================================================================

VStruct::VStruct(VName AName)
: VMemberBase(MEMBER_Struct, AName)
{
}

//==========================================================================
//
//	VStruct::Serialise
//
//==========================================================================

void VStruct::Serialise(VStream& Strm)
{
	guard(VStruct::Serialise);
	VMemberBase::Serialise(Strm);
	vuint8 IsVector;
	vint32 AvailableSize;
	vint32 AvailableOfs;
	Strm << OuterClass
		<< ParentStruct
		<< IsVector
		<< STRM_INDEX(Size)
		<< Fields
		<< STRM_INDEX(AvailableSize)
		<< STRM_INDEX(AvailableOfs);
	unguard;
}

//==========================================================================
//
//	VStruct::InitReferences
//
//==========================================================================

void VStruct::InitReferences()
{
	guard(VStruct::InitReferences);
	if (ObjectFlags & CLASSOF_RefsInitialised)
	{
		return;
	}

	ReferenceFields = NULL;
	if (ParentStruct)
	{
		ParentStruct->InitReferences();
		ReferenceFields = ParentStruct->ReferenceFields;
	}

	for (VField* F = Fields; F; F = F->Next)
	{
		switch (F->Type.Type)
		{
		case ev_reference:
		case ev_delegate:
			F->NextReference = ReferenceFields;
			ReferenceFields = F;
			break;
		
		case ev_struct:
			F->Type.Struct->InitReferences();
			if (F->Type.Struct->ReferenceFields)
			{
				F->NextReference = ReferenceFields;
				ReferenceFields = F;
			}
			break;
		
		case ev_array:
			if (F->Type.ArrayInnerType == ev_reference)
			{
				F->NextReference = ReferenceFields;
				ReferenceFields = F;
			}
			else if (F->Type.ArrayInnerType == ev_struct)
			{
				F->Type.Struct->InitReferences();
				if (F->Type.Struct->ReferenceFields)
				{
					F->NextReference = ReferenceFields;
					ReferenceFields = F;
				}
			}
			break;
		}
	}

	ObjectFlags |= CLASSOF_RefsInitialised;
	unguard;
}

//==========================================================================
//
//	VStruct::SerialiseObject
//
//==========================================================================

void VStruct::SerialiseObject(VStream& Strm, byte* Data)
{
	guard(VStruct::SerialiseObject);
	//	Serialise parent struct's fields.
	if (ParentStruct)
	{
		ParentStruct->SerialiseObject(Strm, Data);
	}
	//	Serialise fields.
	for (VField* F = Fields; F; F = F->Next)
	{
		//	Skip native and transient fields.
		if (F->Flags & (FIELD_Native | FIELD_Transient))
		{
			continue;
		}
		VField::SerialiseFieldValue(Strm, Data + F->Ofs, F->Type);
	}
	unguardf(("(%s)", *Name));
}

//==========================================================================
//
//	VStruct::CleanObject
//
//==========================================================================

void VStruct::CleanObject(byte* Data)
{
	guard(VStruct::CleanObject);
	for (VField* F = ReferenceFields; F; F = F->NextReference)
	{
		VField::CleanField(Data + F->Ofs, F->Type);
	}
	unguardf(("(%s)", *Name));
}

//==========================================================================
//
//	operator VStream << mobjinfo_t
//
//==========================================================================

VStream& operator<<(VStream& Strm, mobjinfo_t& MI)
{
	return Strm << STRM_INDEX(MI.doomednum)
		<< MI.class_id;
}

//==========================================================================
//
//	VClass::VClass
//
//==========================================================================

VClass::VClass(VName AName)
: VMemberBase(MEMBER_Class, AName)
{
	guard(VClass::VClass);
	LinkNext = GClasses;
	GClasses = this;
	unguard;
}

//==========================================================================
//
//	VClass::VClass
//
//==========================================================================

VClass::VClass(ENativeConstructor, size_t ASize, dword AClassFlags, 
	VClass *AParent, EName AName, void(*ACtor)(void*))
: VMemberBase(MEMBER_Class, AName)
, ObjectFlags(CLASSOF_Native)
, ParentClass(AParent)
, ClassSize(ASize)
, ClassFlags(AClassFlags)
, ClassConstructor(ACtor)
{
	guard(native VClass::VClass);
	LinkNext = GClasses;
	GClasses = this;
	unguard;
}

//==========================================================================
//
//	VClass::~VClass
//
//==========================================================================

VClass::~VClass()
{
	guard(VClass::~VClass);
	if (!GObjInitialized)
	{
		return;
	}
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
	int PrevSize = ClassSize;
	VClass* PrevParent = ParentClass;
	if (!ClassVTable)
	{
		Strm << ParentClass
			<< Fields
			<< States
			<< STRM_INDEX(VTableOffset)
			<< STRM_INDEX(ClassNumMethods)
			<< STRM_INDEX(ClassSize);
		if ((ObjectFlags & CLASSOF_Native) && ClassSize != PrevSize)
		{
			Sys_Error("Bad class size, class %s, C++ %d, VavoomC %d)",
				GetName(), PrevSize, ClassSize);
		}
		if ((ObjectFlags & CLASSOF_Native) && ParentClass != PrevParent)
		{
			Sys_Error("Bad parent class, class %s, C++ %s, VavoomC %s)",
				GetName(), PrevParent ? PrevParent->GetName() : "(none)",
				ParentClass ? ParentClass->GetName() : "(none)");
		}
	}
	else
	{
		//FIXME Class already has been loaded.
		vint32 Dummy;
		Strm << STRM_INDEX(Dummy)
			<< STRM_INDEX(Dummy)
			<< STRM_INDEX(Dummy)
			<< STRM_INDEX(Dummy)
			<< STRM_INDEX(Dummy)
			<< STRM_INDEX(Dummy);
	}
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
	return GSpriteNames.AddItem(Name);
	unguard;
}

//==========================================================================
//
//	VClass::FindModel
//
//==========================================================================

int VClass::FindModel(VName Name)
{
	guard(VClass::FindModel);
	for (int i = 0; i < GModelNames.Num(); i++)
		if (GModelNames[i] == Name)
			return i;
	return GModelNames.AddItem(Name);
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
	for (int i = 0; i < ClassNumMethods; i++)
	{
		if (ClassVTable[i]->Name == AName)
		{
			return ClassVTable[i];
		}
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
//	VClass::InitReferences
//
//==========================================================================

void VClass::InitReferences()
{
	guard(VClass::InitReferences);
	if (ObjectFlags & CLASSOF_RefsInitialised)
	{
		return;
	}

	ReferenceFields = NULL;
	if (GetSuperClass())
	{
		GetSuperClass()->InitReferences();
		ReferenceFields = GetSuperClass()->ReferenceFields;
	}

	for (VField* F = Fields; F; F = F->Next)
	{
		switch (F->Type.Type)
		{
		case ev_reference:
		case ev_delegate:
			F->NextReference = ReferenceFields;
			ReferenceFields = F;
			break;
		
		case ev_struct:
			F->Type.Struct->InitReferences();
			if (F->Type.Struct->ReferenceFields)
			{
				F->NextReference = ReferenceFields;
				ReferenceFields = F;
			}
			break;
		
		case ev_array:
			if (F->Type.ArrayInnerType == ev_reference)
			{
				F->NextReference = ReferenceFields;
				ReferenceFields = F;
			}
			else if (F->Type.ArrayInnerType == ev_struct)
			{
				F->Type.Struct->InitReferences();
				if (F->Type.Struct->ReferenceFields)
				{
					F->NextReference = ReferenceFields;
					ReferenceFields = F;
				}
			}
			break;
		}
	}

	ObjectFlags |= CLASSOF_RefsInitialised;
	unguard;
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
		guard(Field);
		VField::SerialiseFieldValue(Strm, (byte*)Obj + F->Ofs, F->Type);
		unguardf(("(%s)", *F->Name));
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
	guard(CleanObject);
	for (VField* F = ReferenceFields; F; F = F->NextReference)
	{
		VField::CleanField((byte*)Obj + F->Ofs, F->Type);
	}
	unguardf(("(%s)", GetName()));
}

//**************************************************************************
//
//	$Log$
//	Revision 1.13  2006/03/18 16:51:15  dj_jl
//	Renamed type class names, better code serialisation.
//
//	Revision 1.12  2006/03/13 21:22:21  dj_jl
//	Added support for read-only, private and transient fields.
//	
//	Revision 1.11  2006/03/13 18:32:45  dj_jl
//	Added function to check if a state is in the range.
//	
//	Revision 1.10  2006/03/12 20:06:02  dj_jl
//	States as objects, added state variable type.
//	
//	Revision 1.9  2006/03/10 19:31:25  dj_jl
//	Use serialisation for progs files.
//	
//	Revision 1.8  2006/03/06 13:02:32  dj_jl
//	Cleaning up references to destroyed objects.
//	
//	Revision 1.7  2006/02/27 20:45:26  dj_jl
//	Rewrote names class.
//	
//	Revision 1.6  2006/02/26 20:52:48  dj_jl
//	Proper serialisation of level and players.
//	
//	Revision 1.5  2005/11/24 20:09:23  dj_jl
//	Removed unused fields from Object class.
//	
//	Revision 1.4  2004/08/21 15:03:07  dj_jl
//	Remade VClass to be standalone class.
//	
//	Revision 1.3  2002/03/09 18:05:34  dj_jl
//	Added support for defining native functions outside pr_cmds
//	
//	Revision 1.2  2002/01/07 12:16:43  dj_jl
//	Changed copyright year
//	
//	Revision 1.1  2001/12/27 17:35:41  dj_jl
//	Split VClass in seperate module
//	
//	Revision 1.1  2001/12/18 19:03:17  dj_jl
//	A lots of work on VObject
//	
//**************************************************************************
