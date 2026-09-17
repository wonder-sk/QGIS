/***************************************************************************
  qgsmeshlayer3drenderer.cpp
  --------------------------
  Date                 : January 2019
  Copyright            : (C) 2019 by Peter Petrik
  Email                : zilolv at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmeshlayer3drenderer.h"

#include "qgs3dmapsettings.h"
#include "qgs3drendercontext.h"
#include "qgsmesh3dsymbol.h"
#include "qgsmeshlayer.h"
#include "qgsxmlutils.h"

#include <QString>

using namespace Qt::StringLiterals;

QgsMeshLayer3DRendererMetadata::QgsMeshLayer3DRendererMetadata()
  : Qgs3DRendererAbstractMetadata( u"mesh"_s )
{}

QgsAbstract3DRenderer *QgsMeshLayer3DRendererMetadata::createRenderer( QDomElement &elem, const QgsReadWriteContext &context )
{
  QgsMeshLayer3DRenderer *r = new QgsMeshLayer3DRenderer;
  r->readXml( elem, context );
  return r;
}

// ---------


QgsMeshLayer3DRenderer::QgsMeshLayer3DRenderer( QgsMesh3DSymbol *s )
  : mSymbol( s )
{}

QgsMeshLayer3DRenderer *QgsMeshLayer3DRenderer::clone() const
{
  QgsMeshLayer3DRenderer *r = new QgsMeshLayer3DRenderer( mSymbol ? ( QgsMesh3DSymbol * ) mSymbol->clone() : nullptr );
  r->mLayerRef = mLayerRef;
  return r;
}

void QgsMeshLayer3DRenderer::setLayer( QgsMeshLayer *layer )
{
  mLayerRef = QgsMapLayerRef( layer );
}

QgsMeshLayer *QgsMeshLayer3DRenderer::layer() const
{
  return qobject_cast<QgsMeshLayer *>( mLayerRef.layer );
}

void QgsMeshLayer3DRenderer::setSymbol( QgsMesh3DSymbol *symbol )
{
  mSymbol.reset( symbol );
}

const QgsMesh3DSymbol *QgsMeshLayer3DRenderer::symbol() const
{
  return mSymbol.get();
}

Qgs3DMapSceneEntity *QgsMeshLayer3DRenderer::createEntity( Qgs3DMapSettings *map ) const
{
  Q_UNUSED( map )
  // the mesh layer entity has not been ported to the QRhi engine yet, so the layer
  // contributes nothing to a 3D scene. QgsMeshGeometry3D and QgsMeshMaterial3D are both
  // there; what is missing is the entity that puts them together.
  return nullptr;
}

void QgsMeshLayer3DRenderer::writeXml( QDomElement &elem, const QgsReadWriteContext &context ) const
{
  QDomDocument doc = elem.ownerDocument();

  elem.setAttribute( u"layer"_s, mLayerRef.layerId );

  QDomElement elemSymbol = doc.createElement( u"symbol"_s );
  if ( mSymbol )
  {
    elemSymbol.setAttribute( u"type"_s, mSymbol->type() );
    mSymbol->writeXml( elemSymbol, context );
  }
  elem.appendChild( elemSymbol );
}

void QgsMeshLayer3DRenderer::readXml( const QDomElement &elem, const QgsReadWriteContext &context )
{
  mLayerRef = QgsMapLayerRef( elem.attribute( u"layer"_s ) );

  const QDomElement elemSymbol = elem.firstChildElement( u"symbol"_s );
  QgsMesh3DSymbol *symbol = new QgsMesh3DSymbol;
  symbol->readXml( elemSymbol, context );
  mSymbol.reset( symbol );
}

void QgsMeshLayer3DRenderer::resolveReferences( const QgsProject &project )
{
  mLayerRef.setLayer( project.mapLayer( mLayerRef.layerId ) );
}
