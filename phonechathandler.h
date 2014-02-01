#ifndef PHONECHATHANDLER_H
#define PHONECHATHANDLER_H

#include <QPointer>
#include <interfaces/iphonemanager.h>
#include <interfaces/imessagewidgets.h>
#include "phonecallwidget.h"
#include "incomingcalldialog.h"
#include "editnumberswidget.h"

class PhoneChatHandler : 
	public QObject
{
	Q_OBJECT;
public:
	PhoneChatHandler(IPhoneManager *APhoneManager, IMessageChatWindow *AWindow);
	~PhoneChatHandler();
	IMessageChatWindow *messageWindow() const;
public:
	bool hasActiveCall() const;
	IPhoneCall *phoneCall() const;
	bool processPhoneCall(IPhoneCall *ACall);
signals:
	void showCallNotification(IPhoneCall *ACall);
protected:
	bool showPhoneCallWidget(IPhoneCall *ACall);
	bool showIncomingCallDialog(IPhoneCall *ACall);
	void setEditNumbersWidgetVisible(bool AVisible);
protected:
	void updateNumbersEditable(const QList<Jid> &AStreams);
	void updateAvailNumbers(const QMultiMap<Jid,Jid> &AAddresses);
protected slots:
	void onPhoneNumbersReadyChanged(const Jid &AStreamJid, bool AReady);
	void onPhoneNumbersChanged(const Jid &AStreamJid, const Jid &AContactJid);
protected slots:
	void onCallMenuAboutToShow();
	void onAudioVideoCallActionTriggered();
	void onNumberCallActionTriggered();
	void onAddNumberActionTriggered();
	void onIncomingCallDialogDestroyed();
	void onActiveCallStateChanged();
protected slots:
	void onEditNumbersWidgetCloseRequested();
	void onEditNumbersWidgetAppendRequested(const QString &ANumber);
	void onEditNumbersWidgetRemoveRequested(const QString &ANumber);
protected slots:
	void onChatWindowAvailAddressChanged();
	void onChatWindowBoxWidgetHandleMoved(int AOrderId, int ASize);
private:
	IPhoneManager *FPhoneManager;
	IMessageChatWindow *FWindow;
private:
	Menu *FCallMenu;
	bool FNumbersEditable;
	QStringList FAvailNumbers;
	QPointer<EditNumbersWidget> FEditNumbersWidget;
	QPointer<PhoneCallWidget> FPhoneCallWidget;
	QPointer<IncomingCallDialog> FIncomingCallDialog;
};

#endif // PHONECHATHANDLER_H
