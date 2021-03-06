/*
 * QDigiDocClient
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

#include "SignatureDialog.h"

#include "ui_SignatureDialog.h"
#include "Application.h"

#include <common/CertificateWidget.h>
#include <common/Common.h>
#include <common/DateTime.h>
#include <common/SslCertificate.h>

#include <digidocpp/DataFile.h>

#include <QtCore/QTextStream>
#include <QtCore/QUrl>
#if QT_VERSION >= 0x050000
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#else
#include <QtGui/QMessageBox>
#include <QtGui/QPushButton>
Q_DECLARE_METATYPE(QSslCertificate)
#endif
#include <QtGui/QDesktopServices>
#include <QtGui/QTextDocument>
#include <QtNetwork/QSslKey>

SignatureWidget::SignatureWidget( const DigiDocSignature &signature, unsigned int signnum, QWidget *parent )
:	QLabel( parent )
,	num( signnum )
,	s( signature )
{
	setObjectName( QString("signatureWidget%1").arg(signnum) );
	setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Preferred );
	setWordWrap( true );
	setTextInteractionFlags( Qt::LinksAccessibleByKeyboard|Qt::LinksAccessibleByMouse );
	connect( this, SIGNAL(linkActivated(QString)), SLOT(link(QString)) );

	const SslCertificate cert = s.cert();
	QString accessibility, content, tooltip;
	QTextStream sa( &accessibility );
	QTextStream sc( &content );
	QTextStream st( &tooltip );

	if( cert.type() & SslCertificate::TempelType )
		sc << "<img src=\":/images/ico_stamp_blue_16.png\">";
	else
		sc << "<img src=\":/images/ico_person_blue_16.png\">";
	sc << "<b>" << Qt::escape( cert.toString( cert.showCN() ? "CN" : "GN SN" ) ) << "</b>";

	if( !s.location().isEmpty() )
	{
		sa << " " << tr("Location") << " " << s.location();
		sc << "<br />" << Qt::escape( s.location() );
		st << Qt::escape( s.location() ) << "<br />";
	}
	if( !s.role().isEmpty() )
	{
		sa << " " << tr("Role") << " " << s.role();
		sc << "<br />" << Qt::escape( s.role() );
		st << Qt::escape( s.role() ) << "<br />";
	}
	DateTime date( s.dateTime().toLocalTime() );
	if( !date.isNull() )
	{
		sa << " " << tr("Signed on") << " "
			<< date.formatDate( "dd. MMMM yyyy" ) << " "
			<< tr("time") << " "
			<< date.toString( "hh:mm" );
		sc << "<br />" << tr("Signed on") << " "
			<< date.formatDate( "dd. MMMM yyyy" ) << " "
			<< tr("time") << " "
			<< date.toString( "hh:mm" );
		st << tr("Signed on") << " "
			<< date.formatDate( "dd. MMMM yyyy" ) << " "
			<< tr("time") << " "
			<< date.toString( "hh:mm" );
	}
	setToolTip( tooltip );

	sa << " " << tr("Signature is") << " ";
	sc << "<table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\"><tr>";
	sc << "<td>" << tr("Signature is") << " ";
	switch( s.validate() )
	{
	case DigiDocSignature::Valid:
		sa << tr("valid");
		sc << "<font color=\"green\">" << tr("valid");
		break;
	case DigiDocSignature::Warning:
		sa << tr("valid") << " (" << tr("Warnings") << ")";
		sc << "<font color=\"green\">" << tr("valid") << "</font> <font>(" << tr("Warnings") << ")";
		break;
	case DigiDocSignature::Test:
		sa << tr("valid") << " (" << tr("Test signature") << ")";
		sc << "<font color=\"green\">" << tr("valid") << "</font> <font>(" << tr("Test signature") << ")";
		break;
	case DigiDocSignature::Invalid:
		sa << tr("not valid");
		sc << "<font color=\"red\">" << tr("not valid");
		break;
	case DigiDocSignature::Unknown:
		sa << tr("unknown");
		sc << "<font color=\"red\">" << tr("unknown");
		break;
	}
	sc << "</font>";
	sc << "</td><td align=\"right\">";
	sc << "<a href=\"details\" style=\"color: #509B00\" title=\"" << tr("Show details") << "\">" << tr("Show details") << "</a>";
	sc << "</td></tr><tr><td></td>";
	sc << "<td align=\"right\">";
	if( s.parent()->isSupported() )
		sc << "<a href=\"remove\" style=\"color: #509B00\" title=\"" << tr("Remove") << "\">" << tr("Remove") << "</a>";
	sc << "</td></tr></table>";

	setText( content );
	setAccessibleName( tr("Signature") + " " + cert.toString( cert.showCN() ? "CN" : "GN SN" ) );
	setAccessibleDescription( accessibility );
}

void SignatureWidget::link( const QString &url )
{
	if( url == "details" )
		(new SignatureDialog( s, qApp->activeWindow() ))->open();
	else if( url == "remove" )
	{
		SslCertificate c = s.cert();
		QString msg = tr("Remove signature %1")
			.arg( c.toString( c.showCN() ? "CN serialNumber" : "GN SN serialNumber" ) );
		QMessageBox::StandardButton b = QMessageBox::warning( qApp->activeWindow(), msg, msg,
			QMessageBox::Ok|QMessageBox::Cancel, QMessageBox::Cancel );
		if( b == QMessageBox::Ok )
			Q_EMIT removeSignature( num );
	}
}

void SignatureWidget::mouseDoubleClickEvent( QMouseEvent *e )
{
	if( e->button() == Qt::LeftButton )
		link( "details" );
}



class SignatureDialogPrivate: public Ui::SignatureDialog {};

SignatureDialog::SignatureDialog( const DigiDocSignature &signature, QWidget *parent )
:	QDialog( parent )
,	s( signature )
,	d( new SignatureDialogPrivate )
{
	d->setupUi( this );
	d->error->hide();
	setAttribute( Qt::WA_DeleteOnClose );

	const SslCertificate c = s.cert();
#define addCertButton(cert, button) if(!cert.isNull()) \
	d->buttonBox->addButton(button, QDialogButtonBox::ActionRole)->setProperty("cert", QVariant::fromValue(cert));
	addCertButton(s.cert(), tr("Show signer's certificate"));
	addCertButton(s.ocspCert(), tr("Show OCSP certificate"));
	addCertButton(s.tsaCert(), tr("Show TSA certificate"));
	addCertButton(qApp->confValue( Application::TSLCert ).value<QSslCertificate>(), tr("Show TSL certificate"));

	QString status;
	switch( s.validate() )
	{
	case DigiDocSignature::Valid:
		status = tr("Signature is valid");
		break;
	case DigiDocSignature::Warning:
		status = QString("%1 (%2)").arg( tr("Signature is valid"), tr("Warnings") );
		if( !s.lastError().isEmpty() )
			d->error->setPlainText( s.lastError() );
		if( s.warning() & DigiDocSignature::WrongNameSpace )
		{
			d->info->setText( tr(
				"This Digidoc document has not been created according to specification, "
				"but the digital signature is legally valid. Please inform the document creator "
				"of this issue. <a href='http://www.id.ee/?id=36511'>Additional information</a>.") );
		}
		if( s.warning() & DigiDocSignature::DigestWeak )
		{
			d->info->setText( tr(
				"The current BDOC container uses weaker encryption method than officialy accepted in Estonia.") );
		}
		break;
	case DigiDocSignature::Test:
		status = QString("%1 (%2)").arg( tr("Signature is valid"), tr("Test signature") );
		if( !s.lastError().isEmpty() )
			d->error->setPlainText( s.lastError() );
		d->info->setText( tr(
			"Test signature is signed with test certificates that are similar to the "
			"certificates of real tokens, but digital signatures with legal force cannot "
			"be given with them as there is no actual owner of the card. "
			"<a href='http://www.id.ee/index.php?id=30494'>Additional information</a>.") );
		break;
	case DigiDocSignature::Invalid:
		status = tr("Signature is not valid");
		d->error->setPlainText( s.lastError().isEmpty() ? tr("Unknown error") : s.lastError() );
		d->info->setText( tr(
			"This is an invalid signature or malformed digitally signed file. The signature is not valid.") );
		break;
	case DigiDocSignature::Unknown:
		status = tr("Signature status unknown");
		d->error->setPlainText( s.lastError().isEmpty() ? tr("Unknown error") : s.lastError() );
		d->info->setText( tr(
			"Signature status is displayed unknown if you don't have all validity confirmation service "
			"certificates and/or certificate authority certificates installed into your computer. "
			"<a href='http://www.id.ee/index.php?id=35941'>Additional information</a>.") );
		break;
	}
	if( d->error->toPlainText().isEmpty() && d->info->text().isEmpty() )
		d->tabWidget->removeTab( 0 );
	else
		d->buttonBox->addButton( QDialogButtonBox::Help );
	d->title->setText( c.toString( c.showCN() ? "CN serialNumber" : "GN SN serialNumber" ) + "\n" + status );
	setWindowTitle( c.toString( c.showCN() ? "CN serialNumber" : "GN SN serialNumber" ) + " - " + status );

	const QStringList l = s.locations();
	d->signerCity->setText( l.value( 0 ) );
	d->signerState->setText( l.value( 1 ) );
	d->signerZip->setText( l.value( 2 ) );
	d->signerCountry->setText( l.value( 3 ) );

	Q_FOREACH( const QString &role, s.roles() )
	{
		QLineEdit *line = new QLineEdit( role, d->signerRoleGroup );
		line->setReadOnly( true );
		d->signerRoleGroupLayout->addRow( line );
	}

	// Certificate info
	QTreeWidget *t = d->signatureView;
	t->header()->setResizeMode( 0, QHeaderView::ResizeToContents );
	addItem( t, tr("TSL URL"), qApp->confValue( Application::TSLUrl ).toString() );
	addItem( t, tr("Signer's computer time (UTC)"), DateTime( s.signTime() ).toStringZ( "dd.MM.yyyy hh:mm:ss" ) );
	addItem( t, tr("Signature method"), s.signatureMethod() );
	addItem( t, tr("Container format"), s.parent()->mediaType() );
	if( s.type() != DigiDocSignature::DDocType )
		addItem( t, tr("Signature format"), s.profile() );
	if( !s.policy().isEmpty() )
	{
		#define toVer(X) (X)->toUInt() - 1
		QStringList ver = s.policy().split( "." );
		if( ver.size() >= 3 )
			addItem( t, tr("Signature policy"), QString("%1.%2.%3").arg( toVer(ver.end()-3) ).arg( toVer(ver.end()-2) ).arg( toVer(ver.end()-1) ) );
		else
			addItem( t, tr("Signature policy"), s.policy() );
	}
	addItem( t, tr("Signed file count"), QString::number( s.parent()->documentModel()->rowCount() ) );
	addItem( t, tr("Signer Certificate issuer"), c.issuerInfo( QSslCertificate::CommonName ) );
	if( !s.spuri().isEmpty() )
		addItem( t, "SPUri", s.spuri() );

	// OCSP info
	switch( s.type() )
	{
	case DigiDocSignature::TSType:
	{
		addItem( t, tr("Signature Timestamp"), DateTime( s.tsaTime().toLocalTime() ).toStringZ( "dd.MM.yyyy hh:mm:ss" ));
		addItem( t, tr("Signature Timestamp") + " (UTC)", DateTime( s.tsaTime() ).toStringZ( "dd.MM.yyyy hh:mm:ss" ) );
		addItem( t, tr("TSA Certificate issuer"), SslCertificate(s.tsaCert()).issuerInfo( QSslCertificate::CommonName ) );
	} //Fall through to OCSP info
	case DigiDocSignature::DDocType:
	case DigiDocSignature::TMType:
	{
		SslCertificate ocsp = s.ocspCert();
		addItem( t, tr("OCSP Certificate issuer"), ocsp.issuerInfo( QSslCertificate::CommonName ) );
		addItem( t, tr("OCSP time"), DateTime( s.ocspTime().toLocalTime() ).toStringZ( "dd.MM.yyyy hh:mm:ss" ) );
		addItem( t, tr("OCSP time") + " (UTC)", DateTime( s.ocspTime() ).toStringZ( "dd.MM.yyyy hh:mm:ss" ) );
		addItem( t, tr("Hash value of signature"), SslCertificate::toHex( s.ocspNonce() ) );
		break;
	}
	default: break;
	}
}

SignatureDialog::~SignatureDialog() { delete d; }

void SignatureDialog::addItem( QTreeWidget *view, const QString &variable, const QString &value )
{
	QTreeWidgetItem *i = new QTreeWidgetItem( view );
	i->setText( 0, variable );
	i->setText( 1, value );
	view->addTopLevelItem( i );
}

void SignatureDialog::buttonClicked( QAbstractButton *button )
{
	if( !button->property("cert").isNull() )
		CertificateDialog( button->property("cert").value<QSslCertificate>(), this ).exec();
	else if( button == d->buttonBox->button( QDialogButtonBox::Help ) )
		Common::showHelp( s.lastError(), s.lastErrorCode() );
	else if( button == d->buttonBox->button( QDialogButtonBox::Close ) )
		close();
}

void SignatureDialog::on_more_linkActivated( const QString & )
{
	d->error->setVisible( !d->error->isVisible() );
}

void SignatureDialog::on_signatureView_doubleClicked( const QModelIndex &index )
{
	if( index.row() == 8 && index.column() == 1 )
		QDesktopServices::openUrl( index.data().toUrl() );
}
