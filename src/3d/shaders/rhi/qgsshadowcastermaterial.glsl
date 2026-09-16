// The shadow caster material's own uniform block.
// Must stay in step with QgsShadowCasterUniforms in
// src/3d/engine/materials/qgsshadowcastermaterial3d.h.

#ifndef QGS_SHADOW_CASTER_MATERIAL_GLSL
#define QGS_SHADOW_CASTER_MATERIAL_GLSL

layout( std140, binding = 3 ) uniform QgsShadowCasterBlock
{
  // instancing only: mesh space to object space, and the rotation and scale used for
  // instances that do not carry their own. Ignored when the material is not instanced.
  mat4 meshMatrix;
  vec4 symbolRotation;
  vec4 symbolScale;
} material;

#endif // QGS_SHADOW_CASTER_MATERIAL_GLSL
