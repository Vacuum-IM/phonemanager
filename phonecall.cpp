#include "phonecall.h"

#include <definitions/phonemanager/namespaces.h>
#include <definitions/phonemanager/optionvalues.h>
#include <definitions/phonemanager/internalerrors.h>
#include <definitions/phonemanager/sipcallhandlerorders.h>
#include <utils/options.h>
#include <utils/logger.h>

#define DIRECT_CALL_DOMAIN        "sip.qip.ru"
#define CALL_REQUEST_TIMEOUT      15000

#define SHC_PHONE_CALL_ACTION     "/iq[@type='set']/query[@cid='%1'][@xmlns='" NS_VACUUM_SIP_PHONE "']"

PhoneCall::PhoneCall(IPhoneManager *APhoneManager, ISipPhone *ASipPhone, const Jid &AStreamJid,
	const QString &ACallId, const QString &ANumber, QObject *AParent) : QObject(AParent)
{
	FSipPhone = ASipPhone;
	FPhoneManager = APhoneManager;
	FStanzaProcessor = NULL;

	FRole = IPhoneCall::Caller;
	FStreamJid = AStreamJid;
	FNumber = FPhoneManager->normalizedNumber(ANumber);
	FCallId = ACallId;
	FWithVideo = false;

	initialize();

	QString host = FSipPhone->accountConfig(FSipAccountId).serverHost;
	FRemoteUri = QString("<sip:%1>").arg(FNumber+"@"+host);
}

PhoneCall::PhoneCall(IPhoneManager *APhoneManager, ISipPhone *ASipPhone, IStanzaProcessor *AStanzaProcessor, const Jid &AStreamJid, 
	const QString &ACallId, const QSet<Jid> &AReceivers, QObject *AParent) : QObject(AParent)
{
	FSipPhone = ASipPhone;
	FPhoneManager = APhoneManager;
	FStanzaProcessor = AStanzaProcessor;

	FRole = IPhoneCall::Caller;
	FStreamJid = AStreamJid;
	FReceivers = AReceivers;
	FCallId = ACallId;
	FWithVideo = false;

	initialize();
}

PhoneCall::PhoneCall(IPhoneManager *APhoneManager, ISipPhone *ASipPhone, IStanzaProcessor *AStanzaProcessor, const Jid &AStreamJid, 
	const QString &ACallId, const Jid &ACaller, const QString &ARemoteUri, bool AWithVideo, QObject *AParent) : QObject(AParent)
{
	FSipPhone = ASipPhone;
	FPhoneManager = APhoneManager;
	FStanzaProcessor = AStanzaProcessor;

	FRole = IPhoneCall::Receiver;
	FStreamJid = AStreamJid;
	FContactJid = ACaller;
	FCallId = ACallId;
	FRemoteUri = ARemoteUri;
	FWithVideo = AWithVideo;

	initialize();

	FActiveReceivers += FContactJid;
	setState(IPhoneCall::Ringing);
}

PhoneCall::~PhoneCall()
{
	destroyCall();
	if (FSipCall)
		delete FSipCall->instance();
	emit callDestroyed();
}

