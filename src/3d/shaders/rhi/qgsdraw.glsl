// Per-draw uniforms, written once per render command into the frame's uniform ring.
// Must stay in step with QgsDrawUniforms in src/3d/engine/qgsrenderuniforms.h.

layout( std140, binding = 1 ) uniform QgsDrawBlock
{
  mat4 modelMatrix;
  mat4 modelViewProjectionMatrix;
  mat3 modelNormalMatrix;
} draw;
