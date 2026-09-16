#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgshighlightoutlinematerial.glsl"

// Draws a band around whatever the mask pass covered.
//
// Qt3D did this by drawing the highlighted geometry four more times, each a few pixels
// off, with a stencil test that kept only what fell outside the original. That needs a
// stencil buffer, and under QRhi a render target cannot have both a stencil buffer and a
// depth texture the post processing chain can sample - on the Metal backend a depth
// texture carries no stencil at all. Dilating a coverage mask needs neither, costs one
// small texture and one full screen pass instead of four passes over scene geometry, and
// gives the same picture.

layout( location = 0 ) in vec2 texCoord;
layout( location = 0 ) out vec4 fragColor;

layout( binding = 4 ) uniform sampler2D maskTexture;

void main()
{
  // inside the feature the fill has already been drawn, so there is nothing to add
  if ( texture( maskTexture, texCoord ).r > 0.5 )
    discard;

  vec2 texelSize = 1.0 / vec2( textureSize( maskTexture, 0 ) );

  // a square dilation: the band is as wide in the diagonals as along the axes, which is
  // what keeps a corner from looking pinched
  for ( int y = -material.outlineWidth; y <= material.outlineWidth; ++y )
  {
    for ( int x = -material.outlineWidth; x <= material.outlineWidth; ++x )
    {
      if ( texture( maskTexture, texCoord + vec2( x, y ) * texelSize ).r > 0.5 )
      {
        fragColor = material.outlineColor;
        return;
      }
    }
  }

  discard;
}
