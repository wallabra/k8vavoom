// ////////////////////////////////////////////////////////////////////////// //
// http://www.samiam.org/rg32/
// RadioGatun 32 class
// slightly modified by Ketmar // Invisible Vector
#include "core.h"


RG32::RG32 () : finished(true) {
}


RG32::RG32 (const void *data, vuint32 datalen) : finished(true) {
  put(data, datalen);
}

void RG32::put (const void *data, vuint32 datalen) {
  if (finished) { finished = false; init(); }
  const vuint8 *in = (const vuint8 *)data;
  while (datalen--) input_char(*in++);
}

void RG32::finish (vuint8 res[BitSize/8]) {
  if (!finished) {
    finished = true;
    input_char(1); // 1 at end
    if (pword == 0 && iword == 0) {
      for (int c = 0; c < 16; ++c) belt();
    } else {
      p[pword%3] = iword;
      // map input to belt and mill
      for (int c = 0; c < 3; ++c) {
        b[c*13] ^= p[c];
        a[16+c] ^= p[c];
      }
      for (int c = 0; c < 17; ++c) belt();
    }
    // get nums
    unsigned dpos = 0;
    for (unsigned f = 0; f < BitSize/32; ++f) {
      if (mplace < 1 || mplace > 2) {
        belt();
        mplace = 1;
      }
      vuint32 out = a[mplace++];
      // endian swap so the test vectors match up
      //out = (out<<24)|((out&0xff00)<<8)|((out&0xff0000)>>8)|(out>>24);
      //return out;
      res[dpos++] = out&0xff;
      res[dpos++] = (out>>8)&0xff;
      res[dpos++] = (out>>16)&0xff;
      res[dpos++] = (out>>24)&0xff;
    }
  } else {
    for (unsigned f = 0; f < BitSize/8; ++f) res[f] = 0;
  }
}


void RG32::init () {
  for (unsigned z = 0; z < 20; ++z) a[z] = 0;
  for (unsigned z = 0; z < 40; ++z) b[z] = 0;
  place = 0;
  iword = 0;
  pword = 0;
  mplace = 0;
}


void RG32::mill () {
  vuint32 A[19];
  //vuint32 x;
  //int y = 0, r = 0, z = 0, q = 0;
  for (unsigned i = 0; i < 19; ++i) {
    unsigned y = (i*7)%19;
    unsigned r = ((i*(i+1))/2)%32;
    unsigned x = a[y]^(a[((y+1)%19)]|(~a[(y+2)%19]));
    A[i] = (x>>r)|(x<<(32-r));
  }
  for (unsigned i = 0; i < 19; ++i) {
    unsigned y = i;
    unsigned z = (i+1)%19;
    unsigned q = (i+4)%19;
    a[i] = A[y]^A[z]^A[q];
  }
  a[0] ^= 1;
}


void RG32::belt () {
  vuint32 q[3];
  //int s = 0, i = 0, v = 0;
  for (unsigned s = 0; s < 3; ++s) q[s] = b[s*13+12];
  for (unsigned i = 12; i > 0; --i) {
    for (unsigned s = 0; s < 3; ++s) {
      unsigned v = i-1;
      if (v < 0) v = 12;
      b[(s*13)+i] = b[(s*13)+v];
    }
  }
  for (unsigned s = 0; s < 3; ++s) b[s*13] = q[s];
  for (unsigned i = 0; i < 12; ++i) {
    unsigned s = (i+1)+(i%3)*13;
    b[s] ^= a[i+1];
  }
  mill();
  for (unsigned i = 0; i < 3; ++i) a[i+13] ^= q[i];
}


void RG32::input_char (vuint8 input) {
  //unsigned q = 0, c = 0, r = 0, done = 0;

  unsigned q = input&0xff;
  ++place;

  if (place == 1) {
    iword |= q;
    return;
  }
  if (place == 2) {
    iword |= q << 8;
    return;
  }
  if (place == 3) {
    iword |= q << 16;
    return;
  }
  if (place > 5) {
    // shouldn't ever happen
    place = 0;
    iword = 0;
    return;
  }

  iword |= q << 24;

  p[pword] = iword;
  iword = 0;
  place = 0;
  ++pword;
  if (pword < 3) return;
  if (pword > 3) {
    // shouldn't ever happen
    pword = 0;
    return;
  }

  // iword and pword done; run the belt and continue
  // Map input to belt and mill
  for (unsigned c = 0; c < 3; ++c) {
    b[c*13] ^= p[c];
    a[16+c] ^= p[c];
    p[c] = 0;
  }
  belt();
  pword = 0;
  place = 0;
}
