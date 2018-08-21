// //////////////////////////////////////////////////////////
// xxhash32.h
// Copyright (c) 2016 Stephan Brumme. All rights reserved.
// see http://create.stephan-brumme.com/disclaimer.html
//
// #include <stdint.h> // for uint32_t and uint64_t

/// XXHash (32 bit), based on Yann Collet's descriptions, see http://cyan4973.github.io/xxHash/
/** How to use:
    vuint32 myseed = 0;
    XXHash32 myhash(myseed);
    myhash.put(pointerToSomeBytes,     numberOfBytes);
    myhash.put(pointerToSomeMoreBytes, numberOfMoreBytes); // call put() as often as you like to ...
    // and compute hash:
    vuint32 result = myhash.finish();

    // or all of the above in one single line:
    vuint32 result2 = XXHash32::hash(mypointer, numBytes, myseed);

    Note: my code is NOT endian-aware !
**/
class XXHash32 {
public:
  /// create new XXHash (32 bit)
  /** @param seed your seed value, even zero is a valid seed and e.g. used by LZ4 **/
  explicit XXHash32 (vuint32 seed) {
    state[0] = seed+Prime1+Prime2;
    state[1] = seed+Prime2;
    state[2] = seed;
    state[3] = seed-Prime1;
    bufferSize  = 0;
    totalLength = 0;
  }

  /// put a chunk of bytes
  /** @param  input  pointer to a continuous block of data
      @param  length number of bytes
      @return false if parameters are invalid / zero **/
  bool put (const void *input, vint32 length) {
    // no data ?
    if (!input || length <= 0) return false;

    totalLength += (vuint32)length;
    // byte-wise access
    const vuint8 *data = (const vuint8 *)input;

    // unprocessed old data plus new data still fit in temporary buffer?
    if (bufferSize+(vuint32)length < MaxBufferSize) {
      // just put new data
      while (length-- > 0) buffer[bufferSize++] = *data++;
      return true;
    }

    // point beyond last byte
    const vuint8 *stop = data+(vuint32)length;
    const vuint8 *stopBlock = stop-MaxBufferSize;

    // some data left from previous update?
    if (bufferSize > 0) {
      // make sure temporary buffer is full (16 bytes)
      while (bufferSize < MaxBufferSize) buffer[bufferSize++] = *data++;
      // process these 16 bytes (4x4)
      process(buffer, state[0], state[1], state[2], state[3]);
    }

    // copying state to local variables helps optimizer A LOT
    vuint32 s0 = state[0], s1 = state[1], s2 = state[2], s3 = state[3];
    // 16 bytes at once
    while (data <= stopBlock) {
      // local variables s0..s3 instead of state[0]..state[3] are much faster
      process(data, s0, s1, s2, s3);
      data += 16;
    }
    // copy back
    state[0] = s0; state[1] = s1; state[2] = s2; state[3] = s3;

    // copy remainder to temporary buffer
    bufferSize = stop-data;
    for (vuint32 i = 0; i < bufferSize; ++i) buffer[i] = data[i];

    // done
    return true;
  }

  /// get current hash
  /** @return 32 bit XXHash **/
  vuint32 finish () const {
    vuint32 result = (vuint32)totalLength;

    // fold 128 bit state into one single 32 bit value
    if (totalLength >= MaxBufferSize) {
      result += rotateLeft(state[0],  1)+
                rotateLeft(state[1],  7)+
                rotateLeft(state[2], 12)+
                rotateLeft(state[3], 18);
    } else {
      // internal state wasn't set in put(), therefore original seed is still stored in state2
      result += state[2]+Prime5;
    }

    // process remaining bytes in temporary buffer
    const vuint8 *data = buffer;
    // point beyond last byte
    const vuint8 *stop = data+bufferSize;

    // at least 4 bytes left ? => eat 4 bytes per step
    for (; data+4 <= stop; data += 4) result = rotateLeft(result+*(vuint32*)data*Prime3, 17)*Prime4;

    // take care of remaining 0..3 bytes, eat 1 byte per step
    while (data != stop) result = rotateLeft(result+(*data++)*Prime5, 11)*Prime1;

    // mix bits
    result ^= result>>15;
    result *= Prime2;
    result ^= result>>13;
    result *= Prime3;
    result ^= result>>16;
    return result;
  }

  /// combine constructor, put() and finish() in one static function (C style)
  /** @param  input  pointer to a continuous block of data
      @param  length number of bytes
      @param  seed your seed value, e.g. zero is a valid seed and used by LZ4
      @return 32 bit XXHash **/
  static vuint32 hash (const void* input, vint32 length, vuint32 seed) {
    XXHash32 hasher(seed);
    hasher.put(input, length);
    return hasher.finish();
  }

private:
  /// magic constants :-)
  static const vuint32 Prime1 = 2654435761U;
  static const vuint32 Prime2 = 2246822519U;
  static const vuint32 Prime3 = 3266489917U;
  static const vuint32 Prime4 =  668265263U;
  static const vuint32 Prime5 =  374761393U;

  /// temporarily store up to 15 bytes between multiple put() calls
  static const vuint32 MaxBufferSize = 15+1;

  // internal state and temporary buffer
  vuint32 state[4]; // state[2] == seed if totalLength < MaxBufferSize
  vuint8 buffer[MaxBufferSize];
  vuint32 bufferSize;
  vuint32 totalLength;

  /// rotate bits, should compile to a single CPU instruction (ROL)
  static inline vuint32 rotateLeft(vuint32 x, vuint8 bits) {
    return (x<<bits)|(x>>(32-bits));
  }

  /// process a block of 4x4 bytes, this is the main part of the XXHash32 algorithm
  static inline void process(const void *data, vuint32 &state0, vuint32 &state1, vuint32 &state2, vuint32 &state3) {
    const vuint32 *block = (const vuint32 *)data;
    state0 = rotateLeft(state0+block[0]*Prime2, 13)*Prime1;
    state1 = rotateLeft(state1+block[1]*Prime2, 13)*Prime1;
    state2 = rotateLeft(state2+block[2]*Prime2, 13)*Prime1;
    state3 = rotateLeft(state3+block[3]*Prime2, 13)*Prime1;
  }
};
