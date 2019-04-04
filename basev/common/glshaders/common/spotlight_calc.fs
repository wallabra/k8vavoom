  //float attenuation = 1.0/(1.0+(LightRadius-LightMin)*DistToLight);
  //attenuation *= 255.0;
  //attenuation = 1.0 / (1.0 + light.attenuation * pow(distanceToLight, 2));

  // cone restrictions (affects attenuation)
  vec3 surfaceToLight = normalize(VertToLight);
  float distanceToLight = length(VertToLight);

  //vec3 ConeDirection = vec3(1.0, 0.0, 0.0);
  //float ConeAngle = 50.0;

  //float lightToSurfaceAngle = degrees(acos(dot(-surfaceToLight, normalize(ConeDirection))));
  float lightToSurfaceAngle = degrees(acos(dot(-surfaceToLight, ConeDirection)));

  //if (lightToSurfaceAngle > ConeAngle) attenuation = 0.0;
  //attenuation *= min(0.5, (ConeAngle-lightToSurfaceAngle)/ConeAngle);
  //attenuation *= clamp((ConeAngle-lightToSurfaceAngle)/ConeAngle, 0.0, 1.0);
  //attenuation *= smoothstep(0.0, 1.0, clamp((ConeAngle-lightToSurfaceAngle)/ConeAngle, 0.0, 1.0));
  attenuation *= sin(clamp((ConeAngle-lightToSurfaceAngle)/ConeAngle, 0.0, 1.0)*(3.14159265358979323846/2.0));
