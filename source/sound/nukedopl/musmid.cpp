//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2020 Ketmar Dark
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
#include "musmid.h"


// ////////////////////////////////////////////////////////////////////////// //
static const uint8_t opl_voltable[128] = {
  0x00, 0x01, 0x03, 0x05, 0x06, 0x08, 0x0a, 0x0b,
  0x0d, 0x0e, 0x10, 0x11, 0x13, 0x14, 0x16, 0x17,
  0x19, 0x1a, 0x1b, 0x1d, 0x1e, 0x20, 0x21, 0x22,
  0x24, 0x25, 0x27, 0x29, 0x2b, 0x2d, 0x2f, 0x31,
  0x32, 0x34, 0x36, 0x37, 0x39, 0x3b, 0x3c, 0x3d,
  0x3f, 0x40, 0x42, 0x43, 0x44, 0x45, 0x47, 0x48,
  0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4f, 0x50, 0x51,
  0x52, 0x53, 0x54, 0x54, 0x55, 0x56, 0x57, 0x58,
  0x59, 0x5a, 0x5b, 0x5c, 0x5c, 0x5d, 0x5e, 0x5f,
  0x60, 0x60, 0x61, 0x62, 0x63, 0x63, 0x64, 0x65,
  0x65, 0x66, 0x67, 0x67, 0x68, 0x69, 0x69, 0x6a,
  0x6b, 0x6b, 0x6c, 0x6d, 0x6d, 0x6e, 0x6e, 0x6f,
  0x70, 0x70, 0x71, 0x71, 0x72, 0x72, 0x73, 0x73,
  0x74, 0x75, 0x75, 0x76, 0x76, 0x77, 0x77, 0x78,
  0x78, 0x79, 0x79, 0x7a, 0x7a, 0x7b, 0x7b, 0x7b,
  0x7c, 0x7c, 0x7d, 0x7d, 0x7e, 0x7e, 0x7f, 0x7f
};

