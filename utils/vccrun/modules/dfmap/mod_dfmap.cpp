//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
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

#include "mod_dfmap.h"

// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_CLASS(V, DFMap);


// ////////////////////////////////////////////////////////////////////////// //
void VDFMap::initialize () {
  header = nullptr;
  textures = nullptr;
  textureCount = 0;
  panels = nullptr;
  panelCount = 0;
  items = nullptr;
  itemCount = 0;
  monsters = nullptr;
  monsterCount = 0;
  areas = nullptr;
  areaCount = 0;
  triggers = nullptr;
  triggerCount = 0;
}


void VDFMap::clear () {
  delete header;

  for (int f = 0; f < textureCount; ++f) delete textures[f];
  delete[] textures;

  for (int f = 0; f < panelCount; ++f) delete panels[f];
  delete[] panels;

  for (int f = 0; f < itemCount; ++f) delete items[f];
  delete[] items;

  for (int f = 0; f < monsterCount; ++f) delete monsters[f];
  delete[] monsters;

  for (int f = 0; f < areaCount; ++f) delete areas[f];
  delete[] areas;

  for (int f = 0; f < triggerCount; ++f) delete triggers[f];
  delete[] triggers;

  initialize();
}


void VDFMap::Destroy () {
  clear();
  Super::Destroy();
}


// ////////////////////////////////////////////////////////////////////////// //
enum /*BlockType*/ {
  BT_None     = 0,
  BT_Textures = 1,
  BT_Panels   = 2,
  BT_Items    = 3,
  BT_Areas    = 4,
  BT_Monsters = 5,
  BT_Triggers = 6,
  BT_Header   = 7,
};


struct BufReader {
  vuint32 pos;
  vuint32 size;
  const vuint8 *data;
  bool wasError;

  BufReader (const vuint8 *adata, vuint32 asize) : pos(0), size(asize), data(adata), wasError(false) {}

  VStr readStrFixed (vuint32 fixlen, bool fixSlashes=false) {
    if (fixlen > size-pos) wasError = true;
    if (wasError) return VStr();
    if (fixlen == 0) return VStr();
    vuint32 len = 0;
    vuint32 stpos = pos;
    pos += fixlen;
    while (len < fixlen && data[stpos+len]) ++len;
    VStr res((const char *)data+stpos, len);
    if (fixSlashes) {
      char *xd = res.GetMutableCharPointer(0);
      for (vuint32 f = 0; f < len; ++f) if (xd[f] == '\\') xd[f] = '/';
    }
    return res;
  }

  int readU8 () {
    if (size-pos < 1) wasError = true;
    if (wasError) return 0;
    return data[pos++];
  }

  int readI8 () {
    if (size-pos < 1) wasError = true;
    if (wasError) return 0;
    int res = data[pos++];
    if (res > 0x7f) res -= 0x100;
    return res;
  }

  int readU16 () {
    if (size-pos < 2) wasError = true;
    if (wasError) return 0;
    vuint32 res = data[pos++];
    res |= data[pos++]<<8;
    return res;
  }

  int readI16 () {
    if (size-pos < 2) wasError = true;
    if (wasError) return 0;
    int res = data[pos++];
    res |= data[pos++]<<8;
    if (res > 0x7fff) res -= 0x10000;
    return res;
  }

  vuint32 readU32 () {
    if (size-pos < 2) wasError = true;
    if (wasError) return 0;
    vuint32 res = data[pos++];
    res |= data[pos++]<<8;
    res |= data[pos++]<<16;
    res |= data[pos++]<<24;
    return res;
  }

  int readI32 () {
    if (size-pos < 2) wasError = true;
    if (wasError) return 0;
    vuint32 res = data[pos++];
    res |= data[pos++]<<8;
    res |= data[pos++]<<16;
    res |= data[pos++]<<24;
    return (int)res; //UB, but i don't give a shit
  }
};


