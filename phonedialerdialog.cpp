#include "phonedialerdialog.h"

#include <definitions/resources.h>
#include <definitions/phonemanager/menuicons.h>
#include <utils/widgetmanager.h>
#include <utils/iconstorage.h>
#include <utils/logger.h>

static const QString PhoneDialerDialogStyleSheet =
"QPushButton#pbtCall:enabled                "
"{                                          "
"  color: white;                            "
"  min-height: 26px;                        "
"  min-width: 125px;                        "
"  border-width: 4px;                       "
"  border-image: url(%1);                   "
"}                                          "
"QPushButton#pbtCall:hover:enabled          "
"{                                          "
"  border-image: url(%2);                   "
"}                                          "
"QPushButton#pbtCall:pressed:enabled        "
"{                                          "
"  border-image: url(%3);                   "
"}                                          "
;

PhoneDialerDialog::PhoneDialerDialog(IPhoneManager *APhoneManager, QWidget *AParent) : QDialog(AParent)
{
	REPORT_VIEW;
	ui.setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose,true);

	setStyleSheet(PhoneDialerDialogStyleSheet
		.arg(IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->fileFullName(MNI_PHONEMANAGER_BUTTON_ACCEPT))
		.arg(IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->fileFullName(MNI_PHONEMANAGER_BUTTON_ACCEPT_HOVER))
		.arg(IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->fileFullName(MNI_PHONEMANAGER_BUTTON_ACCEPT_PRESSED)));

	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(this,MNI_PHONEMANAGER_DIALER_DIALOG,0,0,"windowIcon");
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.pbtCall,MNI_PHONEMANAGER_CALL_ANSWER);

	FPhoneManager = APhoneManager;
	connect(FPhoneManager->instance(),SIGNAL(callsEnableChanged()),SLOT(updateCallButtonState()));
	connect(FPhoneManager->instance(),SIGNAL(numbersStreamsChanged()),SLOT(updateCallHistory()));
	connect(FPhoneManager->instance(),SIGNAL(callDestroyed(IPhoneCall *)),SLOT(updateCallHistory()));

	ui.wdtHistory->setLayout(new QVBoxLayout);
	ui.wdtHistory->layout()->setMargin(0);
	FHistoryWidget = new CallHistoryViewWidget(FPhoneManager,ui.wdtHistory);
	connect(FHistoryWidget,SIGNAL(clicked(const QModelIndex &)),SLOT(onHistoryViewItemClicked(const QModelIndex &)));
	connect(FHistoryWidget,SIGNAL(doubleClicked(const QModelIndex &)),SLOT(onHistoryViewItemDoubleClicked(const QModelIndex &)));
	ui.wdtHistory->layout()->addWidget(FHistoryWidget);

	ui.lblFullHistory->setText(QString("<a href='show-full-history'>%1</a>").arg(tr("Full history of calls")));
	connect(ui.lblFullHistory,SIGNAL(linkActivated(const QString &)),SLOT(onLabelLinkActivated(const QString &)));

	ui.pbtCall->setFocus();
	connect(ui.pbtCall,SIGNAL(clicked()),SLOT(onCallButtonClicked()));

	onDialerNumberChanged();
	connect(ui.dwtDialer,SIGNAL(numberChanged()),SLOT(onDialerNumberChanged()));

	updateCallHistory();
}

PhoneDialerDialog::~PhoneDialerDialog()
{

}

bool PhoneDialerDialog::isCallEnabled() const
{
	QString number = FPhoneManager->normalizedNumber(ui.dwtDialer->number());
	return FPhoneManager->isCallsEnabled() && FPhoneManager->isAudioCallsAvailable() && FPhoneManager->isValidNumber(number);
}

void PhoneDialerDialog::updateCallHistory()
{
	IPhoneCallHistoryRequest request;
	request.maxItems = 6;
	request.order = Qt::DescendingOrder;
	request.grouping = IPhoneCallHistoryRequest::GroupByWith;
	request.filtering = IPhoneCallHistoryRequest::FilterNumberCalls;
	FHistoryWidget->showCallHistoryRequest(FPhoneManager->numbersStreams(),request);
}

void PhoneDialerDialog::updateCallButtonState()
{
	ui.pbtCall->setEnabled(isCallEnabled());
}

void PhoneDialerDialog::onDialerNumberChanged()
{
	updateCallButtonState();
}

void PhoneDialerDialog::onCallButtonClicked()
{
	if (isCallEnabled())
	{
		IPhoneCall *call = FPhoneManager->newCall(FPhoneManager->availCallStreams().value(0),ui.dwtDialer->number());
		if (call)
		{
			if (FPhoneManager->showCallWindow(call))
			{
				hide();
				connect(call->instance(),SIGNAL(callDestroyed()),SLOT(onPhoneCallDestroyed()));
				call->startCall();
			}
			else
			{
				call->destroyCall(0);
			}
		}
	}
}

void PhoneDialerDialog::onPhoneCallDestroyed()
{
	WidgetManager::showActivateRaiseWindow(this);
}

void PhoneDialerDialog::onLabelLinkActivated(const QString &ALink)
{
	if (ALink == "show-full-history")
		FPhoneManager->showCallHistoryWindow();
}

void PhoneDialerDialog::onHistoryViewItemClicked(const QModelIndex &AIndex)
{
	IPhoneCallHistoryItem item = FHistoryWidget->modelIndexToHistoryItem(AIndex);
	if (item.isNumberCall)
		ui.dwtDialer->setNumber(FPhoneManager->formattedNumber(item.with));
}

void PhoneDialerDialog::onHistoryViewItemDoubleClicked(const QModelIndex &AIndex)
{
	IPhoneCallHistoryItem item = FHistoryWidget->modelIndexToHistoryItem(AIndex);
	if (item.isNumberCall)
	{
		ui.dwtDialer->setNumber(FPhoneManager->formattedNumber(item.with));
		onCallButtonClicked();
	}
}