bool PhoneCall::stanzaReadWrite(int AHandleId, const Jid &AStreamJid, Stanza &AStanza, bool &AAccept)
{
	if (AStreamJid==FStreamJid && AHandleId==FSHICallAction)
	{
		QDomElement actionElem = AStanza.firstElement("query",NS_VACUUM_SIP_PHONE).firstChildElement("action");
		QString actionType =  actionElem.attribute("type");

		LOG_STRM_INFO(AStreamJid,QString("Phone call action request received, from=%1, action=%2, call=%3").arg(AStanza.from(),actionType,FCallId));
		if (role()==IPhoneCall::Caller && FActiveReceivers.contains(AStanza.from()))
		{
			AAccept = true;
			Stanza reply = FStanzaProcessor->makeReplyResult(AStanza);

			if (actionType == "accept")
			{
				FContactJid = AStanza.from();
				FRemoteUri = actionElem.text();
				FWithVideo = QVariant(actionElem.attribute("video")).toBool();

				sendNotifyAction(FActiveReceivers-=FContactJid,"accepted");
				FActiveReceivers = QSet<Jid>() << FContactJid;

				setState(IPhoneCall::Connecting);
				if (FSipPhone->isAccountRegistered(FSipAccountId))
					startAfterRegistration(true);
				else if (!FSipPhone->setAccountRegistered(FSipAccountId,true))
					startAfterRegistration(false);
				else
					FWaitRegistration = true;
			}
			else if (actionType == "decline")
			{
				FContactJid = AStanza.from();
				FActiveReceivers-=FContactJid;
				hangupCall(IPhoneCall::Declined);
			}
			else if (actionType == "busy")
			{
				FActiveReceivers -= AStanza.from();
				if (FActiveReceivers.isEmpty())
				{
					FContactJid = AStanza.from();
					hangupCall(IPhoneCall::Busy);
				}
			}
			else if (actionType == "timeout")
			{
				FActiveReceivers -= AStanza.from();
				if (FActiveReceivers.isEmpty())
				{
					FContactJid = AStanza.from();
					hangupCall(IPhoneCall::NotAnswered);
				}
			}
			else if (actionType == "error")
			{
				FContactJid = AStanza.from();
				FActiveReceivers -= FContactJid;
				hangupCall(IPhoneCall::ErrorOccured,actionElem.text());
			}
			else
			{
				reply = FStanzaProcessor->makeReplyError(AStanza,XmppStanzaError::EC_BAD_REQUEST);
			}

			FStanzaProcessor->sendStanzaOut(AStreamJid,reply);
		}
		else if (role()==IPhoneCall::Receiver && FActiveReceivers.contains(AStanza.from()))
		{
			AAccept = true;
			Stanza reply = FStanzaProcessor->makeReplyResult(AStanza);

			if (actionType == "decline")
			{
				FActiveReceivers -= AStanza.from();
				hangupCall(IPhoneCall::Declined);
			}
			else if (actionType == "accepted")
			{
				FActiveReceivers -= AStanza.from();
				hangupCall(IPhoneCall::RemotelyAccepted);
			}
			else if (actionType == "declined")
			{
				FActiveReceivers -= AStanza.from();
				hangupCall(IPhoneCall::RemotelyDeclined);
			}
			else if (actionType == "timeout")
			{
				FActiveReceivers -= AStanza.from();
				hangupCall(IPhoneCall::NotAnswered);
			}
			else if (actionType == "error")
			{
				FActiveReceivers -= AStanza.from();
				hangupCall(IPhoneCall::ErrorOccured,actionElem.text());
			}
			else
			{
				reply = FStanzaProcessor->makeReplyError(AStanza,XmppStanzaError::EC_BAD_REQUEST);
			}

			FStanzaProcessor->sendStanzaOut(AStreamJid,reply);
		}
		else
		{
			LOG_STRM_WARNING(AStreamJid,QString("Failed to process received phone call action request from=%1, action=%2, call=%3: Unexpected request").arg(AStanza.from(),actionType,FCallId));
		}
	}
	return false;
}

void PhoneCall::stanzaRequestResult(const Jid &AStreamJid, const Stanza &AStanza)
{
	Q_UNUSED(AStreamJid);
	if (FCallRequests.contains(AStanza.id()))
	{
		Jid receiver = FCallRequests.take(AStanza.id());
		LOG_STRM_INFO(AStreamJid,QString("Phone call request result received, from=%1, receiver=%2, type=%3, call=%4").arg(AStanza.from(),receiver.full(),AStanza.type(),FCallId));

		if (role() == IPhoneCall::Caller)
		{
			if (AStanza.type() == "error")
				FActiveReceivers -= receiver;
			else if (state() == IPhoneCall::Calling)
				setState(IPhoneCall::Ringing);
			if (FActiveReceivers.isEmpty())
				hangupCall(IPhoneCall::ErrorOccured,tr("Callee is unavailable"));
		}
		else if (role() == IPhoneCall::Receiver)
		{
			if (AStanza.type() == "error")
			{
				FActiveReceivers -= receiver;
				hangupCall(IPhoneCall::ErrorOccured,tr("Callee is unavailable"));
			}
		}
	}
}

bool PhoneCall::sipCallReceive(int AOrder, ISipCall *ACall)
{
	if (AOrder == SCHO_PHONECALL)
	{
		if (FSipCall==NULL && ACall->accountId()==FSipAccountId && ACall->remoteUri()==FRemoteUri)
		{
			setSipCall(ACall);
			return ACall->startCall(FWithVideo);
		}
	}
	return false;
}

