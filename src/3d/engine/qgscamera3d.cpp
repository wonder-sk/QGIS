/***************************************************************************
  qgscamera3d.cpp
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

#include "qgscamera3d.h"

void QgsCamera3D::setPosition( const QVector3D &position )
{
  if ( mPosition == position )
    return;

  mPosition = position;
  mViewMatrixDirty = true;
}

void QgsCamera3D::setViewCenter( const QVector3D &viewCenter )
{
  if ( mViewCenter == viewCenter )
    return;

  mViewCenter = viewCenter;
  mViewMatrixDirty = true;
}

void QgsCamera3D::setUpVector( const QVector3D &upVector )
{
  if ( mUpVector == upVector )
    return;

  mUpVector = upVector;
  mViewMatrixDirty = true;
}

void QgsCamera3D::setPerspectiveProjection( float fieldOfView, float aspectRatio, float nearPlane, float farPlane )
{
  mProjectionType = Qgis::Map3DProjectionType::Perspective;
  mFieldOfView = fieldOfView;
  mAspectRatio = aspectRatio;
  mNearPlane = nearPlane;
  mFarPlane = farPlane;
  mProjectionMatrixDirty = true;
}

void QgsCamera3D::setOrthographicProjection( float left, float right, float bottom, float top, float nearPlane, float farPlane )
{
  mProjectionType = Qgis::Map3DProjectionType::Orthographic;
  mLeft = left;
  mRight = right;
  mBottom = bottom;
  mTop = top;
  mNearPlane = nearPlane;
  mFarPlane = farPlane;
  mProjectionMatrixDirty = true;
}

void QgsCamera3D::setFieldOfView( float fieldOfView )
{
  if ( mFieldOfView == fieldOfView )
    return;

  mFieldOfView = fieldOfView;
  mProjectionMatrixDirty = true;
}

void QgsCamera3D::setAspectRatio( float aspectRatio )
{
  if ( mAspectRatio == aspectRatio )
    return;

  mAspectRatio = aspectRatio;
  mProjectionMatrixDirty = true;
}

void QgsCamera3D::setNearPlane( float nearPlane )
{
  if ( mNearPlane == nearPlane )
    return;

  mNearPlane = nearPlane;
  mProjectionMatrixDirty = true;
}

void QgsCamera3D::setFarPlane( float farPlane )
{
  if ( mFarPlane == farPlane )
    return;

  mFarPlane = farPlane;
  mProjectionMatrixDirty = true;
}

QMatrix4x4 QgsCamera3D::viewMatrix() const
{
  if ( mViewMatrixDirty )
  {
    mViewMatrix.setToIdentity();
    mViewMatrix.lookAt( mPosition, mViewCenter, mUpVector );
    mViewMatrixDirty = false;
  }
  return mViewMatrix;
}

QMatrix4x4 QgsCamera3D::projectionMatrix() const
{
  if ( mProjectionMatrixDirty )
  {
    mProjectionMatrix.setToIdentity();
    switch ( mProjectionType )
    {
      case Qgis::Map3DProjectionType::Perspective:
        mProjectionMatrix.perspective( mFieldOfView, mAspectRatio, mNearPlane, mFarPlane );
        break;
      case Qgis::Map3DProjectionType::Orthographic:
        mProjectionMatrix.ortho( mLeft, mRight, mBottom, mTop, mNearPlane, mFarPlane );
        break;
    }
    mProjectionMatrixDirty = false;
  }
  return mProjectionMatrix;
}
