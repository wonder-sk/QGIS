// The metal rough PBR shading model, ported from shaders/metalrough.frag, which came from
// qt3d/src/extras/shaders/gl3/metalrough.inc.frag.
//
// Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
//
// The #ifdefs for anisotropy and the clear coat are branches on material.featureFlags
// here; everything else follows the original closely so that the two can still be read
// side by side. Image based lighting is not ported: it needs a cube map and the
// environment light's spherical harmonics, neither of which the QRhi engine has yet.

#ifndef QGS_METALROUGH_GLSL
#define QGS_METALROUGH_GLSL

#include "qgslights.glsl"
#include "qgsmetalroughmaterial.glsl"

const float QGS_PI = 3.14159265359;

float remapRoughness( const in float roughness )
{
  // As per page 14 of
  // http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr.pdf
  // we remap the roughness to give a more perceptually linear response of "bluriness".
  const float maxSpecPower = 999999.0;
  const float minRoughness = sqrt( 2.0 / ( maxSpecPower + 2.0 ) );
  return max( roughness * roughness, minRoughness );
}

float normalDistribution( const in float nDotH, const in float alpha )
{
  // Trowbridge-Reitz GGX, see https://google.github.io/filament/Filament.md.html
  float alpha2 = alpha * alpha;
  float nDotH2 = nDotH * nDotH;
  float denom = ( nDotH2 * ( alpha2 - 1.0 ) + 1.0 );

  // alpha is already clamped by remapRoughness(), so denom cannot quite reach zero
  return alpha2 / ( QGS_PI * denom * denom );
}

vec3 fresnelFactor( const in vec3 color, const in float cosineFactor )
{
  vec3 f = color;
  vec3 F = f + ( 1.0 - f ) * pow( clamp( 1.0 - cosineFactor, 0.0, 1.0 ), 5.0 );
  return clamp( F, f, vec3( 1.0 ) );
}

float geometrySchlickGGX( const in float nDotV, const in float k )
{
  // see https://learnopengl.com/PBR/Theory, "Geometry function"
  return nDotV / ( nDotV * ( 1.0 - k ) + k );
}

// analytical approximation of the split-sum for the environment BRDF, as per
// https://www.unrealengine.com/blog/physically-based-shading-on-mobile
vec2 environmentBrdfApproximation( const in float roughness, const in float viewDotNormal )
{
  vec4 c0 = vec4( -1.0, -0.0275, -0.572, 0.022 );
  vec4 c1 = vec4( 1.0, 0.0425, 1.04, -0.04 );
  vec4 r = roughness * c0 + c1;
  float a004 = min( r.x * r.x, exp2( -9.28 * viewDotNormal ) ) * r.x + r.y;
  return vec2( -1.04, 1.04 ) * a004 + r.zw;
}

// anisotropic normal distribution function
float normalDistributionAnisotropic( const in float tDotH, const in float bDotH, const in float nDotH, const in float alphaT, const in float alphaB )
{
  float d = tDotH * tDotH / ( alphaT * alphaT ) + bDotH * bDotH / ( alphaB * alphaB ) + nDotH * nDotH;
  return 1.0 / max( QGS_PI * alphaT * alphaB * d * d, 0.0001 );
}

// anisotropic visibility function, replacing the geometric and denominator terms, as per
// https://google.github.io/filament/Filament.md.html, 4.10.15, Listing 16
float visibilityAnisotropic( const in float nDotL, const in float nDotV, const in float tDotV, const in float bDotV, const in float tDotL, const in float bDotL, const in float alphaT, const in float alphaB )
{
  float lambdaV = nDotL * length( vec3( alphaT * tDotV, alphaB * bDotV, nDotV ) );
  float lambdaL = nDotV * length( vec3( alphaT * tDotL, alphaB * bDotL, nDotL ) );
  return max( 0.5 / max( lambdaV + lambdaL, 0.0001 ), 0.0 );
}

float geometricModel( const in float lDotN, const in float vDotN, const in float roughness )
{
  // for direct lighting, using the Epic Unreal Engine 4 fix of k = (roughness + 1)^2 / 8
  float k = ( ( roughness + 1.0 ) * ( roughness + 1.0 ) ) / 8.0;

  // geometrySchlickGGX() names its argument nDotV, but the dot product is commutative
  return geometrySchlickGGX( vDotN, k ) * geometrySchlickGGX( lDotN, k );
}

vec3 specularModel( const in vec3 F0, const in float sDotH, const in float sDotN, const in float vDotN, const in float roughness )
{
  // clamp sDotN and vDotN to a small positive value so that the denominator in the
  // reflection equation cannot go to infinity, and use the clamped values in the
  // geometric factor too so that no seam appears where they were clamped
  float sDotNPrime = max( sDotN, 0.001 );
  float vDotNPrime = max( vDotN, 0.001 );

  vec3 F = fresnelFactor( F0, sDotH );
  float G = geometricModel( sDotNPrime, vDotNPrime, roughness );

  return clamp( F * G / ( 4.0 * sDotNPrime * vDotNPrime ), vec3( 0.0 ), vec3( 1.0 ) );
}

float visibilityKelemen( const in float lDotH )
{
  // https://google.github.io/filament/Filament.md.html#materialsystem/clearcoatmodel, Listing 13
  return 0.25 / max( lDotH * lDotH, 0.00001 );
}

float pow5( const in float x )
{
  float x2 = x * x;
  return x2 * x2 * x;
}

// https://github.com/google/filament/blob/master/shaders/src/surface_brdf.fs
float fresnelSchlick( const in float f0, const in float f90, const in float VoH )
{
  return f0 + ( f90 - f0 ) * pow5( 1.0 - VoH );
}