static const uint16_t opl_freqtable[284+384] = {
  0x0133, 0x0133, 0x0134, 0x0134, 0x0135, 0x0136, 0x0136, 0x0137,
  0x0137, 0x0138, 0x0138, 0x0139, 0x0139, 0x013a, 0x013b, 0x013b,
  0x013c, 0x013c, 0x013d, 0x013d, 0x013e, 0x013f, 0x013f, 0x0140,
  0x0140, 0x0141, 0x0142, 0x0142, 0x0143, 0x0143, 0x0144, 0x0144,
  0x0145, 0x0146, 0x0146, 0x0147, 0x0147, 0x0148, 0x0149, 0x0149,
  0x014a, 0x014a, 0x014b, 0x014c, 0x014c, 0x014d, 0x014d, 0x014e,
  0x014f, 0x014f, 0x0150, 0x0150, 0x0151, 0x0152, 0x0152, 0x0153,
  0x0153, 0x0154, 0x0155, 0x0155, 0x0156, 0x0157, 0x0157, 0x0158,
  0x0158, 0x0159, 0x015a, 0x015a, 0x015b, 0x015b, 0x015c, 0x015d,
  0x015d, 0x015e, 0x015f, 0x015f, 0x0160, 0x0161, 0x0161, 0x0162,
  0x0162, 0x0163, 0x0164, 0x0164, 0x0165, 0x0166, 0x0166, 0x0167,
  0x0168, 0x0168, 0x0169, 0x016a, 0x016a, 0x016b, 0x016c, 0x016c,
  0x016d, 0x016e, 0x016e, 0x016f, 0x0170, 0x0170, 0x0171, 0x0172,
  0x0172, 0x0173, 0x0174, 0x0174, 0x0175, 0x0176, 0x0176, 0x0177,
  0x0178, 0x0178, 0x0179, 0x017a, 0x017a, 0x017b, 0x017c, 0x017c,
  0x017d, 0x017e, 0x017e, 0x017f, 0x0180, 0x0181, 0x0181, 0x0182,
  0x0183, 0x0183, 0x0184, 0x0185, 0x0185, 0x0186, 0x0187, 0x0188,
  0x0188, 0x0189, 0x018a, 0x018a, 0x018b, 0x018c, 0x018d, 0x018d,
  0x018e, 0x018f, 0x018f, 0x0190, 0x0191, 0x0192, 0x0192, 0x0193,
  0x0194, 0x0194, 0x0195, 0x0196, 0x0197, 0x0197, 0x0198, 0x0199,
  0x019a, 0x019a, 0x019b, 0x019c, 0x019d, 0x019d, 0x019e, 0x019f,
  0x01a0, 0x01a0, 0x01a1, 0x01a2, 0x01a3, 0x01a3, 0x01a4, 0x01a5,
  0x01a6, 0x01a6, 0x01a7, 0x01a8, 0x01a9, 0x01a9, 0x01aa, 0x01ab,
  0x01ac, 0x01ad, 0x01ad, 0x01ae, 0x01af, 0x01b0, 0x01b0, 0x01b1,
  0x01b2, 0x01b3, 0x01b4, 0x01b4, 0x01b5, 0x01b6, 0x01b7, 0x01b8,
  0x01b8, 0x01b9, 0x01ba, 0x01bb, 0x01bc, 0x01bc, 0x01bd, 0x01be,
  0x01bf, 0x01c0, 0x01c0, 0x01c1, 0x01c2, 0x01c3, 0x01c4, 0x01c4,
  0x01c5, 0x01c6, 0x01c7, 0x01c8, 0x01c9, 0x01c9, 0x01ca, 0x01cb,
  0x01cc, 0x01cd, 0x01ce, 0x01ce, 0x01cf, 0x01d0, 0x01d1, 0x01d2,
  0x01d3, 0x01d3, 0x01d4, 0x01d5, 0x01d6, 0x01d7, 0x01d8, 0x01d8,
  0x01d9, 0x01da, 0x01db, 0x01dc, 0x01dd, 0x01de, 0x01de, 0x01df,
  0x01e0, 0x01e1, 0x01e2, 0x01e3, 0x01e4, 0x01e5, 0x01e5, 0x01e6,
  0x01e7, 0x01e8, 0x01e9, 0x01ea, 0x01eb, 0x01ec, 0x01ed, 0x01ed,
  0x01ee, 0x01ef, 0x01f0, 0x01f1, 0x01f2, 0x01f3, 0x01f4, 0x01f5,
  0x01f6, 0x01f6, 0x01f7, 0x01f8, 0x01f9, 0x01fa, 0x01fb, 0x01fc,
  0x01fd, 0x01fe, 0x01ff, 0x0200, 0x0201, 0x0201, 0x0202, 0x0203,
  0x0204, 0x0205, 0x0206, 0x0207, 0x0208, 0x0209, 0x020a, 0x020b,
  0x020c, 0x020d, 0x020e, 0x020f, 0x0210, 0x0210, 0x0211, 0x0212,
  0x0213, 0x0214, 0x0215, 0x0216, 0x0217, 0x0218, 0x0219, 0x021a,
  0x021b, 0x021c, 0x021d, 0x021e, 0x021f, 0x0220, 0x0221, 0x0222,
  0x0223, 0x0224, 0x0225, 0x0226, 0x0227, 0x0228, 0x0229, 0x022a,
  0x022b, 0x022c, 0x022d, 0x022e, 0x022f, 0x0230, 0x0231, 0x0232,
  0x0233, 0x0234, 0x0235, 0x0236, 0x0237, 0x0238, 0x0239, 0x023a,
  0x023b, 0x023c, 0x023d, 0x023e, 0x023f, 0x0240, 0x0241, 0x0242,
  0x0244, 0x0245, 0x0246, 0x0247, 0x0248, 0x0249, 0x024a, 0x024b,
  0x024c, 0x024d, 0x024e, 0x024f, 0x0250, 0x0251, 0x0252, 0x0253,
  0x0254, 0x0256, 0x0257, 0x0258, 0x0259, 0x025a, 0x025b, 0x025c,
  0x025d, 0x025e, 0x025f, 0x0260, 0x0262, 0x0263, 0x0264, 0x0265,
  0x0266, 0x0267, 0x0268, 0x0269, 0x026a, 0x026c, 0x026d, 0x026e,
  0x026f, 0x0270, 0x0271, 0x0272, 0x0273, 0x0275, 0x0276, 0x0277,
  0x0278, 0x0279, 0x027a, 0x027b, 0x027d, 0x027e, 0x027f, 0x0280,
  0x0281, 0x0282, 0x0284, 0x0285, 0x0286, 0x0287, 0x0288, 0x0289,
  0x028b, 0x028c, 0x028d, 0x028e, 0x028f, 0x0290, 0x0292, 0x0293,
  0x0294, 0x0295, 0x0296, 0x0298, 0x0299, 0x029a, 0x029b, 0x029c,
  0x029e, 0x029f, 0x02a0, 0x02a1, 0x02a2, 0x02a4, 0x02a5, 0x02a6,
  0x02a7, 0x02a9, 0x02aa, 0x02ab, 0x02ac, 0x02ae, 0x02af, 0x02b0,
  0x02b1, 0x02b2, 0x02b4, 0x02b5, 0x02b6, 0x02b7, 0x02b9, 0x02ba,
  0x02bb, 0x02bd, 0x02be, 0x02bf, 0x02c0, 0x02c2, 0x02c3, 0x02c4,
  0x02c5, 0x02c7, 0x02c8, 0x02c9, 0x02cb, 0x02cc, 0x02cd, 0x02ce,
  0x02d0, 0x02d1, 0x02d2, 0x02d4, 0x02d5, 0x02d6, 0x02d8, 0x02d9,
  0x02da, 0x02dc, 0x02dd, 0x02de, 0x02e0, 0x02e1, 0x02e2, 0x02e4,
  0x02e5, 0x02e6, 0x02e8, 0x02e9, 0x02ea, 0x02ec, 0x02ed, 0x02ee,
  0x02f0, 0x02f1, 0x02f2, 0x02f4, 0x02f5, 0x02f6, 0x02f8, 0x02f9,
  0x02fb, 0x02fc, 0x02fd, 0x02ff, 0x0300, 0x0302, 0x0303, 0x0304,
  0x0306, 0x0307, 0x0309, 0x030a, 0x030b, 0x030d, 0x030e, 0x0310,
  0x0311, 0x0312, 0x0314, 0x0315, 0x0317, 0x0318, 0x031a, 0x031b,
  0x031c, 0x031e, 0x031f, 0x0321, 0x0322, 0x0324, 0x0325, 0x0327,
  0x0328, 0x0329, 0x032b, 0x032c, 0x032e, 0x032f, 0x0331, 0x0332,
  0x0334, 0x0335, 0x0337, 0x0338, 0x033a, 0x033b, 0x033d, 0x033e,
  0x0340, 0x0341, 0x0343, 0x0344, 0x0346, 0x0347, 0x0349, 0x034a,
  0x034c, 0x034d, 0x034f, 0x0350, 0x0352, 0x0353, 0x0355, 0x0357,
  0x0358, 0x035a, 0x035b, 0x035d, 0x035e, 0x0360, 0x0361, 0x0363,
  0x0365, 0x0366, 0x0368, 0x0369, 0x036b, 0x036c, 0x036e, 0x0370,
  0x0371, 0x0373, 0x0374, 0x0376, 0x0378, 0x0379, 0x037b, 0x037c,
  0x037e, 0x0380, 0x0381, 0x0383, 0x0384, 0x0386, 0x0388, 0x0389,
  0x038b, 0x038d, 0x038e, 0x0390, 0x0392, 0x0393, 0x0395, 0x0397,
  0x0398, 0x039a, 0x039c, 0x039d, 0x039f, 0x03a1, 0x03a2, 0x03a4,
  0x03a6, 0x03a7, 0x03a9, 0x03ab, 0x03ac, 0x03ae, 0x03b0, 0x03b1,
  0x03b3, 0x03b5, 0x03b7, 0x03b8, 0x03ba, 0x03bc, 0x03bd, 0x03bf,
  0x03c1, 0x03c3, 0x03c4, 0x03c6, 0x03c8, 0x03ca, 0x03cb, 0x03cd,
  0x03cf, 0x03d1, 0x03d2, 0x03d4, 0x03d6, 0x03d8, 0x03da, 0x03db,
  0x03dd, 0x03df, 0x03e1, 0x03e3, 0x03e4, 0x03e6, 0x03e8, 0x03ea,
  0x03ec, 0x03ed, 0x03ef, 0x03f1, 0x03f3, 0x03f5, 0x03f6, 0x03f8,
  0x03fa, 0x03fc, 0x03fe, 0x036c
};


