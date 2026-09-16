// The Phong material's own uniform block, shared by its vertex and fragment stages.
// Must stay in step with QgsPhongUniforms in src/3d/engine/materials/qgsphongmaterial3d.h.

#ifndef QGS_PHONG_MATERIAL_GLSL
#define QGS_PHONG_MATERIAL_GLSL

layout( std140, binding = 3 ) uniform QgsPhongBlock
{
  vec4 ambientColor;        // rgb used
  vec4 diffuseColor;        // rgb used
  vec4 specularColor;       // rgb used
  float shininess;
  float opacity;
  float unused0;
  float unused1;

  // instancing only: mesh space to object space, often an axis flip or a glTF node
  // transform. Ignored when the material is not instanced.
  mat4 meshMatrix;
  mat3 meshNormalMatrix;

  // instancing only: the rotation and scale used for instances that do not carry their
  // own. w of symbolScale is unused.
  vec4 symbolRotation;
  vec4 symbolScale;
} material;

#endif // QGS_PHONG_MATERIAL_GLSL