bool PhoneCall::isActive() const
{
	return IPhoneCall::Inited<FState &&  FState<IPhoneCall::Disconnected;
}

bool PhoneCall::isNumberCall() const
{
	return !FNumber.isNull();
}

QString PhoneCall::callId() const
{
	return FCallId;
}

Jid PhoneCall::streamJid() const
{
	return FStreamJid;
}

Jid PhoneCall::contactJid() const
{
	return FContactJid;
}

QSet<Jid> PhoneCall::receivers() const
{
	return FReceivers;
}

QString PhoneCall::number() const
{
	return FNumber;
}

IPhoneCall::Role PhoneCall::role() const
{
	return FRole;
}

IPhoneCall::State PhoneCall::state() const
{
	return FState;
}

quint32 PhoneCall::hangupCode() const
{
	return FHangupCode;
}

QString PhoneCall::hangupText() const
{
	return FHangupText;
}

QDateTime PhoneCall::startTime() const
{
	return FStartTime;
}

quint32 PhoneCall::durationTime() const
{
	return FSipCall!=NULL ? FSipCall->durationTime() : 0;
}

QString PhoneCall::durationString() const
{
	QTime time = QTime(0, 0, 0, 0).addMSecs(durationTime());
	return time.toString(time.hour()>0 ? "h:mm:ss" : "m:ss");
}

bool PhoneCall::startCall(bool AWithVideo)
{
	if (role()==IPhoneCall::Caller && state()==IPhoneCall::Inited)
	{
		LOG_STRM_INFO(streamJid(),QString("Starting phone call, call=%1, video=%2").arg(FCallId).arg(AWithVideo));

		FWithVideo = AWithVideo;
		setMediaProperty(VideoCapture,Enabled,AWithVideo);
		FStartTime = QDateTime::currentDateTime();

		if (isNumberCall())
		{
			setState(IPhoneCall::Calling);

			if (FSipPhone->isAccountRegistered(FSipAccountId))
				return startAfterRegistration(true);
			else if (!FSipPhone->setAccountRegistered(FSipAccountId,true))
				return startAfterRegistration(false);

			FWaitRegistration = true;
			return true;
		}
		else
		{
			foreach(const Jid &receiver, FReceivers)
			{
				Stanza request("iq");
				request.setTo(receiver.full()).setType("set").setId(FStanzaProcessor->newId());

				QDomElement queryElem = request.addElement("query", NS_VACUUM_SIP_PHONE);
				queryElem.setAttribute("cid",FCallId);
				QDomElement actionElem = queryElem.appendChild(request.createElement("action")).toElement();
				actionElem.setAttribute("type","request");
				actionElem.setAttribute("video",QVariant(FWithVideo).toString());
				actionElem.appendChild(request.createTextNode(FSipPhone->accountUri(FSipAccountId)));

				if (FStanzaProcessor->sendStanzaRequest(this,FStreamJid,request,CALL_REQUEST_TIMEOUT))
				{
					FActiveReceivers += receiver;
					FCallRequests.insert(request.id(), receiver);
					LOG_STRM_INFO(streamJid(),QString("Phone call start request sent to=%1, id=%2, call=%3").arg(request.to(),request.id(),FCallId));
				}
				else
				{
					LOG_STRM_WARNING(streamJid(),QString("Failed to send phone call start request to=%1, call=%2").arg(request.to(),FCallId));
				}
			}

			if (!FActiveReceivers.isEmpty())
			{
				setState(IPhoneCall::Calling);
				return true;
			}
			else
			{
				hangupCall(IPhoneCall::ErrorOccured,tr("Callee is unavailable"));
			}
		}
	}
	else if (role()==IPhoneCall::Receiver && state()==IPhoneCall::Ringing)
	{
		LOG_STRM_INFO(streamJid(),QString("Starting phone call, call=%1, video=%2").arg(FCallId).arg(AWithVideo));

		FWithVideo = AWithVideo;
		setMediaProperty(VideoCapture,Enabled,AWithVideo);
		FStartTime = QDateTime::currentDateTime();

		setState(IPhoneCall::Connecting);
		if (FSipPhone->isAccountRegistered(FSipAccountId))
			return startAfterRegistration(true);
		else if (!FSipPhone->setAccountRegistered(FSipAccountId,true))
			return startAfterRegistration(false);

		FWaitRegistration = true;
		return true;
	}
	return false;
}

