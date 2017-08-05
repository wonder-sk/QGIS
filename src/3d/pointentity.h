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
    Qt3DRender::QGeometryRenderer * shapeGeometryRenderer(const QString& shape);
    void applyInstanceRendering(Qt3DRender::QGeometryRenderer * renderer);
    Qt3DRender::QMaterial * getMaterial(Qt3DRender::QMaterial* refMaterial = nullptr);

    void applyColorsToEffect(Qt3DRender::QEffect *effect,  const QColor& diffuse,  const QColor& ambient,  const QColor& specular, float shininess);
    void applyColorsToEffect(Qt3DRender::QEffect *effect,  Qt3DRender::QMaterial* refMaterial);
    void applyColorsToEffect(Qt3DRender::QEffect *effect); //from symbol

    void applyColorsFromModelMaterial(Qt3DRender::QEffect *effect, Qt3DRender::QMaterial* refMaterial);

    const Map3D &map;
    QgsVectorLayer *layer;
    const Point3DSymbol &symbol;
    Qt3DRender::QSceneLoader * modelLoader;

  private slots:
    void applyInstanceRenderingOn3dModel(Qt3DRender::QSceneLoader::Status status);
};

#endif // POINTENTITY_H
