#ifdef VV_TEXTURED_ZDOOM_SHADE
// texture shade; red channel is used as intensity.
// this is how it was done in ZDoom, sadly.
// anyway, one should not shade anything that is not monochrome.
// this "kinda works" for red blood decals too.
vec3 TexShade;

vec4 GetStdTexel (sampler2D smp, vec2 tcoord) {
  vec4 clr = texture2D(smp, tcoord);
  clr.a = clr.r; // red channel is intensity
  clr.rgb = TexShade.rgb;
  return clr;
}

vec4 GetStdTexelSimpleShade (sampler2D smp, vec2 tcoord) {
  vec4 clr = texture2D(smp, tcoord);
  return vec4(255, 255, 255, clr.r);
}

#else
# define GetStdTexel             texture2D
# define GetStdTexelSimpleShade  texture2D
#endif