// ////////////////////////////////////////////////////////////////////////// //
// simple DooM MUS / Midi player

//==========================================================================
//
//  OPLPlayer::SynthResetVoice
//
//==========================================================================
void OPLPlayer::SynthResetVoice (SynthVoice &voice) {
  voice.freq = 0;
  voice.voice_dual = false;
  voice.voice_data = nullptr;
  voice.patch = nullptr;
  voice.chan = nullptr;
  voice.velocity = 0;
  voice.key = 0;
  voice.note = 0;
  voice.pan = 0x30;
  voice.finetune = 0;
  voice.tl[0] = 0x3f;
  voice.tl[1] = 0x3f;
}


//==========================================================================
//
//  OPLPlayer::SynthResetChip
//
//==========================================================================
void OPLPlayer::SynthResetChip (OPL3Chip &chip) {
  for (uint16_t i = 0x40; i < 0x56; ++i) OPL3_WriteReg(&chip, i, 0x3f);
  for (uint16_t i = 0x60; i < 0xf6; ++i) OPL3_WriteReg(&chip, i, 0x00);
  for (uint16_t i = 0x01; i < 0x40; ++i) OPL3_WriteReg(&chip, i, 0x00);

  OPL3_WriteReg(&chip, 0x01, 0x20);

  if (!mOPL2Mode) {
    OPL3_WriteReg(&chip, 0x105, 0x01);
    for (uint16_t i = 0x140; i < 0x156; ++i) OPL3_WriteReg(&chip, i, 0x3f);
    for (uint16_t i = 0x160; i < 0x1f6; ++i) OPL3_WriteReg(&chip, i, 0x00);
    for (uint16_t i = 0x101; i < 0x140; ++i) OPL3_WriteReg(&chip, i, 0x00);
    OPL3_WriteReg(&chip, 0x105, 0x01);
  } else {
    OPL3_WriteReg(&chip, 0x105, 0x00);
  }
}


//==========================================================================
//
//  OPLPlayer::SynthResetMidi
//
//==========================================================================
void OPLPlayer::SynthResetMidi (SynthMidiChannel &channel) {
  channel.volume = 100;
  channel.volume_t = opl_voltable[channel.volume]+1;
  channel.pan = 64;
  channel.reg_pan = 0x30;
  channel.pitch = 64;
  channel.patch = &mGenMidi.patch[0];
  channel.drum = false;
  if (&channel == &mSynthMidiChannels[15]) channel.drum = true;
}


//==========================================================================
//
//  OPLPlayer::SynthInit
//
//==========================================================================
void OPLPlayer::SynthInit () {
  const uint8_t opl_slotoffset[9] = {0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12};

  for (unsigned i = 0; i < 18; ++i) {
    mSynthVoices[i].bank = (uint8_t)(i/9);
    mSynthVoices[i].op_base = opl_slotoffset[i%9];
    mSynthVoices[i].ch_base = i%9;
    SynthResetVoice(mSynthVoices[i]);
  }

  for (unsigned i = 0; i < 16; ++i) SynthResetMidi(mSynthMidiChannels[i]);

  SynthResetChip(chip);

  mSynthVoiceNum = (mOPL2Mode ? 9 : 18);

  for (uint8_t i = 0; i < mSynthVoiceNum; i++) mSynthVoicesFree[i] = &mSynthVoices[i];

  mSynthVoicesAllocatedNum = 0;
  mSynthVoicesFreeNum = mSynthVoiceNum;
}


//==========================================================================
//
//  OPLPlayer::SynthWriteReg
//
//==========================================================================
void OPLPlayer::SynthWriteReg (uint32_t bank, uint16_t reg, uint8_t data) {
  reg |= bank<<8;
  OPL3_WriteReg(&chip, reg, data);
}


//==========================================================================
//
//  OPLPlayer::SynthVoiceOff
//
//==========================================================================
void OPLPlayer::SynthVoiceOff (SynthVoice* voice) {
  SynthWriteReg(voice->bank, (uint16_t)(0xb0+voice->ch_base), (uint8_t)(voice->freq>>8));
  voice->freq = 0;
  for (uint32_t i = 0; i < mSynthVoicesAllocatedNum; ++i) {
    if (mSynthVoicesAllocated[i] == voice) {
      for (uint32_t j = i; j < mSynthVoicesAllocatedNum-1; ++j) {
        mSynthVoicesAllocated[j] = mSynthVoicesAllocated[j+1];
      }
      break;
    }
  }
  --mSynthVoicesAllocatedNum;
  mSynthVoicesFree[mSynthVoicesFreeNum++] = voice;
}


//==========================================================================
//
//  OPLPlayer::SynthVoiceFreq
//
//==========================================================================
void OPLPlayer::SynthVoiceFreq (SynthVoice* voice) {
  int32_t freq = voice->chan->pitch+voice->finetune+32*voice->note;
  uint32_t block = 0;

  if (freq < 0) {
    freq = 0;
  } else if (freq >= 284) {
    freq -= 284;
    block = freq/384;
    if (block > 7) block = 7;
    freq %= 384;
    freq += 284;
  }

  freq = (block<<10)|opl_freqtable[freq];

  SynthWriteReg(voice->bank, 0xa0+voice->ch_base, freq&0xff);
  SynthWriteReg(voice->bank, (uint16_t)(0xb0+voice->ch_base), (uint8_t)((freq>>8)|0x20));

  voice->freq = freq;
}


