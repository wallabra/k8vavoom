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
//**  Copyright (C) 2018-2021 Ketmar Dark
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
#include "../gamedefs.h"
#include "network.h"
#include "net_message.h"


static VCvarB net_debug_dump_chan_objmap("net_debug_dump_chan_objmap", false, "Dump objectmap communication?");


//==========================================================================
//
//  hash2str
//
//==========================================================================
VVA_OKUNUSED static VStr hash2str (const vuint8 hash[SHA256_DIGEST_SIZE]) {
  const char *hex = "0123456789abcdef";
  VStr res;
  for (int f = 0; f < SHA256_DIGEST_SIZE; ++f) {
    res += hex[(hash[f]>>4)&0x0fu];
    res += hex[hash[f]&0x0fu];
  }
  return res;
}


//==========================================================================
//
//  VObjectMapChannel::VObjectMapChannel
//
//==========================================================================
VObjectMapChannel::VObjectMapChannel (VNetConnection *AConnection, vint32 AIndex, vuint8 AOpenedLocally)
  : VChannel(AConnection, CHANNEL_ObjectMap, AIndex, AOpenedLocally)
  , CurrName(1) // NAME_None is implicit
  , CurrClass(1) // `None` class is implicit
  , needOpenMessage(true)
  , cprBufferSize(0)
  , cprBufferPos(0)
  , cprBuffer(nullptr)
  , InitialDataDone(false)
  , NextNameToSend(0)
{
  // do it on channel creation, why not
  if (Connection->IsServer()) CompressNames();
}


//==========================================================================
//
//  VObjectMapChannel::~VObjectMapChannel
//
//==========================================================================
VObjectMapChannel::~VObjectMapChannel () {
  ClearCprBuffer();
  if (Connection) Connection->ObjMapSent = true; // why not?
}


//==========================================================================
//
//  VObjectMapChannel::GetName
//
//==========================================================================
VStr VObjectMapChannel::GetName () const noexcept {
  return VStr(va("ompchan #%d(%s)", Index, GetTypeName()));
}


//==========================================================================
//
//  VObjectMapChannel::IsQueueFull
//
//==========================================================================
int VObjectMapChannel::IsQueueFull () noexcept {
  return
    OutListBits >= 64000*8 ? -1 : // oversaturated
    OutListBits >= 60000*8 ? 1 : // full
    0; // ok
}


//==========================================================================
//
//  VObjectMapChannel::BuildNetFieldsHash
//
//  build map of replicated fields and RPC methods.
//  this is used to check if both sides have the same progs.
//  it doesn't matter what exactly this thing contains, we only
//  interested if it is equal. so we won't even send it over the net,
//  we will only check its hash.
//
//==========================================================================
void VObjectMapChannel::BuildNetFieldsHash (vuint8 hash[SHA256_DIGEST_SIZE]) {
  sha256_ctx hashctx;
  sha256_init(&hashctx);
  for (auto &&cls : Connection->ObjMap->ClassLookup) {
    if (!cls) continue;
    // class name
    sha256_update(&hashctx, cls->GetName(), strlen(cls->GetName()));
    // class replication field names and types
    for (VField *fld = cls->NetFields; fld; fld = fld->NextNetField) {
      sha256_update(&hashctx, fld->GetName(), strlen(fld->GetName()));
      VStr tp = fld->Type.GetName();
      sha256_update(&hashctx, *tp, tp.length());
    }
    // class replication methods
    for (VMethod *mt = cls->NetMethods; mt; mt = mt->NextNetMethod) {
      sha256_update(&hashctx, mt->GetName(), strlen(mt->GetName()));
      // return type
      {
        VStr tp = mt->ReturnType.GetName();
        sha256_update(&hashctx, *tp, tp.length());
      }
      // number of arguments
      sha256_update(&hashctx, &mt->NumParams, sizeof(mt->NumParams));
      sha256_update(&hashctx, &mt->ParamsSize, sizeof(mt->ParamsSize));
      // flags
      sha256_update(&hashctx, &mt->ParamFlags, sizeof(mt->ParamFlags[0])*mt->NumParams);
      // param types
      for (int f = 0; f < mt->NumParams; ++f) {
        VStr tp = mt->ParamTypes[f].GetName();
        sha256_update(&hashctx, *tp, tp.length());
      }
    }
  }
  sha256_final(&hashctx, hash);
}


