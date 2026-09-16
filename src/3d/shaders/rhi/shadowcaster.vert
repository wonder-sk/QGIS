#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsview.glsl"
#include "qgsdraw.glsl"
#include "qgsshadowcastermaterial.glsl"

// Position only, deliberately: the shadow map records how far the nearest surface is from
// the light and nothing else, so anything a material would do with normals, colours or
// textures is wasted here. A geometry carrying more attributes than this is fine - only a
// shader reading an attribute the geometry lacks is a problem.

layout( location = 0 ) in vec3 vertexPosition;

#ifdef INSTANCING
layout( location = 3 ) in vec3 instanceTranslation;
#ifdef USE_INSTANCE_SCALE
layout( location = 4 ) in vec3 instanceScale;
#endif
#ifdef USE_INSTANCE_ROTATION
layout( location = 5 ) in vec4 instanceRotation;
#endif

vec3 rotateByQuat( vec3 v, vec4 q )
{
  return v + 2.0 * cross( q.xyz, cross( q.xyz, v ) + q.w * v );
}
#endif

void main()
{
#ifdef INSTANCING
#ifdef USE_INSTANCE_SCALE
  vec3 thisInstanceScale = instanceScale;
#else
  vec3 thisInstanceScale = material.symbolScale.xyz;
#endif

#ifdef USE_INSTANCE_ROTATION
  vec4 thisInstanceRotation = instanceRotation;
#else
  vec4 thisInstanceRotation = material.symbolRotation;
#endif

  vec3 objectPosition = ( material.meshMatrix * vec4( vertexPosition, 1.0 ) ).xyz;
  vec3 localPosition = rotateByQuat( objectPosition * thisInstanceScale, thisInstanceRotation ) + instanceTranslation;
#else
  vec3 localPosition = vertexPosition;
#endif

  gl_Position = draw.modelViewProjectionMatrix * vec4( localPosition, 1.0 );
}
