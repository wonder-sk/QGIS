#version 440
#extension GL_GOOGLE_include_directive : require

#include "qgsview.glsl"
#include "qgsdraw.glsl"
#include "qgspointcloudmaterial.glsl"

// Ported from shaders/pointcloud.vert.
//
// The style is a baked variant here rather than a run time flag, because each style reads
// a different set of vertex attributes, and a shader input that no attribute feeds makes
// the graphics pipeline fail to build. TRIANGULATE is baked for the same reason: it is
// what adds the normal attribute. Everything the fragment stage needs is written out
// unconditionally, so that a single fragment shader serves every variant.

layout( location = 0 ) in vec3 vertexPosition;

#if defined( STYLE_COLOR_RAMP ) || defined( STYLE_CLASSIFICATION )
layout( location = 1 ) in float vertexParameter;
#endif

#ifdef STYLE_RGB
layout( location = 2 ) in vec3 vertexColor;
#endif

#ifdef STYLE_CLASSIFICATION
// the per-point size the classification renderer overrides the symbol's size with
layout( location = 3 ) in float vertexSize;
#endif

#ifdef TRIANGULATE
layout( location = 4 ) in vec3 vertexNormal;
#endif

layout( location = 0 ) out vec3 worldPosition;
layout( location = 1 ) out vec3 worldNormal;
layout( location = 2 ) out vec3 pointColor;
layout( location = 3 ) out float pointParameter;
layout( location = 4 ) flat out float classParameter;

void main()
{
  worldPosition = vec3( draw.modelMatrix * vec4( vertexPosition, 1.0 ) );

#ifdef TRIANGULATE
  // the normals the triangulator produced are already in world space
  worldNormal = vertexNormal;
#else
  worldNormal = vec3( 0.0, 0.0, 1.0 );
#endif

#ifdef STYLE_RGB
  pointColor = vertexColor;
#else
  pointColor = vec3( 1.0 );
#endif

#if defined( STYLE_COLOR_RAMP ) || defined( STYLE_CLASSIFICATION )
  pointParameter = vertexParameter;
  classParameter = vertexParameter;
#else
  pointParameter = 0.0;
  classParameter = 0.0;
#endif

  // only the point drawn path has a point size to set at all; a triangulated chunk is
  // drawn with triangle topology, where writing gl_PointSize is at best ignored
#ifndef TRIANGULATE
#ifdef STYLE_CLASSIFICATION
  gl_PointSize = vertexSize;
#else
  gl_PointSize = material.pointSize;
#endif
#endif

  gl_Position = draw.modelViewProjectionMatrix * vec4( vertexPosition, 1.0 );
}
