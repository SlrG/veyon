/*
 * FeatureWorkerManager.cpp - class for managing feature worker instances
 *
 * Copyright (c) 2017 Tobias Doerffel <tobydox/at/users/dot/sf/dot/net>
 *
 * This file is part of iTALC - http://italc.sourceforge.net
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

#include <QCoreApplication>
#include <QDir>
#include <QThread>
#include <QTimer>

#include "FeatureWorkerManager.h"
#include "ItalcConfiguration.h"
#include "ItalcCore.h"
#include "FeatureManager.h"


Q_DECLARE_METATYPE(Feature)
Q_DECLARE_METATYPE(FeatureMessage)


FeatureWorkerManager::FeatureWorkerManager( FeatureManager& featureManager ) :
	QObject(),
	m_featureManager( featureManager ),
	m_tcpServer( this )
{
	connect( &m_tcpServer, &QTcpServer::newConnection,
			 this, &FeatureWorkerManager::acceptConnection );

	if( !m_tcpServer.listen( QHostAddress::LocalHost, ItalcCore::config->featureWorkerManagerPort() ) )
	{
		qCritical( "FeatureWorkerManager: can't listen on localhost!" );
	}

	qRegisterMetaType<Feature>();
	qRegisterMetaType<FeatureMessage>();
}



FeatureWorkerManager::~FeatureWorkerManager()
{
}



void FeatureWorkerManager::startWorker( const Feature& feature )
{
	if( thread() != QThread::currentThread() )
	{
		QMetaObject::invokeMethod( this, "startWorker", Qt::BlockingQueuedConnection,
								   Q_ARG( const Feature&, feature ) );
		return;
	}

	stopWorker( feature );

	Worker worker;

	worker.process = new QProcess( this );
	worker.process->setProcessChannelMode( QProcess::ForwardedChannels );

	connect( worker.process, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
			 worker.process, &QProcess::deleteLater );

	qDebug() << "Starting worker for feature" << feature.displayName() << feature.uid();
	worker.process->start( workerProcessFilePath(), QStringList( { feature.uid().toString() } ) );

	m_workersMutex.lock();
	m_workers[feature.uid()] = worker;
	m_workersMutex.unlock();
}



void FeatureWorkerManager::stopWorker( const Feature &feature )
{
	if( thread() != QThread::currentThread() )
	{
		QMetaObject::invokeMethod( this, "stopWorker", Qt::BlockingQueuedConnection,
								   Q_ARG( const Feature&, feature ) );
		return;
	}

	m_workersMutex.lock();

	if( m_workers.contains( feature.uid() ) )
	{
		qDebug() << "Stopping worker for feature" << feature.displayName() << feature.uid();

		auto& worker = m_workers[feature.uid()];

		if( worker.socket )
		{
			worker.socket->close();
			worker.socket->deleteLater();
		}

		if( worker.process )
		{
			QTimer* killTimer = new QTimer;
			connect( killTimer, &QTimer::timeout, worker.process, &QProcess::terminate );
			connect( killTimer, &QTimer::timeout, worker.process, &QProcess::kill );
			connect( killTimer, &QTimer::timeout, killTimer, &QTimer::deleteLater );
			killTimer->start( 5000 );
		}

		m_workers.remove( feature.uid() );
	}

	m_workersMutex.unlock();
}



void FeatureWorkerManager::sendMessage( const FeatureMessage& message )
{
	if( thread() != QThread::currentThread() )
	{
		QMetaObject::invokeMethod( this, "sendMessage", Qt::BlockingQueuedConnection,
								   Q_ARG( const FeatureMessage&, message ) );
		return;
	}

	m_workersMutex.lock();

	if( m_workers.contains( message.featureUid() ) )
	{
		auto& worker = m_workers[message.featureUid()];

		if( worker.socket )
		{
			message.send( worker.socket );
		}
		else
		{
			worker.pendingMessages += message;
		}
	}
	else
	{
		qCritical() << "FeatureWorkerManager::sendMessage(): trying to send message to non existent worker for feature"
					<< message.featureUid();
	}

	m_workersMutex.unlock();
}



void FeatureWorkerManager::acceptConnection()
{
	qDebug( "FeatureWorkerManager: accepting connection" );

	QTcpSocket* socket = m_tcpServer.nextPendingConnection();

	// connect to readyRead() signal of new connection
	connect( socket, &QTcpSocket::readyRead,
			 this, [=] () { processConnection( socket ); } );
}



void FeatureWorkerManager::processConnection( QTcpSocket* socket )
{
	FeatureMessage message( socket );
	message.receive();

	m_workersMutex.lock();

	// set socket information
	if( m_workers.contains( message.featureUid() ) )
	{
		if( message.arguments().isEmpty() == false )
		{
			m_featureManager.handleServiceFeatureMessage( message, socket, *this );
		}

		// send pending messages
		auto& worker = m_workers[message.featureUid()];

		worker.socket = socket;

		for( auto message : worker.pendingMessages )
		{
			message.send( socket );
		}

		worker.pendingMessages.clear();
	}
	else
	{
		qCritical() << "FeatureWorkerManager: got data from non-existing worker!" << message.featureUid();
	}

	m_workersMutex.unlock();
}



QString FeatureWorkerManager::workerProcessFilePath()
{
	return QCoreApplication::applicationDirPath() + QDir::separator() + "italc-worker";
}

