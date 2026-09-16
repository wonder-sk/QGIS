// Cascaded shadow maps: the pass's shadow matrices, the shadow map itself, and the
// filtering that turns them into a visibility factor.
//
// Ported from shaders/shadows.inc.frag. Must stay in step with QgsShadowUniforms in
// src/3d/engine/qgsrenderuniforms.h and with QgsShadowRenderPass.
//
// Two differences from the Qt3D version, both forced by QRhi rather than chosen:
//
// - the shadow map is an R32F colour array holding gl_FragCoord.z, not a depth texture
//   array. QRhi's render target description has no layer selector for a depth attachment,
//   only for a colour one, so a cascade per layer has to go through colour.
// - how clip space depth is spelled differs per backend, so the conversion from a
//   cascade's clip coordinates to the value stored in the map goes through
//   view.clipDepthScale rather than the 0.5 * z + 0.5 that desktop GL alone needs.

#ifndef QGS_SHADOWS_GLSL
#define QGS_SHADOWS_GLSL

#include "qgsview.glsl"

// if you change the number of cascades, you also need to update Qgs3D::NUM_SHADOW_CASCADES
const int NUMBER_CASCADES = 4;

layout( std140, binding = 12 ) uniform QgsShadowBlock
{
  // world space to each cascade's clip space, with the backend's clip space correction
  // applied, so that z lines up with what the shadow pass wrote
  mat4 csmMatrices[NUMBER_CASCADES];

  // world space to each cascade's exact bounds, deliberately WITHOUT the clip space
  // correction, so that the inside test below can assume the same [-1, 1] cube everywhere
  mat4 csmBoundsMatrices[NUMBER_CASCADES];

  float shadowBias;
  float maxShadowDistance;
  int renderShadows;
  int shadowLightIndex;

  // for testing only: shade each cascade in its own colour to make the splits visible
  int showCascadeSplits;
} shadows;

layout( binding = 13 ) uniform sampler2DArray shadowTexture;

struct CascadeInfo
{
  int index1;
  int index2;
  float blend; // 0 = use only index1, 1 = use only index2
};

const float CASCADE_BLEND_BAND = 0.1;
// band to fade all shadows over as the max shadow distance is approached, as a proportion
// of the max shadow distance
const float MAX_SHADOW_DISTANCE_FADE_OVER = 0.05;

// from https://github.com/Delt06/unity-pcf-poisson/blob/master/Assets/Shaders/LitPoissonSampling.shader
const int POISSON_SAMPLES = 16;
// how far to spread the shadow samples, which makes shadows softer or sharper.
// Values below 2 look pixelated.
const float filterRadius = 2.0;
const vec2 poissonDisk[16] = vec2[](
    vec2( -0.94201624, -0.39906216 ),
    vec2( 0.94558609, -0.76890725 ),
    vec2( -0.094184101, -0.92938870 ),
    vec2( 0.34495938, 0.29387760 ),
    vec2( -0.91588581, 0.45771432 ),
    vec2( -0.81544232, -0.87912464 ),
    vec2( -0.38277543, 0.27676845 ),
    vec2( 0.97484398, 0.75648379 ),
    vec2( 0.44323325, -0.97511554 ),
    vec2( 0.53742981, -0.47373420 ),
    vec2( -0.26496911, -0.41893023 ),
    vec2( 0.79197514, 0.19090188 ),
    vec2( -0.24188840, 0.99706507 ),
    vec2( -0.81409955, 0.91437590 ),
    vec2( 0.19984126, 0.78641367 ),
    vec2( 0.14383161, -0.14100790 )
);

float qgsShadowRand( vec2 co )
{
  // https://stackoverflow.com/a/4275343
  return fract( sin( dot( co.xy, vec2( 12.9898, 78.233 ) ) ) * 43758.5453 );
}

CascadeInfo calcCascadeInfo( vec3 worldPosition )
{
  CascadeInfo info;
  info.index1 = -1;
  info.index2 = -1;
  info.blend = 1.0;

  for ( int i = 0; i < NUMBER_CASCADES; ++i )
  {
    // map based cascade selection, see
    // https://learn.microsoft.com/en-us/windows/win32/dxtecharts/cascaded-shadow-maps#map-based-cascade-selection
    vec4 projCoords = shadows.csmBoundsMatrices[i] * vec4( worldPosition, 1.0 );
    vec3 ndc = projCoords.xyz / projCoords.w;
    bool insideXY = all( lessThan( abs( ndc.xy ), vec2( 1.0 ) ) );
    bool insideZ = ndc.z > -1.0 && ndc.z < 1.0;
    if ( insideXY && insideZ )
    {
      info.index1 = i;
      if ( i < NUMBER_CASCADES - 1 )
      {
        // when this is not the final cascade, blend smoothly into the next one at the seam
        float maxDist = max( max( abs( ndc.x ), abs( ndc.y ) ), abs( ndc.z ) );
        info.blend = smoothstep( 1.0 - CASCADE_BLEND_BAND, 1.0, maxDist );
        if ( info.blend > 0.0 )
          info.index2 = i + 1;
      }
      break;
    }
  }

  return info;
}

