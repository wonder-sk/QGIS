/***************************************************************************
    qgsdatetimeeditconfig.cpp
     --------------------------------------
    Date                 : 03.2014
    Copyright            : (C) 2014 Denis Rouzaud
    Email                : denis.rouzaud@gmail.com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsdatetimeeditconfig.h"
#include "qgsdatetimeeditfactory.h"
#include "qgsvectorlayer.h"

QgsDateTimeEditConfig::QgsDateTimeEditConfig( QgsVectorLayer* vl, int fieldIdx, QWidget* parent )
    : QgsEditorConfigWidget( vl, fieldIdx, parent )
{
  setupUi( this );

  mDemoDateTimeEdit->setDateTime( QDateTime::currentDateTime() );

  connect( mDisplayFormatEdit, SIGNAL( textChanged( QString ) ), this, SLOT( updateDemoWidget() ) );
  connect( mCalendarPopupCheckBox, SIGNAL( toggled( bool ) ), this, SLOT( updateDemoWidget() ) );

  connect( mFieldFormatComboBox, SIGNAL( currentIndexChanged( int ) ), this, SLOT( updateFieldFormat( int ) ) );
  connect( mFieldFormatEdit, SIGNAL( textChanged( QString ) ), this, SLOT( updateDisplayFormat( QString ) ) );
  connect( mDisplayFormatComboBox, SIGNAL( currentIndexChanged( int ) ), this, SLOT( displayFormatChanged( int ) ) );

  connect( mFieldHelpToolButton, SIGNAL( clicked( bool ) ), this, SLOT( showHelp( bool ) ) );
  connect( mDisplayHelpToolButton, SIGNAL( clicked( bool ) ), this, SLOT( showHelp( bool ) ) );

  connect( mFieldFormatEdit, SIGNAL( textChanged( QString ) ), this, SIGNAL( changed() ) );
  connect( mDisplayFormatEdit, SIGNAL( textChanged( QString ) ), this, SIGNAL( changed() ) );
  connect( mCalendarPopupCheckBox, SIGNAL( toggled( bool ) ), this, SIGNAL( changed() ) );
  connect( mAllowNullCheckBox, SIGNAL( toggled( bool ) ), this, SIGNAL( changed() ) );

  // initialize
  updateFieldFormat( mFieldFormatComboBox->currentIndex() );
  displayFormatChanged( mDisplayFormatComboBox->currentIndex() );
}


void QgsDateTimeEditConfig::updateDemoWidget()
{
  mDemoDateTimeEdit->setDisplayFormat( mDisplayFormatEdit->text() );
  mDemoDateTimeEdit->setCalendarPopup( mCalendarPopupCheckBox->isChecked() );
}


void QgsDateTimeEditConfig::updateFieldFormat( int idx )
{
  if ( idx == 0 )
  {
    mFieldFormatEdit->setText( QGSDATETIMEEDIT_DATEFORMAT );
  }
  else if ( idx == 1 )
  {
    mFieldFormatEdit->setText( QGSDATETIMEEDIT_TIMEFORMAT );
  }
  else if ( idx == 2 )
  {
    mFieldFormatEdit->setText( QGSDATETIMEEDIT_DATETIMEFORMAT );
  }

  mFieldFormatEdit->setVisible( idx == 3 );
  mFieldHelpToolButton->setVisible( idx == 3 );
  if ( mFieldHelpToolButton->isHidden() && mDisplayHelpToolButton->isHidden() )
  {
    mHelpScrollArea->setVisible( false );
  }
}


void QgsDateTimeEditConfig::updateDisplayFormat( const QString& fieldFormat )
{
  if ( mDisplayFormatComboBox->currentIndex() == 0 )
  {
    mDisplayFormatEdit->setText( fieldFormat );
  }
}


void QgsDateTimeEditConfig::displayFormatChanged( int idx )
{
  mDisplayFormatEdit->setEnabled( idx == 1 );
  mDisplayHelpToolButton->setVisible( idx == 1 );
  if ( mFieldHelpToolButton->isHidden() && mDisplayHelpToolButton->isHidden() )
  {
    mHelpScrollArea->setVisible( false );
  }
  if ( idx == 0 )
  {
    mDisplayFormatEdit->setText( mFieldFormatEdit->text() );
  }
}

void QgsDateTimeEditConfig::showHelp( bool buttonChecked )
{
  mFieldHelpToolButton->setChecked( buttonChecked );
  mDisplayHelpToolButton->setChecked( buttonChecked );
  mHelpScrollArea->setVisible( buttonChecked );
}


QgsEditorWidgetConfig QgsDateTimeEditConfig::config()
{
  QgsEditorWidgetConfig myConfig;

  myConfig.insert( QStringLiteral( "field_format" ), mFieldFormatEdit->text() );
  myConfig.insert( QStringLiteral( "display_format" ), mDisplayFormatEdit->text() );
  myConfig.insert( QStringLiteral( "calendar_popup" ), mCalendarPopupCheckBox->isChecked() );
  myConfig.insert( QStringLiteral( "allow_null" ), mAllowNullCheckBox->isChecked() );

  return myConfig;
}


QString QgsDateTimeEditConfig::defaultFormat( const QVariant::Type type )
{
  switch ( type )
  {
    case QVariant::DateTime:
      return QGSDATETIMEEDIT_DATETIMEFORMAT;
      break;
    case QVariant::Time:
      return QGSDATETIMEEDIT_TIMEFORMAT;
      break;
    default:
      return QGSDATETIMEEDIT_DATEFORMAT;
  }
}


void QgsDateTimeEditConfig::setConfig( const QgsEditorWidgetConfig &config )
{
  const QgsField fieldDef = layer()->fields().at( field() );
  const QString fieldFormat = config.value( QStringLiteral( "field_format" ), defaultFormat( fieldDef.type() ) ).toString();
  mFieldFormatEdit->setText( fieldFormat );

  if ( fieldFormat == QGSDATETIMEEDIT_DATEFORMAT )
    mFieldFormatComboBox->setCurrentIndex( 0 );
  else if ( fieldFormat == QGSDATETIMEEDIT_TIMEFORMAT )
    mFieldFormatComboBox->setCurrentIndex( 1 );
  else if ( fieldFormat == QGSDATETIMEEDIT_DATETIMEFORMAT )
    mFieldFormatComboBox->setCurrentIndex( 2 );
  else
    mFieldFormatComboBox->setCurrentIndex( 3 );

  QString displayFormat = config.value( QStringLiteral( "display_format" ), defaultFormat( fieldDef.type() ) ).toString();
  mDisplayFormatEdit->setText( displayFormat );
  if ( displayFormat == mFieldFormatEdit->text() )
  {
    mDisplayFormatComboBox->setCurrentIndex( 0 );
  }
  else
  {
    mDisplayFormatComboBox->setCurrentIndex( 1 );
  }

  mCalendarPopupCheckBox->setChecked( config.value( QStringLiteral( "calendar_popup" ) , false ).toBool() );
  mAllowNullCheckBox->setChecked( config.value( QStringLiteral( "allow_null" ), true ).toBool() );
}
