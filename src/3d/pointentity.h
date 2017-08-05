#ifndef POINTENTITY_H
#define POINTENTITY_H

#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QSceneLoader>
#include <Qt3DCore/QEntity>


class Map3D;
class Point3DSymbol;

class QgsVectorLayer;


//! Entity that handles rendering of points as 3D objects
class PointEntity : public Qt3DCore::QEntity
{
  public:
    PointEntity( const Map3D &map, QgsVectorLayer *layer, const Point3DSymbol &symbol, Qt3DCore::QNode *parent = nullptr );

  private:
    Qt3DRender::QGeometry* shapeGeometry(const QString& shape);
    Qt3DRender::QGeometryRenderer * instancedGeometryRenderer(Qt3DRender::QGeometry* geometry);
    Qt3DRender::QMaterial * instancedRenderingMaterial();

    const Map3D &map;
    QgsVectorLayer *layer;
    const Point3DSymbol &symbol;
};

#endif // POINTENTITY_H
