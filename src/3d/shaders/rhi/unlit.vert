#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsview.glsl"
#include "qgsdraw.glsl"

layout( location = 0 ) in vec3 vertexPosition;

#ifdef VERTEX_COLOR
layout( location = 1 ) in vec4 vertexColor;
#endif

#ifdef INSTANCING
layout( location = 2 ) in vec3 instanceTranslation;
#endif

layout( location = 0 ) out vec4 color;

layout( std140, binding = 3 ) uniform QgsUnlitBlock
{
  vec4 materialColor;
} material;

void main()
{
  vec3 position = vertexPosition;

#ifdef INSTANCING
  position += instanceTranslation;
#endif

#ifdef VERTEX_COLOR
  color = material.materialColor * vertexColor;
#else
  color = material.materialColor;
#endif

  gl_Position = draw.modelViewProjectionMatrix * vec4( position, 1.0 );
}
