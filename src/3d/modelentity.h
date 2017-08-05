#ifndef MODELENTITY_H
#define MODELENTITY_H

#include <Qt3DCore/QEntity>
#include <QVector3D>

class Map3D;
class Point3DSymbol;


//! Entity that handles rendering a point from 3d model
class ModelEntity : public Qt3DCore::QEntity
{
  public:
    ModelEntity( const QVector3D& position, const Point3DSymbol &symbol, Qt3DCore::QNode *parent = nullptr );
};

#endif // MODELENTITY_H