//==========================================================================
//
//  VObjectMapChannel::ClearCprBuffer
//
//==========================================================================
void VObjectMapChannel::ClearCprBuffer () {
  delete cprBuffer;
  cprBuffer = nullptr;
  cprBufferSize = cprBufferPos = 0;
}


//==========================================================================
//
//  VObjectMapChannel::CompressNames
//
//==========================================================================
void VObjectMapChannel::CompressNames () {
  ClearCprBuffer(); // just in case

  // write to buffer
  const int nameCount = Connection->ObjMap->NameLookup.length();
  TArray<vuint8> nameBuf;
  for (int f = 1; f < nameCount; ++f) {
    const char *text = *VName::CreateWithIndex(f);
    const int len = VStr::Length(text);
    vassert(len > 0 && len <= NAME_SIZE);
    nameBuf.append((vuint8)len);
    while (*text) nameBuf.append((vuint8)(*text++));
  }

  // compress buffer
  mz_ulong resbufsz = mz_compressBound((mz_ulong)nameBuf.length());
  if (resbufsz > 0x1fffffff) Sys_Error("too many names!");
  if (!resbufsz) resbufsz = 1;

  cprBuffer = new vuint8[(unsigned)resbufsz];
  mz_ulong destlen = (mz_ulong)resbufsz;
  int res = mz_compress2((unsigned char *)cprBuffer, &destlen, (const unsigned char *)nameBuf.ptr(), (mz_ulong)nameBuf.length(), 9);
  if (res != MZ_OK) {
    GCon->Logf(NAME_DevNet, "%s: cannot compress names", *GetDebugName());
    Connection->Close();
    return;
  }

  cprBufferSize = (int)destlen;
  unpDataSize = nameBuf.length();
  GCon->Logf(NAME_DevNet, "%s: compressed %d names from %d to %d", *GetDebugName(), nameCount, unpDataSize, cprBufferSize);
}


//==========================================================================
//
//  VObjectMapChannel::DecompressNames
//
//==========================================================================
void VObjectMapChannel::DecompressNames () {
  if (unpDataSize == 0) return; // nothing to do
  vassert(unpDataSize > 0);

  // decompress buffer
  TArray<vuint8> nameBuf;
  nameBuf.setLength(unpDataSize);
  mz_ulong destlen = unpDataSize;
  int res = mz_uncompress((unsigned char *)nameBuf.ptr(), &destlen, (const unsigned char *)cprBuffer, (mz_ulong)cprBufferSize);
  if (res != MZ_OK || destlen != (unsigned)unpDataSize) {
    GCon->Logf(NAME_DevNet, "%s: cannot decompress names", *GetDebugName());
    Connection->Close();
    return;
  }

  // read from buffer
  char buf[NAME_SIZE+1];
  int pos = 0;
  for (int f = 1; f < Connection->ObjMap->NameLookup.length(); ++f) {
    if (pos >= nameBuf.length()) {
      GCon->Logf(NAME_DevNet, "%s: cannot decompress names", *GetDebugName());
      Connection->Close();
      return;
    }
    int len = (int)nameBuf[pos++];
    if (len < 1 || len > NAME_SIZE) {
      GCon->Logf(NAME_Debug, "%s: invalid name length (%d)", *GetDebugName(), len);
      Connection->Close();
      return;
    }
    if (pos+len > nameBuf.length()) {
      GCon->Logf(NAME_DevNet, "%s: cannot decompress names", *GetDebugName());
      Connection->Close();
      return;
    }
    memcpy(buf, nameBuf.ptr()+pos, len);
    buf[len] = 0;
    pos += len;

    VName Name(buf);
    Connection->ObjMap->ReceivedName(f, Name);
  }

  if (pos != nameBuf.length()) {
    GCon->Logf(NAME_DevNet, "%s: cannot decompress names", *GetDebugName());
    Connection->Close();
    return;
  }

  //ClearCprBuffer();
  // completion signal
  delete cprBuffer;
  cprBuffer = nullptr;
  CurrName = Connection->ObjMap->NameLookup.length();
}


