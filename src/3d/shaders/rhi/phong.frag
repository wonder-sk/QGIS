#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsview.glsl"
#include "qgsphongmaterial.glsl"
#include "qgsphong.glsl"

layout( location = 0 ) in vec3 worldPosition;
layout( location = 1 ) in vec3 worldNormal;

#ifdef DATA_DEFINED
layout( location = 2 ) in vec3 ambientColor;
layout( location = 3 ) in vec3 diffuseColor;
layout( location = 4 ) in vec3 specularColor;
#endif

layout( location = 0 ) out vec4 fragColor;

void main()
{
  vec3 worldView = normalize( view.eyePosition - worldPosition );

#ifdef DATA_DEFINED
  fragColor = phongFunction(
    vec4( ambientColor, material.opacity ),
    vec4( diffuseColor, material.opacity ),
    vec4( specularColor, material.opacity ),
    material.shininess,
    worldPosition,
    worldView,
    worldNormal );
#else
  fragColor = phongFunction(
    vec4( material.ambientColor.rgb, material.opacity ),
    vec4( material.diffuseColor.rgb, material.opacity ),
    vec4( material.specularColor.rgb, material.opacity ),
    material.shininess,
    worldPosition,
    worldView,
    worldNormal );
#endif
}
