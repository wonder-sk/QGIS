#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsbloommaterial.glsl"

// 13-tap filter, following the CoD: Advanced Warfare approach from ACM Siggraph 2014.
// See also https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom by Jorge Jimenez.
// The wide tap pattern is what stops bright sub-pixel highlights from flickering as the
// camera moves.

layout( location = 0 ) in vec2 texCoord;
layout( location = 0 ) out vec4 fragColor;

layout( binding = 4 ) uniform sampler2D srcTexture;

void main()
{
  vec2 srcTexelSize = 1.0 / vec2( textureSize( srcTexture, 0 ) );
  float x = srcTexelSize.x;
  float y = srcTexelSize.y;

  // Take 13 samples around the current texel:
  // a - b - c
  // - j - k -
  // d - e - f
  // - l - m -
  // g - h - i
  // ('e' is the current texel)

  vec3 a = texture( srcTexture, vec2( texCoord.x - 2.0 * x, texCoord.y + 2.0 * y ) ).rgb;
  vec3 b = texture( srcTexture, vec2( texCoord.x, texCoord.y + 2.0 * y ) ).rgb;
  vec3 c = texture( srcTexture, vec2( texCoord.x + 2.0 * x, texCoord.y + 2.0 * y ) ).rgb;

  vec3 d = texture( srcTexture, vec2( texCoord.x - 2.0 * x, texCoord.y ) ).rgb;
  vec3 e = texture( srcTexture, vec2( texCoord.x, texCoord.y ) ).rgb;
  vec3 f = texture( srcTexture, vec2( texCoord.x + 2.0 * x, texCoord.y ) ).rgb;

  vec3 g = texture( srcTexture, vec2( texCoord.x - 2.0 * x, texCoord.y - 2.0 * y ) ).rgb;
  vec3 h = texture( srcTexture, vec2( texCoord.x, texCoord.y - 2.0 * y ) ).rgb;
  vec3 i = texture( srcTexture, vec2( texCoord.x + 2.0 * x, texCoord.y - 2.0 * y ) ).rgb;

  vec3 j = texture( srcTexture, vec2( texCoord.x - x, texCoord.y + y ) ).rgb;
  vec3 k = texture( srcTexture, vec2( texCoord.x + x, texCoord.y + y ) ).rgb;
  vec3 l = texture( srcTexture, vec2( texCoord.x - x, texCoord.y - y ) ).rgb;
  vec3 m = texture( srcTexture, vec2( texCoord.x + x, texCoord.y - y ) ).rgb;

  // weights chosen so that the overlapping sample squares still sum to one
  vec3 bloom = e * 0.125;
  bloom += ( a + c + g + i ) * 0.03125;
  bloom += ( b + d + f + h ) * 0.0625;
  bloom += ( j + k + l + m ) * 0.125;

  fragColor = vec4( bloom, 1.0 );
}
