/***************************************************************************
  qgsrulebased3drenderer.cpp
  --------------------------------------
  Date                 : January 2019
  Copyright            : (C) 2019 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsrulebased3drenderer.h"

#include <memory>

#include "qgs3dmapsettings.h"
#include "qgs3drendercontext.h"
#include "qgs3dsymbolregistry.h"
#include "qgs3dutils.h"
#include "qgsapplication.h"
#include "qgsvectorlayer.h"
#include "qgsvectorlayer3drenderer.h"
#include "qgsxmlutils.h"

#include <QString>

using namespace Qt::StringLiterals;

QgsRuleBased3DRendererMetadata::QgsRuleBased3DRendererMetadata()
  : Qgs3DRendererAbstractMetadata( u"rulebased"_s )
{}

QgsAbstract3DRenderer *QgsRuleBased3DRendererMetadata::createRenderer( QDomElement &elem, const QgsReadWriteContext &context )
{
  QDomElement rulesElem = elem.firstChildElement( u"rules"_s );

  QgsRuleBased3DRenderer::Rule *root = QgsRuleBased3DRenderer::Rule::create( rulesElem, context );
  if ( !root )
    return nullptr;

  QgsRuleBased3DRenderer *r = new QgsRuleBased3DRenderer( root );
  r->readXml( elem, context );
  return r;
}

// ---------

////////////////////

QgsRuleBased3DRenderer::Rule::Rule( QgsAbstract3DSymbol *symbol, const QString &filterExp, const QString &description, bool elseRule )
  : mSymbol( symbol )
  , mFilterExp( filterExp )
  , mDescription( description )
  , mElseRule( elseRule )

{
  initFilter();
}

QgsRuleBased3DRenderer::Rule::~Rule()
{
  qDeleteAll( mChildren );
  // do NOT delete parent
}

void QgsRuleBased3DRenderer::Rule::setSymbol( QgsAbstract3DSymbol *symbol )
{
  if ( mSymbol.get() == symbol )
    return;

  mSymbol.reset( symbol );
}

QgsRuleBased3DRenderer::RuleList QgsRuleBased3DRenderer::Rule::descendants() const
{
  RuleList l;
  for ( Rule *c : mChildren )
  {
    l += c;
    l += c->descendants();
  }
  return l;
}

void QgsRuleBased3DRenderer::Rule::initFilter()
{
  if ( mElseRule || mFilterExp.compare( "ELSE"_L1, Qt::CaseInsensitive ) == 0 )
  {
    mElseRule = true;
    mFilter.reset( nullptr );
  }
  else if ( !mFilterExp.isEmpty() )
  {
    mFilter = std::make_unique<QgsExpression>( mFilterExp );
  }
  else
  {
    mFilter.reset( nullptr );
  }
}

void QgsRuleBased3DRenderer::Rule::updateElseRules()
{
  mElseRules.clear();
  for ( Rule *rule : std::as_const( mChildren ) )
  {
    if ( rule->isElse() )
      mElseRules << rule;
  }
}

void QgsRuleBased3DRenderer::Rule::appendChild( QgsRuleBased3DRenderer::Rule *rule )
{
  mChildren.append( rule );
  rule->mParent = this;
  updateElseRules();
}

void QgsRuleBased3DRenderer::Rule::insertChild( int i, QgsRuleBased3DRenderer::Rule *rule )
{
  mChildren.insert( i, rule );
  rule->mParent = this;
  updateElseRules();
}

void QgsRuleBased3DRenderer::Rule::removeChildAt( int i )
{
  delete mChildren.at( i );
  mChildren.removeAt( i );
  updateElseRules();
}

const QgsRuleBased3DRenderer::Rule *QgsRuleBased3DRenderer::Rule::findRuleByKey( const QString &key ) const
{
  // we could use a hash / map for search if this will be slow...

  if ( key == mRuleKey )
    return this;

  for ( Rule *rule : std::as_const( mChildren ) )
  {
    const Rule *r = rule->findRuleByKey( key );
    if ( r )
      return r;
  }
  return nullptr;
}

QgsRuleBased3DRenderer::Rule *QgsRuleBased3DRenderer::Rule::findRuleByKey( const QString &key )
{
  if ( key == mRuleKey )
    return this;

  for ( Rule *rule : std::as_const( mChildren ) )
  {
    Rule *r = rule->findRuleByKey( key );
    if ( r )
      return r;
  }
  return nullptr;
}

QgsRuleBased3DRenderer::Rule *QgsRuleBased3DRenderer::Rule::clone() const
{
  QgsAbstract3DSymbol *symbol = mSymbol.get() ? mSymbol->clone() : nullptr;
  Rule *newrule = new Rule( symbol, mFilterExp, mDescription );
  newrule->setActive( mIsActive );
  // clone children
  for ( Rule *rule : std::as_const( mChildren ) )
    newrule->appendChild( rule->clone() );
  return newrule;
}

QgsRuleBased3DRenderer::Rule *QgsRuleBased3DRenderer::Rule::create( const QDomElement &ruleElem, const QgsReadWriteContext &context )
{
  QgsAbstract3DSymbol *symbol = nullptr;
  QDomElement elemSymbol = ruleElem.firstChildElement( u"symbol"_s );
  if ( !elemSymbol.isNull() )
  {
    QString symbolType = elemSymbol.attribute( u"type"_s );
    symbol = QgsApplication::symbol3DRegistry()->createSymbol( symbolType );
    if ( symbol )
      symbol->readXml( elemSymbol, context );
  }

  QString filterExp = ruleElem.attribute( u"filter"_s );
  QString description = ruleElem.attribute( u"description"_s );
  QString ruleKey = ruleElem.attribute( u"key"_s );
  Rule *rule = new Rule( symbol, filterExp, description );

  if ( !ruleKey.isEmpty() )
    rule->mRuleKey = ruleKey;

  rule->setActive( ruleElem.attribute( u"active"_s, u"1"_s ).toInt() );

  QDomElement childRuleElem = ruleElem.firstChildElement( u"rule"_s );
  while ( !childRuleElem.isNull() )
  {
    Rule *childRule = create( childRuleElem, context );
    if ( childRule )
    {
      rule->appendChild( childRule );
    }
    else
    {
      //QgsDebugError( u"failed to init a child rule!"_s );
    }
    childRuleElem = childRuleElem.nextSiblingElement( u"rule"_s );
  }

  return rule;
}

QDomElement QgsRuleBased3DRenderer::Rule::save( QDomDocument &doc, const QgsReadWriteContext &context ) const
{
  QDomElement ruleElem = doc.createElement( u"rule"_s );

  if ( mSymbol )
  {
    QDomElement elemSymbol = doc.createElement( u"symbol"_s );
    elemSymbol.setAttribute( u"type"_s, mSymbol->type() );
    mSymbol->writeXml( elemSymbol, context );
    ruleElem.appendChild( elemSymbol );
  }

  if ( !mFilterExp.isEmpty() )
    ruleElem.setAttribute( u"filter"_s, mFilterExp );
  if ( !mDescription.isEmpty() )
    ruleElem.setAttribute( u"description"_s, mDescription );
  if ( !mIsActive )
    ruleElem.setAttribute( u"active"_s, 0 );
  ruleElem.setAttribute( u"key"_s, mRuleKey );

  for ( RuleList::const_iterator it = mChildren.constBegin(); it != mChildren.constEnd(); ++it )
  {
    Rule *rule = *it;
    ruleElem.appendChild( rule->save( doc, context ) );
  }
  return ruleElem;
}

bool QgsRuleBased3DRenderer::Rule::isFilterOK( const QgsFeature &f, Qgs3DRenderContext &context ) const
{
  if ( !mFilter || mElseRule )
    return true;

  context.expressionContext().setFeature( f );
  QVariant res = mFilter->evaluate( &context.expressionContext() );
  return res.toInt() != 0;
}

////////////////////

QgsRuleBased3DRenderer::QgsRuleBased3DRenderer( QgsRuleBased3DRenderer::Rule *root )
  : mRootRule( root )
{}

QgsRuleBased3DRenderer::~QgsRuleBased3DRenderer()
{}

QgsRuleBased3DRenderer *QgsRuleBased3DRenderer::clone() const
{
  Rule *rootRule = mRootRule->clone();

  // normally with clone() the individual rules get new keys (UUID), but here we want to keep
  // the tree of rules intact, so that other components that may use the rule keys work nicely (e.g. map themes)
  rootRule->setRuleKey( mRootRule->ruleKey() );
  RuleList origDescendants = mRootRule->descendants();
  RuleList clonedDescendants = rootRule->descendants();
  Q_ASSERT( origDescendants.count() == clonedDescendants.count() );
  for ( int i = 0; i < origDescendants.count(); ++i )
    clonedDescendants[i]->setRuleKey( origDescendants[i]->ruleKey() );

  QgsRuleBased3DRenderer *r = new QgsRuleBased3DRenderer( rootRule );
  copyBaseProperties( r );
  return r;
}

Qgs3DMapSceneEntity *QgsRuleBased3DRenderer::createEntity( Qgs3DMapSettings *map ) const
{
  Q_UNUSED( map )
  // the vector layer chunk loaders have not been ported to the QRhi engine yet, so the
  // layer contributes nothing to a 3D scene
  return nullptr;
}

void QgsRuleBased3DRenderer::writeXml( QDomElement &elem, const QgsReadWriteContext &context ) const
{
  QDomDocument doc = elem.ownerDocument();

  writeXmlBaseProperties( elem, context );

  QDomElement rulesElem = mRootRule->save( doc, context );
  rulesElem.setTagName( u"rules"_s ); // instead of just "rule"
  elem.appendChild( rulesElem );
}

void QgsRuleBased3DRenderer::readXml( const QDomElement &elem, const QgsReadWriteContext &context )
{
  readXmlBaseProperties( elem, context );

  // root rule is read before class constructed
}

std::unique_ptr<QgsRuleBased3DRenderer> QgsRuleBased3DRenderer::convertFromRenderer( const QgsAbstractVectorLayer3DRenderer *renderer, QgsVectorLayer * )
{
  std::unique_ptr< QgsRuleBased3DRenderer > r;
  if ( renderer->type() == "rulebased"_L1 )
  {
    r.reset( dynamic_cast<const QgsRuleBased3DRenderer *>( renderer )->clone() );
  }
  else if ( renderer->type() == "vector"_L1 )
  {
    const QgsVectorLayer3DRenderer *singleSymbolRenderer = dynamic_cast<const QgsVectorLayer3DRenderer *>( renderer );
    if ( !singleSymbolRenderer )
      return nullptr;

    std::unique_ptr< QgsAbstract3DSymbol > origSymbol( singleSymbolRenderer->symbol()->clone() );
    auto rootRule = std::make_unique< Rule >( nullptr );
    rootRule->appendChild( new Rule( origSymbol.release() ) );

    r = std::make_unique< QgsRuleBased3DRenderer >( rootRule.release() );
  }

  if ( r )
  {
    renderer->copyBaseProperties( r.get() );
  }

  return r;
}
