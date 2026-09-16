// Per-view uniforms, constant for a whole render pass.
// Must stay in step with QgsViewUniforms in src/3d/engine/qgsrenderuniforms.h.

#ifndef QGS_VIEW_GLSL
#define QGS_VIEW_GLSL

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

  // multiply any dFdy() by this before using it to build a direction in world space: the
  // framebuffer's rows run bottom up on OpenGL and top down everywhere else, so the same
  // derivative comes out with the opposite sign depending on the backend
  float framebufferYSign;
} view;

#endif // QGS_VIEW_GLSL
