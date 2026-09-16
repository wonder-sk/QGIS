// The gradient background material's own uniform block.
// Must stay in step with QgsBackgroundUniforms in
// src/3d/engine/materials/qgsbackgroundmaterial3d.h.

#ifndef QGS_BACKGROUND_MATERIAL_GLSL
#define QGS_BACKGROUND_MATERIAL_GLSL

layout( std140, binding = 3 ) uniform QgsBackgroundBlock
{
  vec4 topColor;
  vec4 bottomColor;
} material;

#endif // QGS_BACKGROUND_MATERIAL_GLSL
