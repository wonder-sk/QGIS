#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsview.glsl"
#include "qgsshadows.glsl"
#include "qgspostprocessingmaterial.glsl"

// The last pass: takes the forward pass's HDR colour and everything computed from it, and
// produces the sRGB image the user sees.
//
// Ported from shaders/postprocess.frag. Its ENABLE_EFFECTS define is gone: the Qt3D
// version needed it because the material preview reused this shader without the extra
// textures, and here a preview simply uses its own pass.

layout( location = 0 ) in vec2 texCoord;
layout( location = 0 ) out vec4 fragColor;

layout( binding = 4 ) uniform sampler2D colorTexture;
layout( binding = 5 ) uniform sampler2D depthTexture;
layout( binding = 6 ) uniform sampler2D ambientOcclusionTexture;
layout( binding = 7 ) uniform sampler2D bloomTexture;

vec3 worldPosFromDepth( float depth )
{
  float z = ( depth - view.clipDepthOffset ) / view.clipDepthScale;

  vec4 clipSpacePosition = vec4( qgsScreenUvToClip( texCoord ), z, 1.0 );
  vec4 viewSpacePosition = view.inverseProjectionMatrix * clipSpacePosition;
  viewSpacePosition /= viewSpacePosition.w;

  vec4 worldSpacePosition = view.inverseViewMatrix * viewSpacePosition;
  worldSpacePosition /= worldSpacePosition.w;

  return worldSpacePosition.xyz;
}

float linearizeDepth( float depth )
{
  float ndc = ( depth - view.clipDepthOffset ) / view.clipDepthScale;
  return ( 2.0 * view.nearPlane * view.farPlane ) / ( view.farPlane + view.nearPlane - ndc * ( view.farPlane - view.nearPlane ) );
}

// eye dome lighting: darken a pixel in proportion to how much nearer it is than the
// pixels around it, which is what gives an unlit point cloud its sense of shape
float edlFactor( vec2 coords )
{
  vec2 texelSize = 2.0 / vec2( textureSize( depthTexture, 0 ) );
  vec2 neighbours[4] = vec2[4]( vec2( -1.0, 0.0 ), vec2( 1.0, 0.0 ), vec2( 0.0, -1.0 ), vec2( 0.0, 1.0 ) );
  float factor = 0.0;
  float centerDepth = linearizeDepth( texture( depthTexture, coords ).r ) / view.farPlane;
  for ( int i = 0; i < 4; i++ )
  {
    vec2 neighbourCoords = coords + material.edlDistance * texelSize * neighbours[i];
    float neighbourDepth = linearizeDepth( texture( depthTexture, neighbourCoords ).r ) / view.farPlane;
    neighbourDepth = ( neighbourDepth == 1.0 ) ? 0.0 : neighbourDepth;
    if ( neighbourDepth != 0.0 )
    {
      if ( centerDepth == 0.0 )
        factor += 1.0;
      else
        factor += max( 0.0, centerDepth - neighbourDepth );
    }
  }
  return factor / 4.0;
}

vec3 acesApprox( vec3 v )
{
  v *= 0.6;
  float a = 2.51;
  float b = 0.03;
  float c = 2.43;
  float d = 0.59;
  float e = 0.14;
  return clamp( ( v * ( a * v + b ) ) / ( v * ( c * v + d ) + e ), 0.0, 1.0 );
}

void main()
{
  vec3 finalColor = texture( colorTexture, texCoord ).rgb;

  if ( shadows.showCascadeSplits != 0 && shadows.renderShadows != 0 )
  {
    float depth = texture( depthTexture, texCoord ).r;
    if ( depth < 1.0 )
      finalColor = mix( finalColor, calcCascadeTint( worldPosFromDepth( depth ) ), 0.5 );
  }

  if ( material.edlEnabled != 0 )
    finalColor *= exp( -edlFactor( texCoord ) * material.edlStrength );

  if ( material.ambientOcclusionEnabled != 0 )
    finalColor *= texture( ambientOcclusionTexture, texCoord ).r;

  finalColor *= exp2( material.exposureAdjustment );

  // bloom comes after exposure but before tone mapping, because it needs HDR range
  if ( material.bloomEnabled != 0 )
    finalColor += texture( bloomTexture, texCoord ).rgb * material.bloomFactor;

  // tone map into [0, 1]
  if ( material.toneMapping == 1 )
  {
    finalColor = acesApprox( finalColor );
  }
  else
  {
    // a hard clamp instead: detail in bright areas is lost, but colours in the 0-1 range
    // come out exactly as they went in, which is what a map wants
    finalColor = min( finalColor, vec3( 1.0 ) );
  }

  // gamma correction is hardcoded to 2.2
  fragColor = vec4( pow( finalColor, vec3( 1.0 / 2.2 ) ), 1.0 );
}
