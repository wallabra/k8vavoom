#version 120
$include "common/common.inc"

#ifdef VV_AMBIENT_MASKED_WALL
$include "common/texture_vars.vs"
#endif

#ifdef VV_AMBIENT_BRIGHTMAP_WALL
$include "common/texture_vars.vs"
#endif

//#ifdef VV_AMBIENT_GLOW
uniform float FloorZ;
uniform float CeilingZ;

varying float floorHeight;
varying float ceilingHeight;
//#endif


void main () {
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;
#ifdef VV_AMBIENT_MASKED_WALL
  $include "common/texture_calc.vs"
#endif
#ifdef VV_AMBIENT_BRIGHTMAP_WALL
  $include "common/texture_calc.vs"
#endif
//#ifdef VV_AMBIENT_GLOW
  floorHeight = gl_Vertex.z-FloorZ;
  ceilingHeight = CeilingZ-gl_Vertex.z;
//#endif
}
