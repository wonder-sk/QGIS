#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsview.glsl"
#include "qgsambientocclusionmaterial.glsl"

layout( location = 0 ) in vec3 vertexPosition;

layout( location = 0 ) out vec2 texCoord;
// the ray from the eye to this corner of the far plane, which multiplied by a linear depth
// gives the view space position of the fragment
layout( location = 1 ) noperspective out vec3 viewRay;

void main()
{
  texCoord = qgsClipToScreenUv( vertexPosition.xy );

  viewRay = vec3(
    -vertexPosition.x * material.tanHalfFov * view.aspectRatio,
    -vertexPosition.y * material.tanHalfFov,
    1.0 // multiplied by a linear depth later, so z stays at one
  );

  gl_Position = vec4( vertexPosition.xy, 0.0, 1.0 );
}