vec3 adjustF0ForClearCoatInterface( const in vec3 F0 )
{
  // adjust F0 for the clear coat to material interface (IOR 1.5) instead of air to
  // material, see https://google.github.io/filament/Filament.md.html section 4.9.14
  vec3 f0Sqrt = sqrt( clamp( F0, vec3( 0.0 ), vec3( 1.0 ) ) );
  vec3 f0Num = vec3( 1.0 ) - 5.0 * f0Sqrt;
  vec3 f0Den = vec3( 5.0 ) - f0Sqrt;
  vec3 f0Base = ( f0Num * f0Num ) / ( f0Den * f0Den );
  return mix( F0, f0Base, material.clearCoatFactor );
}

vec3 pbrModel( const in int lightIndex,
               const in vec3 wPosition,
               const in vec3 n, // NORMALIZED world normal
               const in vec3 wView,
               const in vec3 t,
               const in vec3 b,
               const in vec3 baseColor,
               const in float metalness,
               const in float roughness,
               const in float alpha,
               const in float ambientOcclusion,
               const in float anisotropy )
{
  vec3 v = wView;
  float vDotN = dot( v, n );

  LightParams light = calculateLightParams( lightIndex, wPosition, n, wView );

  vec3 diffuseColor = ( 1.0 - metalness ) * baseColor * lightColor( lightIndex );
  vec3 diffuse = diffuseColor * light.sDotN / QGS_PI;

  vec3 dielectricColor = vec3( 0.16 * material.reflectance * material.reflectance );
  vec3 F0 = mix( dielectricColor, baseColor, metalness );

  bool clearCoat = qgsHasFeature( QGS_MR_CLEAR_COAT );
  if ( clearCoat )
    F0 = adjustF0ForClearCoatInterface( F0 );

  vec3 specularFactor = vec3( 0.0 );
  if ( light.sDotN > 0.0 )
  {
    if ( qgsHasFeature( QGS_MR_ANISOTROPY ) )
    {
      float alphaT = max( alpha * ( 1.0 + anisotropy ), 0.0001 );
      float alphaB = max( alpha * ( 1.0 - anisotropy ), 0.0001 );

      float D = normalDistributionAnisotropic( dot( t, light.h ), dot( b, light.h ), dot( n, light.h ), alphaT, alphaB );
      float V = visibilityAnisotropic( light.sDotN, vDotN, dot( t, v ), dot( b, v ), dot( t, light.s ), dot( b, light.s ), alphaT, alphaB );
      vec3 F = fresnelFactor( F0, light.sDotH );

      specularFactor = D * V * F;
    }
    else
    {
      specularFactor = specularModel( F0, light.sDotH, light.sDotN, vDotN, roughness );
      specularFactor *= normalDistribution( light.nDotH, alpha );
    }

    // multi-scatter energy compensation for direct light, see
    // https://google.github.io/filament/Filament.md.html section 4.7.2
    vec2 environmentBrdf = environmentBrdfApproximation( roughness, vDotN );
    float directionalAlbedo = environmentBrdf.x + environmentBrdf.y;
    specularFactor *= vec3( 1.0 ) + F0 * ( 1.0 / max( directionalAlbedo, 0.001 ) - 1.0 );
  }

  vec3 specular = lightColor( lightIndex ) * specularFactor * light.sDotN;

  // blend between diffuse and specular to conserve energy, see
  // https://learnopengl.com/PBR/Theory, "Energy conservation"
  vec3 kS = fresnelFactor( F0, light.sDotH );
  vec3 Fd = diffuse * ( vec3( 1.0 ) - kS );
  vec3 Fr = specular;

  vec3 layeredColor;
  if ( clearCoat )
  {
    // https://google.github.io/filament/Filament.md.html section 4.9.14, Listing 14
    float ccPerceptualRoughness = clamp( material.clearCoatRoughness, 0.089, 1.0 );
    float ccAlpha = ccPerceptualRoughness * ccPerceptualRoughness;

    // the clear coat has no normal of its own here, so the material normal is used
    float Dc = normalDistribution( light.nDotH, ccAlpha );
    float Vc = visibilityKelemen( light.sDotH );
    // a fixed IOR of 1.5 gives a reflectance of 0.04
    float Fc = fresnelSchlick( 0.04, 1.0, light.sDotH ) * material.clearCoatFactor;

    vec3 Frc = vec3( Dc * Vc * Fc ) * light.sDotN * lightColor( lightIndex );

    // account for the energy lost in the base layer
    layeredColor = ( Fd + Fr * ( 1.0 - Fc ) ) * ( 1.0 - Fc ) + Frc;
  }
  else
  {
    layeredColor = Fd + Fr;
  }

  vec3 color = light.visibilityFactor * light.att * lightIntensity( lightIndex ) * layeredColor;
  return color * ambientOcclusion;
}

vec4 metalRoughFunction( const in vec4 baseColor,
                         const in float metalness,
                         const in float roughness,
                         const in float ambientOcclusion,
                         const in float anisotropy,
                         const in vec3 worldPosition,
                         const in vec3 worldView,
                         const in vec3 worldNormal,
                         const in vec3 t,
                         const in vec3 b,
                         const in vec3 emission )
{
  vec3 cLinear = vec3( 0.0 );

  // remap roughness for a perceptually more linear correspondence
  float alpha = remapRoughness( roughness );

  for ( int i = 0; i < lighting.lightCount; ++i )
  {
    cLinear += pbrModel( i, worldPosition, worldNormal, worldView, t, b,
                         baseColor.rgb, metalness, roughness, alpha,
                         ambientOcclusion, anisotropy );
  }

  cLinear += emission * material.emissiveFactor;

  return vec4( cLinear, 1.0 );
}

#endif // QGS_METALROUGH_GLSL
