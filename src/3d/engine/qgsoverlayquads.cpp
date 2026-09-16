/***************************************************************************
  qgsoverlayquads.cpp
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
 ***************************************************************************/

#include "qgsoverlayquads.h"

#include "qgsrendercommand.h"
#include "qgsrenderpass.h"
#include "qgsscreenquad.h"

using namespace Qt::StringLiterals;

QgsOverlayQuads::QgsOverlayQuads() = default;

QgsOverlayQuads::~QgsOverlayQuads() = default;

QgsOverlayTextureMaterial3D *QgsOverlayQuads::addOverlay( QRhiTexture *texture )
{
  auto material = std::make_unique<QgsOverlayTextureMaterial3D>();
  material->setTexture( texture );

  QgsOverlayTextureMaterial3D *result = material.get();
  mOverlays.push_back( std::move( material ) );
  return result;
}

void QgsOverlayQuads::clear()
{
  mOverlays.clear();
}

QgsOverlayTextureMaterial3D *QgsOverlayQuads::overlay( int index ) const
{
  if ( index < 0 || index >= static_cast<int>( mOverlays.size() ) )
    return nullptr;
  return mOverlays[static_cast<size_t>( index )].get();
}

void QgsOverlayQuads::appendCommands( QgsRenderPass *pass, const QgsScreenQuad *quad ) const
{
  if ( !pass || !quad )
    return;

  for ( const auto &overlay : mOverlays )
  {
    QgsRenderCommand command;
    command.geometry = quad->geometry();
    command.material = overlay.get();
    pass->addCommand( command );
  }
}

void QgsOverlayQuads::releaseGpuResources()
{
  for ( const auto &overlay : mOverlays )
    overlay->releaseGpuResources();
}
