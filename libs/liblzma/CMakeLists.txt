add_definitions(-DHAVE_INTTYPES_H)
add_definitions(-DHAVE_STDINT_H)
add_definitions(-DHAVE_LIMITS_H)
add_definitions(-DHAVE_STDBOOL_H)
add_definitions(-DHAVE_STRING_H)
add_definitions(-DHAVE_STRINGS_H)
add_definitions(-DHAVE_MEMORY_H)

if(NOT WIN32)
  if(NOT NO_PTHREAD)
    add_definitions(-DMYTHREAD_POSIX)
  else(NOT NO_PTHREAD)
    add_definitions(-DMYTHREAD_C11)
  endif(NOT NO_PTHREAD)
else(NOT WIN32)
  #add_definitions(-DMYTHREAD_VISTA)
  add_definitions(-DMYTHREAD_WIN95)
endif(NOT WIN32)

add_definitions(-DHAVE_CHECK_CRC32)
add_definitions(-DHAVE_CHECK_CRC64)
add_definitions(-DHAVE_CHECK_SHA256)
add_definitions(-DHAVE_DECODER_ARM)
add_definitions(-DHAVE_DECODER_ARMTHUMB)
add_definitions(-DHAVE_DECODER_DELTA)
add_definitions(-DHAVE_DECODER_IA64)
add_definitions(-DHAVE_DECODER_LZMA1)
add_definitions(-DHAVE_DECODER_LZMA2)
add_definitions(-DHAVE_DECODER_POWERPC)
add_definitions(-DHAVE_DECODER_SPARC)
add_definitions(-DHAVE_DECODER_X86)
add_definitions(-DHAVE_ENCODER_ARM)
add_definitions(-DHAVE_ENCODER_ARMTHUMB)
add_definitions(-DHAVE_ENCODER_DELTA)
add_definitions(-DHAVE_ENCODER_IA64)
add_definitions(-DHAVE_ENCODER_LZMA1)
add_definitions(-DHAVE_ENCODER_LZMA2)
add_definitions(-DHAVE_ENCODER_POWERPC)
add_definitions(-DHAVE_ENCODER_SPARC)
add_definitions(-DHAVE_ENCODER_X86)
add_definitions(-DHAVE_MF_BT2)
add_definitions(-DHAVE_MF_BT3)
add_definitions(-DHAVE_MF_BT4)
add_definitions(-DHAVE_MF_HC3)
add_definitions(-DHAVE_MF_HC4)
add_definitions(-DHAVE_SYS_PARAM_H)
add_definitions(-DHAVE_MBRTOWC)
add_definitions(-DHAVE_WCWIDTH)

include_directories(
  tcommon
  api
  common
  check
  lz
  rangecoder
  lzma
  delta
  simple
)

add_library(lzma STATIC
  check/check.c
  check/crc32_table.c
  check/crc32_fast.c
  check/crc64_table.c
  check/crc64_fast.c
  check/sha256.c

  common/common.c
  common/block_util.c
  common/easy_preset.c
  common/filter_common.c
  common/hardware_physmem.c
  common/index.c
  common/stream_flags_common.c
  common/vli_size.c
  common/hardware_cputhreads.c
  common/alone_encoder.c
  common/block_buffer_encoder.c
  common/block_encoder.c
  common/block_header_encoder.c
  common/easy_buffer_encoder.c
  common/easy_encoder.c
  common/easy_encoder_memusage.c
  common/filter_buffer_encoder.c
  common/filter_encoder.c
  common/filter_flags_encoder.c
  common/index_encoder.c
  common/stream_buffer_encoder.c
  common/stream_encoder.c
  common/stream_flags_encoder.c
  common/vli_encoder.c
  common/outqueue.c
  common/stream_encoder_mt.c
  common/alone_decoder.c
  common/auto_decoder.c
  common/block_buffer_decoder.c
  common/block_decoder.c
  common/block_header_decoder.c
  common/easy_decoder_memusage.c
  common/filter_buffer_decoder.c
  common/filter_decoder.c
  common/filter_flags_decoder.c
  common/index_decoder.c
  common/index_hash.c
  common/stream_buffer_decoder.c
  common/stream_decoder.c
  common/stream_flags_decoder.c
  common/vli_decoder.c

  delta/delta_common.c
  delta/delta_encoder.c
  delta/delta_decoder.c

  lz/lz_encoder.c
  lz/lz_encoder_mf.c
  lz/lz_decoder.c

  lzma/lzma_encoder_presets.c
  lzma/lzma_encoder.c
  lzma/lzma_encoder_optimum_fast.c
  lzma/lzma_encoder_optimum_normal.c
  lzma/fastpos_table.c
  lzma/lzma_decoder.c
  lzma/lzma2_encoder.c
  lzma/lzma2_decoder.c

  rangecoder/price_table.c

  simple/simple_coder.c
  simple/simple_encoder.c
  simple/simple_decoder.c
  simple/x86.c
  simple/powerpc.c
  simple/ia64.c
  simple/arm.c
  simple/armthumb.c
  simple/sparc.c
)
