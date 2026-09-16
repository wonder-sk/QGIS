#version 440

// The vertex stage every full screen pass shares. Positions arrive already in clip space,
// so there is no camera here at all; the work is all in the fragment stage.
//
// texCoord runs with the framebuffer, not against it: clip y = -1 is memory row 0 on every
// backend, because QRhi's clip space correction and the framebuffer's own row order cancel
// out. So sampling a texture another pass rendered needs no per backend flip.

layout( location = 0 ) in vec3 vertexPosition;

layout( location = 0 ) out vec2 texCoord;

void main()
{
  texCoord = vertexPosition.xy * 0.5 + 0.5;
  gl_Position = vec4( vertexPosition.xy, 0.0, 1.0 );
}
