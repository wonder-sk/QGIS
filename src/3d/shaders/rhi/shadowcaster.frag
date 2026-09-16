#version 440

// The shadow map is a single channel float colour attachment rather than a depth texture,
// because QRhi can only address one layer of a texture array through a colour attachment.
// gl_FragCoord.z is in [0, 1] on every backend, so what lands here is directly comparable
// with what qgsshadows.glsl works out for a fragment being lit.

layout( location = 0 ) out float fragDepth;

void main()
{
  fragDepth = gl_FragCoord.z;
}
