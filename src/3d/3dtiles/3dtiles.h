#ifndef _3DTILES_H
#define _3DTILES_H

#include <QVector3D>
#include <QMatrix4x4>
#include <QDebug>

#include <proj.h>

#include <Qt3DCore>

#include "nlohmann/json.hpp"
using json = nlohmann::json;


//! QVector3D but using doubles
struct VEC3D
{
  VEC3D( double _x = 0, double _y = 0, double _z = 0 ): x( _x ), y( _y ), z( _z ) {}
  VEC3D( const QVector3D &v ): x( v.x() ), y( v.y() ), z( v.z() ) {}
  VEC3D operator+( const VEC3D &other ) const { return VEC3D( x + other.x, y + other.y, z + other.z ); }
  VEC3D operator-( const VEC3D &other ) const { return VEC3D( x - other.x, y - other.y, z - other.z ); }
  bool isNull() const { return x == 0 && y == 0 && z == 0; }
  double x, y, z;
};

QDebug operator<<( QDebug debug, const VEC3D &v );

VEC3D reproject( PJ *transform, VEC3D v, bool inv = false );

struct SceneContext;


struct AABB
{
  VEC3D v0, v1;
};

struct OBB
{
  VEC3D center;
  //VEC3D x, y, z;
  QMatrix4x4 rot;  // ideally should be double matrix

  static OBB fromJson( json &box )
  {
    OBB obb;
    obb.center = VEC3D( box[0].get<double>(), box[1].get<double>(), box[2].get<double>() );
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
    qDebug() << center.x << center.y << center.z;
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

  QVector<VEC3D> corners()
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
    QVector<VEC3D> cor( unit.count() );
    for ( int i = 0; i < unit.count(); ++i )
    {
      cor[i] = center + rot.mapVector( unit[i] );
    }

    return cor;
  }

  // returns coords in map coordinates minus origin
  QVector<VEC3D> cornersSceneCoords( SceneContext &ctx );

  // in map coordinates minus origin
  AABB aabb( SceneContext &ctx );

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
  VEC3D tileTranslationEcef;

  // scene-wide stuff
  VEC3D sceneOriginTargetCrs;  // can be specified optionally (or uses first tile's center)
  PJ *ecefToTargetCrs;

  QString targetCrs;  // EPSG code - or if empty will use ECEF
  QStringList files;   // what files to load

  QString tilesetJsonPath;
  int tilesetLevel;   // which level of content to use to show in the scene

  QString relativePathBase;  // how to resolve URIs starting with "./"

  // internal
  PJ_CONTEXT *projContext = nullptr;
  Tile rootTile;
};




Tile loadTilesetJson( QString tilesetPath, QString relativePathBase );

Qt3DCore::QEntity *gltfToEntity( QString path, SceneContext &ctx );

QStringList collectTilesAtLevel( const Tile &tile, int level, QString relativePathBase );

//Qt3DCore::QEntity *init3DTilesScene(SceneContext &ctx);
bool init3DTilesScene( SceneContext &ctx );
Qt3DCore::QEntity *loadAllSceneTiles( SceneContext &ctx );

#endif // _3DTILES_H
