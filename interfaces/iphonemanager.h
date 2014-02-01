#ifndef IPHONEMANAGER_H
#define IPHONEMANAGER_H

#include <QMap>
#include <QSize>
#include <QUuid>
#include <QDateTime>
#include <utils/jid.h>
#include <utils/xmpperror.h>

#define PHONEMANAGER_UUID "{09A407D6-18EE-463C-B4BA-C6BEC80A137B}"

class IPhoneCall
{
public:
	enum Role {
		Caller,
		Receiver
	};
	enum State {
		Inited,
		Calling,
		Ringing,
		Connecting,
		Confirmed,
		Disconnected,
		Aborted
	};
	enum HangupCode {
		Undefined,
		Busy,
		Decline,
		Declined,
		NotAnswered,
		RemotelyAccepted,
		RemotelyDeclined,
		ErrorOccured
	};
	enum Media {
		AudioCapture,
		AudioPlayback,
		VideoCapture,
		VideoPlayback
	};
	enum MediaProperty {
		Avail,
		Enabled,
		Volume
	};
public:
	virtual QObject *instance() =0;
	// Call
	virtual bool isActive() const =0;
	virtual bool isNumberCall() const =0;
	virtual QString callId() const =0;
	virtual Jid streamJid() const =0;
	virtual Jid contactJid() const =0;
	virtual QSet<Jid> receivers() const =0;
	virtual QString number()  const =0;
	virtual Role role() const =0;
	virtual State state() const =0;
	virtual quint32 hangupCode() const =0;
	virtual QString hangupText() const =0;
	virtual QDateTime startTime() const =0;
	virtual quint32 durationTime() const =0;
	virtual QString durationString() const =0;
	virtual bool sendDtmf(const char *ADigits) =0;
	virtual bool startCall(bool AWithVideo = false) =0;
	virtual bool hangupCall(HangupCode ACode=Declined, const QString &AText=QString::null) =0;
	virtual bool destroyCall(unsigned long AWaitForDisconnected = ULONG_MAX) =0;
	// Media
	virtual bool hasActiveMedia() const =0;
	virtual bool isVideoCallRequested() const =0;
	virtual QWidget *getVideoPreviewWidget(QWidget *AParent) const =0;
	virtual QWidget *getVideoPlaybackWidget(QWidget *AParent) const =0;
	virtual QVariant mediaProperty(Media AMedia, MediaProperty AProperty) const =0;
	virtual bool setMediaProperty(Media AMedia, MediaProperty AProperty, const QVariant &AValue) =0;
protected:
	virtual void stateChanged() =0;
	virtual void mediaChanged() =0;
	virtual void callDestroyed() =0;
	virtual void dtmfSent(const char *ADigits) =0;
};

class IPhoneCallHandler
{
public:
	virtual bool phoneCallReceive(int AOrder, IPhoneCall *ACall) = 0;
};

struct IPhoneCallHistoryItem
{
	int role;
	int hangupCode;
	quint32 duration;
	bool isNumberCall;
	QString with;
	QDateTime start;
};

struct IPhoneCallHistoryRequest
{
	enum Sorting {
		NoSorting,
		SortByStart
	};
	enum Grouping {
		NoGrouping,
		GroupByWith,
		GroupByYear,
		GroupByMonth,
		GroupByDay
	};
	enum Filtering {
		NoFiltering,
		FilterNumberCalls     = 0x01,
		FilterDirectCalls     = 0x02
	};
	IPhoneCallHistoryRequest() {
		maxItems = 0;
		sorting = SortByStart;
		grouping = NoGrouping;
		filtering = NoFiltering;
		order = Qt::AscendingOrder;
	}
	QString with;
	QDateTime start;
	QDateTime end;
	int maxItems;
	Sorting sorting;
	Grouping grouping;
	Filtering filtering;
	Qt::SortOrder order;
};

