#ifndef PHONECALL_H
#define PHONECALL_H

#include <QSet>
#include <QTimer>
#include <interfaces/iphonemanager.h>
#include <interfaces/isipphone.h>
#include <interfaces/istanzaprocessor.h>

class PhoneCall : 
	public QObject,
	public IPhoneCall,
	public ISipCallHandler,
	public IStanzaHandler,
	public IStanzaRequestOwner
{
	Q_OBJECT;
	Q_INTERFACES(IPhoneCall ISipCallHandler IStanzaHandler IStanzaRequestOwner);
public:
	PhoneCall(IPhoneManager *APhoneManager, ISipPhone *ASipPhone, const Jid &AStreamJid, 
		const QString &ACallId, const QString &ANumber, QObject *AParent);
	PhoneCall(IPhoneManager *APhoneManager, ISipPhone *ASipPhone, IStanzaProcessor *AStanzaProcessor, const Jid &AStreamJid, 
		const QString &ACallId, const QSet<Jid> &AReceivers, QObject *AParent);
	PhoneCall(IPhoneManager *APhoneManager, ISipPhone *ASipPhone, IStanzaProcessor *AStanzaProcessor, const Jid &AStreamJid, 
		const QString &ACallId, const Jid &ACaller, const QString &ARemoteUri, bool AWithVideo, QObject *AParent);
	~PhoneCall();
	virtual QObject *instance() { return this; }
	// IStanzaHandler
	virtual bool stanzaReadWrite(int AHandleId, const Jid &AStreamJid, Stanza &AStanza, bool &AAccept);
	// IStanzaRequestOwner
	virtual void stanzaRequestResult(const Jid &AStreamJid, const Stanza &AStanza);
	// ISipCallHandler
	virtual bool sipCallReceive(int AOrder, ISipCall *ACall);
	// IPhoneCall
	virtual bool isActive() const;
	virtual bool isNumberCall() const;
	virtual QString callId() const;
	virtual Jid streamJid() const;
	virtual Jid contactJid() const;
	virtual QSet<Jid> receivers() const;
	virtual QString number() const;
	virtual Role role() const;
	virtual State state() const;
	virtual quint32 hangupCode() const;
	virtual QString hangupText() const;
	virtual QDateTime startTime() const;
	virtual quint32 durationTime() const;
	virtual QString durationString() const;
	virtual bool sendDtmf(const char *ADigits);
	virtual bool startCall(bool AWithVideo = false);
	virtual bool hangupCall(HangupCode ACode=Decline, const QString &AText=QString::null);
	virtual bool destroyCall(unsigned long AWaitForDisconnected = UINT_MAX);
	// Media
	virtual bool hasActiveMedia() const;
	virtual bool isVideoCallRequested() const;
	virtual QWidget *getVideoPreviewWidget(QWidget *AParent) const;
	virtual QWidget *getVideoPlaybackWidget(QWidget *AParent) const;
	virtual QVariant mediaProperty(Media AMedia, MediaProperty AProperty) const;
	virtual bool setMediaProperty(Media AMedia, MediaProperty AProperty, const QVariant &AValue);
signals:
	void stateChanged();
	void mediaChanged();
	void callDestroyed();
	void dtmfSent(const char *ADigits);
protected:
	void initialize();
	void setSipCall(ISipCall *ASipCall);
	void setState(State AState);
	bool startAfterRegistration(bool ARegistered);
	void sendNotifyAction(const QSet<Jid> &ARceivers, const QString &AType, const QString &ADescription=QString::null);
protected:
	QVariant getSipStreamProperty(Media AMedia, MediaProperty AProperty) const;
	bool getSipStreamParams(Media AMedia, MediaProperty AProperty, ISipMedia::Type &ASipMedia, ISipMedia::Direction &ASipDir, ISipMediaStream::Property &ASipProp) const;
protected:
	void insertCallHandler();
	void removeCallHandler();
	void insertStanzaHandler();
	void removeStanzaHandler();
protected slots:
	void onRingTimerTimeout();
	void onSipCallStateChanged();
	void onSipCallMediaChanged();
	void onSipCallDestroyed();
	void onSipAccountRemoved(const QUuid &ASipId);
	void onSipAccountRegistationChanged(const QUuid &ASipId, bool ARegistered);
private:
	ISipPhone *FSipPhone;
	IPhoneManager *FPhoneManager;
	IStanzaProcessor *FStanzaProcessor;
private:
	Role FRole;
	State FState;
	Jid FStreamJid;
	Jid FContactJid;
	QString FNumber;
	QString FCallId;
	QSet<Jid> FReceivers;
	quint32 FHangupCode;
	QString FHangupText;
	QDateTime FStartTime;
private:
	ISipCall *FSipCall;
	QString FSipAccountId;
	QString FRemoteUri;
	bool FWithVideo;
	bool FDelayedDestroy;
	bool FWaitRegistration;
private:
	QTimer FRingTimer;
	int FSHICallAction;
	QSet<Jid> FActiveReceivers;
	QMap<QString,Jid> FCallRequests;
private:
	bool FSipMediaInited;
	QMap<Media, QMap<MediaProperty,QVariant> > FMediaProperties;
};

#endif // PHONECALL_H
