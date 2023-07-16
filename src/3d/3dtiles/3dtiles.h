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

#include "nlohmann/json.hpp"
using json = nlohmann::json;


QgsVector3D reproject( QgsCoordinateTransform &ct, QgsVector3D v, bool inv = false );


struct CoordsContext
{
  QgsVector3D sceneOriginTargetCrs;
  QgsMatrix4x4 nodeTransform;
  std::unique_ptr<QgsCoordinateTransform> ecefToTargetCrs;    // ecef in EPSG:4978
  std::unique_ptr<QgsCoordinateTransform> regionToTargetCrs;  // region bounding volumes are in EPSG:4979
};


struct OBB
{
  QgsVector3D center;
  double half[9];

  static OBB fromJson( json &box )
  {
    OBB obb;
    obb.center = QgsVector3D( box[0].get<double>(), box[1].get<double>(), box[2].get<double>() );
    for ( int i = 0; i < 9; ++i )
      obb.half[i] = box[i + 3].get<double>();

    return obb;
  }

  void dump()
  {
    qDebug() << "OBB";
    qDebug() << center.x() << center.y() << center.z();
  }

  QgsVector3D boxSize()
  {
    QgsVector3D axis1( half[0], half[1], half[2] );
    QgsVector3D axis2( half[3], half[4], half[5] );
    QgsVector3D axis3( half[6], half[7], half[8] );
    return QgsVector3D( 2 * axis1.length(), 2 * axis2.length(), 2 * axis3.length() );
  }

  bool isTooBig()
  {
    QgsVector3D size = boxSize();
    return size.x() > 1e5 || size.y() > 1e5 || size.z() > 1e5;
  }

  QVector<QgsVector3D> corners()
  {
    QgsVector3D a1( half[0], half[1], half[2] ), a0( -half[0], -half[1], -half[2] );
    QgsVector3D b1( half[3], half[4], half[5] ), b0( -half[3], -half[4], -half[5] );
    QgsVector3D c1( half[6], half[7], half[8] ), c0( -half[6], -half[7], -half[8] );
    QVector<QgsVector3D> cor( 8 );
    for ( int i = 0; i < 8; ++i )
    {
      QgsVector3D aa = ( i % 2 == 0 ? a1 : a0 );
      QgsVector3D bb = ( i % 4 == 0 ? b1 : b0 );
      QgsVector3D cc = ( i / 4 == 0 ? c1 : c0 );
      QgsVector3D q = aa + bb + cc;
      cor[i] = center + q;
    }

    return cor;
  }

  // returns coords in map coordinates minus origin
  QVector<QgsVector3D> cornersSceneCoords( CoordsContext &ctx );

  // in map coordinates minus origin
  QgsBox3d aabb( CoordsContext &ctx );

  // transform this OBB using a 4x4 transformation matrix
  void transform( const QgsMatrix4x4 &tr )
  {
    const double *ptr = tr.constData();
    QgsMatrix4x4 mm( ptr[0], ptr[4], ptr[8], 0,
                     ptr[1], ptr[5], ptr[9], 0,
                     ptr[2], ptr[6], ptr[10], 0,
                     0, 0, 0, 1 );

    QgsVector3D col1 = mm.map( QgsVector3D( half[0], half[1], half[2] ) );
    QgsVector3D col2 = mm.map( QgsVector3D( half[3], half[4], half[5] ) );
    QgsVector3D col3 = mm.map( QgsVector3D( half[6], half[7], half[8] ) );
    half[0] = col1.x(); half[1] = col1.y(); half[2] = col1.z();
    half[3] = col2.x(); half[4] = col2.y(); half[5] = col2.z();
    half[6] = col3.x(); half[7] = col3.y(); half[8] = col3.z();

    center = tr.map( center );
  }

};


struct Tile
{
  enum BoundsType { BoundsOBB, BoundsRegion } boundsType;
  OBB obb;  // only valid if "box" was used as the boundingVolume

  bool largeBounds = false;   // indicates that the bounds are huge and should be considered as coverting the whole scene

  // TODO: this should not be minus scene origin to avoid confusion
  QgsBox3d region;  // actual axis-aligned bounding box in our target CRS minus scene origin

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

#endif // _3DTILES_H
