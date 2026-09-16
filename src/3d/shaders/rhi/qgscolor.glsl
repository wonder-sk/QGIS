// Colour space conversions shared by the materials that sample pictures.

#ifndef QGS_COLOR_GLSL
#define QGS_COLOR_GLSL

/**
 * Converts an sRGB encoded colour to linear, the same curve the hardware applies to an
 * sRGB texture format.
 *
 * Only needed where the backend does not - see QgsRhiEngine::decodesSrgbTextures(). This
 * runs after filtering rather than before it, so it is the poorer of the two paths and is
 * used only when the better one is unavailable.
 */
vec3 qgsSrgbToLinear( vec3 srgb )
{
  bvec3 cutoff = lessThanEqual( srgb, vec3( 0.04045 ) );
  vec3 low = srgb / 12.92;
  vec3 high = pow( ( srgb + vec3( 0.055 ) ) / 1.055, vec3( 2.4 ) );
  return mix( high, low, vec3( cutoff ) );
}

#endif // QGS_COLOR_GLSL