class IPhoneManager
{
public:
	virtual QObject *instance() =0;
	// Calls
	virtual bool isCallsEnabled() const =0;
	virtual bool isAudioCallsAvailable() const =0;
	virtual bool isVideoCallsAvailable() const =0;
	virtual QUuid defaultSipAccountId() const =0;
	virtual QList<Jid> availCallStreams() const =0;
	virtual QList<IPhoneCall *> phoneCalls(bool AActiveOnly=false) const =0;
	virtual IPhoneCall *newCall(const Jid &AStreamJid, const QString &ANumber) =0;
	virtual IPhoneCall *newCall(const Jid &AStreamJid, const QSet<Jid> &AReceivers) =0;
	virtual bool isCallsSupported(const Jid &AStreamJid, const Jid &AContactJid) const =0;
	virtual bool showCallWindow(IPhoneCall *ACall) =0;
	virtual void showDialerDialog() =0;
	// Call History
	virtual QString saveCallHistoryItem(IPhoneCall *ACall) =0;
	virtual QString saveCallHistoryItem(const Jid &AStreamJid, const IPhoneCallHistoryItem &AItem) =0;
	virtual QString loadCallHistoryItems(const Jid &AStreamJid, const IPhoneCallHistoryRequest &ARequest) =0;
	virtual QString removeCallHistoryItems(const Jid &AStreamJid, const IPhoneCallHistoryRequest &ARequest) =0;
	virtual void showCallHistoryWindow() =0;
	// Call Contact Info
	virtual Jid callContactJid(IPhoneCall *ACall) const =0;
	virtual QString callContactName(IPhoneCall *ACall) const =0;
	virtual QString callContactName(const Jid &AStreamJid, const IPhoneCallHistoryItem &AItem) const =0;
	virtual QImage callContactAvatarImage(IPhoneCall *ACall, const QSize &AMaxSize=QSize(), bool AGray=false) const=0;
	// Numbers
	virtual QList<Jid> numbersStreams() const =0;
	virtual bool isNumbersReady(const Jid &AStreamJid) const =0;
	virtual bool isValidNumber(const QString &ANumber) const =0;
	virtual QString formattedNumber(const QString &ANumber) const =0;
	virtual QString normalizedNumber(const QString &ANumber) const =0;
	virtual QHash<Jid, QStringList> streamNumbers(const Jid &AStreamJid) const =0;
	virtual QStringList contactNumbers(const Jid &AStreamJid, const Jid &AContactJid) const =0;
	virtual bool isRosterContact(const Jid &AStreamJid, const Jid &AContactJid) const =0;
	virtual Jid findContactByNumber(const Jid &AStreamJid, const QString &ANumber) const =0;
	virtual bool appendContactNumber(const Jid &AStreamJid, const Jid &AContactJid, const QString	&ANumber) =0;
	virtual bool removeContactNumber(const Jid &AStreamJid, const Jid &AContactJid, const QString	&ANumber = QString::null) =0;
	// Call Handlers
	virtual QMultiMap<int,IPhoneCallHandler *> callHandlers() const =0;
	virtual void insertCallHandler(int AOrder, IPhoneCallHandler *AHandler) =0;
	virtual void removeCallHandler(int AOrder, IPhoneCallHandler *AHandler) =0;
protected:
	// Calls
	virtual void callsEnableChanged() =0;
	virtual void callCreated(IPhoneCall *ACall) =0;
	virtual void callDestroyed(IPhoneCall *ACall) =0;
	virtual void callStateChanged(IPhoneCall *ACall) =0;
	virtual void callMediaChanged(IPhoneCall *ACall) =0;
	// Numbers
	virtual void numbersStreamsChanged() =0;
	virtual void numbersReadyChanged(const Jid &AStreamJid, bool AReady) =0;
	virtual void numbersChanged(const Jid &AStreamJid, const Jid &AContactJid) =0;
	// Call History
	virtual void callHistoryRequestFailed(const QString &AId, const XmppError &AError) =0;
	virtual void callHistoryItemSaved(const QString &AId, const IPhoneCallHistoryItem &AItem) =0;
	virtual void callHistoryItemsLoaded(const QString &AId, const QList<IPhoneCallHistoryItem> &AItems) =0;
	virtual void callHistoryItemsRemoved(const QString &AId, const IPhoneCallHistoryRequest &ARequest) =0;
};

Q_DECLARE_INTERFACE(IPhoneCall,"QIP.Plugin.IPhoneCall/1.0")
Q_DECLARE_INTERFACE(IPhoneCallHandler,"QIP.Plugin.IPhoneCallHandler/1.0")
Q_DECLARE_INTERFACE(IPhoneManager,"QIP.Plugin.IPhoneManager/1.0")

#endif // IPHONEMANAGER_H
