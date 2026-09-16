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

  // clip space z to stored depth: depth = z * clipDepthScale + clipDepthOffset.
  // ( 1.0, 0.0 ) where clip depth already runs [0, 1], ( 0.5, 0.5 ) where it runs [-1, 1]
  float clipDepthScale;
  float clipDepthOffset;
} view;

// Clip space xy to the texture coordinates of an attachment another pass rendered into.
//
// The two conventions do not line up by themselves: clip y runs up on every backend QGIS
// uses, while a texture's v runs with the framebuffer's rows, which are bottom up on
// OpenGL and top down everywhere else. So on a top down backend the v of a rendered
// texture runs against clip y, and sampling it the naive way mirrors the whole image.
//
// Any conversion between clip space and the texture coordinates of something a pass
// rendered goes through here: the full screen quads, the ambient occlusion kernel's
// reprojection, and the shadow map lookup.
vec2 qgsClipToScreenUv( vec2 clipXY )
{
  return vec2( clipXY.x, clipXY.y * view.framebufferYSign ) * 0.5 + 0.5;
}

// The inverse of qgsClipToScreenUv(), which is the same transform: the sign is its own
// inverse and so is mapping [-1, 1] to [0, 1] and back.
vec2 qgsScreenUvToClip( vec2 uv )
{
  vec2 clipXY = uv * 2.0 - 1.0;
  return vec2( clipXY.x, clipXY.y * view.framebufferYSign );
}

#endif // QGS_VIEW_GLSL
