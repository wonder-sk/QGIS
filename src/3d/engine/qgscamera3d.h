/***************************************************************************
  qgscamera3d.h
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

#ifndef QGSCAMERA3D_H
#define QGSCAMERA3D_H

#include "qgis.h"
#include "qgis_3d.h"

#include <QMatrix4x4>
#include <QVector3D>

#define SIP_NO_FILE

/**
 * \ingroup qgis_3d
 * \brief Where the scene is looked at from, and how it is projected.
 *
 * This is what replaces Qt3DRender::QCamera. It is a plain copyable value rather than an
 * entity: nothing in QGIS ever parented anything to the camera, and the two places that
 * wanted a snapshot of it - the depth buffer's camera and the camera a drag started from -
 * had to clone a QEntity to get one, which this makes an ordinary copy.
 *
 * The view is defined the same way Qt3D defined it, by an eye position, the point it looks
 * at and an up vector, so ports read the same. The projection is either perspective, given
 * a vertical field of view and an aspect ratio, or orthographic, given the four sides of
 * the box; nearPlane() and farPlane() apply to both.
 *
 * Matrices are cached and recomputed on the first read after a change, because navigation
 * reads them far more often than it writes the camera.
 *
 * \note Not available in Python bindings
 * \since QGIS 3.46
 */
class _3D_EXPORT QgsCamera3D
{
  public:
    QgsCamera3D() = default;

    //! Sets the eye position, in world coordinates
    void setPosition( const QVector3D &position );

    //! Returns the eye position, in world coordinates
    QVector3D position() const { return mPosition; }

    //! Sets the point the camera looks at, in world coordinates
    void setViewCenter( const QVector3D &viewCenter );

    //! Returns the point the camera looks at, in world coordinates
    QVector3D viewCenter() const { return mViewCenter; }

    //! Sets which way is up for the camera. Need not be perpendicular to the view vector.
    void setUpVector( const QVector3D &upVector );

    //! Returns which way is up for the camera
    QVector3D upVector() const { return mUpVector; }

    //! Returns the vector from the eye to the point it is looking at
    QVector3D viewVector() const { return mViewCenter - mPosition; }

    //! Sets a perspective projection with vertical field of view \a fieldOfView, in degrees
    void setPerspectiveProjection( float fieldOfView, float aspectRatio, float nearPlane, float farPlane );

    //! Sets an orthographic projection from the sides of the box it projects
    void setOrthographicProjection( float left, float right, float bottom, float top, float nearPlane, float farPlane );

    //! Returns whether the camera projects perspectively or orthographically
    Qgis::Map3DProjectionType projectionType() const { return mProjectionType; }

    //! Returns the left side of an orthographic camera's box, in view coordinates
    float left() const { return mLeft; }

    //! Returns the right side of an orthographic camera's box, in view coordinates
    float right() const { return mRight; }

    //! Returns the bottom side of an orthographic camera's box, in view coordinates
    float bottom() const { return mBottom; }

    //! Returns the top side of an orthographic camera's box, in view coordinates
    float top() const { return mTop; }

    //! Sets the vertical field of view in degrees. Only meaningful for a perspective camera.
    void setFieldOfView( float fieldOfView );

    //! Returns the vertical field of view in degrees
    float fieldOfView() const { return mFieldOfView; }

    //! Sets the ratio of the viewport's width to its height
    void setAspectRatio( float aspectRatio );

    //! Returns the ratio of the viewport's width to its height
    float aspectRatio() const { return mAspectRatio; }

    //! Sets the distance to the near clipping plane
    void setNearPlane( float nearPlane );

    //! Returns the distance to the near clipping plane
    float nearPlane() const { return mNearPlane; }

    //! Sets the distance to the far clipping plane
    void setFarPlane( float farPlane );

    //! Returns the distance to the far clipping plane
    float farPlane() const { return mFarPlane; }

    //! Returns the matrix taking world coordinates to view coordinates
    QMatrix4x4 viewMatrix() const;

    /**
     * Returns the matrix taking view coordinates to clip coordinates.
     *
     * This is the plain projection, with clip space z in [-1,1] as OpenGL and Qt3D define
     * it. QgsRenderPass applies whatever correction the current backend needs on top,
     * so this matrix means the same thing whichever backend is in use.
     */
    QMatrix4x4 projectionMatrix() const;

    //! Returns projectionMatrix() * viewMatrix(), which is what frustum culling tests against
    QMatrix4x4 viewProjectionMatrix() const { return projectionMatrix() * viewMatrix(); }

  private:
    QVector3D mPosition { 0.0f, 0.0f, 1.0f };
    QVector3D mViewCenter { 0.0f, 0.0f, 0.0f };
    QVector3D mUpVector { 0.0f, 1.0f, 0.0f };

    Qgis::Map3DProjectionType mProjectionType = Qgis::Map3DProjectionType::Perspective;
    float mFieldOfView = 45.0f;
    float mAspectRatio = 1.0f;
    float mNearPlane = 0.1f;
    float mFarPlane = 1000.0f;

    // only used by an orthographic camera
    float mLeft = -1.0f;
    float mRight = 1.0f;
    float mBottom = -1.0f;
    float mTop = 1.0f;

    mutable QMatrix4x4 mViewMatrix;
    mutable QMatrix4x4 mProjectionMatrix;
    mutable bool mViewMatrixDirty = true;
    mutable bool mProjectionMatrixDirty = true;
};

#endif // QGSCAMERA3D_H
