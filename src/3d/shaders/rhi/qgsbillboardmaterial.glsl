// The billboard material's own uniform block.
// Must stay in step with QgsBillboardUniforms in
// src/3d/engine/materials/qgsbillboardmaterial3d.h.

#ifndef QGS_BILLBOARD_MATERIAL_GLSL
#define QGS_BILLBOARD_MATERIAL_GLSL

const uint QGS_BILLBOARD_VERTICAL_OFFSET = 1u;
const uint QGS_BILLBOARD_PERSPECTIVE_SCALE = 2u;

layout( std140, binding = 3 ) uniform QgsBillboardBlock
{
  // the size every billboard is drawn at, in pixels, or in world units when the
  // perspective scale flag is set. Ignored when each instance carries its own size.
  vec2 billboardSize;

  // how far the quad is shifted along its own vertical axis, as a fraction of its height:
  // 0.5 anchors its bottom edge to the vertex, -0.5 its top edge
  float verticalOffset;

  uint featureFlags;
} material;

bool qgsBillboardHasFeature( const in uint flag )
{
  return ( material.featureFlags & flag ) != 0u;
}

#endif // QGS_BILLBOARD_MATERIAL_GLSL