bool PhoneCall::sendDtmf(const char *ADigits)
{
	return FSipCall!=NULL ? FSipCall->sendDtmf(ADigits) : false;
}

bool PhoneCall::hangupCall(HangupCode ACode, const QString &AText)
{
	if (IPhoneCall::Calling<=state() && state()<=IPhoneCall::Confirmed)
	{
		FHangupCode = ACode;
		FHangupText = AText;

		LOG_STRM_INFO(streamJid(),QString("Hanging up phone call, code=%1, text=%2, call=%3").arg(ACode).arg(AText).arg(FCallId));

		if (FSipCall)
		{
			switch (ACode)
			{
			case IPhoneCall::Busy:
				FSipCall->hangupCall(ISipCall::SC_BusyHere);
				break;
			case IPhoneCall::ErrorOccured:
				FSipCall->hangupCall(ISipCall::SC_InternalServerError);
				break;
			case IPhoneCall::NotAnswered:
				FSipCall->hangupCall(ISipCall::SC_DoesNotExistAnywhere);
				break;
			default:
				FSipCall->hangupCall(ISipCall::SC_Decline);
			}
		}

		switch (ACode)
		{
		case Busy:
			sendNotifyAction(FActiveReceivers,"busy");
			break;
		case Decline:
			sendNotifyAction(FActiveReceivers,"decline");
			break;
		case Declined:
			sendNotifyAction(FActiveReceivers,"declined");
			break;
		case NotAnswered:
			sendNotifyAction(FActiveReceivers,"timeout");
			break;
		case ErrorOccured:
			sendNotifyAction(FActiveReceivers,"error",AText);
			break;
		case RemotelyAccepted:
		case RemotelyDeclined:
			break;
		}

		setState(ACode!=ErrorOccured ? Disconnected : Aborted);
		return true;
	}
	return false;
}

bool PhoneCall::destroyCall(unsigned long AWaitForDisconnected)
{
	LOG_STRM_INFO(streamJid(),QString("Destroying phone call, wait=%1, call=%2").arg(AWaitForDisconnected).arg(FCallId));

	hangupCall(IPhoneCall::Decline);
	
	if (FSipCall)
	{
		FDelayedDestroy = true;
		return FSipCall->destroyCall(AWaitForDisconnected);
	}

	deleteLater();
	return true;
}

bool PhoneCall::hasActiveMedia() const
{
	return FSipCall!=NULL ? FSipCall->hasActiveMediaStream() : false;
}

bool PhoneCall::isVideoCallRequested() const
{
	return FWithVideo;
}

QWidget *PhoneCall::getVideoPreviewWidget(QWidget *AParent) const
{
	LOG_STRM_DEBUG(streamJid(),QString("Phone call video preview widget requested, call=%1").arg(FCallId));
	ISipDevice device = FSipPhone->findDevice(ISipMedia::Video,0);
	return FSipPhone->startVideoPreview(device,AParent);
}

QWidget *PhoneCall::getVideoPlaybackWidget(QWidget *AParent) const
{
	if (FSipCall)
	{
		LOG_STRM_DEBUG(streamJid(),QString("Phone call video playback widget requested, call=%1").arg(FCallId));
		ISipMediaStream stream = FSipCall->mediaStreams(ISipMedia::Video).value(0);
		return FSipCall->getVideoPlaybackWidget(stream.index,AParent);
	}
	else
	{
		REPORT_ERROR("Failed to get phone call video playback widget: SIP call not assigned");
	}
	return NULL;
}

QVariant PhoneCall::mediaProperty(Media AMedia, MediaProperty AProperty) const
{
	switch (AProperty)
	{
	case IPhoneCall::Avail:
		{
			ISipMedia::Type sipType;
			ISipMedia::Direction sipDir;
			ISipMediaStream::Property sipProp;
			if (getSipStreamParams(AMedia,IPhoneCall::Enabled,sipType,sipDir,sipProp))
				return !FSipPhone->availDevices(sipType,sipDir).isEmpty();
			else
				REPORT_ERROR(QString("Failed to get phone call media property, media=%1, property=%2: SIP stream params not found").arg(AMedia).arg(AProperty));
		}
		break;
	default:
		{
			QVariant value = FMediaProperties.value(AMedia).value(AProperty);
			return value.isNull() ? getSipStreamProperty(AMedia,AProperty) : value;
		}
	}
	return QVariant();
}

