// texture and lightmap coordinate vars (for t/s calculations)

uniform vec3 SAxis;
uniform vec3 TAxis;
uniform float SOffs;
uniform float TOffs;
uniform float TexIW;
uniform float TexIH;
// this is for lightmap
uniform float TexMinS;
uniform float TexMinT;
uniform float CacheS;
uniform float CacheT;

// put these also into fragment shader
varying vec2 TextureCoordinate;
varying vec2 LightmapCoordinate;
