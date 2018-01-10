/*
 * ConfigCommandLinePlugin.cpp - implementation of ConfigCommandLinePlugin class
 *
 * Copyright (c) 2017-2018 Tobias Junghans <tobydox@users.sf.net>
 *
 * This file is part of Veyon - http://veyon.io
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

#include "DefaultNetworkObjectDirectoryConfigurationPage.h"
#include "DefaultNetworkObjectDirectory.h"
#include "DefaultNetworkObjectDirectoryPlugin.h"

DefaultNetworkObjectDirectoryPlugin::DefaultNetworkObjectDirectoryPlugin( QObject* parent ) :
	QObject( parent ),
	m_configuration()
{
}



DefaultNetworkObjectDirectoryPlugin::~DefaultNetworkObjectDirectoryPlugin()
{
}



NetworkObjectDirectory *DefaultNetworkObjectDirectoryPlugin::createNetworkObjectDirectory( QObject* parent )
{
	return new DefaultNetworkObjectDirectory( m_configuration, parent );
}



ConfigurationPage *DefaultNetworkObjectDirectoryPlugin::createConfigurationPage()
{
	return new DefaultNetworkObjectDirectoryConfigurationPage( m_configuration );
}