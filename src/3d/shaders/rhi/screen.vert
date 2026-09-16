#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsview.glsl"

// The vertex stage every full screen pass shares. Positions arrive already in clip space,
// so there is no camera here at all; the work is all in the fragment stage.
//
// texCoord runs with the framebuffer's rows rather than with clip space, because that is
// what sampling an attachment another pass rendered needs. The two only agree where the
// framebuffer is Y up, so the conversion is qgsClipToScreenUv()'s, not a bare 0.5x + 0.5.

layout( location = 0 ) in vec3 vertexPosition;

layout( location = 0 ) out vec2 texCoord;

void main()
{
  texCoord = qgsClipToScreenUv( vertexPosition.xy );
  gl_Position = vec4( vertexPosition.xy, 0.0, 1.0 );
}
