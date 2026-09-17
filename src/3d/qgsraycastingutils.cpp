/***************************************************************************
  qgsraycastingutils_h.cpp
  --------------------------------------
  Date                 : June 2018
  Copyright            : (C) 2018 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsraycastingutils.h"

#include "qgsaabb.h"
#include "qgslogger.h"
#include "qgsray3d.h"

///@cond PRIVATE


namespace QgsRayCastingUtils
{
  bool rayBoxIntersection( const QgsRay3D &ray, const QgsAABB &nodeBbox )
  {
    // https://tavianator.com/fast-branchless-raybounding-box-intersections/
    // https://tavianator.com/fast-branchless-raybounding-box-intersections-part-2-nans/

    const QVector3D dirInv = ray.directionInversed();

    double boxXMax = nodeBbox.xMax;
    double boxYMax = nodeBbox.yMax;
    double boxZMax = nodeBbox.zMax;

    // intersection() does not like yMin==yMax (excludes borders)
    if ( boxXMax == nodeBbox.xMin )
      boxXMax += 0.1;
    if ( boxYMax == nodeBbox.yMin )
      boxYMax += 0.1;
    if ( boxZMax == nodeBbox.zMin )
      boxZMax += 0.1;


    double t1 = ( nodeBbox.xMin - ray.origin().x() ) * dirInv.x();
    double t2 = ( boxXMax - ray.origin().x() ) * dirInv.x();
    double tmin = std::min( t1, t2 );
    double tmax = std::max( t1, t2 );

    t1 = ( nodeBbox.yMin - ray.origin().y() ) * dirInv.y();
    t2 = ( boxYMax - ray.origin().y() ) * dirInv.y();
    tmin = std::max( tmin, std::min( std::min( t1, t2 ), tmax ) );
    tmax = std::min( tmax, std::max( std::max( t1, t2 ), tmin ) );

    t1 = ( nodeBbox.zMin - ray.origin().z() ) * dirInv.z();
    t2 = ( boxZMax - ray.origin().z() ) * dirInv.z();
    tmin = std::max( tmin, std::min( std::min( t1, t2 ), tmax ) );
    tmax = std::min( tmax, std::max( std::max( t1, t2 ), tmin ) );

    return tmax > std::max( tmin, 0.0 );
  }


  // copied from intersectsSegmentTriangle() from qt3d/src/render/backend/triangleboundingvolume.cpp
  // by KDAB, licensed under the terms of LGPL
  bool rayTriangleIntersection( const QgsRay3D &ray, float maxDist, const QVector3D &a, const QVector3D &b, const QVector3D &c, QVector3D &uvw, float &t )
  {
    // Note: a, b, c in clockwise order
    // RealTime Collision Detection page 192

    const QVector3D ab = b - a;
    const QVector3D ac = c - a;
    const QVector3D qp = ( ray.origin() - ray.point( maxDist ) );

    const QVector3D n = QVector3D::crossProduct( ab, ac );
    const float d = QVector3D::dotProduct( qp, n );

    if ( d <= 0.0f || std::isnan( d ) )
      return false;

    const QVector3D ap = ray.origin() - a;
    t = QVector3D::dotProduct( ap, n );

    if ( t < 0.0f || t > d )
      return false;

    const QVector3D e = QVector3D::crossProduct( qp, ap );
    uvw.setY( QVector3D::dotProduct( ac, e ) );

    if ( uvw.y() < 0.0f || uvw.y() > d )
      return false;

    uvw.setZ( -QVector3D::dotProduct( ab, e ) );

    if ( uvw.z() < 0.0f || uvw.y() + uvw.z() > d )
      return false;

    const float ood = 1.0f / d;
    t *= ood;
    uvw.setY( uvw.y() * ood );
    uvw.setZ( uvw.z() * ood );
    uvw.setX( 1.0f - uvw.y() - uvw.z() );

    return true;
  }

} // namespace QgsRayCastingUtils


/// @endcond