float calcSingleCascadeShadowFactor( int cascadeIndex, vec3 worldPosition )
{
  float layerIndex = float( cascadeIndex );
  mat4 lightMatrix = shadows.csmMatrices[cascadeIndex];
  vec2 texelSize = 1.0 / vec2( textureSize( shadowTexture, 0 ).xy );

  vec4 lightSpacePos = lightMatrix * vec4( worldPosition, 1.0 );
  vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

  vec2 uvCoords = 0.5 * projCoords.xy + 0.5;
  float z = projCoords.z * view.clipDepthScale + view.clipDepthOffset;

  if ( uvCoords.x < texelSize.x || uvCoords.x > 1.0 - texelSize.x ||
       uvCoords.y < texelSize.y || uvCoords.y > 1.0 - texelSize.y || z > 1.0 )
  {
    return 1.0;
  }

  float shadow = 0.0;

  // random angle, in radians, based on the fragment's world position
  float angle = qgsShadowRand( worldPosition.xy ) * 6.28318530718;
  float s = sin( angle );
  float c = cos( angle );
  mat2 rot = mat2( c, -s, s, c );

  // Poisson disk filtering of the shadow map
  for ( int i = 0; i < POISSON_SAMPLES; ++i )
  {
    vec2 offset = rot * poissonDisk[i] * filterRadius;
    vec3 arrayCoord = vec3( uvCoords + offset * texelSize, layerIndex );

    float pcfDepth = texture( shadowTexture, arrayCoord ).r;
    shadow += z - shadows.shadowBias > pcfDepth ? 0.0 : 1.0;
  }
  return shadow / float( POISSON_SAMPLES );
}

float calcVisibilityAfterShadowing( vec3 worldPosition )
{
  CascadeInfo info = calcCascadeInfo( worldPosition );
  if ( info.index1 < 0 )
    return 1.0;

  float shadow1 = calcSingleCascadeShadowFactor( info.index1, worldPosition );

  float finalShadow = shadow1;
  if ( info.index2 >= 0 )
  {
    float shadow2 = calcSingleCascadeShadowFactor( info.index2, worldPosition );
    finalShadow = mix( shadow1, shadow2, info.blend );
  }

  // fade off as the distance to the fragment approaches the maximum shadow distance
  float distToCamera = length( worldPosition - view.eyePosition );
  float fadeStartDistance = shadows.maxShadowDistance * ( 1.0 - MAX_SHADOW_DISTANCE_FADE_OVER );
  float distanceFade = smoothstep( fadeStartDistance, shadows.maxShadowDistance, distToCamera );

  return mix( finalShadow, 1.0, distanceFade );
}

vec3 cascadeTint( int cascadeIndex )
{
  // a unique colour per cascade index
  const vec3 colors[9] = vec3[9](
    vec3( 1.0, 0.2, 0.2 ),
    vec3( 0.2, 1.0, 0.2 ),
    vec3( 0.2, 0.4, 1.0 ),
    vec3( 1.0, 1.0, 0.0 ),
    vec3( 1.0, 0.0, 1.0 ),
    vec3( 0.0, 1.0, 1.0 ),
    vec3( 1.0, 0.5, 0.0 ),
    vec3( 0.5, 0.0, 1.0 ),
    vec3( 1.0, 1.0, 1.0 )
  );
  return colors[clamp( cascadeIndex, 0, 8 )];
}

vec3 calcCascadeTint( vec3 worldPosition )
{
  CascadeInfo info = calcCascadeInfo( worldPosition );
  if ( info.index1 < 0 )
    return vec3( 1.0 );

  vec3 finalColor = cascadeTint( info.index1 );
  if ( info.index2 >= 0 )
    finalColor = mix( finalColor, cascadeTint( info.index2 ), info.blend );

  float distToCamera = length( worldPosition - view.eyePosition );
  float fadeStartDistance = shadows.maxShadowDistance * ( 1.0 - MAX_SHADOW_DISTANCE_FADE_OVER );
  float distanceFade = smoothstep( fadeStartDistance, shadows.maxShadowDistance, distToCamera );

  return mix( finalColor, vec3( 1.0 ), distanceFade );
}

#endif // QGS_SHADOWS_GLSL