bool PhoneCall::setMediaProperty(Media AMedia, MediaProperty AProperty, const QVariant &AValue)
{
	ISipMedia::Type sipType;
	ISipMedia::Direction sipDir;
	ISipMediaStream::Property sipProp;
	if (getSipStreamParams(AMedia,AProperty,sipType,sipDir,sipProp))
	{
		LOG_STRM_DEBUG(streamJid(),QString("Changing phone call media property, media=%1, property=%2, value=%3, call=%4").arg(AMedia).arg(AProperty).arg(AValue.toString()).arg(FCallId));

		if (FSipCall && FSipCall->isActive())
		{
			int dev = FSipCall->mediaStreams(sipType).value(0).index;
			return FSipCall->setMediaStreamProperty(dev,sipDir,sipProp,AValue);
		}
		else if (FMediaProperties.value(AMedia).contains(AProperty) && FMediaProperties.value(AMedia).value(AProperty)!=AValue)
		{
			FMediaProperties[AMedia][AProperty] = AValue;
			emit mediaChanged();
			return true;
		}
	}
	else
	{
		REPORT_ERROR(QString("Failed to set phone call media property, media=%1, property=%2: SIP stream params not found").arg(AMedia).arg(AProperty));
	}
	return false;
}

void PhoneCall::initialize()
{
	FSipCall = NULL;
	FSHICallAction = -1;
	FDelayedDestroy = false;
	FWaitRegistration = false;
	FState = IPhoneCall::Inited;
	FHangupCode = IPhoneCall::Undefined;

	FSipMediaInited = false;
	FMediaProperties[VideoCapture][Enabled] = false;
	FMediaProperties[AudioPlayback][Enabled] = true;
	FMediaProperties[AudioPlayback][Volume] = 1.0;
	FMediaProperties[AudioCapture][Enabled] = true;
	FMediaProperties[AudioCapture][Volume] = 1.0;

	FRingTimer.setSingleShot(true);
	FRingTimer.setInterval(Options::node(OPV_PHONEMANAGER_CALL_TIMEOUT).value().toInt());
	connect(&FRingTimer,SIGNAL(timeout()),SLOT(onRingTimerTimeout()));

	FSipAccountId = FPhoneManager->defaultSipAccountId();

	connect(FSipPhone->instance(),SIGNAL(accountRemoved(const QUuid &)),SLOT(onSipAccountRemoved(const QUuid &)));
	connect(FSipPhone->instance(),SIGNAL(accountRegistrationChanged(const QUuid &, bool)),SLOT(onSipAccountRegistationChanged(const QUuid &, bool)));
}

void PhoneCall::setSipCall(ISipCall *ASipCall)
{
	FSipCall = ASipCall;
	connect(FSipCall->instance(),SIGNAL(stateChanged()),SLOT(onSipCallStateChanged()));
	connect(FSipCall->instance(),SIGNAL(mediaChanged()),SLOT(onSipCallMediaChanged()));
	connect(FSipCall->instance(),SIGNAL(callDestroyed()),SLOT(onSipCallDestroyed()));
	connect(FSipCall->instance(),SIGNAL(dtmfSent(const char *)),SIGNAL(dtmfSent(const char *)));
	LOG_STRM_INFO(streamJid(),QString("SIP call assigned to phone call, call=%1, uri=%2").arg(FCallId,FSipCall->remoteUri()));
}

void PhoneCall::setState(State AState)
{
	if (FState < AState)
	{
		LOG_STRM_INFO(streamJid(),QString("Phone call state changed to=%1, call=%2").arg(AState).arg(FCallId));

		FState = AState;
		switch(FState)
		{
		case IPhoneCall::Calling:
		case IPhoneCall::Ringing:
			insertStanzaHandler();
			FRingTimer.start();
			break;
		case IPhoneCall::Confirmed:
		case IPhoneCall::Disconnected:
		case IPhoneCall::Aborted:
			removeCallHandler();
			removeStanzaHandler();
			FActiveReceivers.clear();
			break;
		default:
			break;
		}

		FWaitRegistration = false;
		emit stateChanged();
	}
}

