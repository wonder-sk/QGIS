// The highlight outline material's own uniform block.
// Must stay in step with QgsHighlightOutlineUniforms in
// src/3d/engine/materials/qgshighlightoutlinematerial3d.h.

#ifndef QGS_HIGHLIGHT_OUTLINE_MATERIAL_GLSL
#define QGS_HIGHLIGHT_OUTLINE_MATERIAL_GLSL

layout( std140, binding = 3 ) uniform QgsHighlightOutlineBlock
{
  vec4 outlineColor;

  // how far the band reaches out from the edge of a highlighted feature, in pixels
  int outlineWidth;
} material;

#endif // QGS_HIGHLIGHT_OUTLINE_MATERIAL_GLSL
