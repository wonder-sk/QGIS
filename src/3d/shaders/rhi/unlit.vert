#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsview.glsl"
#include "qgsdraw.glsl"
#include "qgsunlitmaterial.glsl"

layout( location = 0 ) in vec3 vertexPosition;

#ifdef VERTEX_COLOR
layout( location = 1 ) in vec4 vertexColor;
#endif

#ifdef DATA_DEFINED
layout( location = 2 ) in vec3 dataDefinedBaseColor;
#endif

#ifdef INSTANCING
layout( location = 3 ) in vec3 instanceTranslation;
#ifdef USE_INSTANCE_SCALE
layout( location = 4 ) in vec3 instanceScale;
#endif
#ifdef USE_INSTANCE_ROTATION
layout( location = 5 ) in vec4 instanceRotation;
#endif
#endif

layout( location = 0 ) out vec4 color;

#ifdef INSTANCING
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

  // mesh space to object space, then object space to chunk space by applying the
  // per-instance scale, rotation and translation in that order
  vec3 objectPosition = ( material.meshMatrix * vec4( vertexPosition, 1.0 ) ).xyz;
  vec3 localPosition = rotateByQuat( objectPosition * thisInstanceScale, thisInstanceRotation ) + instanceTranslation;
#else
  vec3 localPosition = vertexPosition;
#endif

  // the colour is resolved here rather than in the fragment stage, so that the fragment
  // shader has no variants at all
#ifdef DATA_DEFINED
  color = vec4( dataDefinedBaseColor, 1.0 );
#elif defined( VERTEX_COLOR )
  color = material.materialColor * vertexColor;
#else
  color = material.materialColor;
#endif

  gl_Position = draw.modelViewProjectionMatrix * vec4( localPosition, 1.0 );
}
