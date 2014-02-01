#include "phonechathandler.h"

#include <QApplication>
#include <definitions/resources.h>
#include <definitions/phonemanager/menuicons.h>
#include <definitions/phonemanager/toolbargroups.h>
#include <definitions/phonemanager/messagechatwindowwidgets.h>
#include <utils/options.h>

#define ADR_STREAM_JID             Action::DR_StreamJid
#define ADR_RECEIVERS              Action::DR_Parametr1
#define ADR_NUMBER                 Action::DR_Parametr1
#define ADR_VIDEO                  Action::DR_Parametr2

PhoneChatHandler::PhoneChatHandler(IPhoneManager *APhoneManager, IMessageChatWindow *AWindow) : QObject(AWindow->instance())
{
	FNumbersEditable = false;

	FWindow = AWindow;
	connect(FWindow->address()->instance(),SIGNAL(availAddressesChanged()),SLOT(onChatWindowAvailAddressChanged()));
	connect(FWindow->messageWidgetsBox(),SIGNAL(handleMoved(int,int)),SLOT(onChatWindowBoxWidgetHandleMoved(int,int)));

	FPhoneManager = APhoneManager;
	connect(FPhoneManager->instance(),SIGNAL(numbersReadyChanged(const Jid &, bool)),SLOT(onPhoneNumbersReadyChanged(const Jid &, bool)));
	connect(FPhoneManager->instance(),SIGNAL(numbersChanged(const Jid &, const Jid &)),SLOT(onPhoneNumbersChanged(const Jid &, const Jid &)));

	if (FPhoneManager->isAudioCallsAvailable())
	{
		FCallMenu = new Menu(AWindow->toolBarWidget()->toolBarChanger()->toolBar());
		FCallMenu->menuAction()->setIcon(RSR_STORAGE_MENUICONS,MNI_PHONEMANAGER_CALL);
		connect(FCallMenu,SIGNAL(aboutToShow()),SLOT(onCallMenuAboutToShow()));
		QToolButton *callButton = AWindow->toolBarWidget()->toolBarChanger()->insertAction(FCallMenu->menuAction(),TBG_MWTBW_PHONECALL);
		callButton->setPopupMode(QToolButton::InstantPopup);
	}

	updateAvailNumbers(FWindow->address()->availAddresses(true));
	updateNumbersEditable(FWindow->address()->availAddresses(true).keys());
}

PhoneChatHandler::~PhoneChatHandler()
{

}

IMessageChatWindow *PhoneChatHandler::messageWindow() const
{
	return FWindow;
}

bool PhoneChatHandler::hasActiveCall() const
{
	IPhoneCall *call = phoneCall();
	return call!=NULL && call->isActive();
}

IPhoneCall *PhoneChatHandler::phoneCall() const
{
	if (!FPhoneCallWidget.isNull())
		return FPhoneCallWidget->call();
	if (!FIncomingCallDialog.isNull())
		return FIncomingCallDialog->call();
	return NULL;
}

bool PhoneChatHandler::processPhoneCall(IPhoneCall *ACall)
{
	if (!hasActiveCall())
	{
		if (ACall->role()==IPhoneCall::Receiver && ACall->state()==IPhoneCall::Ringing)
			return showIncomingCallDialog(ACall);
		else
			return showPhoneCallWidget(ACall);
	}
	return false;
}

