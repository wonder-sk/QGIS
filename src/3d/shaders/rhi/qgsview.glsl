// Per-view uniforms, constant for a whole render pass.
// Must stay in step with QgsViewUniforms in src/3d/engine/qgsrenderuniforms.h.

layout( std140, binding = 0 ) uniform QgsViewBlock
{
  mat4 viewMatrix;
  mat4 projectionMatrix;
  mat4 viewProjectionMatrix;
  mat4 inverseViewMatrix;
  mat4 inverseProjectionMatrix;
  vec3 eyePosition;
  float nearPlane;
  float farPlane;
  float aspectRatio;
  float viewportWidth;
  float viewportHeight;
} view;
