// The pass's light sources, and the per-light quantities shading models need.
// Must stay in step with QgsLightUniforms in src/3d/engine/qgsrenderuniforms.h.
//
// Ported from shaders/light.inc.frag. The light struct is packed into four vec4s rather
// than written out member by member, because std140 puts each vec3 on a 16 byte boundary
// anyway - spelling the packing out is what makes the C++ side a plain array with no
// padding to guess at.

#ifndef QGS_LIGHTS_GLSL
#define QGS_LIGHTS_GLSL

#include "qgsshadows.glsl"

const int MAX_LIGHTS = 8;
const int TYPE_POINT = 0;
const int TYPE_DIRECTIONAL = 1;
const int TYPE_SPOT = 2;

struct QgsLight
{
  vec4 positionAndType;    // xyz world position, w type
  vec4 colorAndIntensity;  // rgb colour, a intensity
  vec4 directionAndCutOff; // xyz direction, w spot cut-off half angle in degrees
  vec4 attenuation;        // x constant, y linear, z quadratic
};

layout( std140, binding = 2 ) uniform QgsLightBlock
{
  int lightCount;
  QgsLight lights[MAX_LIGHTS];
} lighting;

struct LightParams
{
  vec3 s;                 // light direction vector, from the surface towards the light
  float sDotN;            // cosine of the angle between light direction and surface normal, clamped to >= 0
  float att;              // distance attenuation and spot cone falloff
  float visibilityFactor; // 0 fully shadowed, 1 fully lit
  vec3 h;                 // half vector between light direction and view direction
  float sDotH;            // cosine of the angle between light direction and half vector, clamped to >= 0
  float nDotH;            // cosine of the angle between normal and half vector, clamped to >= 0
};

vec3 lightColor( const in int lightIndex )
{
  return lighting.lights[lightIndex].colorAndIntensity.rgb;
}

float lightIntensity( const in int lightIndex )
{
  return lighting.lights[lightIndex].colorAndIntensity.a;
}

/**
 * Works out how the light at lightIndex reaches a surface point.
 *
 * wPosition is the world position, n the NORMALIZED world normal, and wView the
 * normalized direction from the surface towards the eye.
 */
LightParams calculateLightParams( const in int lightIndex,
                                  const in vec3 wPosition,
                                  const in vec3 n,
                                  const in vec3 wView )
{
  QgsLight light = lighting.lights[lightIndex];
  int type = int( light.positionAndType.w );

  LightParams res;
  res.s = vec3( 0.0 );
  res.att = 1.0;
  res.sDotN = 0.0;
  res.visibilityFactor = 1.0;

  if ( type != TYPE_DIRECTIONAL )
  {
    // point and spot lights. The position is already in world space.
    vec3 sUnnormalized = light.positionAndType.xyz - wPosition;
    res.s = normalize( sUnnormalized );
    res.sDotN = dot( res.s, n );

    if ( res.sDotN > 0.0 )
    {
      if ( light.attenuation.x != 0.0 || light.attenuation.y != 0.0 || light.attenuation.z != 0.0 )
      {
        float dist = length( sUnnormalized );
        res.att = 1.0 / ( light.attenuation.x + light.attenuation.y * dist + light.attenuation.z * dist * dist );
      }

      if ( type == TYPE_SPOT )
      {
        // is the fragment inside the cone?
        if ( degrees( acos( dot( -res.s, light.directionAndCutOff.xyz ) ) ) > light.directionAndCutOff.w )
          res.sDotN = 0.0;
      }
    }
  }
  else
  {
    // directional lights. The direction is already in world space.
    res.s = normalize( -light.directionAndCutOff.xyz );
    res.sDotN = dot( res.s, n );

    // only the one light the shadow pass rendered from casts shadows, which is what keeps
    // a scene to a single shadow map however many lights it has
    if ( shadows.renderShadows != 0 && lightIndex == shadows.shadowLightIndex )
      res.visibilityFactor = calcVisibilityAfterShadowing( wPosition );
  }

  res.h = normalize( res.s + wView );
  res.sDotH = max( dot( res.s, res.h ), 0.0 );
  res.sDotN = max( res.sDotN, 0.0 );
  res.nDotH = max( dot( n, res.h ), 0.0 );
  return res;
}

#endif // QGS_LIGHTS_GLSL
