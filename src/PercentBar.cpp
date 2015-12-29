/*
 *   File name: PercentBar.cpp
 *   Summary:	Functions and item delegate for percent bar
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */

#include <QPainter>
#include <QTreeView>

#include "PercentBar.h"
#include "DirTreeModel.h"
#include "FileInfo.h"
#include "Logger.h"
#include "Exception.h"


using namespace QDirStat;


PercentBarDelegate::PercentBarDelegate( QTreeView * treeView ):
    QStyledItemDelegate( 0 ),
    _treeView( treeView )
{
    _percentBarCol = DataColumns::toViewCol( PercentBarCol );
    _fillColors << QColor( 0,	  0, 255 )
		<< QColor( 128,	  0, 128 )
		<< QColor( 231, 147,  43 )
		<< QColor(   4, 113,   0 )
		<< QColor( 176,	  0,   0 )
		<< QColor( 204, 187,   0 )
		<< QColor( 162,	 98,  30 )
		<< QColor(   0, 148, 146 )
		<< QColor( 217,	 94,   0 )
		<< QColor(   0, 194,  65 )
		<< QColor( 194, 108, 187 )
		<< QColor(   0, 179, 255 );

    _barBackground = QColor( 160, 160, 160 );

    // TO DO: Read colors from config

    connect( DataColumns::instance(), SIGNAL( columnsChanged() ),
             this,                    SLOT  ( columnsChanged() ) );
}


PercentBarDelegate::~PercentBarDelegate()
{

}


void PercentBarDelegate::columnsChanged()
{
    _percentBarCol = DataColumns::toViewCol( PercentBarCol );
}


void PercentBarDelegate::paint( QPainter		   * painter,
				const QStyleOptionViewItem & option,
				const QModelIndex	   & index ) const
{
    if ( ! index.isValid() || index.column() != _percentBarCol )
	return QStyledItemDelegate::paint( painter, option, index );

    QVariant data = index.data( RawDataRole );

    if ( data.isValid() )
    {
	bool ok = true;
	float percent = data.toFloat( &ok );

	if ( ok && percent >= 0.0 )
	{
	    if ( percent > 100.0f )
	    {
                if ( percent > 103.0f )
                    logError() << "Percent maxed out: " << percent << endl;
		percent = 100.0f;
	    }

	    int depth = treeLevel( index ) - 2; // compensate for invisible root and toplevel
	    int indentPixel  = ( depth * _treeView->indentation() ) / 2;
	    QColor fillColor = _fillColors.at( depth % _fillColors.size() );

	    paintPercentBar( percent,
			     painter,
			     indentPixel,
			     option.rect,
			     fillColor,
			     _barBackground );
	}
	else // percent < 0.0 => tree is busy => use as read job column
	{
	    return QStyledItemDelegate::paint( painter, option, index );
	}
    }
}


int PercentBarDelegate::treeLevel( const QModelIndex & index ) const
{
    int level = 0;
    QModelIndex item = index;

    while ( item.isValid() )
    {
	item = item.parent();
	++level;
    }

    return level;
}


QSize PercentBarDelegate::sizeHint( const QStyleOptionViewItem & option,
				    const QModelIndex	       & index) const
{
    QSize size = QStyledItemDelegate::sizeHint( option, index );

    if ( ! index.isValid() || index.column() != _percentBarCol )
	return size;

    size.setWidth( 180 ); // TO DO: Find a better value

    return size;
}



namespace QDirStat
{
    void paintPercentBar( float		 percent,
			  QPainter *	 painter,
			  int		 indentPixel,
			  const QRect  & cellRect,
			  const QColor & fillColor,
			  const QColor & barBackground )
    {
	int penWidth = 2;
	int extraMargin = 4;
	int itemMargin = 4;
	int x = cellRect.x() + itemMargin;
	int y = cellRect.y() + extraMargin;
	int w = cellRect.width() - 2 * itemMargin;
	int h = cellRect.height() - 2 * extraMargin;
	int fillWidth;

	painter->eraseRect( cellRect );
	w -= indentPixel;
	x += indentPixel;

	if ( w > 0 )
	{
	    QPen pen( painter->pen() );
	    pen.setWidth( 0 );
	    painter->setPen( pen );
	    painter->setBrush( Qt::NoBrush );
	    fillWidth = (int) ( ( w - 2 * penWidth ) * percent / 100.0);


	    // Fill bar background.

	    painter->fillRect( x + penWidth, y + penWidth,
			       w - 2 * penWidth + 1, h - 2 * penWidth + 1,
			       barBackground );
	    /*
	     * Notice: The Xlib XDrawRectangle() function always fills one
	     * pixel less than specified. Altough this is very likely just a
	     * plain old bug, it is documented that way. Obviously, Qt just
	     * maps the fillRect() call directly to XDrawRectangle() so they
	     * inherited that bug (although the Qt doc stays silent about
	     * it). So it is really necessary to compensate for that missing
	     * pixel in each dimension.
	     *
	     * If you don't believe it, see for yourself.
	     * Hint: Try the xmag program to zoom into the drawn pixels.
	     **/

	    // Fill the desired percentage.

	    painter->fillRect( x + penWidth, y + penWidth,
			       fillWidth+1, h - 2 * penWidth+1,
			       fillColor );


	    // Draw 3D shadows.

	    QColor background = painter->background().color();

	    pen.setColor( contrastingColor ( Qt::black, background ) );
	    painter->setPen( pen );
	    painter->drawLine( x, y, x+w, y );
	    painter->drawLine( x, y, x, y+h );

	    pen.setColor( contrastingColor( barBackground.dark(), background ) );
	    painter->setPen( pen );
	    painter->drawLine( x+1, y+1, x+w-1, y+1 );
	    painter->drawLine( x+1, y+1, x+1, y+h-1 );

	    pen.setColor( contrastingColor( barBackground.light(), background ) );
	    painter->setPen( pen );
	    painter->drawLine( x+1, y+h, x+w, y+h );
	    painter->drawLine( x+w, y, x+w, y+h );

	    pen.setColor( contrastingColor( Qt::white, background ) );
	    painter->setPen( pen );
	    painter->drawLine( x+2, y+h-1, x+w-1, y+h-1 );
	    painter->drawLine( x+w-1, y+1, x+w-1, y+h-1 );
	}
    }


    QColor contrastingColor( const QColor &desiredColor,
			     const QColor &contrastColor )
    {
	if ( desiredColor != contrastColor )
	{
	    return desiredColor;
	}

	if ( contrastColor != contrastColor.lighter() )
	{
	    // try a little lighter
	    return contrastColor.lighter();
	}
	else
	{
	    // try a little darker
	    return contrastColor.darker();
	}
    }

} // namespace QDirStat
