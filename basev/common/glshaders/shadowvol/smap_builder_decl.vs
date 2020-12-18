#ifndef VV_SHADOWMAP_SPRITE
# ifndef VV_SMAP_NOBUF
attribute vec3 Position;
# endif
#endif

uniform vec3 LightPos;
uniform mat4 LightMPV;
varying vec3 VertToLight;
