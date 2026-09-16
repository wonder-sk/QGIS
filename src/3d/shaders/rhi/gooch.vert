#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsview.glsl"
#include "qgsdraw.glsl"
#include "qgsgoochmaterial.glsl"

layout( location = 0 ) in vec3 vertexPosition;
layout( location = 1 ) in vec3 vertexNormal;

#ifdef DATA_DEFINED
layout( location = 2 ) in vec3 dataDefinedDiffuseColor;
layout( location = 3 ) in vec3 dataDefinedWarmColor;
layout( location = 4 ) in vec3 dataDefinedCoolColor;
layout( location = 5 ) in vec3 dataDefinedSpecularColor;
#endif

#ifdef INSTANCING
layout( location = 6 ) in vec3 instanceTranslation;
#ifdef USE_INSTANCE_SCALE
layout( location = 7 ) in vec3 instanceScale;
#endif
#ifdef USE_INSTANCE_ROTATION
layout( location = 8 ) in vec4 instanceRotation;
#endif
#endif

layout( location = 0 ) out vec3 worldPosition;
layout( location = 1 ) out vec3 worldNormal;

#ifdef DATA_DEFINED
layout( location = 2 ) out vec3 diffuseColor;
layout( location = 3 ) out vec3 warmColor;
layout( location = 4 ) out vec3 coolColor;
layout( location = 5 ) out vec3 specularColor;
#endif

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

  vec3 objectNormal = material.meshNormalMatrix * vertexNormal;
  vec3 localNormal = rotateByQuat( objectNormal / thisInstanceScale, thisInstanceRotation );
#else
  vec3 localPosition = vertexPosition;
  vec3 localNormal = vertexNormal;
#endif

  // chunk space to world space
  worldPosition = vec3( draw.modelMatrix * vec4( localPosition, 1.0 ) );
  worldNormal = normalize( draw.modelNormalMatrix * localNormal );

#ifdef DATA_DEFINED
  diffuseColor = dataDefinedDiffuseColor;
  warmColor = dataDefinedWarmColor;
  coolColor = dataDefinedCoolColor;
  specularColor = dataDefinedSpecularColor;
#endif

  gl_Position = draw.modelViewProjectionMatrix * vec4( localPosition, 1.0 );
}
