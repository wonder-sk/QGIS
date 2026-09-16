// The post processing material's own uniform block.
// Must stay in step with QgsPostprocessingUniforms in
// src/3d/engine/materials/qgspostprocessingmaterial3d.h.
//
// Everything in here is a run time flag rather than a baked variant, because none of it
// changes which vertex attributes are read - the pass draws the same two triangles either
// way. The branches are uniform across the whole draw, so they cost close to nothing.

#ifndef QGS_POSTPROCESSING_MATERIAL_GLSL
#define QGS_POSTPROCESSING_MATERIAL_GLSL

layout( std140, binding = 3 ) uniform QgsPostprocessingBlock
{
  int edlEnabled;
  float edlStrength;
  float edlDistance;
  int ambientOcclusionEnabled;

  int bloomEnabled;
  float bloomFactor;
  float exposureAdjustment;
  int toneMapping; // 0 clamp, 1 ACES
} material;

#endif // QGS_POSTPROCESSING_MATERIAL_GLSL