bool PhoneCall::startAfterRegistration(bool ARegistered)
{
	LOG_STRM_INFO(streamJid(),QString("Continue phone call starting after registration on SIP server, registered=%1, call=%2").arg(ARegistered).arg(FCallId));

	if (ARegistered)
	{
		if (role() == IPhoneCall::Caller)
		{
			LOG_STRM_INFO(streamJid(),QString("Creating SIP call for phone call, call=%1, uri=%2").arg(FCallId,FRemoteUri));

			ISipCall *sipCall = FSipPhone->newCall(FSipAccountId,FRemoteUri);
			if (sipCall)
			{
				setSipCall(sipCall);
				return sipCall->startCall(FWithVideo);
			}
			else
			{
				hangupCall(IPhoneCall::ErrorOccured,tr("Invalid SIP Address '%1'").arg(FRemoteUri));
			}
		}
		else if (role() == IPhoneCall::Receiver)
		{
			Stanza stanza("iq");
			stanza.setTo(FContactJid.full()).setType("set").setId(FStanzaProcessor->newId());

			QDomElement queryElem = stanza.addElement("query", NS_VACUUM_SIP_PHONE);
			queryElem.setAttribute("cid",FCallId);
			QDomElement actionElem = queryElem.appendChild(stanza.createElement("action")).toElement();
			actionElem.setAttribute("type","accept");
			actionElem.setAttribute("video",QVariant(FWithVideo).toString());
			actionElem.appendChild(stanza.createTextNode(FSipPhone->accountUri(FSipAccountId)));

			if (FStanzaProcessor->sendStanzaRequest(this,FStreamJid,stanza,CALL_REQUEST_TIMEOUT))
			{
				LOG_STRM_INFO(streamJid(),QString("Phone call accept request sent to=%1, id=%2, call=%3").arg(stanza.to(),stanza.id(),FCallId));
				insertCallHandler();
				FCallRequests.insert(stanza.id(),FContactJid);
				return true;
			}
			else
			{
				LOG_STRM_WARNING(streamJid(),QString("Failed to send phone call accept request to=%1, call=%2").arg(stanza.to(),FCallId));
			}
		}
	}
	else
	{
		hangupCall(IPhoneCall::ErrorOccured,tr("Failed to register at SIP server"));
	}
	return false;
}

void PhoneCall::sendNotifyAction(const QSet<Jid> &ARceivers, const QString &AType, const QString &ADescription)
{
	foreach(const Jid &receiver, ARceivers)
	{
		Stanza stanza("iq");
		stanza.setTo(receiver.full()).setType("set").setId(FStanzaProcessor->newId());

		QDomElement queryElem = stanza.addElement("query",NS_VACUUM_SIP_PHONE);
		queryElem.setAttribute("cid",FCallId);

		QDomElement actionElem = queryElem.appendChild(stanza.createElement("action")).toElement();
		actionElem.setAttribute("type",AType);
		actionElem.appendChild(stanza.createTextNode(ADescription));

		if (FStanzaProcessor->sendStanzaOut(FStreamJid,stanza))
			LOG_STRM_INFO(streamJid(),QString("Phone call notification sent to=%1, action=%2, call=%3").arg(stanza.to(),AType,FCallId));
		else
			LOG_STRM_WARNING(streamJid(),QString("Failed to send phone call notification to=%1, action=%2, call=%3").arg(stanza.to(),AType,FCallId));
	}
}

QVariant PhoneCall::getSipStreamProperty(Media AMedia, MediaProperty AProperty) const
{
	ISipMedia::Type sipType;
	ISipMedia::Direction sipDir;
	ISipMediaStream::Property sipProp;
	if (FSipCall && getSipStreamParams(AMedia,AProperty,sipType,sipDir,sipProp))
	{
		int dev = FSipCall->mediaStreams(sipType).value(0).index;
		return FSipCall->mediaStreamProperty(dev,sipDir,sipProp);
	}
	return QVariant();
}

