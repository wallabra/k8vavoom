// ////////////////////////////////////////////////////////////////////////// //
/* This program gives the 32-bit optimized bitslice implementation of JH using ANSI C

   --------------------------------
   Performance

   Microprocessor: Intel CORE 2 processor (Core 2 Duo Mobile T6600 2.2GHz)
   Operating System: 32-bit Ubuntu 10.04 (Linux kernel 2.6.32-22-generic)
   Speed for long message:
   1) 102.0 cycles/byte   compiler: Intel C++ Compiler 11.1   compilation option: icc -O2
   2) 144.5 cycles/byte   compiler: gcc 4.4.3                 compilation option: gcc -O3

   --------------------------------
   Last Modified: January 16, 2011
*/

typedef vuint8 JH_BitSequence;
typedef vuint32 JH_DataLength;
enum JH_HashReturn { JH_SUCCESS=0, JH_FAIL=1, JH_BAD_HASHLEN=2 };

// divide by 8 to get size in bytes
enum JH_HashSize { JH_224=224, JH_256=256, JH_384=384, JH_512=512 };

// define data alignment
#define JH_DATA_ALIGN16(x) x __attribute__((aligned(16)))

struct JHCtx {
  JH_HashSize hashbitlen; // the message digest size
  vuint32 datasize_in_buffer; // the size of the message remained in buffer; assumed to be multiple of 8bits except for the last partial block at the end of the message
  vuint32 databitlen; // the message size in bits
  JH_DATA_ALIGN16(vuint32 x[8][4]); // the 1024-bit state, ( x[i][0] || x[i][1] || x[i][2] || x[i][3] ) is the ith row of the state in the pseudocode
  vuint8 buffer[64]; // the 512-bit message block to be hashed
};

#undef JH_DATA_ALIGN16


// the API functions

// before hashing a message, initialize the hash state as H0
JH_HashReturn JH_Init (JHCtx *state, JH_HashSize hashbitlen);
// hash each 512-bit message block, except the last partial block
JH_HashReturn JH_Update (JHCtx *state, const JH_BitSequence *data, JH_DataLength databitlen);
// pad the message, process the padded block(s), truncate the hash value H to obtain the message digest
JH_HashReturn JH_Final (JHCtx *state, JH_BitSequence *hashval);

// hash a message
// three inputs: message digest size in bits (hashbitlen); message (data); message length in bits (databitlen)
// one output:   message digest (hashval)
JH_HashReturn JH_Hash (JH_HashSize hashbitlen, const JH_BitSequence *data, JH_DataLength databitlen, JH_BitSequence *hashval);
