#version 120
$include "common/common.inc"

uniform vec4 Light;

#ifdef VV_AMBIENT_MASKED_WALL
uniform sampler2D Texture;
$include "common/texture_vars.fs"
#endif

#ifdef VV_AMBIENT_BRIGHTMAP_WALL
uniform sampler2D Texture;
uniform sampler2D TextureBM;
$include "common/texture_vars.fs"
#endif


//#ifdef VV_AMBIENT_GLOW
// if a is 0, there is no glow
uniform vec4 GlowColorFloor;
uniform vec4 GlowColorCeiling;

varying float floorHeight;
varying float ceilingHeight;


vec4 calcGlow (vec4 light) {
  float fh = ((128.0-clamp(abs(floorHeight), 0.0, 128.0))*GlowColorFloor.a)/128.0;
  float ch = ((128.0-clamp(abs(ceilingHeight), 0.0, 128.0))*GlowColorCeiling.a)/128.0;
  vec4 lt;
  lt.r = clamp(light.r+GlowColorFloor.r*fh+GlowColorCeiling.r*ch, 0.0, 1.0);
  lt.g = clamp(light.g+GlowColorFloor.g*fh+GlowColorCeiling.g*ch, 0.0, 1.0);
  lt.b = clamp(light.b+GlowColorFloor.b*fh+GlowColorCeiling.b*ch, 0.0, 1.0);
  lt.a = light.a;
  return lt;
}
//#endif


void main () {
//#ifdef VV_AMBIENT_GLOW
  vec4 lt = calcGlow(Light);
//#else
//  vec4 lt = Light;
//#endif
#ifdef VV_AMBIENT_MASKED_WALL
  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  //if (TexColour.a <= 0.01) discard;
  if (TexColour.a < 0.666) discard; //FIXME: only normal and masked walls should go thru this
#endif
#ifdef VV_AMBIENT_BRIGHTMAP_WALL
  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  //if (TexColour.a <= 0.01) discard;
  if (TexColour.a < 0.666) discard; //FIXME: only normal and masked walls should go thru this
  vec4 BMColor = texture2D(TextureBM, TextureCoordinate);
  BMColor.rgb *= BMColor.a;
  lt.r = max(lt.r, BMColor.r);
  lt.g = max(lt.g, BMColor.g);
  lt.b = max(lt.b, BMColor.b);
  //lt.rgb = BMColor.rgb;
#endif
  gl_FragColor = lt;
}