//==========================================================================
//
//  VObjectMapChannel::ReceivedCloseAck
//
//  sets `ObjMapSent` flag
//
//==========================================================================
void VObjectMapChannel::ReceivedCloseAck () {
  ClearCprBuffer();
  if (Connection) Connection->ObjMapSent = true;
  VChannel::ReceivedCloseAck(); // just in case
}


//==========================================================================
//
//  VObjectMapChannel::Tick
//
//==========================================================================
void VObjectMapChannel::Tick () {
  VChannel::Tick();
  if (IsLocalChannel()) Update();
}


//==========================================================================
//
//  VObjectMapChannel::UpdateSendPBar
//
//==========================================================================
void VObjectMapChannel::UpdateSendPBar () {
  RNet_PBarUpdate("sending names and classes", cprBufferPos+CurrClass, cprBufferSize+Connection->ObjMap->ClassLookup.length());
}


//==========================================================================
//
//  VObjectMapChannel::UpdateRecvPBar
//
//==========================================================================
void VObjectMapChannel::UpdateRecvPBar (bool forced) {
  RNet_PBarUpdate("loading names and classes", cprBufferPos+CurrClass, cprBufferSize+Connection->ObjMap->ClassLookup.length(), forced);
}


//==========================================================================
//
//  VObjectMapChannel::Update
//
//==========================================================================
void VObjectMapChannel::Update () {
  if (InitialDataDone) { LiveUpdate(); return; }

  if (!OpenAcked && !needOpenMessage) {
    // nothing to do yet (we sent open message, and waiting for the ack)
    UpdateSendPBar();
    return;
  }

  if (CurrClass == Connection->ObjMap->ClassLookup.length()) {
    // everything has been sent
    Close(); // just in case
    return;
  }

  //GCon->Logf(NAME_DevNet, "%s:000: qbytes=%d; outbytes=%d; sum=%d", *GetDebugName(), Connection->SaturaDepth, Connection->Out.GetNumBytes(), Connection->SaturaDepth+Connection->Out.GetNumBytes());

  // do not overflow queue
  if (!CanSendData()) {
    //GCon->Logf(NAME_DevNet, "%s:666: qbytes=%d; outbytes=%d; sum=%d; queued=%d (%d)", *GetDebugName(), Connection->SaturaDepth, Connection->Out.GetNumBytes(), Connection->SaturaDepth+Connection->Out.GetNumBytes(), GetSendQueueSize(), OutListBits);
    UpdateSendPBar();
    return;
  }

  VMessageOut outmsg(this);
  // send counters in the first message
  if (needOpenMessage) {
    needOpenMessage = false;
    outmsg.bOpen = true;
    RNet_PBarReset();
    GCon->Logf(NAME_DevNet, "opened class/name channel for %s", *Connection->GetAddress());
    // write replication data hash
    vuint8 hash[SHA256_DIGEST_SIZE];
    BuildNetFieldsHash(hash);
    outmsg.Serialise(hash, (int)sizeof(hash));
    //GCon->Logf(NAME_DevNet, "%s: rephash=%s", *GetDebugName(), *hash2str(hash));
    // send number of names
    vint32 NumNames = Connection->ObjMap->NameLookup.length();
    outmsg.WriteUInt((unsigned)NumNames);
    GCon->Logf(NAME_DevNet, "sending total %d names", NumNames);
    // send number of classes
    vint32 NumClasses = Connection->ObjMap->ClassLookup.length();
    outmsg.WriteUInt((unsigned)NumClasses);
    GCon->Logf(NAME_DevNet, "sending total %d classes", NumClasses);
    outmsg.WriteUInt((unsigned)cprBufferSize);
    outmsg.WriteUInt((unsigned)unpDataSize);
    vassert(cprBufferPos == 0);
  }

  VBitStreamWriter strm(MAX_MSG_SIZE_BITS+64, true); // allow expand, why not?

  // send packed names while we have anything to send
  while (cprBufferPos < cprBufferSize) {
    strm << cprBuffer[cprBufferPos];
    if (WillOverflowMsg(&outmsg, strm)) {
      FlushMsg(&outmsg);
      //if (net_debug_dump_chan_objmap) GCon->Logf(NAME_DevNet, "  ...names: [%d/%d] (%d)", cprBufferPos, cprBufferSize, GetSendQueueSize());
      if (!OpenAcked) { UpdateSendPBar(); return; } // if not opened, don't spam with packets yet
      // is queue full?
      if (!CanSendData()) {
        //GCon->Logf(NAME_DevNet, "%s:000: qbytes=%d; outbytes=%d; sum=%d; queued=%d (%d)", *GetDebugName(), Connection->SaturaDepth, Connection->Out.GetNumBytes(), Connection->SaturaDepth+Connection->Out.GetNumBytes(), GetSendQueueSize(), OutListBits);
        UpdateSendPBar();
        return;
      }
    }
    PutStream(&outmsg, strm);
    ++cprBufferPos;
  }

  // send classes while we have anything to send
  while (CurrClass < Connection->ObjMap->ClassLookup.length()) {
    VName Name = Connection->ObjMap->ClassLookup[CurrClass]->GetVName();
    Connection->ObjMap->SerialiseName(strm, Name);
    // send message if this class will not fit
    if (WillOverflowMsg(&outmsg, strm)) {
      FlushMsg(&outmsg);
      //if (net_debug_dump_chan_objmap) GCon->Logf(NAME_DevNet, "  ...classes: [%d/%d] (%d)", CurrClass+1, Connection->ObjMap->ClassLookup.length(), GetSendQueueSize());
      if (!OpenAcked) { UpdateSendPBar(); return; } // if not opened, don't spam with packets yet
      // is queue full?
      if (!CanSendData()) {
        //GCon->Logf(NAME_DevNet, "%s:001: qbytes=%d; outbytes=%d; sum=%d; queued=%d (%d)", *GetDebugName(), Connection->SaturaDepth, Connection->Out.GetNumBytes(), Connection->SaturaDepth+Connection->Out.GetNumBytes(), GetSendQueueSize(), OutListBits);
        UpdateSendPBar();
        return;
      }
    }
    PutStream(&outmsg, strm);
    ++CurrClass;
    //if (net_debug_dump_chan_objmap) GCon->Logf(NAME_DevNet, "  :class: [%d/%d]: <%s>", CurrClass, Connection->ObjMap->ClassLookup.length(), *Name);
  }

  // this is the last message
  PutStream(&outmsg, strm);
  FlushMsg(&outmsg);

  // nope, don't close, we'll transmit any new names here
  //Close();
  InitialDataDone = true;
  CurrName = Connection->ObjMap->NameLookup.length();
  NextNameToSend = CurrName;
  // now it will be waiting for the ack

  GCon->Logf(NAME_DevNet, "done writing initial objects (%d) and names (%d)", CurrClass, CurrName);
}


