#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsbloommaterial.glsl"

// 9-tap tent filter, following the CoD: Advanced Warfare approach from ACM Siggraph 2014.
// Each level is blended additively onto the next one up, which is what spreads a highlight
// over a wide, smooth halo rather than a hard ring.

layout( location = 0 ) in vec2 texCoord;
layout( location = 0 ) out vec4 fragColor;

layout( binding = 4 ) uniform sampler2D srcTexture;

void main()
{
  float x = material.filterRadius;
  // see https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom#comment-6311220794
  float y = material.filterRadius * material.aspectRatio;

  // Take 9 samples around the current texel:
  // a - b - c
  // d - e - f
  // g - h - i
  // ('e' is the current texel)
  vec3 a = texture( srcTexture, vec2( texCoord.x - x, texCoord.y + y ) ).rgb;
  vec3 b = texture( srcTexture, vec2( texCoord.x, texCoord.y + y ) ).rgb;
  vec3 c = texture( srcTexture, vec2( texCoord.x + x, texCoord.y + y ) ).rgb;

  vec3 d = texture( srcTexture, vec2( texCoord.x - x, texCoord.y ) ).rgb;
  vec3 e = texture( srcTexture, vec2( texCoord.x, texCoord.y ) ).rgb;
  vec3 f = texture( srcTexture, vec2( texCoord.x + x, texCoord.y ) ).rgb;

  vec3 g = texture( srcTexture, vec2( texCoord.x - x, texCoord.y - y ) ).rgb;
  vec3 h = texture( srcTexture, vec2( texCoord.x, texCoord.y - y ) ).rgb;
  vec3 i = texture( srcTexture, vec2( texCoord.x + x, texCoord.y - y ) ).rgb;

  //  1   | 1 2 1 |
  // -- * | 2 4 2 |
  // 16   | 1 2 1 |
  vec3 bloom = e * 4.0;
  bloom += ( b + d + f + h ) * 2.0;
  bloom += ( a + c + g + i );
  bloom *= ( 1.0 / 16.0 );

  fragColor = vec4( bloom, 1.0 );
}
