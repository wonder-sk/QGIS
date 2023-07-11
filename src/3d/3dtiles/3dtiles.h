#ifndef _3DTILES_H
#define _3DTILES_H

#include <QVector3D>
#include <QMatrix4x4>
#include <QDebug>

#include <Qt3DCore>

#include "qgsbox3d.h"
#include "qgscoordinatetransform.h"
#include "qgsvector3d.h"

#include "nlohmann/json.hpp"
using json = nlohmann::json;


QgsVector3D reproject( QgsCoordinateTransform &ct, QgsVector3D v, bool inv = false );

struct SceneContext;


struct OBB
{
  QgsVector3D center;
  QMatrix4x4 rot;  // ideally should be double matrix

  static OBB fromJson( json &box )
  {
    OBB obb;
    obb.center = QgsVector3D( box[0].get<double>(), box[1].get<double>(), box[2].get<double>() );
    obb.rot = QMatrix4x4(
                box[3].get<double>(), box[6].get<double>(), box[9].get<double>(), 0,
                box[4].get<double>(), box[7].get<double>(), box[10].get<double>(), 0,
                box[5].get<double>(), box[8].get<double>(), box[11].get<double>(), 0,
                0, 0, 0, 1 );

    return obb;
  }

  void dump()
  {
    qDebug() << "OBB";
    qDebug() << center.x() << center.y() << center.z();
    //qDebug() << rot;
  }

  QVector3D boxSize()
  {
    QVector3D axis1 = rot.mapVector( QVector3D( -1, -1, -1 ) ) - rot.mapVector( QVector3D( 1, -1, -1 ) );
    QVector3D axis2 = rot.mapVector( QVector3D( -1, -1, -1 ) ) - rot.mapVector( QVector3D( -1, 1, -1 ) );
    QVector3D axis3 = rot.mapVector( QVector3D( -1, -1, -1 ) ) - rot.mapVector( QVector3D( -1, 1, -1 ) );
    qDebug() << "OBB size" << axis1.length() << axis2.length() << axis3.length();
    return QVector3D( axis1.length(), axis2.length(), axis3.length() );
  }

  bool isTooBig()
  {
    QVector3D size = boxSize();
    return size.x() > 1e5 || size.y() > 1e5 || size.z() > 1e5;
  }

  QVector<QgsVector3D> corners()
  {
    const QVector<QVector3D> unit =
    {
      QVector3D( -1, -1, -1 ),
      QVector3D( +1, -1, -1 ),
      QVector3D( -1, +1, -1 ),
      QVector3D( +1, +1, -1 ),
      QVector3D( -1, -1, +1 ),
      QVector3D( +1, -1, +1 ),
      QVector3D( -1, +1, +1 ),
      QVector3D( +1, +1, +1 ),
    };
    QVector<QgsVector3D> cor( unit.count() );
    for ( int i = 0; i < unit.count(); ++i )
    {
      cor[i] = center + rot.mapVector( unit[i] );
    }

    return cor;
  }

  // returns coords in map coordinates minus origin
  QVector<QgsVector3D> cornersSceneCoords( SceneContext &ctx );

  // in map coordinates minus origin
  QgsBox3d aabb( SceneContext &ctx );

};

struct Tile
{
  OBB obb;
  double geomError;
  QVector<Tile> children;
  QString contentUri;
};


struct SceneContext
{
  // tile-specific
  QgsVector3D tileTranslationEcef;

  // scene-wide stuff
  QgsVector3D sceneOriginTargetCrs;  // can be specified optionally (or uses first tile's center)
  std::unique_ptr<QgsCoordinateTransform> ecefToTargetCrs;

  QString targetCrs;  // EPSG code - or if empty will use ECEF
  QStringList files;   // what files to load

  QString tilesetJsonPath;
  int tilesetLevel;   // which level of content to use to show in the scene

  QString relativePathBase;  // how to resolve URIs starting with "./"

  // internal
  Tile rootTile;
};




Tile loadTilesetJson( QString tilesetPath, QString relativePathBase );

Qt3DCore::QEntity *gltfToEntity( QString path, SceneContext &ctx );

QStringList collectTilesAtLevel( const Tile &tile, int level, QString relativePathBase );

//Qt3DCore::QEntity *init3DTilesScene(SceneContext &ctx);
bool init3DTilesScene( SceneContext &ctx );
Qt3DCore::QEntity *loadAllSceneTiles( SceneContext &ctx );

#endif // _3DTILES_H
