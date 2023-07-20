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



Tile loadTilesetJson( QString tilesetPath, QString relativePathBase, CoordsContext &coordsCtx, QgsMatrix4x4 trParent );

Qt3DCore::QEntity *gltfToEntity( QString path, CoordsContext &coordsCtx );
Qt3DCore::QEntity *gltfMemoryToEntity( const QByteArray &data, CoordsContext &coordsCtx, QString baseUri );

Qt3DCore::QEntity *loadAllSceneTiles( const Tile &rootTile, int tilesetLevel, QString relativePathBase, CoordsContext &coordsCtx );

Qt3DCore::QEntity *entityForOBB( const QVector<QgsOrientedBox3D> &obbs, CoordsContext &coordsCtx );

#endif // _3DTILES_H
