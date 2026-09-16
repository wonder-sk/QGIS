#version 440

// Packs the depth buffer into an ordinary colour image so that it can be read back.
//
// QGIS needs the depth of a pixel on the CPU to turn a mouse position into a point in the
// scene, which is what every camera navigation gesture is built on. Reading a depth
// texture back directly is not portable, so the value is spread over the three colour
// channels here and put back together by Qgs3DUtils::decodeDepth().

layout( location = 0 ) in vec2 texCoord;
layout( location = 0 ) out vec4 fragColor;

layout( binding = 4 ) uniform sampler2D depthTexture;

void main()
{
  float z = texture( depthTexture, texCoord ).r;
  fragColor.b = float( int( z * 255 ) ) / 255.0;
  z = z * 255.0 - fragColor.b * 255.0;
  fragColor.g = float( int( z * 255 ) ) / 255.0;
  z = z * 255.0 - fragColor.g * 255.0;
  fragColor.r = float( int( z * 255 ) ) / 255.0;
  fragColor.a = 1.0;
}
