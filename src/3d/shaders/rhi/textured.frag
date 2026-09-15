#version 440

layout( location = 0 ) in vec2 texCoord;
layout( location = 0 ) out vec4 fragColor;

layout( std140, binding = 2 ) uniform QgsTexturedBlock
{
  float opacity;
} material;

layout( binding = 3 ) uniform sampler2D diffuseTexture;

void main()
{
  vec4 color = texture( diffuseTexture, texCoord );
  fragColor = vec4( color.rgb, color.a * material.opacity );
}
