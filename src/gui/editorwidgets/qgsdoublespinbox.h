/***************************************************************************
    qgsdoublespinbox.h
     --------------------------------------
    Date                 : 09.2014
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

#ifndef QGSDOUBLESPINBOX_H
#define QGSDOUBLESPINBOX_H

#include <QDoubleSpinBox>

class QgsSpinBoxLineEdit;

/** \ingroup gui
 * @brief The QgsSpinBox is a spin box with a clear button that will set the value to the defined clear value.
 * The clear value can be either the minimum or the maiximum value of the spin box or a custom value.
 * This value can then be handled by a special value text.
 */
class GUI_EXPORT QgsDoubleSpinBox : public QDoubleSpinBox
{
    Q_OBJECT
    Q_PROPERTY( bool showClearButton READ showClearButton WRITE setShowClearButton )
    Q_PROPERTY( bool clearValue READ clearValue WRITE setClearValue )
    Q_PROPERTY( bool expressionsEnabled READ expressionsEnabled WRITE setExpressionsEnabled )

  public:

    //! Behaviour when widget is cleared.
    enum ClearValueMode
    {
      MinimumValue, //!< Reset value to minimum()
      MaximumValue, //!< Reset value to maximum()
      CustomValue, //!< Reset value to custom value (see setClearValue() )
    };

    /** Constructor for QgsDoubleSpinBox.
     * @param parent parent widget
     */
    explicit QgsDoubleSpinBox( QWidget *parent = nullptr );

    /** Sets whether the widget will show a clear button. The clear button
     * allows users to reset the widget to a default or empty state.
     * @param showClearButton set to true to show the clear button, or false to hide it
     * @see showClearButton()
     */
    void setShowClearButton( const bool showClearButton );

    /** Returns whether the widget is showing a clear button.
     * @see setShowClearButton()
     */
    bool showClearButton() const {return mShowClearButton;}

    /** Sets if the widget will allow entry of simple expressions, which are
     * evaluated and then discarded.
     * @param enabled set to true to allow expression entry
     * @note added in QGIS 2.7
     */
    void setExpressionsEnabled( const bool enabled );

    /** Returns whether the widget will allow entry of simple expressions, which are
     * evaluated and then discarded.
     * @returns true if spin box allows expression entry
     * @note added in QGIS 2.7
     */
    bool expressionsEnabled() const {return mExpressionsEnabled;}

    //! Set the current value to the value defined by the clear value.
    virtual void clear() override;

    /**
     * Defines the clear value as a custom value and will automatically set the clear value mode to CustomValue.
     * @param customValue defines the numerical value used as the clear value
     * @param clearValueText is the text displayed when the spin box is at the clear value. If not specified, no special value text is used.
     * @see setClearValue()
     */
    void setClearValue( double customValue, const QString& clearValueText = QString() );

    /**
     * Defines if the clear value should be the minimum or maximum values of the widget or a custom value.
     * @param mode mode to user for clear value
     * @param clearValueText is the text displayed when the spin box is at the clear value. If not specified, no special value text is used.
     */
    void setClearValueMode( ClearValueMode mode, const QString& clearValueText = QString() );

    /** Returns the value used when clear() is called.
     * @see setClearValue()
     */
    double clearValue() const;

    virtual double valueFromText( const QString & text ) const override;
    virtual QValidator::State validate( QString & input, int & pos ) const override;
    void paintEvent( QPaintEvent* e ) override;

  protected:
    virtual void changeEvent( QEvent* event ) override;

  private slots:
    void changed( double value );

  private:
    int frameWidth() const;
    bool shouldShowClearForValue( const double value ) const;

    QgsSpinBoxLineEdit* mLineEdit;

    bool mShowClearButton;
    ClearValueMode mClearValueMode;
    double mCustomClearValue;

    bool mExpressionsEnabled;

    QString stripped( const QString &originalText ) const;
};

#endif // QGSDOUBLESPINBOX_H
