#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsview.glsl"
#include "qgsdraw.glsl"
#include "qgsmetalroughmaterial.glsl"

layout( location = 0 ) in vec3 vertexPosition;
layout( location = 1 ) in vec3 vertexNormal;

#ifdef HAS_TEXCOORD
layout( location = 2 ) in vec2 vertexTexCoord;
#endif

#ifdef HAS_TANGENT
layout( location = 3 ) in vec4 vertexTangent;
#endif

#ifdef DATA_DEFINED
layout( location = 4 ) in vec3 dataDefinedBaseColor;
layout( location = 5 ) in vec3 dataDefinedEmissionColor;
#endif

#ifdef DATA_DEFINED_TEXTURE_TRANSFORMS
// per-vertex texture transform: offset in xy, scale in z, rotation in degrees in w
layout( location = 6 ) in vec4 ddTextureTransform;
#endif

#ifdef INSTANCING
layout( location = 7 ) in vec3 instanceTranslation;
#ifdef USE_INSTANCE_SCALE
layout( location = 8 ) in vec3 instanceScale;
#endif
#ifdef USE_INSTANCE_ROTATION
layout( location = 9 ) in vec4 instanceRotation;
#endif
#endif

// the names here have to match the fragment stage's inputs exactly, not just the
// locations: the OpenGL backend links varyings by name, so a shader that runs on Metal or
// Vulkan can still fail to link on OpenGL.
// every variant writes all four of these, whether or not it has an attribute behind them,
// because the fragment stage reads all four and only looks at the ones the feature flags
// say are in use
layout( location = 0 ) out vec3 worldPosition;
layout( location = 1 ) out vec3 worldNormal;
layout( location = 2 ) out vec4 worldTangent;
layout( location = 3 ) out vec2 texCoord;

#ifdef DATA_DEFINED
layout( location = 4 ) out vec3 vsBaseColor;
layout( location = 5 ) out vec3 vsEmissionColor;
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

#ifdef HAS_TANGENT
  vec3 localTangent = rotateByQuat( vertexTangent.xyz * thisInstanceScale, thisInstanceRotation );
  float tangentHandedness = vertexTangent.w;
#else
  vec3 localTangent = vec3( 0.0 );
  float tangentHandedness = 1.0;
#endif
#else
  vec3 localPosition = vertexPosition;
  vec3 localNormal = vertexNormal;

#ifdef HAS_TANGENT
  vec3 localTangent = vertexTangent.xyz;
  float tangentHandedness = vertexTangent.w;
#else
  vec3 localTangent = vec3( 0.0 );
  float tangentHandedness = 1.0;
#endif
#endif

  // chunk space to world space
  worldPosition = vec3( draw.modelMatrix * vec4( localPosition, 1.0 ) );
  worldNormal = normalize( draw.modelNormalMatrix * localNormal );

  if ( length( localTangent ) > 0.0 )
    worldTangent = vec4( normalize( vec3( draw.modelMatrix * vec4( localTangent, 0.0 ) ) ), tangentHandedness );
  else
    worldTangent = vec4( 0.0, 0.0, 0.0, tangentHandedness );

#if defined( INSTANCING ) && defined( HAS_TEXCOORD )
  // instanced meshes carry their own texture coordinates, so the material's texture
  // transform does not apply to them, exactly as it did not under Qt3D
  texCoord = vertexTexCoord;
#elif defined( HAS_TEXCOORD )
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
#else
  texCoord = vec2( 0.0 );
#endif

#ifdef DATA_DEFINED
  vsBaseColor = dataDefinedBaseColor;
  vsEmissionColor = dataDefinedEmissionColor;
#endif

  gl_Position = draw.modelViewProjectionMatrix * vec4( localPosition, 1.0 );
}
