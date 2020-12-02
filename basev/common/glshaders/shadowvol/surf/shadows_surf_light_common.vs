uniform vec3 ViewOrigin;
uniform vec3 LightPos;
#ifdef VV_SHADOW_CHECK_TEXTURE
$include "common/texture_vars.vs"
#endif

/*attribute*/uniform vec3 SurfNormal;
/*attribute*/uniform float SurfDist;

varying vec3 Normal;
varying vec3 VertToLight;
varying float Dist;
varying float VDist;

#ifdef VV_SHADOWMAPS
varying vec3 VertWorldPos;
uniform mat4 LightView;
#endif


void main () {
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;

#ifdef VV_SHADOW_CHECK_TEXTURE
  $include "common/texture_calc.vs"
#endif

  Normal = SurfNormal;

  float LightDist = dot(LightPos, SurfNormal);
  float ViewDist = dot(ViewOrigin, SurfNormal);
  Dist = LightDist-SurfDist;
  VDist = ViewDist-SurfDist;

  VertToLight = LightPos-gl_Vertex.xyz;

#ifdef VV_SHADOWMAPS
  VertWorldPos = (LightView*gl_Vertex).xyz;
#endif
}
