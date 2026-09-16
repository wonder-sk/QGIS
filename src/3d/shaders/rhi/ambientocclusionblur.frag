#version 440

// A 4x4 box blur, just wide enough to smooth out the 4x4 noise pattern the ambient
// occlusion pass rotates its sample kernel with.

layout( location = 0 ) in vec2 texCoord;
layout( location = 0 ) out float fragColor;

layout( binding = 4 ) uniform sampler2D occlusionTexture;

void main()
{
  float result = 0.0;
  ivec2 minCoord = ivec2( 0 );
  ivec2 maxCoord = textureSize( occlusionTexture, 0 ) - ivec2( 1 );
  for ( int x = -2; x < 2; ++x )
  {
    for ( int y = -2; y < 2; ++y )
    {
      // clamped to the texture, otherwise the scene picks up a darkened border
      ivec2 fetchCoord = clamp( ivec2( gl_FragCoord.xy ) + ivec2( x, y ), minCoord, maxCoord );
      result += texelFetch( occlusionTexture, fetchCoord, 0 ).r;
    }
  }
  fragColor = result / 16.0;
}
