/*
 * QDigiDocCrypto
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#pragma once

#include <QtCore/QObject>

class QSslCertificate;
class LdapSearchPrivate;
class LdapSearch: public QObject
{
	Q_OBJECT

public:
	LdapSearch( QObject *parent = 0 );
	~LdapSearch();

	void search( const QString &search );

Q_SIGNALS:
	void searchResult( const QList<QSslCertificate> &result );
	void error( const QString &msg );

private:
	bool init();
	void timerEvent( QTimerEvent *e );
	void setLastError( const QString &msg, int err );

	LdapSearchPrivate *d;
};
