#include "sipaccountoptionswidget.h"

#include <definitions/sipphone/optionvalues.h>
#include <definitions/phonemanager/optionvalues.h>
#include <utils/options.h>

SipAccountOptionsWidget::SipAccountOptionsWidget(QWidget *AParent) : QWidget(AParent)
{
	ui.setupUi(this);

	connect(ui.lneSipLogin,SIGNAL(textChanged(const QString &)),SIGNAL(modified()));
	connect(ui.lneSipPassword,SIGNAL(textChanged(const QString &)),SIGNAL(modified()));

	connect(ui.lneSipServerHost,SIGNAL(textChanged(const QString &)),SIGNAL(modified()));
	connect(ui.spbSipServerPort,SIGNAL(valueChanged(int)),SIGNAL(modified()));

	connect(ui.lneSipProxyHost,SIGNAL(textChanged(const QString &)),SIGNAL(modified()));
	connect(ui.spbSipProxyPort,SIGNAL(valueChanged(int)),SIGNAL(modified()));

	connect(ui.lneSipStun,SIGNAL(textChanged(const QString &)),SIGNAL(modified()));
	connect(ui.chbSipIce,SIGNAL(stateChanged(int)),SIGNAL(modified()));

	reset();
}

SipAccountOptionsWidget::~SipAccountOptionsWidget()
{

}

void SipAccountOptionsWidget::apply()
{
	Options::node(OPV_PHONEMANAGER_SIP_LOGIN).setValue(ui.lneSipLogin->text());
	Options::node(OPV_PHONEMANAGER_SIP_PASSWORD).setValue(Options::encrypt(ui.lneSipPassword->text()));

	Options::node(OPV_PHONEMANAGER_SIP_SERVERHOST).setValue(ui.lneSipServerHost->text());
	Options::node(OPV_PHONEMANAGER_SIP_SERVERPORT).setValue(ui.spbSipServerPort->value());

	Options::node(OPV_PHONEMANAGER_SIP_PROXYHOST).setValue(ui.lneSipProxyHost->text());
	Options::node(OPV_PHONEMANAGER_SIP_PROXYPORT).setValue(ui.spbSipProxyPort->value());

	Options::node(OPV_SIPPHONE_STUNSERVER).setValue(ui.lneSipStun->text());
	Options::node(OPV_SIPPHONE_ICEENABLED).setValue(ui.chbSipIce->isChecked());

	emit childApply();
}

void SipAccountOptionsWidget::reset()
{
	ui.lneSipLogin->setText(Options::node(OPV_PHONEMANAGER_SIP_LOGIN).value().toString());
	ui.lneSipPassword->setText(Options::decrypt(Options::node(OPV_PHONEMANAGER_SIP_PASSWORD).value().toByteArray()).toString());

	ui.lneSipServerHost->setText(Options::node(OPV_PHONEMANAGER_SIP_SERVERHOST).value().toString());
	ui.spbSipServerPort->setValue(Options::node(OPV_PHONEMANAGER_SIP_SERVERPORT).value().toInt());

	ui.lneSipProxyHost->setText(Options::node(OPV_PHONEMANAGER_SIP_PROXYHOST).value().toString());
	ui.spbSipProxyPort->setValue(Options::node(OPV_PHONEMANAGER_SIP_PROXYPORT).value().toInt());

	ui.lneSipStun->setText(Options::node(OPV_SIPPHONE_STUNSERVER).value().toString());
	ui.chbSipIce->setChecked(Options::node(OPV_SIPPHONE_ICEENABLED).value().toBool());

	emit childReset();
}
