// The Gooch shading model, ported from shaders/gooch.frag, which in turn follows
// http://www.cs.northwestern.edu/~ago820/SIG98/abstract.html

#ifndef QGS_GOOCH_GLSL
#define QGS_GOOCH_GLSL

#include "qgslights.glsl"

/**
 * Shades a surface point with the Gooch model.
 *
 * Unlike the Phong model this deliberately ignores each light's colour, intensity and
 * attenuation: Gooch shading is a technical illustration model, where the tone carries
 * the surface orientation rather than the light's own character. Only the direction of
 * each light matters.
 */
vec3 goochModel( const in vec3 worldPos,
                 const in vec3 worldNormal,
                 const in vec3 worldView,
                 const in vec3 kd,
                 const in vec3 ks,
                 const in vec3 warm,
                 const in vec3 cool,
                 const in float alpha,
                 const in float beta,
                 const in float shininess )
{
  // equation (3): the two extremes the tone is interpolated between
  vec3 kcool = clamp( cool + alpha * kd, 0.0, 1.0 );
  vec3 kwarm = clamp( warm + beta * kd, 0.0, 1.0 );

  vec3 n = normalize( worldNormal );
  vec3 result = vec3( 0.0 );

  for ( int i = 0; i < lighting.lightCount; ++i )
  {
    LightParams light = calculateLightParams( i, worldPos, n, worldView );

    // the tone needs the cosine over its full [-1,1] range, so it cannot use
    // light.sDotN, which calculateLightParams() has already clamped to >= 0
    float sDotN = dot( light.s, n );

    // equation (2): blend the two tones by the angle, mapped onto [0,1]
    vec3 intensity = mix( kcool, kwarm, ( 1.0 + sDotN ) / 2.0 );

    float specular = 0.0;
    if ( sDotN > 0.0 )
    {
      vec3 r = reflect( -light.s, n );
      specular = pow( max( dot( r, worldView ), 0.0 ), shininess );
    }

    result += intensity + ks * specular;
  }

  return result;
}

#endif // QGS_GOOCH_GLSL