//==========================================================================
//
//  VObjectMapChannel::LiveUpdate
//
//==========================================================================
void VObjectMapChannel::LiveUpdate () {
  if (NextNameToSend >= VName::GetNumNames()) return; // no new names
  if (!CanSendData()) return; // not yet

  // use bitstream and split it to the messages here
  VMessageOut Msg(this);
  VBitStreamWriter strm(MAX_MSG_SIZE_BITS+64, false); // no expand

  while (NextNameToSend < VName::GetNumNames()) {
    const char *text = *VName::CreateWithIndex(NextNameToSend);
    const int len = VStr::Length(text);
    if (net_debug_dump_chan_objmap) GCon->Logf(NAME_DevNet, "%s: sending new name #%d (%s) (CurrName=%d; names=%d; known=%d)", *GetDebugName(), NextNameToSend, text, CurrName, VName::GetNumNames(), Connection->ObjMap->NameLookup.length());
    vassert(len > 0 && len <= NAME_SIZE);
    strm << STRM_INDEX(len);
    strm.Serialise((char *)text, len);
    ++NextNameToSend;
    if (PutStream(&Msg, strm)) {
      if (!CanSendData()) break; // stop right here
    }
  }
  FlushMsg(&Msg);
}


//==========================================================================
//
//  VObjectMapChannel::LiveParse
//
//==========================================================================
void VObjectMapChannel::LiveParse (VMessageIn &Msg) {
  int nameAck = 0;
  char buf[NAME_SIZE+1];
  // got new name(s)
  while (!Msg.AtEnd()) {
    vint32 len = 0;
    Msg << STRM_INDEX(len);
    if (Msg.IsError() || len < 1 || len > NAME_SIZE) {
      GCon->Logf(NAME_DevNet, "%s: invalid remote name lengh (%d) or read error", *GetDebugName(), len);
      Connection->Close();
      return;
    }
    Msg.Serialise(buf, len);
    buf[len] = 0;
    Connection->ObjMap->ReceivedName(CurrName, VName(buf));
    nameAck = CurrName++;
  }
  // send ack
  if (nameAck) {
    VMessageOut outmsg(this);
    outmsg << STRM_INDEX(nameAck);
    SendMessage(&outmsg);
    if (net_debug_dump_chan_objmap) GCon->Logf(NAME_DevNet, "%s: sent ack for name #%d", *GetDebugName(), nameAck);
  }
}


