/*
 * ComputerMonitoringWidget.cpp - provides a view with computer monitor thumbnails
 *
 * Copyright (c) 2017-2020 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include <QApplication>
#include <QMenu>
#include <QScrollBar>
#include <QShowEvent>

#include "ComputerControlListModel.h"
#include "ComputerMonitoringWidget.h"
#include "ComputerMonitoringModel.h"
#include "VeyonMaster.h"
#include "FeatureManager.h"
#include "VeyonConfiguration.h"


ComputerMonitoringWidget::ComputerMonitoringWidget( QWidget *parent ) :
	FlexibleListView( parent ),
	m_featureMenu( new QMenu( this ) )
{
	const auto computerMonitoringThumbnailSpacing = VeyonCore::config().computerMonitoringThumbnailSpacing();

	setContextMenuPolicy( Qt::CustomContextMenu );
	setAcceptDrops( true );
	setDragEnabled( true );
	setDragDropMode( QAbstractItemView::DropOnly );
	setDefaultDropAction( Qt::MoveAction );
	setSelectionMode( QAbstractItemView::ExtendedSelection );
	setFlow( QListView::LeftToRight );
	setWrapping( true );
	setResizeMode( QListView::Adjust );
	setSpacing( computerMonitoringThumbnailSpacing  );
	setViewMode( QListView::IconMode );
	setUniformItemSizes( true );
	setSelectionRectVisible( true );

	setUidRole( ComputerControlListModel::UidRole );

	connect( this, &QListView::doubleClicked, this, &ComputerMonitoringWidget::runDoubleClickFeature );
    connect( this, &QListView::customContextMenuRequested,
			 this, [this]( QPoint pos ) { showContextMenu( mapToGlobal( pos ) ); } );

	initializeView( this );

	setModel( dataModel() );
}



ComputerControlInterfaceList ComputerMonitoringWidget::selectedComputerControlInterfaces() const
{
	ComputerControlInterfaceList computerControlInterfaces;

	const auto selectedIndices = selectionModel()->selectedIndexes(); // clazy:exclude=inefficient-qlist
	computerControlInterfaces.reserve( selectedIndices.size() );

	for( const auto& index : selectedIndices )
	{
		computerControlInterfaces.append( model()->data( index, ComputerControlListModel::ControlInterfaceRole )
											  .value<ComputerControlInterface::Pointer>() );
	}

	return computerControlInterfaces;
}



bool ComputerMonitoringWidget::performIconSizeAutoAdjust()
{
	if( ComputerMonitoringView::performIconSizeAutoAdjust() == false)
	{
		return false;
	}

	m_ignoreResizeEvent = true;

	auto size = iconSize().width();

	setComputerScreenSize( size );
	QApplication::processEvents();

	while( verticalScrollBar()->isVisible() == false &&
		   horizontalScrollBar()->isVisible() == false &&
		   size < MaximumComputerScreenSize )
	{
		size += IconSizeAdjustStepSize;
		setComputerScreenSize( size );
		QApplication::processEvents();
	}

	while( ( verticalScrollBar()->isVisible() ||
			 horizontalScrollBar()->isVisible() ) &&
		   size > MinimumComputerScreenSize )
	{
		size -= IconSizeAdjustStepSize;
		setComputerScreenSize( size );
		QApplication::processEvents();
	}

	Q_EMIT computerScreenSizeAdjusted( size );

	m_ignoreResizeEvent = false;

	return true;
}




void ComputerMonitoringWidget::setUseCustomComputerPositions( bool enabled )
{
	setFlexible( enabled );
}



void ComputerMonitoringWidget::alignComputers()
{
	alignToGrid();
}



void ComputerMonitoringWidget::showContextMenu( QPoint globalPos )
{
	populateFeatureMenu( selectedComputerControlInterfaces() );

	m_featureMenu->exec( globalPos );
}



void ComputerMonitoringWidget::setIconSize( const QSize& size )
{
	QAbstractItemView::setIconSize( size );
}



void ComputerMonitoringWidget::setColors( const QColor& backgroundColor, const QColor& textColor )
{
	auto pal = palette();
	pal.setColor( QPalette::Base, backgroundColor );
	pal.setColor( QPalette::Text, textColor );
	setPalette( pal );
}



QJsonArray ComputerMonitoringWidget::saveComputerPositions()
{
	return savePositions();
}



bool ComputerMonitoringWidget::useCustomComputerPositions()
{
	return flexible();
}



void ComputerMonitoringWidget::loadComputerPositions( const QJsonArray& positions )
{
	loadPositions( positions );
}



void ComputerMonitoringWidget::populateFeatureMenu(  const ComputerControlInterfaceList& computerControlInterfaces )
{
	Plugin::Uid previousPluginUid;

	m_featureMenu->clear();

	for( const auto& feature : master()->features() )
	{
        if( feature.testFlag( Feature::Internal ) || feature.testFlag( Feature::NoContext ) )
		{
			continue;
		}

		Plugin::Uid pluginUid = master()->featureManager().pluginUid( feature );

		if( previousPluginUid.isNull() == false &&
			pluginUid != previousPluginUid &&
			feature.testFlag( Feature::Mode ) == false )
		{
			m_featureMenu->addSeparator();
		}

		previousPluginUid = pluginUid;

		auto active = false;

		auto label = feature.displayName();
		if( feature.displayNameActive().isEmpty() == false &&
			isFeatureOrSubFeatureActive( computerControlInterfaces, feature.uid() ) )
		{
			label = feature.displayNameActive();
			active = true;
		}

		const auto subFeatures = master()->subFeatures( feature.uid() );
		if( subFeatures.isEmpty() || active )
		{
			addFeatureToMenu( feature, label );
		}
		else
		{
			addSubFeaturesToMenu( feature, subFeatures, label );
		}
	}
}



void ComputerMonitoringWidget::addFeatureToMenu( const Feature& feature, const QString& label )
{
	m_featureMenu->addAction( QIcon( feature.iconUrl() ),
							  label,
							  m_featureMenu, [=] () { runFeature( feature ); } );
}



void ComputerMonitoringWidget::addSubFeaturesToMenu( const Feature& parentFeature, const FeatureList& subFeatures, const QString& label )
{
	auto menu = m_featureMenu->addMenu( QIcon( parentFeature.iconUrl() ), label );

	for( const auto& subFeature : subFeatures )
	{
		menu->addAction( QIcon( subFeature.iconUrl() ), subFeature.displayName(), m_featureMenu,
						 [=]() { runFeature( subFeature ); }, subFeature.shortcut() );
	}
}



void ComputerMonitoringWidget::runDoubleClickFeature( const QModelIndex& index )
{
	const Feature& feature = master()->featureManager().feature( VeyonCore::config().computerDoubleClickFeature() );

	if( index.isValid() && feature.isValid() )
	{
		selectionModel()->select( index, QItemSelectionModel::SelectCurrent );
		runFeature( feature );
	}
}



void ComputerMonitoringWidget::startMousePressAndHoldFeature( )
{
   Q_EMIT mousePressAndHold( );
   const Feature& feature = master()->featureManager().feature( VeyonCore::config().computerLeftClickAndHoldFeature() );
   const auto selectedInterfaces = selectedComputerControlInterfaces();
   if ( !m_ignoreMousePressAndHoldEvent &&
        !isFeatureOrSubFeatureActive( selectedInterfaces, feature.uid() ) &&
        selectedInterfaces.count() > 0 &&
        selectedInterfaces.count() < 2 &&
        selectedInterfaces.first()->state() == ComputerControlInterface::State::Connected &&
        selectedInterfaces.first()->hasValidFramebuffer() )
   {
        m_ignoreMousePressAndHoldEvent = true;
        if( feature.isValid() )
        {
            runFeature( feature );
        }
   }
}

void ComputerMonitoringWidget::stopMousePressAndHoldFeature( )
{
    const Feature& feature = master()->featureManager().feature( VeyonCore::config().computerLeftClickAndHoldFeature() );
    const auto m_master( VeyonCore::instance()->findChild<VeyonMaster *>() );
    const auto selectedInterfaces = selectedComputerControlInterfaces();
    const auto previewHandlingNecessary = feature.uid() == "fddd638a-90a7-45a1-a339-ea6409a5eee5";
    if( isFeatureOrSubFeatureActive( selectedInterfaces, feature.uid() ) )
    {
        master()->featureManager().stopFeature( *m_master, feature, selectedInterfaces );
    }
    if ( previewHandlingNecessary )
    {
        auto widgets = QApplication::topLevelWidgets();
        for( const auto& widget : widgets )
        {
            auto windowTitle = widget->windowTitle();
            if ( !windowTitle.isEmpty() )
            {
                const auto str = selectedInterfaces.first()->computer().name();
                if ( windowTitle.contains(str, Qt::CaseInsensitive) )
                {
                    widget->close();
                }

            }
       }
    }

}

void ComputerMonitoringWidget::mousePressEvent( QMouseEvent* event )
{
    if(event->type() == QEvent::MouseButtonPress)
    {
        if( event->buttons() == Qt::LeftButton )
        {
            if( !m_ignoreMousePressAndHoldEvent )
            {
                t_mousePressAndHold.setInterval( 500 );
                t_mousePressAndHold.start();
                connect(&t_mousePressAndHold, &QTimer::timeout, this, &ComputerMonitoringWidget::startMousePressAndHoldFeature );
            }
        }
        QListView::mousePressEvent(event);
    }
}



void ComputerMonitoringWidget::mouseReleaseEvent( QMouseEvent* event )
{
    if(event->type() == QEvent::MouseButtonRelease)
    {
        t_mousePressAndHold.stop();
        if ( m_ignoreMousePressAndHoldEvent )
        {
            stopMousePressAndHoldFeature();
        }
        m_ignoreMousePressAndHoldEvent = false;
        Q_EMIT mousePressAndHoldRelease( );
        QListView::mouseReleaseEvent(event);
    }
}



void ComputerMonitoringWidget::mouseMoveEvent( QMouseEvent* event )
{
    if(event->type() == QEvent::MouseMove)
    {
        t_mousePressAndHold.stop();
        if ( m_ignoreMousePressAndHoldEvent )
        {
            stopMousePressAndHoldFeature();
        }
        m_ignoreMousePressAndHoldEvent = false;
        Q_EMIT mousePressAndHoldRelease( );
        QListView::mouseMoveEvent(event);
    }
}



void ComputerMonitoringWidget::resizeEvent( QResizeEvent* event )
{
	FlexibleListView::resizeEvent( event );

	if( m_ignoreResizeEvent == false )
	{
		initiateIconSizeAutoAdjust();
	}
}



void ComputerMonitoringWidget::showEvent( QShowEvent* event )
{
	if( event->spontaneous() == false )
	{
		initiateIconSizeAutoAdjust();
	}

	FlexibleListView::showEvent( event );
}



void ComputerMonitoringWidget::wheelEvent( QWheelEvent* event )
{
	if( m_ignoreWheelEvent == false &&
		event->modifiers().testFlag( Qt::ControlModifier ) )
	{
		setComputerScreenSize( iconSize().width() + event->angleDelta().y() / 8 );

		Q_EMIT computerScreenSizeAdjusted( computerScreenSize() );

		event->accept();
	}
	else
	{
		QListView::wheelEvent( event );
	}
}


