set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w" )

add_library(timidity STATIC
	common.cpp
	dls1.h
	dls2.h
	gf1.h
	instrum.cpp
	instrum_dls.cpp
	instrum_sf2.cpp
	mix.cpp
	playmidi.cpp
	readmidi.cpp
	resample.cpp
	sf2.h
	tables.cpp
	timidity.cpp
	timidity.h
)
