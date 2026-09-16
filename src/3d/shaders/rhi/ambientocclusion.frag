#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsview.glsl"
#include "qgsambientocclusionmaterial.glsl"

layout( location = 0 ) in vec2 texCoord;
layout( location = 1 ) noperspective in vec3 viewRay;

layout( location = 0 ) out float fragColor;

layout( binding = 4 ) uniform sampler2D depthTexture;

float linearizeDepth( float depth )
{
  float ndc = ( depth - view.clipDepthOffset ) / view.clipDepthScale;
  return ( 2.0 * view.nearPlane * view.farPlane ) / ( view.farPlane + view.nearPlane - ndc * ( view.farPlane - view.nearPlane ) );
}

float ssao( vec3 originPos, vec3 noise )
{
  float cx = cos( noise.x );
  float sx = sin( noise.x );
  float cy = cos( noise.y );
  float sy = sin( noise.y );

  float sy_sx = sy * sx;
  float sy_cx = sy * cx;
  float cy_sx = cy * sx;
  float cy_cx = cy * cx;

  // rotate around the x axis, then the y axis. mat3 values are column major.
  mat3 sampleRotation = mat3(
    cy, 0.0, -sy,
    sy_sx, cx, cy_sx,
    sy_cx, -sx, cy_cx
  );

  float occlusion = 0.0;
  for ( int i = 0; i < SSAO_KERNEL_SIZE; ++i )
  {
    vec3 samplePos = sampleRotation * material.kernel[i].xyz;
    samplePos = samplePos * material.radius + originPos;

    // project the sample position into the depth texture
    vec4 offset = view.projectionMatrix * vec4( samplePos, 1.0 );
    offset.xy /= offset.w;
    offset.xy = qgsClipToScreenUv( offset.xy );

    float sampleDepth = linearizeDepth( texture( depthTexture, offset.xy ).r );

    float rangeCheck = smoothstep( 0.0, 1.0, material.radius / abs( originPos.z - sampleDepth ) );
    occlusion += rangeCheck * step( sampleDepth, samplePos.z );
  }

  return min( 1.0, ( 1.0 - ( occlusion / float( SSAO_KERNEL_SIZE ) ) ) / ( 1.0 - material.threshold ) );
}

void main()
{
  // the view space position of this pixel
  float originDepth = linearizeDepth( texture( depthTexture, texCoord ).r );
  vec3 originPos = viewRay * originDepth;

  int noiseIndex = int( gl_FragCoord.x ) % 4 + 4 * ( int( gl_FragCoord.y ) % 4 );
  vec3 noise = material.noise[noiseIndex].xyz * 2.0 * 3.14159265;

  fragColor = pow( ssao( originPos, noise ), material.intensity );
}
