#version 440

layout( location = 0 ) in vec2 UV;
layout( location = 0 ) out vec4 fragColor;

layout( binding = 4 ) uniform sampler2D billboardTexture;

void main()
{
  // the symbol was rendered into the texture top down, so the vertical coordinate is
  // flipped on the way out rather than the atlas being built the other way up
  fragColor = texture( billboardTexture, vec2( UV.x, 1.0 - UV.y ) );

  if ( fragColor.a < 0.5 )
    discard;
}
