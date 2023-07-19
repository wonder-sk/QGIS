#ifndef _3DTILES_H
#define _3DTILES_H

#include <QVector3D>
#include <QMatrix4x4>
#include <QDebug>

#include <Qt3DCore>

#include "qgsbox3d.h"
#include "qgscoordinatetransform.h"
#include "qgsvector3d.h"
#include "qgsmatrix4x4.h"
#include "qgsorientedbox3d.h"

#include "nlohmann/json.hpp"
using json = nlohmann::json;



struct CoordsContext
{
  QgsVector3D sceneOriginTargetCrs;
  QgsMatrix4x4 nodeTransform;
  std::unique_ptr<QgsCoordinateTransform> ecefToTargetCrs;    // ecef in EPSG:4978
  std::unique_ptr<QgsCoordinateTransform> regionToTargetCrs;  // region bounding volumes are in EPSG:4979
};


// TODO: move
inline QgsVector3D boxSize( const QgsOrientedBox3D &box )
{
  const double *half = box.halfAxes();
  QgsVector3D axis1( half[0], half[1], half[2] );
  QgsVector3D axis2( half[3], half[4], half[5] );
  QgsVector3D axis3( half[6], half[7], half[8] );
  return QgsVector3D( 2 * axis1.length(), 2 * axis2.length(), 2 * axis3.length() );
}


// TODO: move to QgsOrientedBox3D::transfomedExtent( CT ) -> QgsBox3d
inline QgsBox3d ecefOBBtoAABB( const QgsOrientedBox3D &obb, CoordsContext &coordsCtx )
{
  // reproject corners from ECEF to planar CRS
  QVector<QgsVector3D> c = obb.corners();
  for ( int i = 0; i < c.count(); ++i )
  {
    c[i] = coordsCtx.ecefToTargetCrs->transform( c[i] );
  }

  // find AABB for the 8 transformed points
  QgsVector3D v0 = c[0], v1 = c[0];
  for ( const QgsVector3D &v : c )
  {
    if ( v.x() < v0.x() ) v0.setX( v.x() );
    if ( v.y() < v0.y() ) v0.setY( v.y() );
    if ( v.z() < v0.z() ) v0.setZ( v.z() );
    if ( v.x() > v1.x() ) v1.setX( v.x() );
    if ( v.y() > v1.y() ) v1.setY( v.y() );
    if ( v.z() > v1.z() ) v1.setZ( v.z() );
  }
  return QgsBox3d( v0.x(), v0.y(), v0.z(), v1.x(), v1.y(), v1.z() );
}

// TODO: to QgsOrientedBox3D
// transform this OBB using a 4x4 transformation matrix
inline QgsOrientedBox3D transformedOBB( const QgsOrientedBox3D &obb, const QgsMatrix4x4 &tr )
{
  const double *ptr = tr.constData();
  QgsMatrix4x4 mm( ptr[0], ptr[4], ptr[8], 0,
                   ptr[1], ptr[5], ptr[9], 0,
                   ptr[2], ptr[6], ptr[10], 0,
                   0, 0, 0, 1 );

  QgsVector3D trCenter = tr.map( QgsVector3D( obb.centerX(), obb.centerY(), obb.centerZ() ) );

  const double *half = obb.halfAxes();
  QgsVector3D col1 = mm.map( QgsVector3D( half[0], half[1], half[2] ) );
  QgsVector3D col2 = mm.map( QgsVector3D( half[3], half[4], half[5] ) );
  QgsVector3D col3 = mm.map( QgsVector3D( half[6], half[7], half[8] ) );

  return QgsOrientedBox3D( QList<double>() << trCenter.x() << trCenter.y() << trCenter.z(),
                           QList<double>() << col1.x() << col1.y() << col1.z()
                           << col2.x() << col2.y() << col2.z()
                           << col3.x() << col3.y() << col3.z() );
}

// todo to QgsBox3D as operator+ - += -=  ... like QgsRectangle
inline void moveBox3D( QgsBox3d &box, double x, double y, double z )
{
  box = QgsBox3d( box.xMinimum() + x, box.yMinimum() + y, box.zMinimum() + z,
                  box.xMaximum() + x, box.yMaximum() + y, box.zMaximum() + z );
}



struct Tile
{
  enum BoundsType { BoundsOBB, BoundsRegion } boundsType;
  QgsOrientedBox3D obb;  // only valid if "box" was used as the boundingVolume

  bool largeBounds = false;   // indicates that the bounds are huge and should be considered as coverting the whole scene

  QgsBox3d region;  // actual axis-aligned bounding box in our target CRS

  bool additiveStrategy = false;

  // TODO: it would be best to have this as optional (only having instance when node
  // actually contains transform) and node's total transform would be created as needed
  QgsMatrix4x4 transform;  // transform from local coords to ECEF. Includes transform from parent nodes

  double geomError;
  QVector<Tile> children;
  QString contentUri;
};



Tile loadTilesetJson( QString tilesetPath, QString relativePathBase, CoordsContext &coordsCtx );

Qt3DCore::QEntity *gltfToEntity( QString path, CoordsContext &coordsCtx );
Qt3DCore::QEntity *gltfMemoryToEntity( const QByteArray &data, CoordsContext &coordsCtx );

Qt3DCore::QEntity *loadAllSceneTiles( const Tile &rootTile, int tilesetLevel, QString relativePathBase, CoordsContext &coordsCtx );

Qt3DCore::QEntity *entityForOBB( const QVector<QgsOrientedBox3D> &obbs, CoordsContext &coordsCtx );

#endif // _3DTILES_H