//==========================================================================
//
//  OPLPlayer::SynthVoiceVolume
//
//==========================================================================
void OPLPlayer::SynthVoiceVolume (SynthVoice* voice) {
  uint8_t volume = (uint8_t)(0x3f-(voice->chan->volume_t*opl_voltable[voice->velocity])/256);
  if ((voice->tl[0]&0x3f) != volume) {
    voice->tl[0] = (voice->tl[0]&0xc0)|volume;
    SynthWriteReg(voice->bank, 0x43+voice->op_base, voice->tl[0]);
    if (voice->additive) {
      uint8_t volume2 = (uint8_t)(0x3f-voice->additive);
      if (volume2 < volume) volume2 = volume;
      volume2 |= voice->tl[1]&0xc0;
      if (volume2 != voice->tl[1]) {
        voice->tl[1] = volume2;
        SynthWriteReg(voice->bank, 0x40+voice->op_base, voice->tl[1]);
      }
    }
  }
}


//==========================================================================
//
//  OPLPlayer::SynthOperatorSetup
//
//==========================================================================
uint8_t OPLPlayer::SynthOperatorSetup (uint32_t bank, uint32_t base, const GenMidi::Operator* op, bool volume) {
  uint8_t tl = op->ksl;
  if (volume) tl |= 0x3f; else tl |= op->level;
  SynthWriteReg(bank, (uint16_t)(0x40+base), tl);
  SynthWriteReg(bank, (uint16_t)(0x20+base), op->mult);
  SynthWriteReg(bank, (uint16_t)(0x60+base), op->attack);
  SynthWriteReg(bank, (uint16_t)(0x80+base), op->sustain);
  SynthWriteReg(bank, (uint16_t)(0xE0+base), op->wave);
  return tl;
}


//==========================================================================
//
//  OPLPlayer::SynthVoiceOn
//
//==========================================================================
void OPLPlayer::SynthVoiceOn (SynthMidiChannel* channel, const GenMidi::Patch* patch, bool dual, uint8_t key, uint8_t velocity) {
  SynthVoice* voice;
  const GenMidi::Voice* voice_data;
  uint32_t bank;
  uint32_t base;
  int32_t note;

  if (mSynthVoicesFreeNum == 0) return;

  voice = mSynthVoicesFree[0];

  --mSynthVoicesFreeNum;

  for (uint32_t i = 0; i < mSynthVoicesFreeNum; ++i) mSynthVoicesFree[i] = mSynthVoicesFree[i+1];

  mSynthVoicesAllocated[mSynthVoicesAllocatedNum++] = voice;

  voice->chan = channel;
  voice->key = key;
  voice->velocity = velocity;
  voice->patch = patch;
  voice->voice_dual = dual;

  if (dual) {
    voice_data = &patch->voice[1];
    voice->finetune = (int32_t)(patch->finetune>>1)-64;
  } else {
    voice_data = &patch->voice[0];
    voice->finetune = 0;
  }

  voice->pan = channel->reg_pan;

  if (voice->voice_data != voice_data) {
    voice->voice_data = voice_data;
    bank = voice->bank;
    base = voice->op_base;

    voice->tl[0] = SynthOperatorSetup(bank, base+3, &voice_data->car, true);

    if (voice_data->mod.feedback&1) {
      voice->additive = (uint8_t)(0x3f-voice_data->mod.level);
      voice->tl[1] = SynthOperatorSetup(bank, base, &voice_data->mod, true);
    } else {
      voice->additive = 0;
      voice->tl[1] = SynthOperatorSetup(bank, base, &voice_data->mod, false);
    }
  }

  SynthWriteReg(voice->bank, 0xc0+voice->ch_base, voice_data->mod.feedback|voice->pan);

  if (MISC_Read16LE(patch->flags)&GenMidi::Patch::Flag::Fixed) {
    note = patch->note;
  } else {
    if (channel->drum) {
      note = 60;
    } else {
      note = key;
      note += (int16_t)MISC_Read16LE((uint16_t)voice_data->offset);
      while (note < 0) note += 12;
      while (note > 95) note -= 12;
    }
  }
  voice->note = (uint8_t)note;

  SynthVoiceVolume(voice);
  SynthVoiceFreq(voice);
}


//==========================================================================
//
//  OPLPlayer::SynthKillVoice
//
//==========================================================================
void OPLPlayer::SynthKillVoice () {
  SynthVoice* voice;
  if (mSynthVoicesFreeNum > 0) return;
  voice = mSynthVoicesAllocated[0];
  for (uint32_t i = 0; i < mSynthVoicesAllocatedNum; i++) {
    if (mSynthVoicesAllocated[i]->voice_dual || mSynthVoicesAllocated[i]->chan >= voice->chan) {
      voice = mSynthVoicesAllocated[i];
    }
  }
  SynthVoiceOff(voice);
}


//==========================================================================
//
//  OPLPlayer::SynthNoteOff
//
//==========================================================================
void OPLPlayer::SynthNoteOff (SynthMidiChannel* channel, uint8_t note) {
  for (uint32_t i = 0; i < mSynthVoicesAllocatedNum; ) {
    if (mSynthVoicesAllocated[i]->chan == channel && mSynthVoicesAllocated[i]->key == note) {
      SynthVoiceOff(mSynthVoicesAllocated[i]);
    } else {
      ++i;
    }
  }
}


//==========================================================================
//
//  OPLPlayer::SynthNoteOn
//
//==========================================================================
void OPLPlayer::SynthNoteOn (SynthMidiChannel* channel, uint8_t note, uint8_t velo) {
  const GenMidi::Patch* patch;

  if (velo == 0) {
    SynthNoteOff(channel, note);
    return;
  }

  if (channel->drum) {
    if (note < 35 || note > 81) return;
    patch = &mGenMidi.patch[note-35+128];
  } else {
    patch = channel->patch;
  }

  SynthKillVoice();

  SynthVoiceOn(channel, patch, false, note, velo);

  if (mSynthVoicesFreeNum > 0 && MISC_Read16LE(patch->flags)&GenMidi::Patch::Flag::DualVoice) {
    SynthVoiceOn(channel, patch, true, note, velo);
  }
}


