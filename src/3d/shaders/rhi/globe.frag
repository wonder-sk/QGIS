#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsview.glsl"
#include "qgsglobematerial.glsl"

layout( location = 0 ) in vec3 worldPosition;
layout( location = 1 ) in vec3 worldNormal;
layout( location = 2 ) in vec2 texCoord;

layout( location = 0 ) out vec4 fragColor;

layout( binding = 4 ) uniform sampler2D diffuseTexture;

void main()
{
  // the globe's texture is darkened towards the edges of the ellipsoid, which is what
  // makes it read as a sphere rather than as a flat disc. How far the surface turns away
  // from the camera is what decides how dark it gets.
  vec3 cameraWorldPos = vec3( view.inverseViewMatrix[3] );
  vec3 viewDir = normalize( cameraWorldPos - worldPosition );

  // the constants below are artistic choices rather than anything physical

  // only shade from far away, so that a close view does not darken the texture, for
  // instance when looking towards the horizon
  float distFromCamera = length( cameraWorldPos - worldPosition );
  float shadingFactor = smoothstep( 2e5, 5e5, distFromCamera );

  float diff = max( dot( worldNormal, viewDir ), 0.0 );

  fragColor = texture( diffuseTexture, texCoord ) * ( 0.3 + 0.7 * mix( 1.0, diff, shadingFactor ) );
}
