#ifndef PHONEMANAGER_H
#define PHONEMANAGER_H

#include <QTimer>
#include <QPointer>
#include <interfaces/ipluginmanager.h>
#include <interfaces/iphonemanager.h>
#include <interfaces/iroster.h>
#include <interfaces/ipresence.h>
#include <interfaces/isipphone.h>
#include <interfaces/iavatars.h>
#include <interfaces/igateways.h>
#include <interfaces/iprivatestorage.h>
#include <interfaces/imessagewidgets.h>
#include <interfaces/imessageprocessor.h>
#include <interfaces/istanzaprocessor.h>
#include <interfaces/iservicediscovery.h>
#include <interfaces/imainwindow.h>
#include <interfaces/inotifications.h>
#include <interfaces/irostersview.h>
#include <interfaces/imessagestyles.h>
#include <interfaces/ioptionsmanager.h>
#include "phonecall.h"
#include "phonechathandler.h"
#include "phonedialerdialog.h"
#include "phonecallwindow.h"
#include "callhistoryworker.h"
#include "callhistorywindow.h"
#include "sipaccountoptionswidget.h"

struct CallNotifyData {
	bool isNumber;
	Jid streamJid;
	Jid contactJid;
	QString number;
};

class PhoneManager : 
	public QObject,
	public IPlugin,
	public IPhoneManager,
	public IStanzaHandler,
	public IOptionsHolder,
	public IPhoneCallHandler
{
	Q_OBJECT;
	Q_INTERFACES(IPlugin IPhoneManager IPhoneCallHandler IOptionsHolder IStanzaHandler);
public:
	PhoneManager();
	~PhoneManager();
	// IPlugin
	virtual QObject *instance() { return this; }
	virtual QUuid pluginUuid() const { return PHONEMANAGER_UUID; }
	virtual void pluginInfo(IPluginInfo *APluginInfo);
	virtual bool initConnections(IPluginManager *APluginManager, int &AInitOrder);
	virtual bool initObjects();
	virtual bool initSettings();
	virtual bool startPlugin();
	// IStanzaHandler
	virtual bool stanzaReadWrite(int AHandleId, const Jid &AStreamJid, Stanza &AStanza, bool &AAccept);
	// IOptionsHolder
	virtual QMultiMap<int, IOptionsWidget *> optionsWidgets(const QString &ANodeId, QWidget *AParent);
	// IPhoneCallHandler
	virtual bool phoneCallReceive(int AOrder, IPhoneCall *ACall);
	// Calls
	virtual bool isCallsEnabled() const;
	virtual bool isAudioCallsAvailable() const;
	virtual bool isVideoCallsAvailable() const;
	virtual QUuid defaultSipAccountId() const;
	virtual QList<Jid> availCallStreams() const;
	virtual QList<IPhoneCall *> phoneCalls(bool AActiveOnly=false) const;
	virtual IPhoneCall *newCall(const Jid &AStreamJid, const QString &ANumber);
	virtual IPhoneCall *newCall(const Jid &AStreamJid, const QSet<Jid> &AReceivers);
	virtual bool isCallsSupported(const Jid &AStreamJid, const Jid &AContactJid) const;
	virtual bool showCallWindow(IPhoneCall *ACall);
	virtual void showDialerDialog();
	// Call History
	virtual QString saveCallHistoryItem(IPhoneCall *ACall);
	virtual QString saveCallHistoryItem(const Jid &AStreamJid, const IPhoneCallHistoryItem &AItem);
	virtual QString loadCallHistoryItems(const Jid &AStreamJid, const IPhoneCallHistoryRequest &ARequest);
	virtual QString removeCallHistoryItems(const Jid &AStreamJid, const IPhoneCallHistoryRequest &ARequest);
	virtual void showCallHistoryWindow();
	// Call Contact Info
	virtual Jid callContactJid(IPhoneCall *ACall) const;
	virtual QString callContactName(IPhoneCall *ACall) const;
	virtual QString callContactName(const Jid &AStreamJid, const IPhoneCallHistoryItem &AItem) const;
	virtual QImage callContactAvatarImage(IPhoneCall *ACall, const QSize &AMaxSize=QSize(), bool AGray=false) const;
	// Numbers
	virtual QList<Jid> numbersStreams() const;
	virtual bool isNumbersReady(const Jid &AStreamJid) const;
	virtual bool isValidNumber(const QString &ANumber) const;
	virtual QString formattedNumber(const QString &ANumber) const;
	virtual QString normalizedNumber(const QString &ANumber) const;
	virtual QHash<Jid, QStringList> streamNumbers(const Jid &AStreamJid) const;
	virtual QStringList contactNumbers(const Jid &AStreamJid, const Jid &AContactJid) const;
	virtual bool isRosterContact(const Jid &AStreamJid, const Jid &AContactJid) const;
	virtual Jid findContactByNumber(const Jid &AStreamJid, const QString &ANumber) const;
	virtual bool appendContactNumber(const Jid &AStreamJid, const Jid &AContactJid, const QString	&ANumber);
	virtual bool removeContactNumber(const Jid &AStreamJid, const Jid &AContactJid, const QString	&ANumber = QString::null);
	// Call Handlers
	virtual QMultiMap<int,IPhoneCallHandler *> callHandlers() const;
	virtual void insertCallHandler(int AOrder, IPhoneCallHandler *AHandler);
	virtual void removeCallHandler(int AOrder, IPhoneCallHandler *AHandler);
signals:
	// Calls
	void callsEnableChanged();
	void callCreated(IPhoneCall *ACall);
	void callDestroyed(IPhoneCall *ACall);
	void callStateChanged(IPhoneCall *ACall);
	void callMediaChanged(IPhoneCall *ACall);
	// Numbers
	void numbersStreamsChanged();
	void numbersReadyChanged(const Jid &AStreamJid, bool AReady);
	void numbersChanged(const Jid &AStreamJid, const Jid &AContactJid);
	// Call History
	void callHistoryRequestFailed(const QString &AId, const XmppError &AError);
	void callHistoryItemSaved(const QString &AId, const IPhoneCallHistoryItem &AItem);
	void callHistoryItemsLoaded(const QString &AId, const QList<IPhoneCallHistoryItem> &AItems);
	void callHistoryItemsRemoved(const QString &AId, const IPhoneCallHistoryRequest &ARequest);
protected:
	void appendCall(IPhoneCall *ACall);
	void removeCall(IPhoneCall *ACall);
	IPhoneCall *findCallById(const QString &ACallId) const;
	bool processIncomingCall(const Jid &AStreamJid, const QString &ACallId, const Jid &AContactJid, const QString &ARemoteUri, bool AWithVideo);
protected:
	void showCallNotification(IPhoneCall *ACall);
	void showMissedCallNotification(IPhoneCall *ACall);
	void showFinishedCallNotification(IPhoneCall *ACall, const QString &AIconId, const QString &ANotify);
protected:
	void startSaveNumbersToStorage(const Jid &AStreamJid);
	bool saveNumbersToStorage(const Jid &AStreamJid);
	QString phoneStreamFilePath(const Jid &AStreamJid, const QString &AFileName) const;
	QHash<Jid, QStringList> loadNumbersFromXml(const QDomElement &AElement) const;
	void saveNumbersToXml(QDomElement &AElement, const QHash<Jid, QStringList> &ANumbers) const;
	QHash<Jid, QStringList> loadNumbersFromFile(const QString &AFileName) const;
	void saveNumbersToFile(const QString &AFileName, const QHash<Jid, QStringList> &ANumbers) const;
	void updateStreamNumbers(const Jid &AStreamJid, const QHash<Jid, QStringList> &ANumbers);
protected:
	PhoneChatHandler *findChatHandler(IPhoneCall *ACall) const;
	PhoneChatHandler *findChatHandler(const Jid &AStreamJid, const Jid &AContactJid) const;
	PhoneChatHandler *getChatHandler(const Jid &AStreamJid, const Jid &AContactJid);
protected slots:
	void onRosterAdded(IRoster *ARoster);
	void onRosterOpened(IRoster *ARoster);
	void onRosterClosed(IRoster *ARoster);
	void onRosterRemoved(IRoster *ARoster);
	void onRosterStreamJidChanged(IRoster *ARoster, const Jid &ABefore);
	void onRosterItemReceived(IRoster *ARoster, const IRosterItem &AItem, const IRosterItem &ABefore);
protected slots:
	void onPrivateStorateOpened(const Jid &AStreamJid);
	void onPrivateDataLoaded(const QString &AId, const Jid &AStreamJid, const QDomElement &AElement);
	void onPrivateDataChanged(const Jid &AStreamJid, const QString &ATagName, const QString &ANamespace);
protected slots:
	void onMessageChatWindowActivated();
	void onMessageChatWindowCreated(IMessageChatWindow *AWindow);
	void onMessageChatWindowDestroyed(IMessageChatWindow *AWindow);
protected slots:
	void onPhoneCallDestroyed();
	void onPhoneCallStateChanged();
	void onPhoneCallMediaChanged();
	void onSaveNumbersTimerTimeout();
	void onShowDialerDialogActionTriggered();
	void onShowCallNotification(IPhoneCall *ACall);
	void onCallHistoryTaskFinished(CallHistoryTask *ATask);
protected slots:
	void onNotificationActivated(int ANotifyId);
	void onNotificationRemoved(int ANotifyId);
	void onCallNotificationActionTriggered();
	void onChatNotificationActionTriggered();
protected slots:
	void onUpdateDefaultSipAccount();
	void onSipAccountInserted(const QUuid &AAccountId);
	void onSipAccountRemoved(const QUuid &AAccountId);
protected slots:
	void onOptionsOpened();
	void onOptionsClosed();
	void onOptionsChanged(const OptionsNode &ANode);
private:
	ISipPhone *FSipPhone;
	IAvatars *FAvatars;
	IGateways *FGateways;
	IRosterPlugin *FRosterPlugin;
	IPresencePlugin *FPresencePlugin;
	IPrivateStorage *FPrivateStorage;
	IPluginManager *FPluginManager;
	IMessageWidgets *FMessageWidgets;
	IMessageProcessor *FMessageProcessor;
	IStanzaProcessor *FStanzaProcessor;
	IServiceDiscovery *FDiscovery;
	IMainWindowPlugin *FMainWindowPlugin;
	INotifications *FNotifications;
	IMessageStyles *FMessageStyles;
	IOptionsManager *FOptionsManager;
private:
	int FSHICallRequest;
	QTimer FSipAccountTimer;
	QList<IPhoneCall *> FCalls;
	QList<Jid> FAvailCallStreams;
	QPointer<PhoneCallWindow> FCallWindow;
	QPointer<PhoneDialerDialog> FDialerDialog;
	QMap<int, CallNotifyData> FCallNotifications;
	QMultiMap<int, IPhoneCallHandler *> FCallHandlers;
private:
	CallHistoryWorker *FCallHistoryWorker;
	QPointer<CallHistoryWindow> FCallHistoryWindow;
	QMap<Jid, QMap<QString,QString> > FCallHistoryProperties;
private:
	QTimer FSaveNumbersTimer;
	QSet<Jid> FSaveNumbersStreams;
	QMap<Jid, QHash<Jid,QStringList> > FNumbers;
	QMap<IMessageChatWindow *, PhoneChatHandler *> FChatHandlers;
};

#endif // PHONEMANAGER_H
