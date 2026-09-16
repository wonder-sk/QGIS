#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsoverlaytexturematerial.glsl"

layout( location = 0 ) in vec2 texCoord;
layout( location = 0 ) out vec4 fragColor;

layout( binding = 4 ) uniform sampler2D previewTexture;

void main()
{
  // a depth texture shown as it is would be almost uniformly white under a perspective
  // projection; this is for looking at where geometry is, not for measuring it
  if ( material.isDepth != 0 )
    fragColor = vec4( vec3( texture( previewTexture, texCoord ).r ), 1.0 );
  else
    fragColor = vec4( texture( previewTexture, texCoord ).rgb, 1.0 );
}
