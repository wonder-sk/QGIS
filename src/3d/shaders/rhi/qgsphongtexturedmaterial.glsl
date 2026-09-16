// The textured Phong material's own uniform block, shared by its vertex and fragment stages.
// Must stay in step with QgsPhongTexturedUniforms in
// src/3d/engine/materials/qgsphongtexturedmaterial3d.h.

#ifndef QGS_PHONG_TEXTURED_MATERIAL_GLSL
#define QGS_PHONG_TEXTURED_MATERIAL_GLSL

layout( std140, binding = 3 ) uniform QgsPhongTexturedBlock
{
  vec4 ambientColor;  // rgb used
  vec4 specularColor; // rgb used

  float shininess;
  float opacity;

  // how the diffuse texture is sampled. The rotation is in degrees, and both it and the
  // scale are applied about texCoordOffset.
  float texCoordScale;
  float texCoordRotation;
  vec2 texCoordOffset;
  vec2 unused0;

  // instancing only: mesh space to object space, often an axis flip or a glTF node
  // transform. Ignored when the material is not instanced.
  mat4 meshMatrix;
  mat3 meshNormalMatrix;

  // instancing only: the rotation and scale used for instances that do not carry their
  // own. w of symbolScale is unused.
  vec4 symbolRotation;
  vec4 symbolScale;
} material;

#endif // QGS_PHONG_TEXTURED_MATERIAL_GLSL
