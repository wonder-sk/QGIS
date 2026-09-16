#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsview.glsl"
#include "qgsphongtexturedmaterial.glsl"
#include "qgsphong.glsl"

layout( location = 0 ) in vec3 worldPosition;
layout( location = 1 ) in vec3 worldNormal;
layout( location = 2 ) in vec2 texCoord;

layout( location = 0 ) out vec4 fragColor;

layout( binding = 4 ) uniform sampler2D diffuseTexture;

void main()
{
  // the texture supplies the diffuse colour, the uniforms everything else
  vec4 diffuseColor = vec4( texture( diffuseTexture, texCoord ).rgb, material.opacity );

  vec3 worldView = normalize( view.eyePosition - worldPosition );

  fragColor = phongFunction(
    vec4( material.ambientColor.rgb, material.opacity ),
    diffuseColor,
    vec4( material.specularColor.rgb, material.opacity ),
    material.shininess,
    worldPosition,
    worldView,
    worldNormal );
}
