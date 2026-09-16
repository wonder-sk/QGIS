#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsview.glsl"
#include "qgsdraw.glsl"
#include "qgsphongtexturedmaterial.glsl"

layout( location = 0 ) in vec3 vertexPosition;
layout( location = 1 ) in vec3 vertexNormal;
layout( location = 2 ) in vec2 vertexTexCoord;

#ifdef DATA_DEFINED_TEXTURE_TRANSFORMS
// per-vertex texture transform: offset in xy, scale in z, rotation in degrees in w
layout( location = 3 ) in vec4 ddTextureTransform;
#endif

#ifdef INSTANCING
layout( location = 4 ) in vec3 instanceTranslation;
#ifdef USE_INSTANCE_SCALE
layout( location = 5 ) in vec3 instanceScale;
#endif
#ifdef USE_INSTANCE_ROTATION
layout( location = 6 ) in vec4 instanceRotation;
#endif
#endif

layout( location = 0 ) out vec3 worldPosition;
layout( location = 1 ) out vec3 worldNormal;
layout( location = 2 ) out vec2 texCoord;

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

  // instanced meshes carry their own texture coordinates, so the material's texture
  // transform does not apply to them, exactly as it did not under Qt3D
  texCoord = vertexTexCoord;
#else
  vec3 localPosition = vertexPosition;
  vec3 localNormal = vertexNormal;

#ifdef DATA_DEFINED_TEXTURE_TRANSFORMS
  vec2 textureOffset = ddTextureTransform.xy;
  float textureScale = ddTextureTransform.z;
  float textureRotation = ddTextureTransform.w;
#else
  vec2 textureOffset = material.texCoordOffset;
  float textureScale = material.texCoordScale;
  float textureRotation = material.texCoordRotation;
#endif

  // scale and rotate the texture about the offset point
  float rad = radians( textureRotation );
  float c = cos( rad );
  float s = sin( rad );
  mat2 rotation = mat2( c, s, -s, c );

  texCoord = rotation * ( ( vertexTexCoord - textureOffset ) * textureScale ) + textureOffset;
#endif

  // chunk space to world space
  worldPosition = vec3( draw.modelMatrix * vec4( localPosition, 1.0 ) );
  worldNormal = normalize( draw.modelNormalMatrix * localNormal );

  gl_Position = draw.modelViewProjectionMatrix * vec4( localPosition, 1.0 );
}
