  // difference between position of the light source and position of the fragment
  vec3 fromLightToFragment = LightPos2-VertWorldPos;
  // normalized distance to the point light source
  float distanceToLight = length(fromLightToFragment);
  // normalized direction from light source for sampling
  // (k8: there is no need to do that: this is just a direction, and hardware doesn't require it to be normalized)
  fromLightToFragment = normalize(fromLightToFragment);
  // sample shadow cube map
  vec3 ltfdir;
  ltfdir.x = -fromLightToFragment.x;
  ltfdir.y =  fromLightToFragment.y;
  ltfdir.z =  fromLightToFragment.z;
  //float referenceDistanceToLight = texture(ShadowTexture, ltfdir).r;
  float referenceDistanceToLight = textureCube(ShadowTexture, ltfdir).r;
  /*
  float currentDistanceToLight = (distanceToLight-u_nearFarPlane.x)/(u_nearFarPlane.y-u_nearFarPlane.x);
  currentDistanceToLight = clamp(currentDistanceToLight, 0, 1);
  */

  // dunno which one is better (or even which one is right, lol)
  #if 1
  // 0.026
  // 0.039
  float cosTheta = clamp(dot(Normal, normV2L), 0.0, 1.0);
  //float bias = clamp(0.0065*tan(acos(cosTheta)), 0.0, 0.026); // cosTheta is dot( n,l ), clamped between 0 and 1
  float bias = clamp(BiasMul*tan(acos(cosTheta)), BiasMin, BiasMax); // cosTheta is dot( n,l ), clamped between 0 and 1
  #else
  float biasMod = 1.0-clamp(dot(Normal, normV2L), 0.0, 1.0);
  //float bias = 0.001+0.039*biasMod;
  float bias = clamp(BiasMin+BiasMul*biasMod, 0.0, BiasMax); // cosTheta is dot( n,l ), clamped between 0 and 1
  #endif
  // compare distances to determine whether the fragment is in shadow
  float currentDistanceToLight = distanceToLight/LightRadius;
  if (currentDistanceToLight > referenceDistanceToLight+bias) discard;
