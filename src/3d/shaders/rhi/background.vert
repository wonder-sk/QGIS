#version 440

// The gradient behind the scene. Drawn as a full screen quad at the far plane with depth
// writes off, so anything in the scene covers it.

layout( location = 0 ) in vec3 vertexPosition;

layout( location = 0 ) out float height;

void main()
{
  height = vertexPosition.y;
  // z = w puts the quad exactly on the far plane, which passes a LessOrEqual depth test
  // against a cleared depth buffer and fails against anything already drawn
  gl_Position = vec4( vertexPosition.xy, 1.0, 1.0 );
}
