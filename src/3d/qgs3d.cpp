/***************************************************************************
                         qgs3d.cpp
                         ----------
    begin                : July 2020
    copyright            : (C) 2020 by Nyall Dawson
    email                : nyall dot dawson at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgs3d.h"

#include "qgs3drendererregistry.h"
#include "qgs3dsymbolregistry.h"
#include "qgs3dterrainregistry.h"
#include "qgsabstractmaterialsettings.h"
#include "qgsannotationlayer3drenderer.h"
#include "qgsapplication.h"
#include "qgscategorized3drenderer.h"
#include "qgsline3dsymbol.h"
#include "qgsline3dsymbol_p.h"
#include "qgsmeshlayer3drenderer.h"
#include "qgspoint3dsymbol.h"
#include "qgspoint3dsymbol_p.h"
#include "qgspointcloudlayer3drenderer.h"
#include "qgspolygon3dsymbol.h"
#include "qgspolygon3dsymbol_p.h"
#include "qgsrulebased3drenderer.h"
#include "qgssettingsentryenumflag.h"
#include "qgssettingsentryimpl.h"
#include "qgssettingstree.h"
#include "qgsstyle.h"
#include "qgstiledscenelayer3drenderer.h"
#include "qgsvectorlayer3drenderer.h"

#include <QString>

using namespace Qt::StringLiterals;

const QgsSettingsEntryBool *Qgs3D::settingMsaaEnabled = new QgsSettingsEntryBool( u"msaa-enabled"_s, QgsSettingsTree::sTree3DMap, true, u"Whether MSAA is enabled for 3D map rendering"_s );
const QgsSettingsEntryEnumFlag<Qgis::TextureFilterQuality> *Qgs3D::settingTextureFilterQuality
  = new QgsSettingsEntryEnumFlag<Qgis::TextureFilterQuality>( u"texture-filter"_s, QgsSettingsTree::sTree3DMap, Qgis::TextureFilterQuality::Anisotropic16x, u"Texture filter quality"_s );
const QgsSettingsEntryEnumFlag<Qgis::ShadowQuality> *Qgs3D::settingShadowQuality
  = new QgsSettingsEntryEnumFlag<Qgis::ShadowQuality>( u"shadow-quality"_s, QgsSettingsTree::sTree3DMap, Qgis::ShadowQuality::High, u"Shadow rendering quality"_s );

Qgs3D *Qgs3D::instance()
{
  static Qgs3D *sInstance( new Qgs3D() );
  return sInstance;
}

Qgs3D::~Qgs3D() = default;

void Qgs3D::initialize()
{
  if ( instance()->mInitialized )
    return;

  instance()->mInitialized = true;

  QgsApplication::renderer3DRegistry()->addRenderer( new QgsVectorLayer3DRendererMetadata );
  QgsApplication::renderer3DRegistry()->addRenderer( new QgsRuleBased3DRendererMetadata );
  QgsApplication::renderer3DRegistry()->addRenderer( new QgsCategorized3DRendererMetadata );
  QgsApplication::renderer3DRegistry()->addRenderer( new QgsMeshLayer3DRendererMetadata );
  QgsApplication::renderer3DRegistry()->addRenderer( new QgsPointCloudLayer3DRendererMetadata );
  QgsApplication::renderer3DRegistry()->addRenderer( new QgsTiledSceneLayer3DRendererMetadata );
  QgsApplication::renderer3DRegistry()->addRenderer( new QgsAnnotationLayer3DRendererMetadata );

  QgsApplication::symbol3DRegistry()->addSymbolType( new Qgs3DSymbolMetadata( u"point"_s, QObject::tr( "Point" ), &QgsPoint3DSymbol::create, nullptr, Qgs3DSymbolImpl::handlerForPoint3DSymbol ) );
  QgsApplication::symbol3DRegistry()->addSymbolType( new Qgs3DSymbolMetadata( u"line"_s, QObject::tr( "Line" ), &QgsLine3DSymbol::create, nullptr, Qgs3DSymbolImpl::handlerForLine3DSymbol ) );
  QgsApplication::symbol3DRegistry()->addSymbolType( new Qgs3DSymbolMetadata( u"polygon"_s, QObject::tr( "Polygon" ), &QgsPolygon3DSymbol::create, nullptr, Qgs3DSymbolImpl::handlerForPolygon3DSymbol ) );

  // because we are usually populating the 3d registry AFTER QgsApplication initialization, we need to defer creation
  // of 3d symbols in the default style until now
  QgsStyle::defaultStyle()->handleDeferred3DSymbolCreation();
}

QgsMaterialRegistry *Qgs3D::materialRegistry()
{
  return QgsApplication::materialRegistry();
}

Qgs3DTerrainRegistry *Qgs3D::terrainRegistry()
{
  return instance()->mTerrainRegistry;
}

Qgs3D::Qgs3D()
{
  mTerrainRegistry = new Qgs3DTerrainRegistry();
}