//==========================================================================
//
//  OPLPlayer::SynthPitchBend
//
//==========================================================================
void OPLPlayer::SynthPitchBend (SynthMidiChannel* channel, uint8_t pitch) {
  SynthVoice* mSynthChannelVoices[18];
  SynthVoice* mSynthOtherVoices[18];

  uint32_t cnt1 = 0;
  uint32_t cnt2 = 0;

  channel->pitch = pitch;

  for (uint32_t i = 0; i < mSynthVoicesAllocatedNum; ++i) {
    if (mSynthVoicesAllocated[i]->chan == channel) {
      SynthVoiceFreq(mSynthVoicesAllocated[i]);
      mSynthChannelVoices[cnt1++] = mSynthVoicesAllocated[i];
    } else {
      mSynthOtherVoices[cnt2++] = mSynthVoicesAllocated[i];
    }
  }

  for (uint32_t i = 0; i < cnt2; ++i) mSynthVoicesAllocated[i] = mSynthOtherVoices[i];
  for (uint32_t i = 0; i < cnt1; ++i) mSynthVoicesAllocated[i+cnt2] = mSynthChannelVoices[i];
}


//==========================================================================
//
//  OPLPlayer::SynthUpdatePatch
//
//==========================================================================
void OPLPlayer::SynthUpdatePatch (SynthMidiChannel* channel, uint8_t patch) {
  if (patch >= GenMidi::PatchCount) patch = 0;
  channel->patch = &mGenMidi.patch[patch];
}


//==========================================================================
//
//  OPLPlayer::SynthUpdatePan
//
//==========================================================================
void OPLPlayer::SynthUpdatePan (SynthMidiChannel* channel, uint8_t pan) {
  uint8_t new_pan = 0x30;
  if (pan <= 48) new_pan = 0x20; else if (pan >= 96) new_pan = 0x10;
  channel->pan = pan;
  if (channel->reg_pan != new_pan) {
    channel->reg_pan = new_pan;
    for (uint32_t i = 0; i < mSynthVoicesAllocatedNum; ++i) {
      if (mSynthVoicesAllocated[i]->chan == channel) {
        mSynthVoicesAllocated[i]->pan = new_pan;
        SynthWriteReg(mSynthVoicesAllocated[i]->bank,
            0xc0+mSynthVoicesAllocated[i]->ch_base,
            mSynthVoicesAllocated[i]->voice_data->mod.feedback|new_pan);
      }
    }
  }
}


//==========================================================================
//
//  OPLPlayer::SynthUpdateVolume
//
//==========================================================================
void OPLPlayer::SynthUpdateVolume (SynthMidiChannel* channel, uint8_t volume) {
  if (volume&0x80) volume = 0x7f;
  if (channel->volume != volume) {
    channel->volume = volume;
    channel->volume_t = opl_voltable[channel->volume]+1;
    for (uint32_t i = 0; i < mSynthVoicesAllocatedNum; ++i) {
      if (mSynthVoicesAllocated[i]->chan == channel) {
        SynthVoiceVolume(mSynthVoicesAllocated[i]);
      }
    }
  }
}


//==========================================================================
//
//  OPLPlayer::SynthNoteOffAll
//
//==========================================================================
void OPLPlayer::SynthNoteOffAll (SynthMidiChannel* channel) {
  for (uint32_t i = 0; i < mSynthVoicesAllocatedNum; ) {
    if (mSynthVoicesAllocated[i]->chan == channel) {
      SynthVoiceOff(mSynthVoicesAllocated[i]);
    } else {
      ++i;
    }
  }
}


//==========================================================================
//
//  OPLPlayer::SynthEventReset
//
//==========================================================================
void OPLPlayer::SynthEventReset (SynthMidiChannel* channel) {
  SynthNoteOffAll(channel);
  channel->reg_pan = 0x30;
  channel->pan = 64;
  channel->pitch = 64;
}


//==========================================================================
//
//  OPLPlayer::SynthReset
//
//==========================================================================
void OPLPlayer::SynthReset () {
  for (uint8_t i = 0; i < 16; ++i) {
    SynthNoteOffAll(&mSynthMidiChannels[i]);
    SynthResetMidi(mSynthMidiChannels[i]);
  }
}


//==========================================================================
//
//  OPLPlayer::SynthWrite
//
//==========================================================================
void OPLPlayer::SynthWrite (uint8_t command, uint8_t data1, uint8_t data2) {
  SynthMidiChannel* channel = &mSynthMidiChannels[command&0x0f];
  command >>= 4;
  switch (command) {
    case SynthCmd::NoteOff: SynthNoteOff(channel, data1); break;
    case SynthCmd::NoteOn: SynthNoteOn(channel, data1, data2); break;
    case SynthCmd::PitchBend: SynthPitchBend(channel, data2); break;
    case SynthCmd::Patch: SynthUpdatePatch(channel, data1); break;
    case SynthCmd::Control:
      switch (data1) {
        case SynthCtl::Volume: SynthUpdateVolume(channel, data2); break;
        case SynthCtl::Pan: SynthUpdatePan(channel, data2); break;
        case SynthCtl::AllNoteOff: SynthNoteOffAll(channel); break;
        case SynthCtl::Reset: SynthEventReset(channel); break;
        default: break;
      }
      break;
    default: break;
  }
}


//==========================================================================
//
//  OPLPlayer::MIDI_ReadDelay
//
//  ////////////////////////////////////////////////////////////////////// //
//  MIDI
//
//==========================================================================
uint32_t OPLPlayer::MIDI_ReadDelay (const uint8_t** data) {
  const uint8_t* dn = *data;
  uint32_t delay = 0;
  do {
    delay = (delay<<7)|((*dn)&0x7f);
  } while (*dn++&0x80);
  *data = dn;
  return delay;
}


