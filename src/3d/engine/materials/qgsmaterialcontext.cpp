/***************************************************************************
  qgsmaterialcontext.cpp
  --------------------------------------
  Date                 : March 2026
  Copyright            : (C) 2026 by Nyall Dawson
  Email                : nyall dot dawson at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmaterialcontext.h"

#include "qgs3drendercontext.h"

QgsMaterialContext QgsMaterialContext::fromRenderContext( const Qgs3DRenderContext &context )
{
  QgsMaterialContext res;
  res.mSelectedColor = context.selectionColor();
  res.mTextureFilterQuality = context.textureFilterQuality();
  return res;
}

bool QgsMaterialContext::isPreview() const
{
  return mIsPreview;
}

void QgsMaterialContext::setIsPreview( bool isPreview )
{
  mIsPreview = isPreview;
}
