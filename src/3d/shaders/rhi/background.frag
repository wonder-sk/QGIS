#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsbackgroundmaterial.glsl"

layout( location = 0 ) in float height;
layout( location = 0 ) out vec4 fragColor;

void main()
{
  float t = height * 0.5 + 0.5;

  // the ramp is interpolated in sRGB, which is what makes it look even; the colours arrive
  // linear because everything downstream of the forward pass works in linear light
  vec3 topColorSrgb = pow( material.topColor.rgb, vec3( 1.0 / 2.2 ) );
  vec3 bottomColorSrgb = pow( material.bottomColor.rgb, vec3( 1.0 / 2.2 ) );

  vec3 mixedColorSrgb = mix( bottomColorSrgb, topColorSrgb, t );

  fragColor = vec4( pow( mixedColorSrgb, vec3( 2.2 ) ), 1.0 );
}