//==========================================================================
//
//  OPLPlayer::MIDI_LoadSong
//
//==========================================================================
bool OPLPlayer::MIDI_LoadSong () {
  if (songlen <= sizeof(MidiHeader)) return false;
  const MidiHeader* mid = (const MidiHeader *)songdata;

  if (memcmp(mid->header, "MThd", 4) != 0 || MISC_Read32BE(mid->length) != 6) return false;

  mMidiCount = MISC_Read16BE(mid->count);
  const uint8_t *midi_data = mid->data;
  int32_t midilen = (int32_t)songlen-(int32_t)sizeof(MidiHeader);
  mMidiTimebase = MISC_Read16BE(mid->time);

  if (mMidiTracks != nullptr) ::free(mMidiTracks);
  mMidiTracksCount = mMidiCount;

  mMidiTracks = (Track *)malloc(mMidiCount*sizeof(Track));
  memset(mMidiTracks, 0, mMidiCount*sizeof(Track));

  uint32_t trknum = 0;
  while (trknum < mMidiCount) {
    if (midilen < 8) { delete mMidiTracks; return false; } // out of data
    const MidiTrack* track = (const MidiTrack *)midi_data;
    uint32_t datasize = MISC_Read32BE(track->length);
    if (midilen-8 < (int32_t)datasize) { delete mMidiTracks; return false; } // out of data
    if (memcmp(track->header, "MTrk", 4) != 0) {
      // not a track, skip this chunk
      //midi_data = midi_data[datasize+8..$];
      midi_data += datasize+8;
      midilen -= datasize+8;
    } else {
      // track
      mMidiTracks[trknum].length = datasize;
      mMidiTracks[trknum].data = track->data;
      mMidiTracks[trknum].num = trknum;
      ++trknum;
      // move to next chunk
      //midi_data = midi_data[datasize+8..$];
      midi_data += datasize+8;
      midilen -= datasize+8;
    }
  }
  // check if we have all tracks
  if (trknum != mMidiCount) { delete mMidiTracks; return false; } // out of tracks

  mDataFormat = DataFormat::Midi;

  return true;
}


//==========================================================================
//
//  OPLPlayer::MIDI_StartSong
//
//==========================================================================
bool OPLPlayer::MIDI_StartSong () {
  if (mDataFormat != DataFormat::Midi || mPlayerActive) return false;

  for (uint32_t i = 0; i < mMidiCount; ++i) {
    mMidiTracks[i].pointer = mMidiTracks[i].data;
    mMidiTracks[i].time = MIDI_ReadDelay(&mMidiTracks[i].pointer);
    mMidiTracks[i].lastevent = 0x80;
    mMidiTracks[i].finish = 0;
  }

  for (uint32_t i = 0; i < 16; ++i) mMidiChannels[i] = 0xff;

  mMidiChannelcnt = 0;

  mMidiRate = 1000000/(500000/mMidiTimebase);
  mMidiCallrate = mSongTempo;
  mMidiTimer = 0;
  mMidiTimechange = 0;
  mMidiFinished = 0;

  mPlayerActive = true;

  return true;
}


//==========================================================================
//
//  OPLPlayer::MIDI_StopSong
//
//==========================================================================
void OPLPlayer::MIDI_StopSong () {
  if (mDataFormat != DataFormat::Midi || !mPlayerActive) return;
  mPlayerActive = false;
  for (uint32_t i = 0; i < 16; ++i) {
    SynthWrite((uint8_t)((SynthCmd::Control<<4)|i), SynthCtl::AllNoteOff, 0);
  }
  SynthReset();
}


//==========================================================================
//
//  OPLPlayer::MIDI_NextTrack
//
//==========================================================================
OPLPlayer::Track* OPLPlayer::MIDI_NextTrack () {
  Track* mintrack = &mMidiTracks[0];
  for (uint32_t i = 1; i < mMidiCount; i++) {
    if ((mMidiTracks[i].time < mintrack->time && !mMidiTracks[i].finish) || mintrack->finish) {
      mintrack = &mMidiTracks[i];
    }
  }
  return mintrack;
}


//==========================================================================
//
//  OPLPlayer::MIDI_GetChannel
//
//==========================================================================
uint8_t OPLPlayer::MIDI_GetChannel (uint8_t chan) {
  if (chan == 9) return 15;
  if (mMidiChannels[chan] == 0xff) mMidiChannels[chan] = mMidiChannelcnt++;
  return mMidiChannels[chan];
}


//==========================================================================
//
//  OPLPlayer::MIDI_Command
//
//==========================================================================
void OPLPlayer::MIDI_Command (const uint8_t** datap, uint8_t evnt) {
  uint8_t chan;
  const uint8_t* data;
  uint8_t v1, v2;

  data = *datap;
  chan = MIDI_GetChannel(evnt&0x0f);
  switch (evnt&0xf0) {
    case 0x80:
      v1 = *data++;
      v2 = *data++;
      SynthWrite((SynthCmd::NoteOff<<4)|chan, v1, 0);
      break;
    case 0x90:
      v1 = *data++;
      v2 = *data++;
      SynthWrite((SynthCmd::NoteOn<<4)|chan, v1, v2);
      break;
    case 0xa0:
      data += 2;
      break;
    case 0xb0:
      v1 = *data++;
      v2 = *data++;
      switch (v1) {
        case 0x00: case 0x20: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::Bank, v2); break;
        case 0x01: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::Modulation, v2); break;
        case 0x07: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::Volume, v2); break;
        case 0x0a: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::Pan, v2); break;
        case 0x0b: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::Expression, v2); break;
        case 0x40: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::Sustain, v2); break;
        case 0x43: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::Soft, v2); break;
        case 0x5b: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::Reverb, v2); break;
        case 0x5d: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::Chorus, v2); break;
        case 0x78: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::AllNoteOff, v2); break;
        case 0x79: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::Reset, v2); break;
        case 0x7b: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::AllNoteOff, v2); break;
        case 0x7e: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::MonoMode, v2); break;
        case 0x7f: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::PolyMode, v2); break;
        default: break;
      }
      break;
    case 0xc0:
      v1 = *data++;
      SynthWrite((SynthCmd::Patch<<4)|chan, v1, 0);
      break;
    case 0xd0:
      data += 1;
      break;
    case 0xe0:
      v1 = *data++;
      v2 = *data++;
      SynthWrite((SynthCmd::PitchBend<<4)|chan, v1, v2);
      break;
    default: break;
  }
  *datap = data;
}


//==========================================================================
//
//  OPLPlayer::MIDI_FinishTrack
//
//==========================================================================
void OPLPlayer::MIDI_FinishTrack (Track* trck) {
  if (trck->finish) return;
  trck->finish = true;
  ++mMidiFinished;
}


