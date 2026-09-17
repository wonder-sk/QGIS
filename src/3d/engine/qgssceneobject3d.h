/***************************************************************************
  qgssceneobject3d.h
  --------------------------------------
  Date                 : September 2026
  Copyright            : (C) 2026 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSSCENEOBJECT3D_H
#define QGSSCENEOBJECT3D_H

#include <memory>
#include <vector>

#include "qgis_3d.h"
#include "qgsgeometry3d.h"
#include "qgsgputexture.h"
#include "qgsmaterial3d.h"
#include "qgsrendercommand.h"
#include "qgsvector3d.h"

#include <QMatrix4x4>

#define SIP_NO_FILE

class QgsRenderPass;
class QgsShadowRenderPass;

/**
 * \ingroup qgis_3d
 * \brief Something drawable that a loader produced: a transform and a list of parts.
 *
 * This is what replaces the Qt3DCore::QEntity a chunk loader used to build. A chunk was
 * never a tree in any interesting sense - it was an entity with a transform and one or
 * two children, each carrying a geometry renderer and a material - so this is a transform
 * and a flat list of parts, and collectCommands() turns it into draw commands.
 *
 * The object owns its geometries and materials, and is destroyed when its chunk is
 * unloaded, which is what bounds the GPU memory a scene holds.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsSceneObject3D
{
  public:
    QgsSceneObject3D() = default;
    ~QgsSceneObject3D();

    QgsSceneObject3D( const QgsSceneObject3D & ) = delete;
    QgsSceneObject3D &operator=( const QgsSceneObject3D & ) = delete;

    //! One geometry drawn with one material
    struct Part
    {
        //! Geometry to draw. Owned by whatever produced it, which the object keeps.
        const QgsGeometry3D *geometry = nullptr;
        //! Material to draw it with. Owned by the object.
        std::unique_ptr<QgsMaterial3D> material;
        //! Transform from the geometry's own coordinates to the object's
        QMatrix4x4 transform;
        //! Number of instances to draw
        int instanceCount = 1;
        //! Whether the part is drawn in the shadow pass as well as the forward pass
        bool castsShadows = true;
        //! Whether the part blends with what is behind it, and so has to be drawn last
        bool isTransparent = false;
    };

    /**
     * Adds a part drawing \a geometry with \a material, taking ownership of the material.
     *
     * The geometry is not owned; whatever produced it has to outlive the object, which is
     * what the other overload arranges. Returns a pointer to the part, so that a caller
     * that has to reach back into its material later can keep hold of it.
     */
    Part *addPart( const QgsGeometry3D *geometry, std::unique_ptr<QgsMaterial3D> material, const QMatrix4x4 &transform = QMatrix4x4() );

    /**
     * Adds a part drawing what \a geometrySource produced, taking ownership of both it
     * and \a material.
     *
     * A geometry generator - QgsPrimitiveGeometry3D, QgsDemTerrainTileGeometry3D and the
     * rest - owns the GPU buffers its QgsGeometry3D points into, and they have no common
     * base because none was ever needed: under Qt3D the entity tree owned them. Here the
     * scene object does, so that a chunk's buffers go when the chunk does.
     */
    template<typename T> Part *addPart( std::unique_ptr<T> geometrySource, std::unique_ptr<QgsMaterial3D> material, const QMatrix4x4 &transform = QMatrix4x4() )
    {
      const QgsGeometry3D *geometry = geometrySource->geometry();
      mGeometrySources.emplace_back( std::move( geometrySource ) );
      return addPart( geometry, std::move( material ), transform );
    }

    //! Returns the parts the object draws
    const std::vector<std::unique_ptr<Part>> &parts() const { return mParts; }

    /**
     * Takes ownership of \a texture and returns a pointer to it.
     *
     * A material holds textures without owning them, so something has to; for a chunk's
     * own textures - a terrain tile's rendered map image, say - that is the object, so
     * that they are released when the chunk is unloaded.
     */
    QgsGpuTexture *addTexture( std::unique_ptr<QgsGpuTexture> texture );

    //! Returns the textures the object owns, in the order they were added
    const std::vector<std::unique_ptr<QgsGpuTexture>> &textures() const { return mTextures; }

    /**
     * Sets the transform from the object's own coordinates to scene coordinates.
     *
     * This is whatever the object needs beyond where it sits: an axis flip, a scale. Where
     * it sits belongs in setGeoTranslation(), because that is what an origin shift moves.
     */
    void setTransform( const QMatrix4x4 &transform ) { mTransform = transform; }

    //! Returns the transform from the object's own coordinates to scene coordinates
    QMatrix4x4 transform() const { return mTransform; }

    /**
     * Sets where the object sits, in map coordinates.
     *
     * A chunk's vertices are relative to its own origin, and that origin is a map
     * coordinate: it does not move when the scene's origin does. The scene's origin is
     * rebased whenever the camera wanders more than ten kilometres from it, so an object
     * that had the subtraction baked into its transform would be left behind by the shift.
     * worldTransform() does the subtraction instead, against the origin of the moment.
     *
     * This is what QgsGeoTransform is for a Qt3D entity.
     */
    void setGeoTranslation( const QgsVector3D &translation )
    {
      mGeoTranslation = translation;
      mHasGeoTranslation = true;
    }

    //! Returns where the object sits, in map coordinates
    QgsVector3D geoTranslation() const { return mGeoTranslation; }

    /**
     * Returns TRUE if setGeoTranslation() placed the object in map coordinates.
     *
     * An object that was not placed that way is positioned by its transform alone, in
     * world coordinates, and the scene's origin does not enter into where it is drawn.
     */
    bool hasGeoTranslation() const { return mHasGeoTranslation; }

    //! Returns the transform from the object's own coordinates to world coordinates, given the scene's \a origin
    QMatrix4x4 worldTransform( const QgsVector3D &origin ) const;

    //! Sets whether the object is drawn at all
    void setEnabled( bool enabled ) { mEnabled = enabled; }

    //! Returns whether the object is drawn at all
    bool isEnabled() const { return mEnabled; }

    /**
     * Appends a command per part to \a pass, unless the object is disabled.
     *
     * \a baseSortKey is or-ed into each command's sort key, which is how a pass keeps
     * transparent geometry behind opaque geometry.
     */
    void collectCommands( QgsRenderPass &pass, const QgsVector3D &origin = QgsVector3D(), quint32 baseSortKey = 0 ) const;

    /**
     * Appends a command to \a pass for each part that casts a shadow.
     *
     * The shadow pass draws every caster with its own position only material, so the
     * part's material is not carried over; what matters is the geometry and where it is.
     */
    void collectShadowCommands( QgsShadowRenderPass &pass, const QgsVector3D &origin = QgsVector3D() ) const;

    //! Returns roughly how much GPU memory the object's geometries take, in bytes
    qint64 gpuMemorySize() const;

  private:
    //! Builds one command per part, leaving out non-casters when \a shadowPass is TRUE
    void buildCommands( bool shadowPass, const QgsVector3D &origin, quint32 baseSortKey, std::vector<QgsRenderCommand> &commands ) const;

    std::vector<std::unique_ptr<Part>> mParts;
    std::vector<std::unique_ptr<QgsGpuTexture>> mTextures;
    std::vector<std::shared_ptr<void>> mGeometrySources;
    QMatrix4x4 mTransform;
    QgsVector3D mGeoTranslation;
    bool mHasGeoTranslation = false;
    bool mEnabled = true;
};

#endif // QGSSCENEOBJECT3D_H
