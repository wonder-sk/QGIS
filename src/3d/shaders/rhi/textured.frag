#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgstexturedmaterial.glsl"

layout( location = 0 ) in vec2 texCoord;
layout( location = 0 ) out vec4 fragColor;

layout( binding = 4 ) uniform sampler2D diffuseTexture;

void main()
{
  vec4 color = texture( diffuseTexture, texCoord );
  fragColor = vec4( color.rgb, color.a * material.opacity );
}
