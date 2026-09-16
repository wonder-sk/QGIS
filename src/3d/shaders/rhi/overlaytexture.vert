#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsoverlaytexturematerial.glsl"

// A quad placed in a corner of the screen, used to show a texture over the finished scene:
// the 2D map overlay, and the depth map when debugging it.

layout( location = 0 ) in vec3 vertexPosition;

layout( location = 0 ) out vec2 texCoord;

void main()
{
  float y = ( vertexPosition.y + 1.0 ) * 0.5;
  texCoord = vec2( ( vertexPosition.x + 1.0 ) * 0.5, material.flipTextureY != 0 ? 1.0 - y : y );

  vec2 quadTexCoord = material.centerTexCoords + vertexPosition.xy * material.sizeTexCoords;
  gl_Position = vec4( 2.0 * quadTexCoord.x - 1.0, 1.0 - 2.0 * quadTexCoord.y, 0.0, 1.0 );
}
