// The unlit material's own uniform block.
// Must stay in step with QgsUnlitUniforms in src/3d/engine/materials/qgsunlitmaterial3d.h.

#ifndef QGS_UNLIT_MATERIAL_GLSL
#define QGS_UNLIT_MATERIAL_GLSL

layout( std140, binding = 3 ) uniform QgsUnlitBlock
{
  vec4 materialColor;

  // instancing only: mesh space to object space, often an axis flip or a glTF node
  // transform. Ignored when the material is not instanced.
  mat4 meshMatrix;

  // instancing only: the rotation and scale used for instances that do not carry their
  // own. w of symbolScale is unused.
  vec4 symbolRotation;
  vec4 symbolScale;
} material;

#endif // QGS_UNLIT_MATERIAL_GLSL