void PhoneChatHandler::setEditNumbersWidgetVisible(bool AVisible)
{
	if (AVisible && FEditNumbersWidget.isNull())
	{
		FEditNumbersWidget = new EditNumbersWidget(FPhoneManager,NULL);
		FEditNumbersWidget->setAvailNumbers(FAvailNumbers);
		FEditNumbersWidget->setNumbersEditable(FNumbersEditable);
		FWindow->messageWidgetsBox()->insertWidget(MCWW_PHONEMANAGER_EDITNUMBERS,FEditNumbersWidget);
		connect(FEditNumbersWidget,SIGNAL(closeRequested()),SLOT(onEditNumbersWidgetCloseRequested()));
		connect(FEditNumbersWidget,SIGNAL(appendRequested(const QString &)),SLOT(onEditNumbersWidgetAppendRequested(const QString &)));
		connect(FEditNumbersWidget,SIGNAL(removeRequested(const QString &)),SLOT(onEditNumbersWidgetRemoveRequested(const QString &)));
	}
	else if (!AVisible && !FEditNumbersWidget.isNull())
	{
		FWindow->messageWidgetsBox()->removeWidget(FEditNumbersWidget);
		FEditNumbersWidget->deleteLater();
	}
}

bool PhoneChatHandler::showPhoneCallWidget(IPhoneCall *ACall)
{
	if (FPhoneCallWidget.isNull() || FPhoneCallWidget->call()==NULL || !FPhoneCallWidget->call()->isActive())
	{
		SplitterWidget *splitter = FWindow->messageWidgetsBox();
		
		if (!FPhoneCallWidget.isNull())
		{
			splitter->removeWidget(FPhoneCallWidget);
			FPhoneCallWidget->deleteLater();
		}

		FPhoneCallWidget = new PhoneCallWidget(FPhoneManager,ACall,FWindow->messageWidgetsBox());
		splitter->insertWidget(MCWW_PHONEMANAGER_CALL,FPhoneCallWidget,0,MCWW_PHONEMANAGER_CALL_HANDLE);
		
		int widgetHeight = Options::fileValue("phonemanager.chathandler.callwidget.height").toInt();
		if (widgetHeight > 0)
		{
			int handle = splitter->widgetHandle(FPhoneCallWidget);
			splitter->setHandleSize(handle,splitter->handleSize(handle)+widgetHeight);
		}

		connect(ACall->instance(),SIGNAL(stateChanged()),SLOT(onActiveCallStateChanged()),Qt::UniqueConnection);

		FWindow->showTabPage();
		return true;
	}
	return false;
}

bool PhoneChatHandler::showIncomingCallDialog(IPhoneCall *ACall)
{
	if (FIncomingCallDialog.isNull())
	{
		FIncomingCallDialog = new IncomingCallDialog(FPhoneManager,ACall);
		connect(FIncomingCallDialog,SIGNAL(dialogDestroyed()),SLOT(onIncomingCallDialogDestroyed()));
		connect(ACall->instance(),SIGNAL(stateChanged()),SLOT(onActiveCallStateChanged()),Qt::UniqueConnection);
		FIncomingCallDialog->show();
		return true;
	}
	return false;
}

void PhoneChatHandler::updateNumbersEditable(const QList<Jid> &AStreams)
{
	bool editable = !AStreams.isEmpty();
	foreach(const Jid &streamJid, AStreams)
	{
		if (!FPhoneManager->isNumbersReady(streamJid))
		{
			editable = false;
			break;
		}
	}

	if (FNumbersEditable != editable)
	{
		if (!FEditNumbersWidget.isNull())
			FEditNumbersWidget->setNumbersEditable(editable);
		FNumbersEditable = editable;
	}
}

void PhoneChatHandler::updateAvailNumbers(const QMultiMap<Jid,Jid> &AAddresses)
{
	QStringList numbers;
	for (QMultiMap<Jid,Jid>::const_iterator it=AAddresses.constBegin(); it!=AAddresses.constEnd(); ++it)
	{
		foreach(QString number, FPhoneManager->contactNumbers(it.key(),it.value()))
			if (!numbers.contains(number))
				numbers.append(number);
	}
	qSort(numbers.begin(),numbers.end());

	if (FAvailNumbers != numbers)
	{
		if (!FEditNumbersWidget.isNull())
			FEditNumbersWidget->setAvailNumbers(numbers);
		FAvailNumbers = numbers;
	}
}

