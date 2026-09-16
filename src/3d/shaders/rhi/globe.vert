#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsview.glsl"
#include "qgsdraw.glsl"
#include "qgsglobematerial.glsl"

layout( location = 0 ) in vec3 vertexPosition;
layout( location = 1 ) in vec3 vertexNormal;
layout( location = 2 ) in vec2 vertexTexCoord;

layout( location = 0 ) out vec3 worldPosition;
layout( location = 1 ) out vec3 worldNormal;
layout( location = 2 ) out vec2 texCoord;

void main()
{
  worldPosition = vec3( draw.modelMatrix * vec4( vertexPosition, 1.0 ) );
  worldNormal = normalize( draw.modelNormalMatrix * vertexNormal );

  texCoord = vertexTexCoord * material.texCoordScale;

  gl_Position = draw.modelViewProjectionMatrix * vec4( vertexPosition, 1.0 );
}
