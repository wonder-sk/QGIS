#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsview.glsl"
#include "qgsgoochmaterial.glsl"
#include "qgsgooch.glsl"

layout( location = 0 ) in vec3 worldPosition;
layout( location = 1 ) in vec3 worldNormal;

#ifdef DATA_DEFINED
layout( location = 2 ) in vec3 diffuseColor;
layout( location = 3 ) in vec3 warmColor;
layout( location = 4 ) in vec3 coolColor;
layout( location = 5 ) in vec3 specularColor;
#endif

layout( location = 0 ) out vec4 fragColor;

void main()
{
  vec3 worldView = normalize( view.eyePosition - worldPosition );

#ifdef DATA_DEFINED
  vec3 kd = diffuseColor;
  vec3 ks = specularColor;
  vec3 warm = warmColor;
  vec3 cool = coolColor;
#else
  vec3 kd = material.diffuseColor.rgb;
  vec3 ks = material.specularColor.rgb;
  vec3 warm = material.warmColor.rgb;
  vec3 cool = material.coolColor.rgb;
#endif

  vec3 color = goochModel( worldPosition, worldNormal, worldView,
                           kd, ks, warm, cool,
                           material.alpha, material.beta, material.shininess );

  fragColor = vec4( color, 1.0 );
}
