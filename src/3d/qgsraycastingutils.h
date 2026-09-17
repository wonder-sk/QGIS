/***************************************************************************
  qgsraycastingutils.h
  --------------------------------------
  Date                 : April 2024
  Copyright            : (C) 2024 by Matthias Kuhn
  Email                : matthias@opengis.ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSRAYCASTINGUTILS_H
#define QGSRAYCASTINGUTILS_H

#include <QMatrix4x4>
#include <QVector3D>

#define SIP_NO_FILE

class QgsAABB;
class QgsRay3D;

namespace QgsRayCastingUtils
{

  /**
   * Tests whether an axis aligned box is intersected by a ray.
   * \since QGIS 4.0
   */
  bool rayBoxIntersection( const QgsRay3D &ray, const QgsAABB &nodeBbox );

  /**
   * Tests whether a triangle is intersected by a ray.
   * \since QGIS 4.0
   */
  bool rayTriangleIntersection( const QgsRay3D &ray, float maxDist, const QVector3D &a, const QVector3D &b, const QVector3D &c, QVector3D &uvw, float &t );


} // namespace QgsRayCastingUtils

#endif // QGSRAYCASTINGUTILS_H