void PhoneChatHandler::onPhoneNumbersReadyChanged(const Jid &AStreamJid, bool AReady)
{
	Q_UNUSED(AReady);
	QList<Jid> streams = FWindow->address()->availAddresses(true).keys();
	if (streams.contains(AStreamJid))
		updateNumbersEditable(streams);
}

void PhoneChatHandler::onPhoneNumbersChanged(const Jid &AStreamJid, const Jid &AContactJid)
{
	QMultiMap<Jid,Jid> addresses = FWindow->address()->availAddresses(true);
	if (addresses.contains(AStreamJid,AContactJid))
		updateAvailNumbers(addresses);
}

void PhoneChatHandler::onCallMenuAboutToShow()
{
	FCallMenu->clear();

	Jid streamJid = FWindow->streamJid();
	bool callsBusy = hasActiveCall();
	bool callsEnabled = FPhoneManager->isCallsEnabled();

	QSet<QString> receivers;
	QMultiMap<Jid,Jid> addresses = FWindow->address()->availAddresses();
	for (QMultiMap<Jid,Jid>::const_iterator it=addresses.constBegin(); it!=addresses.constEnd(); ++it)
		if (FPhoneManager->isCallsSupported(it.key(),it.value()))
			receivers += it->pFull();

	Action *audioCallAction = new Action(FCallMenu);
	audioCallAction->setText(tr("Call"));
	audioCallAction->setIcon(RSR_STORAGE_MENUICONS,MNI_PHONEMANAGER_CALL);
	audioCallAction->setData(ADR_STREAM_JID,streamJid.full());
	audioCallAction->setData(ADR_RECEIVERS,QStringList(receivers.toList()));
	audioCallAction->setEnabled(!callsBusy && callsEnabled && FPhoneManager->isAudioCallsAvailable() && !receivers.isEmpty());
	connect(audioCallAction,SIGNAL(triggered()),SLOT(onAudioVideoCallActionTriggered()));
	FCallMenu->addAction(audioCallAction,AG_DEFAULT);

	Action *videoCallAction = new Action(FCallMenu);
	videoCallAction->setText(tr("Video Call"));
	videoCallAction->setIcon(RSR_STORAGE_MENUICONS,MNI_PHONEMANAGER_VIDEO_CALL);
	videoCallAction->setData(ADR_STREAM_JID,streamJid.full());
	videoCallAction->setData(ADR_RECEIVERS,QStringList(receivers.toList()));
	videoCallAction->setData(ADR_VIDEO,true);
	videoCallAction->setEnabled(!callsBusy && callsEnabled && FPhoneManager->isVideoCallsAvailable() && !receivers.isEmpty());
	connect(videoCallAction,SIGNAL(triggered()),SLOT(onAudioVideoCallActionTriggered()));
	FCallMenu->addAction(videoCallAction,AG_DEFAULT);

	foreach(const QString &number, FAvailNumbers)
	{
		Action *numberCallAction = new Action(FCallMenu);
		numberCallAction->setText(FPhoneManager->formattedNumber(number));
		numberCallAction->setData(ADR_STREAM_JID,streamJid.full());
		numberCallAction->setData(ADR_NUMBER,number);
		numberCallAction->setEnabled(!callsBusy && callsEnabled);
		connect(numberCallAction,SIGNAL(triggered()),SLOT(onNumberCallActionTriggered()));
		FCallMenu->addAction(numberCallAction,AG_DEFAULT+100);
	}

	Action *editNumbersAction = new Action(FCallMenu);
	editNumbersAction->setText(tr("Edit Numbers..."));
	editNumbersAction->setIcon(RSR_STORAGE_MENUICONS,MNI_PHONEMANAGER_NUMBER_APPEND);
	editNumbersAction->setEnabled(FPhoneManager->isNumbersReady(streamJid));
	connect(editNumbersAction,SIGNAL(triggered()),SLOT(onAddNumberActionTriggered()));
	FCallMenu->addAction(editNumbersAction,AG_DEFAULT+200);
}