//==========================================================================
//
//  OPLPlayer::MIDI_AdvanceTrack
//
//==========================================================================
void OPLPlayer::MIDI_AdvanceTrack (Track* trck) {
  uint8_t evnt;
  uint8_t meta;
  uint8_t length;
  uint32_t tempo;
  const uint8_t* data;

  evnt = *trck->pointer++;

  if (!(evnt&0x80)) {
    evnt = trck->lastevent;
    --trck->pointer;
  }

  switch (evnt) {
    case 0xf0:
    case 0xf7:
      length = (uint8_t)MIDI_ReadDelay(&trck->pointer);
      trck->pointer += length;
      break;
    case 0xff:
      meta = *trck->pointer++;
      length = (uint8_t)MIDI_ReadDelay(&trck->pointer);
      data = trck->pointer;
      trck->pointer += length;
      switch (meta) {
        case 0x2f:
          MIDI_FinishTrack(trck);
          break;
        case 0x51:
          if (length == 0x03) {
            tempo = (data[0]<<16)|(data[1]<<8)|data[2];
            mMidiTimechange += (mMidiTimer*mMidiRate)/mMidiCallrate;
            mMidiTimer = 0;
            mMidiRate = 1000000/(tempo/mMidiTimebase);
          }
          break;
        default: break;
      }
      break;
    default:
      MIDI_Command(&trck->pointer,evnt);
      break;
  }

  trck->lastevent = evnt;
  if (trck->pointer >= trck->data+trck->length) MIDI_FinishTrack(trck);
}


//==========================================================================
//
//  OPLPlayer::MIDI_Callback
//
//==========================================================================
void OPLPlayer::MIDI_Callback () {
  Track* trck;

  if (mDataFormat != DataFormat::Midi || !mPlayerActive) return;

  for (;;) {
    trck = MIDI_NextTrack();
    if (trck->finish || trck->time > mMidiTimechange+(mMidiTimer*mMidiRate)/mMidiCallrate) break;
    MIDI_AdvanceTrack(trck);
    if (!trck->finish) trck->time += MIDI_ReadDelay(&trck->pointer);
  }

  ++mMidiTimer;

  if (mMidiFinished == mMidiCount) {
    if (!mPlayLooped) MIDI_StopSong();
    for (uint32_t i = 0; i < mMidiCount; i++) {
      mMidiTracks[i].pointer = mMidiTracks[i].data;
      mMidiTracks[i].time = MIDI_ReadDelay(&mMidiTracks[i].pointer);
      mMidiTracks[i].lastevent = 0x80;
      mMidiTracks[i].finish = 0;
    }

    for (uint32_t i = 0; i < 16; i++) mMidiChannels[i] = 0xff;

    mMidiChannelcnt = 0;

    mMidiRate = 1000000/(500000/mMidiTimebase);
    mMidiTimer = 0;
    mMidiTimechange = 0;
    mMidiFinished = 0;

    SynthReset();
  }
}


// ////////////////////////////////////////////////////////////////////// //
// MUS

//==========================================================================
//
//  OPLPlayer::MUS_Callback
//
//==========================================================================
void OPLPlayer::MUS_Callback () {
  if (mDataFormat != DataFormat::Mus || !mPlayerActive) return;
  while (mMusTimer == mMusTimeend) {
    uint8_t cmd;
    uint8_t evnt;
    uint8_t chan;
    uint8_t data1;
    uint8_t data2;

    cmd = *mMusPointer++;
    chan = cmd&0x0f;
    evnt = (cmd>>4)&7;

    switch (evnt) {
      case 0x00:
        data1 = *mMusPointer++;
        SynthWrite((SynthCmd::NoteOff<<4)|chan, data1, 0);
        break;
      case 0x01:
        data1 = *mMusPointer++;
        if (data1&0x80) {
          data1 &= 0x7f;
          mMusChanVelo[chan] = *mMusPointer++;
        }
        SynthWrite((SynthCmd::NoteOn<<4)|chan, data1, mMusChanVelo[chan]);
        break;
      case 0x02:
        data1 = *mMusPointer++;
        SynthWrite((SynthCmd::PitchBend<<4)|chan, (data1&1)<<6, data1>>1);
        break;
      case 0x03:
      case 0x04:
        data1 = *mMusPointer++;
        data2 = 0;
        if (evnt == 0x04) data2 = *mMusPointer++;
        switch (data1) {
          case 0x00: SynthWrite((SynthCmd::Patch<<4)|chan, data2, 0); break;
          case 0x01: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::Bank, data2); break;
          case 0x02: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::Modulation, data2); break;
          case 0x03: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::Volume, data2); break;
          case 0x04: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::Pan, data2); break;
          case 0x05: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::Expression, data2); break;
          case 0x06: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::Reverb, data2); break;
          case 0x07: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::Chorus, data2); break;
          case 0x08: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::Sustain, data2); break;
          case 0x09: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::Soft, data2); break;
          case 0x0a: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::AllNoteOff, data2); break;
          case 0x0b: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::AllNoteOff, data2); break;
          case 0x0c: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::MonoMode, data2); break;
          case 0x0d: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::PolyMode, data2); break;
          case 0x0e: SynthWrite((SynthCmd::Control<<4)|chan, SynthCtl::Reset, data2); break;
          case 0x0f: break;
          default: break;
        }
        break;
      case 0x05:
        break;
      case 0x06:
        if (!mPlayLooped) {
          MUS_StopSong();
          return;
        }
        mMusPointer = mMusData;
        cmd = 0;
        SynthReset();
        break;
      case 0x07:
        ++mMusPointer;
        break;
      default: break;
    }

    if (cmd&0x80) {
      mMusTimeend += MIDI_ReadDelay(&mMusPointer);
      break;
    }
  }
  ++mMusTimer;
}


//==========================================================================
//
//  OPLPlayer::MUS_LoadSong
//
//==========================================================================
bool OPLPlayer::MUS_LoadSong () {
  if (songlen <= sizeof(MusHeader)) return false;
  const MusHeader* mus = (const MusHeader *)songdata;
  if (memcmp(mus->header, "MUS\x1a", 4) != 0) return false;
  mMusLength = MISC_Read16LE(mus->length);
  uint32_t musofs = MISC_Read16LE(mus->offset);
  if (musofs >= songlen) return false;
  if (songlen-musofs < mMusLength) return false;
  //fprintf(stderr, "MUS! datalen=%u; muslen=%u; musofs=%u; musend=%u\n", (unsigned)songlen, (unsigned)mMusLength, (unsigned)musofs, (unsigned)(musofs+mMusLength));
  mMusData = &((const uint8_t *)songdata)[musofs];
  mDataFormat = DataFormat::Mus;
  return true;
}


