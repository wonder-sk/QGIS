#include "qgsidentifypanel.h"

#include "qgsfeature.h"
#include "qgsfields.h"
#include "qgsmaplayer.h"

#include <QHeaderView>
#include <QString>
#include <QTreeWidget>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

QgsIdentifyPanel::QgsIdentifyPanel( QWidget *parent )
  : QWidget( parent )
{
  auto *layout = new QVBoxLayout( this );
  layout->setContentsMargins( 0, 0, 0, 0 );
  mTree = new QTreeWidget( this );
  mTree->setColumnCount( 2 );
  mTree->setHeaderLabels( { tr( "Attribute" ), tr( "Value" ) } );
  mTree->header()->setStretchLastSection( true );
  mTree->setAlternatingRowColors( true );
  layout->addWidget( mTree );
}

void QgsIdentifyPanel::clear()
{
  mTree->clear();
}

void QgsIdentifyPanel::showResults( const QList<QgsMapToolIdentify::IdentifyResult> &results )
{
  mTree->clear();
  if ( results.isEmpty() )
  {
    auto *item = new QTreeWidgetItem( mTree, { tr( "(no features found)" ) } );
    item->setDisabled( true );
    return;
  }

  QHash<QgsMapLayer *, QTreeWidgetItem *> layerItems;
  for ( const QgsMapToolIdentify::IdentifyResult &result : results )
  {
    QgsMapLayer *layer = result.mLayer;
    if ( !layer )
      continue;

    QTreeWidgetItem *layerItem = layerItems.value( layer );
    if ( !layerItem )
    {
      layerItem = new QTreeWidgetItem( mTree, { layer->name() } );
      layerItem->setExpanded( true );
      layerItem->setFirstColumnSpanned( true );
      QFont bold = layerItem->font( 0 );
      bold.setBold( true );
      layerItem->setFont( 0, bold );
      layerItems.insert( layer, layerItem );
    }

    const QString label = result.mLabel.isEmpty() ? u"Feature %1"_s.arg( result.mFeature.id() ) : result.mLabel;
    auto *featureItem = new QTreeWidgetItem( layerItem, { label } );
    featureItem->setExpanded( true );

    // Vector: fields + feature attributes
    const QgsFields fields = result.mFields;
    if ( fields.count() > 0 )
    {
      for ( int i = 0; i < fields.count(); ++i )
        new QTreeWidgetItem( featureItem, { fields.at( i ).displayName(), result.mFeature.attribute( i ).toString() } );
    }
    else
    {
      // Raster / mesh: mAttributes map
      for ( auto it = result.mAttributes.constBegin(); it != result.mAttributes.constEnd(); ++it )
        new QTreeWidgetItem( featureItem, { it.key(), it.value() } );
    }

    // Derived attributes (pixel coords, distance, etc.)
    if ( !result.mDerivedAttributes.isEmpty() )
    {
      auto *derived = new QTreeWidgetItem( featureItem, { tr( "(derived)" ) } );
      for ( auto it = result.mDerivedAttributes.constBegin(); it != result.mDerivedAttributes.constEnd(); ++it )
        new QTreeWidgetItem( derived, { it.key(), it.value() } );
    }
  }
  mTree->resizeColumnToContents( 0 );
}

#include "moc_qgsidentifypanel.cpp"
