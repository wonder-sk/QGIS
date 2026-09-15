#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsview.glsl"
#include "qgsdraw.glsl"

layout( location = 0 ) in vec3 vertexPosition;
layout( location = 3 ) in vec2 vertexTexCoord;

layout( location = 0 ) out vec2 texCoord;

void main()
{
  texCoord = vertexTexCoord;
  gl_Position = draw.modelViewProjectionMatrix * vec4( vertexPosition, 1.0 );
}
