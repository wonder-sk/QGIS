// The unlit textured material's own uniform block.
// Must stay in step with QgsTexturedUniforms in src/3d/engine/materials/qgstexturematerial3d.h.

#ifndef QGS_TEXTURED_MATERIAL_GLSL
#define QGS_TEXTURED_MATERIAL_GLSL

layout( std140, binding = 3 ) uniform QgsTexturedBlock
{
  float opacity;

  // 1 when the diffuse texture holds sRGB texels the backend did not decode for us
  float decodeSrgb;

  float unused1;
  float unused2;

  // instancing only: mesh space to object space, often an axis flip or a glTF node
  // transform. Ignored when the material is not instanced.
  mat4 meshMatrix;

  // instancing only: the rotation and scale used for instances that do not carry their
  // own. w of symbolScale is unused.
  vec4 symbolRotation;
  vec4 symbolScale;
} material;

#endif // QGS_TEXTURED_MATERIAL_GLSL