void PhoneChatHandler::onAudioVideoCallActionTriggered()
{
	Action *action = qobject_cast<Action *>(sender());
	if (action)
	{
		QSet<Jid> receivers;
		foreach(const QString &receiver, action->data(ADR_RECEIVERS).toStringList())
			receivers += receiver;

		IPhoneCall *call = FPhoneManager->newCall(action->data(ADR_STREAM_JID).toString(),receivers);
		if (call)
		{
			if(processPhoneCall(call))
				call->startCall(action->data(ADR_VIDEO).toBool());
			else
				call->destroyCall(0);
		}
	}
}

void PhoneChatHandler::onNumberCallActionTriggered()
{
	Action *action = qobject_cast<Action *>(sender());
	if (action)
	{
		IPhoneCall *call = FPhoneManager->newCall(action->data(ADR_STREAM_JID).toString(),action->data(ADR_NUMBER).toString());
		if (call)
		{
			if(processPhoneCall(call))
				call->startCall();
			else
				call->destroyCall(0);
		}
	}
}

void PhoneChatHandler::onAddNumberActionTriggered()
{
	setEditNumbersWidgetVisible(true);
}

void PhoneChatHandler::onIncomingCallDialogDestroyed()
{
	IPhoneCall *call = FIncomingCallDialog->call();
	switch (FIncomingCallDialog->result())
	{
	case IncomingCallDialog::Rejected:
		call->destroyCall(0);
		break;
	case IncomingCallDialog::AcceptedNoVideo:
		showPhoneCallWidget(call);
		call->startCall(false);
		break;
	case IncomingCallDialog::AcceptedWithVideo:
		showPhoneCallWidget(call);
		call->startCall(true);
		break;
	case IncomingCallDialog::CallStateChanged:
		if (call->state()==IPhoneCall::Disconnected || call->state()==IPhoneCall::Aborted)
			call->destroyCall(0);
		break;
	}
}

void PhoneChatHandler::onActiveCallStateChanged()
{
	IPhoneCall *call = qobject_cast<IPhoneCall *>(sender());
	if (call && !call->isActive())
		emit showCallNotification(call);
}

void PhoneChatHandler::onEditNumbersWidgetCloseRequested()
{
	setEditNumbersWidgetVisible(false);
}

void PhoneChatHandler::onEditNumbersWidgetAppendRequested(const QString &ANumber)
{
	QString number = FPhoneManager->normalizedNumber(ANumber);
	if (!FAvailNumbers.contains(number))
	{
		QMultiMap<Jid,Jid> addresses = FWindow->address()->availAddresses(true);
		for (QMultiMap<Jid,Jid>::const_iterator it=addresses.constBegin(); it!=addresses.constEnd(); ++it)
			if (FPhoneManager->appendContactNumber(it.key(),it.value(),ANumber))
				break;
	}
}

void PhoneChatHandler::onEditNumbersWidgetRemoveRequested(const QString &ANumber)
{
	QString number = FPhoneManager->normalizedNumber(ANumber);
	if (FAvailNumbers.contains(number))
	{
		QMultiMap<Jid,Jid> addresses = FWindow->address()->availAddresses(true);
		for (QMultiMap<Jid,Jid>::const_iterator it=addresses.constBegin(); it!=addresses.constEnd(); ++it)
			if (FPhoneManager->removeContactNumber(it.key(),it.value(),number))
				break;
	}
}

void PhoneChatHandler::onChatWindowAvailAddressChanged()
{
	updateAvailNumbers(FWindow->address()->availAddresses(true));
}

void PhoneChatHandler::onChatWindowBoxWidgetHandleMoved(int AOrderId, int ASize)
{
	Q_UNUSED(ASize);
	if (AOrderId==MCWW_PHONEMANAGER_CALL_HANDLE && !FPhoneCallWidget.isNull() && FPhoneCallWidget->isVisible())
		Options::setFileValue(FPhoneCallWidget->height(),"phonemanager.chathandler.callwidget.height");
}