bool VDFMap::loadFrom (VStream &strm) {
  clear();
  if (!strm.IsLoading()) return false;
  int sz = strm.TotalSize()-strm.Tell();
  if (sz < 5) return true;
  if (sz > 1024*1024*8) return false;
  vuint8 *buf = new vuint8[sz+1];
  strm.Serialize(buf, sz);
  if (strm.IsError()) { delete[] buf; return false; }

  if (memcmp(buf, "MAP\x01", 4) != 0) { delete[] buf; return false; }
  vuint32 cpos = 4;

  bool wasError = false;
  while (!wasError && cpos < (vuint32)sz) {
    vuint8 btype = buf[cpos++];
    if (btype == BT_None) break;
    cpos += 4; // skip reserved field
    if (cpos >= (vuint32)sz) { delete[] buf; return false; }
    if ((vuint32)sz-cpos < 4) { delete[] buf; return false; }
    vuint32 bsize = buf[cpos++];
    bsize |= buf[cpos++]<<8;
    bsize |= buf[cpos++]<<16;
    bsize |= buf[cpos++]<<24;
    if (bsize > (vuint32)sz-cpos) { delete[] buf; return false; }
    BufReader rd(buf+cpos, bsize);
    cpos += bsize;
    //fprintf(stderr, "block of type %u; size=%u\n", btype, bsize);
    switch (btype) {
      case BT_Textures:
        if (textures) { delete[] buf; return false; }
        textureCount = rd.size/65;
        textures = new Texture *[textureCount+1];
        memset(textures, 0, sizeof(Texture *)*(textureCount+1));
        for (int f = 0; f < textureCount; ++f) {
          auto it = new Texture;
          it->path = rd.readStrFixed(64, true);
          it->animated = (rd.readU8() ? 1 : 0);
          textures[f] = it;
          if (rd.wasError) break;
        }
        break;
      case BT_Panels:
        if (panels) { delete[] buf; return false; }
        panelCount = rd.size/18;
        panels = new Panel *[panelCount+1];
        memset(panels, 0, sizeof(Panel *)*(panelCount+1));
        for (int f = 0; f < panelCount; ++f) {
          auto it = new Panel;
          it->x = rd.readI32();
          it->y = rd.readI32();
          it->width = rd.readI16();
          it->height = rd.readI16();
          it->texture = rd.readU16();
          it->type = rd.readU16();
          it->alpha = rd.readU8();
          it->flags = rd.readU8();
          panels[f] = it;
          if (rd.wasError) break;
        }
        break;
      case BT_Items:
        if (items) { delete[] buf; return false; }
        itemCount = rd.size/10;
        items = new Item *[itemCount+1];
        memset(items, 0, sizeof(Item *)*(itemCount+1));
        for (int f = 0; f < itemCount; ++f) {
          auto it = new Item;
          it->x = rd.readI32();
          it->y = rd.readI32();
          it->type = rd.readU8();
          it->flags = rd.readU8();
          items[f] = it;
          if (rd.wasError) break;
        }
        break;
      case BT_Areas:
        if (areas) { delete[] buf; return false; }
        areaCount = rd.size/10;
        areas = new Area *[areaCount+1];
        memset(areas, 0, sizeof(Area *)*(areaCount+1));
        for (int f = 0; f < areaCount; ++f) {
          auto it = new Area;
          it->x = rd.readI32();
          it->y = rd.readI32();
          it->type = rd.readU8();
          it->dir = rd.readU8();
          areas[f] = it;
          if (rd.wasError) break;
        }
        break;
      case BT_Monsters:
        if (monsters) { delete[] buf; return false; }
        monsterCount = rd.size/10;
        monsters = new Monster *[monsterCount+1];
        memset(monsters, 0, sizeof(Monster *)*(monsterCount+1));
        for (int f = 0; f < monsterCount; ++f) {
          auto it = new Monster;
          it->x = rd.readI32();
          it->y = rd.readI32();
          it->type = rd.readU8();
          it->dir = rd.readU8();
          monsters[f] = it;
          if (rd.wasError) break;
        }
        break;
      case BT_Triggers:
        if (triggers) { delete[] buf; return false; }
        triggerCount = rd.size/148;
        triggers = new Trigger *[triggerCount+1];
        memset(triggers, 0, sizeof(Trigger *)*(triggerCount+1));
        for (int f = 0; f < triggerCount; ++f) {
          rd.pos = (vuint32)f*148;
          int x = rd.readI32();
          int y = rd.readI32();
          int width = rd.readI16();
          int height = rd.readI16();
          int enabled = (rd.readU8() ? 1 : 0);
          int texturePanel = rd.readI32();
          int type = rd.readU8();
          int actFlags = rd.readU8();
          int keyFlags = rd.readU8();
          //fprintf(stderr, " trigger of type %u\n", type);
          switch (type) {
            case TRIGGER_EXIT:
              {
                auto it = new TriggerExit;
                it->x = x;
                it->y = y;
                it->width = width;
                it->height = height;
                it->enabled = enabled;
                it->texturePanel = texturePanel;
                it->type = type;
                it->actFlags = actFlags;
                it->keyFlags = keyFlags;
                //
                it->map = rd.readStrFixed(16, true);
                triggers[f] = it;
              }
              break;
            case TRIGGER_TELEPORT:
              {
                auto it = new TriggerTeleport;
                it->x = x;
                it->y = y;
                it->width = width;
                it->height = height;
                it->enabled = enabled;
                it->texturePanel = texturePanel;
                it->type = type;
                it->actFlags = actFlags;
                it->keyFlags = keyFlags;
                //
                it->destx = rd.readI32();
                it->desty = rd.readI32();
                it->flags = 0;
                if (rd.readU8()) it->flags |= 0x01; // d2d
                if (rd.readU8()) it->flags |= 0x02; // silent
                it->dir = rd.readU8();
                triggers[f] = it;
              }
              break;
            case TRIGGER_OPENDOOR:
            case TRIGGER_CLOSEDOOR:
            case TRIGGER_DOOR:
            case TRIGGER_DOOR5:
            case TRIGGER_CLOSETRAP:
            case TRIGGER_TRAP:
            case TRIGGER_LIFTUP:
            case TRIGGER_LIFTDOWN:
            case TRIGGER_LIFT:
              {
                auto it = new TriggerPanel;
                it->x = x;
                it->y = y;
                it->width = width;
                it->height = height;
                it->enabled = enabled;
                it->texturePanel = texturePanel;
                it->type = type;
                it->actFlags = actFlags;
                it->keyFlags = keyFlags;
                //
                it->panelId = rd.readI32();
                it->flags = 0;
                if (rd.readU8()) it->flags |= 0x02; // silent
                if (rd.readU8()) it->flags |= 0x01; // d2d
                triggers[f] = it;
              }
              break;
            case TRIGGER_PRESS:
            case TRIGGER_ON:
            case TRIGGER_OFF:
            case TRIGGER_ONOFF:
              {
                auto it = new TriggerPress;
                it->x = x;
                it->y = y;
                it->width = width;
                it->height = height;
                it->enabled = enabled;
                it->texturePanel = texturePanel;
                it->type = type;
                it->actFlags = actFlags;
                it->keyFlags = keyFlags;
                //
                it->destx = rd.readI32();
                it->desty = rd.readI32();
                it->destw = rd.readI16();
                it->desth = rd.readI16();
                it->wait = rd.readU16();
                it->pressCount = rd.readU16();
                it->monsterId = rd.readI32();
                it->extRandom = (rd.readU8() ? 1 : 0);
                triggers[f] = it;
              }
              break;
            case TRIGGER_SECRET:
              {
                auto it = new TriggerSecret;
                it->x = x;
                it->y = y;
                it->width = width;
                it->height = height;
                it->enabled = enabled;
                it->texturePanel = texturePanel;
                it->type = type;
                it->actFlags = actFlags;
                it->keyFlags = keyFlags;
                //
                triggers[f] = it;
              }
              break;
            case TRIGGER_TEXTURE:
              {
                auto it = new TriggerTexture;
                it->x = x;
                it->y = y;
                it->width = width;
                it->height = height;
                it->enabled = enabled;
                it->texturePanel = texturePanel;
                it->type = type;
                it->actFlags = actFlags;
                it->keyFlags = keyFlags;
                //
                it->flags = 0;
                if (rd.readU8()) it->flags |= 0x01; // activateOnce
                if (rd.readU8()) it->flags |= 0x02; // animateOnce
                triggers[f] = it;
              }
              break;
            case TRIGGER_SOUND:
              {
                auto it = new TriggerSound;
                it->x = x;
                it->y = y;
                it->width = width;
                it->height = height;
                it->enabled = enabled;
                it->texturePanel = texturePanel;
                it->type = type;
                it->actFlags = actFlags;
                it->keyFlags = keyFlags;
                //
                it->flags = 0;
                it->soundName = rd.readStrFixed(64, true);
                it->volume = rd.readU8();
                it->pan = rd.readU8();
                if (rd.readU8()) it->flags |= 0x01; // local
                it->playCount = rd.readU8();
                if (rd.readU8()) it->flags |= 0x02; // soundSwitch
                triggers[f] = it;
              }
              break;
            case TRIGGER_SPAWNMONSTER:
              {
                auto it = new TriggerSpawnMonster;
                it->x = x;
                it->y = y;
                it->width = width;
                it->height = height;
                it->enabled = enabled;
                it->texturePanel = texturePanel;
                it->type = type;
                it->actFlags = actFlags;
                it->keyFlags = keyFlags;
                //
                it->destx = rd.readI32();
                it->desty = rd.readI32();
                it->spawnMonsType = rd.readU8();
                it->health = rd.readI32();
                it->dir = rd.readU8();
                it->active = (rd.readU8() ? 1 : 0);
                (void)rd.readU8();
                (void)rd.readU8();
                it->monsCount = rd.readI32();
                it->effect = rd.readU8();
                (void)rd.readU8();
                it->max = rd.readU16();
                it->delay = rd.readU16();
                it->behaviour = rd.readU16();
                triggers[f] = it;
              }
              break;
            case TRIGGER_SPAWNITEM:
              {
                auto it = new TriggerSpawnItem;
                it->x = x;
                it->y = y;
                it->width = width;
                it->height = height;
                it->enabled = enabled;
                it->texturePanel = texturePanel;
                it->type = type;
                it->actFlags = actFlags;
                it->keyFlags = keyFlags;
                //
                it->destx = rd.readI32();
                it->desty = rd.readI32();
                it->spawnItemType = rd.readU8();
                it->flags = 0;
                if (rd.readU8()) it->flags |= 0x01; // bGravity
                if (rd.readU8()) it->flags |= 0x02; // bDMOnly
                (void)rd.readU8();
                it->itemCount = rd.readI32();
                it->effect = rd.readU8();
                (void)rd.readU8();
                it->max = rd.readU16();
                it->delay = rd.readU16();
                triggers[f] = it;
              }
              break;
            case TRIGGER_MUSIC:
              {
                auto it = new TriggerMusic;
                it->x = x;
                it->y = y;
                it->width = width;
                it->height = height;
                it->enabled = enabled;
                it->texturePanel = texturePanel;
                it->type = type;
                it->actFlags = actFlags;
                it->keyFlags = keyFlags;
                //
                it->musicName = rd.readStrFixed(64, true);
                it->musicAction = rd.readU8();
                triggers[f] = it;
              }
              break;
            case TRIGGER_PUSH:
              {
                auto it = new TriggerPush;
                it->x = x;
                it->y = y;
                it->width = width;
                it->height = height;
                it->enabled = enabled;
                it->texturePanel = texturePanel;
                it->type = type;
                it->actFlags = actFlags;
                it->keyFlags = keyFlags;
                //
                it->angle = rd.readU16();
                it->force = rd.readU8();
                it->resetVelocity = (rd.readU8() ? 1 : 0);
                triggers[f] = it;
              }
              break;
            case TRIGGER_SCORE:
              {
                auto it = new TriggerScore;
                it->x = x;
                it->y = y;
                it->width = width;
                it->height = height;
                it->enabled = enabled;
                it->texturePanel = texturePanel;
                it->type = type;
                it->actFlags = actFlags;
                it->keyFlags = keyFlags;
                //
                it->scoreAction = rd.readU8();
                it->scoreCount = rd.readU8();
                it->scoreTeam = rd.readU8();
                it->flags = 0;
                if (rd.readU8()) it->flags |= 0x01; // bScoreCon
                if (rd.readU8()) it->flags |= 0x02; // bScoreMsg
                triggers[f] = it;
              }
              break;
            case TRIGGER_MESSAGE:
              {
                auto it = new TriggerMessage;
                it->x = x;
                it->y = y;
                it->width = width;
                it->height = height;
                it->enabled = enabled;
                it->texturePanel = texturePanel;
                it->type = type;
                it->actFlags = actFlags;
                it->keyFlags = keyFlags;
                //
                it->kind = rd.readU8();
                it->msgDest = rd.readU8();
                it->text = rd.readStrFixed(100);
                it->msgTime = rd.readU16();
                triggers[f] = it;
              }
              break;
            case TRIGGER_DAMAGE:
              {
                auto it = new TriggerDamage;
                it->x = x;
                it->y = y;
                it->width = width;
                it->height = height;
                it->enabled = enabled;
                it->texturePanel = texturePanel;
                it->type = type;
                it->actFlags = actFlags;
                it->keyFlags = keyFlags;
                //
                it->amount = rd.readU16();
                it->interval = rd.readU16();
                triggers[f] = it;
              }
              break;
            case TRIGGER_HEALTH:
              {
                auto it = new TriggerHealth;
                it->x = x;
                it->y = y;
                it->width = width;
                it->height = height;
                it->enabled = enabled;
                it->texturePanel = texturePanel;
                it->type = type;
                it->actFlags = actFlags;
                it->keyFlags = keyFlags;
                //
                it->amount = rd.readU16();
                it->interval = rd.readU16();
                it->flags = 0;
                if (rd.readU8()) it->flags |= 0x01; // bHealMax
                if (rd.readU8()) it->flags |= 0x01; // bSilent
                triggers[f] = it;
              }
              break;
            case TRIGGER_SHOT:
              {
                auto it = new TriggerShot;
                it->x = x;
                it->y = y;
                it->width = width;
                it->height = height;
                it->enabled = enabled;
                it->texturePanel = texturePanel;
                it->type = type;
                it->actFlags = actFlags;
                it->keyFlags = keyFlags;
                //
                it->destx = rd.readI32();
                it->desty = rd.readI32();
                it->shotType = rd.readU8();
                it->shotTarget = rd.readU8();
                it->bSilent = (rd.readU8() == 0 ? 1 : 0); // negbool!
                it->aim = rd.readU8();
                it->panelId = rd.readI32();
                it->sight = rd.readU16();
                it->angle = rd.readU16();
                it->wait = rd.readU16();
                it->accuracy = rd.readU16();
                it->ammo = rd.readU16();
                it->reload = rd.readU16();
                triggers[f] = it;
              }
              break;
            case TRIGGER_EFFECT:
              {
                auto it = new TriggerEffect;
                it->x = x;
                it->y = y;
                it->width = width;
                it->height = height;
                it->enabled = enabled;
                it->texturePanel = texturePanel;
                it->type = type;
                it->actFlags = actFlags;
                it->keyFlags = keyFlags;
                //
                it->fxCount = rd.readU8();
                it->fxType = rd.readU8();
                it->fxSubType = rd.readU8();
                it->fxRed = rd.readU8();
                it->fxGreen = rd.readU8();
                it->fxBlue = rd.readU8();
                it->fxPos = rd.readU8();
                (void)rd.readU8();
                it->wait = rd.readU16();
                it->velX = rd.readI8();
                it->velY = rd.readI8();
                it->spreadL = rd.readU8();
                it->spreadR = rd.readU8();
                it->spreadU = rd.readU8();
                it->spreadD = rd.readU8();
                triggers[f] = it;
              }
              break;
            default: // unknown trigger
              break;
          }
          if (rd.wasError) break;
        }
        break;
      case BT_Header:
        if (header) { delete[] buf; return false; }
        if (bsize != 32+32+256+64+64+2+2) { delete[] buf; return false; }
        header = new Header();
        header->mapName = rd.readStrFixed(32);
        header->author = rd.readStrFixed(32);
        header->desc = rd.readStrFixed(256);
        header->music = rd.readStrFixed(64, true);
        header->sky = rd.readStrFixed(64, true);
        header->width = rd.readU16();
        header->height = rd.readU16();
        break;
      default:
        // skip unknown block
        break;
    }
    wasError = wasError || rd.wasError;
  }
  delete[] buf;

  // should have at least one panel and one item (player start)
  if (!header || !panels || !items) wasError = true;
  return !wasError;
}


// ////////////////////////////////////////////////////////////////////////// //
bool VDFMap::load (const VStr &fname) {
  VStream *st = fsysOpenFile(fname);
  if (!st) {
    st = fsysOpenFile(fname+".map");
    if (!st) return false;
  }
  auto res = loadFrom(*st);
  delete st;
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_FUNCTION(VDFMap, Load) {
  P_GET_STR(fname);
  VClass *iclass = VClass::FindClass("DFMap");
  if (iclass) {
    auto ifileo = VObject::StaticSpawnObject(iclass);
    auto ifile = (VDFMap *)ifileo;
    if (!ifile->load(fname)) { delete ifileo; ifileo = nullptr; }
    RET_REF((VObject *)ifileo);
  } else {
    RET_REF(nullptr);
  }
}