bool PhoneCall::getSipStreamParams(Media AMedia, MediaProperty AProperty, ISipMedia::Type &ASipMedia, ISipMedia::Direction &ASipDir, ISipMediaStream::Property &ASipProp) const
{
	bool ok = true;

	switch (AMedia)
	{
	case IPhoneCall::AudioCapture:
		ASipMedia = ISipMedia::Audio;
		ASipDir = ISipMedia::Capture;
		break;
	case IPhoneCall::AudioPlayback:
		ASipMedia = ISipMedia::Audio;
		ASipDir = ISipMedia::Playback;
		break;
	case IPhoneCall::VideoCapture:
		ASipMedia = ISipMedia::Video;
		ASipDir = ISipMedia::Capture;
		break;
	case IPhoneCall::VideoPlayback:
		ASipMedia = ISipMedia::Video;
		ASipDir = ISipMedia::Playback;
		break;
	default:
		ok = false;
	}

	switch (AProperty)
	{
	case IPhoneCall::Enabled:
		ASipProp = ISipMediaStream::Enabled;
		break;
	case IPhoneCall::Volume:
		ASipProp = ISipMediaStream::Volume;
		break;
	default:
		ok = false;
	}

	return ok;
}

void PhoneCall::insertCallHandler()
{
	FSipPhone->insertCallHandler(SCHO_PHONECALL,this);
}

void PhoneCall::removeCallHandler()
{
	FSipPhone->removeCallHandler(SCHO_PHONECALL,this);
}

void PhoneCall::insertStanzaHandler()
{
	if (!isNumberCall() && FSHICallAction<0)
	{
		IStanzaHandle handle;
		handle.handler = this;
		handle.order = SHO_DEFAULT;
		handle.streamJid = FStreamJid;
		handle.direction = IStanzaHandle::DirectionIn;
		handle.conditions.append(QString(SHC_PHONE_CALL_ACTION).arg(FCallId));
		FSHICallAction = FStanzaProcessor->insertStanzaHandle(handle);
	}
}

void PhoneCall::removeStanzaHandler()
{
	if (!isNumberCall() && FSHICallAction>0)
	{
		FStanzaProcessor->removeStanzaHandle(FSHICallAction);
		FSHICallAction = -1;
	}
}

void PhoneCall::onRingTimerTimeout()
{
	if (state()==IPhoneCall::Calling || state()==IPhoneCall::Ringing)
	{
		LOG_STRM_INFO(streamJid(),QString("Phone call ring timer timeout, call=%1").arg(FCallId));
		hangupCall(IPhoneCall::NotAnswered);
	}
}

void PhoneCall::onSipCallStateChanged()
{
	switch(FSipCall->state())
	{
	case ISipCall::Calling:
		setState(IPhoneCall::Calling);
		break;
	case ISipCall::Ringing:
		setState(IPhoneCall::Ringing);
		break;
	case ISipCall::Connecting:
		setState(IPhoneCall::Connecting);
		break;
	case ISipCall::Confirmed:
		setState(IPhoneCall::Confirmed);
		break;
	case ISipCall::Disconnected:
		switch (FSipCall->statusCode())
		{
		case ISipCall::SC_BusyHere:
		case ISipCall::SC_BusyEverywhere:
			hangupCall(IPhoneCall::Busy,FSipCall->statusText());
			break;
		default:
			hangupCall(IPhoneCall::Declined,FSipCall->statusText());
		}
		break;
	case ISipCall::Aborted:
		hangupCall(IPhoneCall::ErrorOccured,FSipCall->statusText());
		break;
	default:
		break;
	}
}

void PhoneCall::onSipCallMediaChanged()
{
	static bool block = false;
	if (!block && FSipCall && FSipCall->isActive())
	{
		block = true;
		foreach(Media media, FMediaProperties.keys())
		{
			foreach(MediaProperty prop, FMediaProperties.value(media).keys())
			{
				if (!FSipMediaInited)
					setMediaProperty(media,prop,FMediaProperties.value(media).value(prop));
				FMediaProperties[media][prop] = getSipStreamProperty(media,prop);
			}
		}
		FSipMediaInited = true;
		block = false;

		emit mediaChanged();
	}
}

void PhoneCall::onSipCallDestroyed()
{
	FSipCall = NULL;
	if (FDelayedDestroy)
		destroyCall(0);
}

void PhoneCall::onSipAccountRemoved(const QUuid &AAccountId)
{
	if (FSipAccountId == AAccountId)
		destroyCall(0);
}

void PhoneCall::onSipAccountRegistationChanged(const QUuid &AAccountId, bool ARegistered)
{
	if (FWaitRegistration && AAccountId==FSipAccountId)
		startAfterRegistration(ARegistered);
	FWaitRegistration = false;
}
