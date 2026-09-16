// The globe material's own uniform block, shared by its vertex and fragment stages.
// Must stay in step with QgsGlobeUniforms in src/3d/engine/materials/qgsglobematerial3d.h.

#ifndef QGS_GLOBE_MATERIAL_GLSL
#define QGS_GLOBE_MATERIAL_GLSL

layout( std140, binding = 3 ) uniform QgsGlobeBlock
{
  float texCoordScale; // offset 0
  float unused0;       // offset 4
  float unused1;       // offset 8
  float unused2;       // offset 12
} material;

#endif // QGS_GLOBE_MATERIAL_GLSL