//==========================================================================
//
//  OPLPlayer::MUS_StartSong
//
//==========================================================================
bool OPLPlayer::MUS_StartSong () {
  if (mDataFormat != DataFormat::Mus || mPlayerActive) return true;
  mMusPointer = mMusData;
  mMusTimer = 0;
  mMusTimeend = 0;
  mPlayerActive = true;
  return true;
}


//==========================================================================
//
//  OPLPlayer::MUS_StopSong
//
//==========================================================================
void OPLPlayer::MUS_StopSong () {
  if (mDataFormat != DataFormat::Mus || !mPlayerActive) return;
  mPlayerActive = false;
  for (uint32_t i = 0; i < 16; i++) {
    SynthWrite((uint8_t)((SynthCmd::Control<<4)|i), SynthCtl::AllNoteOff, 0);
  }
  SynthReset();
}


//==========================================================================
//
//  OPLPlayer::PlayerInit
//
//==========================================================================
void OPLPlayer::PlayerInit () {
  mSongTempo = DefaultTempo;
  mPlayerActive = false;
  mDataFormat = DataFormat::Unknown;
  mPlayLooped = false;
  mMidiTracks = nullptr;
}


//==========================================================================
//
//  OPLPlayer::loadGenMIDI
//
//==========================================================================
bool OPLPlayer::loadGenMIDI (const void *data, size_t datasize) {
  if (datasize < 8) return false;
  if (memcmp(data, "#OPL_II#", 8) != 0) return false;
  memset((void *)&mGenMidi, 0, sizeof(GenMidi));
  if (datasize > sizeof(GenMidi)+8) datasize = sizeof(GenMidi)+8;
  memcpy((void *)&mGenMidi, ((const uint8_t *)data)+8, datasize-8);
  mGenMidiLoaded = true;
  return true;
}


//==========================================================================
//
//  OPLPlayer::OPLPlayer
//
//==========================================================================
OPLPlayer::OPLPlayer (int32_t asamplerate, bool aopl3mode, bool astereo) {
  mGenMidiLoaded = false;
  songdata = nullptr;
  sendConfig(asamplerate, aopl3mode, astereo);
  SynthInit();
  PlayerInit();
  mOPLCounter = 0;
}


//==========================================================================
//
//  OPLPlayer::sendConfig
//
//==========================================================================
void OPLPlayer::sendConfig (int32_t asamplerate, bool aopl3mode, bool astereo) {
  clearSong();
  if (asamplerate < 4096) asamplerate = 4096;
  if (asamplerate > 96000) asamplerate = 96000;
  mSampleRate = asamplerate;
  OPL3_Reset(&chip, mSampleRate);
  mOPL2Mode = !aopl3mode;
  mStereo = astereo;
  SynthInit();
}


//==========================================================================
//
//  OPLPlayer::clearSong
//
//==========================================================================
void OPLPlayer::clearSong () {
  stop(); // just in case
  mDataFormat = DataFormat::Unknown;
  if (songdata) { ::free(songdata); songdata = nullptr; }
  songlen = 0;
}


//==========================================================================
//
//  OPLPlayer::load
//
//==========================================================================
bool OPLPlayer::load (const void *data, size_t datasize) {
  stop(); // just in case
  clearSong();
  if (!mGenMidiLoaded) return false;
  if (!data || datasize == 0) return false;
  songlen = (uint32_t)datasize;
  songdata = (uint8_t *)malloc(datasize);
  memcpy(songdata, data, datasize);
  if (MUS_LoadSong() || MIDI_LoadSong()) return true;
  clearSong();
  return false;
}


//==========================================================================
//
//  OPLPlayer::play
//
//  returns `false` if song cannot be started (or if it is already playing)
//
//==========================================================================
bool OPLPlayer::play () {
  bool res = false;
  switch (mDataFormat) {
    case DataFormat::Unknown: break;
    case DataFormat::Midi: res = MIDI_StartSong(); break;
    case DataFormat::Mus: res = MUS_StartSong(); break;
  }
  return res;
}


//==========================================================================
//
//  OPLPlayer::stop
//
//==========================================================================
void OPLPlayer::stop () {
  switch (mDataFormat) {
    case DataFormat::Unknown: break;
    case DataFormat::Midi: MIDI_StopSong(); break;
    case DataFormat::Mus: MUS_StopSong(); break;
  }
}


//==========================================================================
//
//  OPLPlayer::generate
//
//  return number of generated *frames*
//  returns 0 if song is complete (and player is not looped)
//
//==========================================================================
uint32_t OPLPlayer::generate (int16_t *buffer, size_t buflen, bool bufIsStereo) {
  if (mDataFormat == DataFormat::Unknown) return 0;
  if (buflen > 0x3fffffffu/64) abort(); //buffer = buffer[0..uint32_t.max/64];
  uint32_t length = (uint32_t)buflen;
  if (mStereo || bufIsStereo) length /= 2;
  if (length < 1) return 0; // oops
  int16_t accm[2];
  uint32_t i = 0;
  while (i < length) {
    if (!mPlayerActive) break;
    while (mOPLCounter >= mSampleRate) {
      if (mPlayerActive) {
        switch (mDataFormat) {
          case DataFormat::Unknown: abort(); //assert(0, "the thing that should not be");
          case DataFormat::Midi: MIDI_Callback(); break;
          case DataFormat::Mus: MUS_Callback(); break;
        }
      }
      mOPLCounter -= mSampleRate;
    }
    mOPLCounter += mSongTempo;
    OPL3_GenerateResampled(&chip, accm);
    if (mStereo) {
      buffer[i*2] = accm[0];
      buffer[i*2+1] = accm[1];
    } else {
      int32_t iv = (accm[0]+accm[1])/2;
      if (iv < -32768) iv = -32768; else if (iv > 32767) iv = 32767;
      if (bufIsStereo) {
        buffer[i*2+0] = buffer[i*2+1] = (int16_t)iv;
      } else {
        buffer[i] = (int16_t)iv;
      }
    }
    ++i;
  }
  return i;
}