//==========================================================================
//
//  VObjectMapChannel::ParseMessage
//
//==========================================================================
void VObjectMapChannel::ParseMessage (VMessageIn &Msg) {
  // if this channel is opened locally, it is used to send data, and to receive acks
  if (IsLocalChannel()) {
    // this must be client ack message
    while (!Msg.AtEnd()) {
      vint32 NameAck = 0;
      Msg << STRM_INDEX(NameAck);
      if (NameAck > VName::GetNumNames()) {
        GCon->Logf(NAME_DevNet, "%s: remote sent invalid name ack %d (max is %d), dropping the connection", *GetDebugName(), NameAck, VName::GetNumNames());
        Connection->Close();
        return;
      }
      if (net_debug_dump_chan_objmap) GCon->Logf(NAME_DevNet, "%s: got ack for name #%d", *GetDebugName(), NameAck);
      // mark initial data completion
      if (NameAck >= CurrName) Connection->ObjMapSent = true;
      // internalise acked names
      Connection->ObjMap->AckNameWithIndex(NameAck);
      CurrName = NameAck+1;
    }
    // remote cannot close the channel
    if (Msg.bClose) {
      GCon->Logf(NAME_DevNet, "%s: remote closed object map channel, dropping the connection", *GetDebugName());
      Connection->Close();
    }
    return;
  }

  if (InitialDataDone) { LiveParse(Msg); return; }

  // read counters from opening message
  if (Msg.bOpen) {
    RNet_PBarReset();
    RNet_OSDMsgShow("receiving names and classes");

    // read replication data hash (it will be checked later)
    Msg.Serialise(serverReplicationHash, (int)sizeof(serverReplicationHash));
    //GCon->Logf(NAME_DevNet, "%s: rephash=%s", *GetDebugName(), *hash2str(serverReplicationHash));

    vint32 NumNames = (int)Msg.ReadUInt();
    if (NumNames < 0 || NumNames > 1024*1024*32) {
      GCon->Logf(NAME_Debug, "%s: invalid number of names (%d)", *GetDebugName(), NumNames);
      Connection->Close();
      return;
    }
    Connection->ObjMap->SetNumberOfKnownNames(NumNames);
    GCon->Logf(NAME_DevNet, "expecting %d names", NumNames);

    vint32 NumClasses = (int)Msg.ReadUInt();
    if (NumClasses < 0 || NumClasses > 1024*1024) {
      GCon->Logf(NAME_Debug, "%s: invalid number of classes (%d)", *GetDebugName(), NumClasses);
      Connection->Close();
      return;
    }
    Connection->ObjMap->ClassLookup.setLength(NumClasses);
    GCon->Logf(NAME_DevNet, "expecting %d classes", NumClasses);

    ClearCprBuffer(); // just in case
    cprBufferSize = (int)Msg.ReadUInt();
    unpDataSize = (int)Msg.ReadUInt();
    vassert(cprBufferPos == 0);
    if (cprBufferSize <= 0 || unpDataSize <= 0 || cprBufferSize > 1024*1024*64 || unpDataSize > 1024*1024*64) {
      GCon->Logf(NAME_Debug, "%s: invalid packed data sizes (%d/%d)", *GetDebugName(), cprBufferSize, unpDataSize);
      Connection->Close();
      return;
    }
    cprBuffer = new vuint8[cprBufferSize];
  }

  if (cprBuffer) {
    while (!Msg.AtEnd() && cprBufferPos < cprBufferSize) {
      Msg << cprBuffer[cprBufferPos];
      if (Msg.IsError()) {
        GCon->Logf(NAME_Debug, "%s: error reading compressed data", *GetDebugName());
        Connection->Close();
        return;
      }
      ++cprBufferPos;
    }
    if (cprBufferPos == cprBufferSize) DecompressNames();
  }

  // read classes
  while (!Msg.AtEnd() && CurrClass < Connection->ObjMap->ClassLookup.length()) {
    VName Name;
    Connection->ObjMap->SerialiseName(Msg, Name);
    VClass *C = VMemberBase::StaticFindClass(Name);
    vassert(C);
    Connection->ObjMap->ClassLookup[CurrClass] = C;
    Connection->ObjMap->ClassMap.Set(C, CurrClass);
    ++CurrClass;
    //if (net_debug_dump_chan_objmap) GCon->Logf(NAME_DevNet, "  :class: [%d/%d]: <%s : %s>", CurrClass, Connection->ObjMap->ClassLookup.length(), *Name, C->GetName());
  }

  UpdateRecvPBar(Msg.bClose);

  if (CurrClass != Connection->ObjMap->ClassLookup.length()) return;

  GCon->Logf(NAME_DevNet, "%s: received initial names (%d) and classes (%d)", *GetDebugName(), CurrName, CurrClass);
  // check replication data hash
  vuint8 hash[SHA256_DIGEST_SIZE];
  BuildNetFieldsHash(hash);
  if (memcmp(hash, serverReplicationHash, SHA256_DIGEST_SIZE) != 0) {
    GCon->Logf(NAME_DevNet, "%s: invalid replication data hash", *GetDebugName());
    Connection->Close();
    Host_Error("invalid replication data hash (incompatible progs)");
  }
  Connection->ObjMapSent = true;
  InitialDataDone = true;
  CurrName = Connection->ObjMap->NameLookup.length();

  // send ack
  vint32 ackn = CurrName;
  VMessageOut outmsg(this);
  outmsg << STRM_INDEX(ackn);
  SendMessage(&outmsg);
}
