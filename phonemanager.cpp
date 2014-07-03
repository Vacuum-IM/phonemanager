#include "phonemanager.h"

#include <QDir>
#include <QFile>
#include <definitions/resources.h>
#include <definitions/notificationdataroles.h>
#include <definitions/phonecallhandlerorders.h>
#include <definitions/phonemanager/menuicons.h>
#include <definitions/phonemanager/namespaces.h>
#include <definitions/phonemanager/optionnodes.h>
#include <definitions/phonemanager/optionvalues.h>
#include <definitions/phonemanager/optionnodeorders.h>
#include <definitions/phonemanager/optionwidgetorders.h>
#include <definitions/phonemanager/toolbargroups.h>
#include <definitions/phonemanager/internalerrors.h>
#include <definitions/phonemanager/rosternotifyorders.h>
#include <definitions/phonemanager/notificationtypes.h>
#include <definitions/phonemanager/notificationtypeorders.h>
#include <definitions/phonemanager/messageeditsendhandlerorders.h>
#include <utils/widgetmanager.h>
#include <utils/options.h>
#include <utils/logger.h>

#define DIR_PHONE                   "phone"
#define FILE_NUMBERS                "numbers.xml"
#define FILE_CALLSHISTORY           "callshistory.db"

#define STORAGE_SAVE_TIMEOUT        100

#define SHC_PHONE_CALL_REQUEST      "/iq[@type='set']/query[@xmlns='" NS_VACUUM_SIP_PHONE "']/action[@type='request']"

#define ADR_NOTIFY_ID               Action::DR_Parametr1

PhoneManager::PhoneManager()
{
	FSipPhone = NULL;
	FAvatars = NULL;
	FGateways = NULL;
	FRosterPlugin = NULL;
	FPresencePlugin = NULL;
	FPrivateStorage = NULL;
	FPluginManager = NULL;
	FMessageWidgets = NULL;
	FMessageProcessor = NULL;
	FStanzaProcessor = NULL;
	FDiscovery = NULL;
	FMainWindowPlugin = NULL;
	FNotifications = NULL;
	FMessageStyles = NULL;
	FOptionsManager = NULL;

	FSHICallRequest = -1;

	FSaveNumbersTimer.setSingleShot(true);
	connect(&FSaveNumbersTimer,SIGNAL(timeout()),SLOT(onSaveNumbersTimerTimeout()));

	FSipAccountTimer.setSingleShot(true);
	connect(&FSipAccountTimer,SIGNAL(timeout()),SLOT(onUpdateDefaultSipAccount()));

	FCallHistoryWorker = new CallHistoryWorker(this);
	connect(FCallHistoryWorker,SIGNAL(taskFinished(CallHistoryTask *)),SLOT(onCallHistoryTaskFinished(CallHistoryTask *)));

#ifdef DEBUG_RESOURCES_DIR
	FileStorage::setResourcesDirs(FileStorage::resourcesDirs() << DEBUG_RESOURCES_DIR);
#endif
}

PhoneManager::~PhoneManager()
{
	FCallHistoryWorker->quit();
	FCallHistoryWorker->wait();
}

void PhoneManager::pluginInfo(IPluginInfo *APluginInfo)
{
	APluginInfo->name = tr("Phone Manager");
	APluginInfo->description = tr("Allows to call and send SMS messages to phones");
	APluginInfo->version = "1.0";
	APluginInfo->author = "Potapov S.A. aka Lion";
	APluginInfo->homePage = "http://www.vacuum-im.org";
	APluginInfo->dependences.append(ROSTER_UUID);
	APluginInfo->dependences.append(SIPPHONE_UUID);
}

bool PhoneManager::initConnections(IPluginManager *APluginManager, int &AInitOrder)
{
	Q_UNUSED(AInitOrder);
	FPluginManager = APluginManager;

	IPlugin *plugin = APluginManager->pluginInterface("IRosterPlugin").value(0,NULL);
	if (plugin)
	{
		FRosterPlugin = qobject_cast<IRosterPlugin *>(plugin->instance());
		if (FRosterPlugin)
		{
			connect(FRosterPlugin->instance(),SIGNAL(rosterAdded(IRoster *)),SLOT(onRosterAdded(IRoster *)));
			connect(FRosterPlugin->instance(),SIGNAL(rosterOpened(IRoster *)),SLOT(onRosterOpened(IRoster *)));
			connect(FRosterPlugin->instance(),SIGNAL(rosterClosed(IRoster *)),SLOT(onRosterClosed(IRoster *)));
			connect(FRosterPlugin->instance(),SIGNAL(rosterRemoved(IRoster *)),SLOT(onRosterRemoved(IRoster *)));
			connect(FRosterPlugin->instance(),SIGNAL(rosterStreamJidChanged(IRoster *, const Jid &)),SLOT(onRosterStreamJidChanged(IRoster *, const Jid &)));
			connect(FRosterPlugin->instance(),SIGNAL(rosterItemReceived(IRoster *, const IRosterItem &, const IRosterItem &)),
				SLOT(onRosterItemReceived(IRoster *, const IRosterItem &, const IRosterItem &)));
		}
	}

	plugin = APluginManager->pluginInterface("IPresencePlugin").value(0,NULL);
	if (plugin)
	{
		FPresencePlugin = qobject_cast<IPresencePlugin *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("IAvatars").value(0,NULL);
	if (plugin)
	{
		FAvatars = qobject_cast<IAvatars *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("IGateways").value(0,NULL);
	if (plugin)
	{
		FGateways = qobject_cast<IGateways *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("IPrivateStorage").value(0,NULL);
	if (plugin)
	{
		FPrivateStorage = qobject_cast<IPrivateStorage *>(plugin->instance());
		if (FPrivateStorage)
		{
			connect(FPrivateStorage->instance(),SIGNAL(storageOpened(const Jid &)),SLOT(onPrivateStorateOpened(const Jid &)));
			connect(FPrivateStorage->instance(),SIGNAL(dataLoaded(const QString &, const Jid &, const QDomElement &)),
				SLOT(onPrivateDataLoaded(const QString &, const Jid &, const QDomElement &)));
			connect(FPrivateStorage->instance(),SIGNAL(dataChanged(const Jid &, const QString &, const QString &)),
				SLOT(onPrivateDataChanged(const Jid &, const QString &, const QString &)));
		}
	}

	plugin = APluginManager->pluginInterface("IMessageWidgets").value(0,NULL);
	if (plugin)
	{
		FMessageWidgets = qobject_cast<IMessageWidgets *>(plugin->instance());
		if (FMessageWidgets)
		{
			connect(FMessageWidgets->instance(),SIGNAL(chatWindowCreated(IMessageChatWindow *)),SLOT(onMessageChatWindowCreated(IMessageChatWindow *)));
			connect(FMessageWidgets->instance(),SIGNAL(chatWindowDestroyed(IMessageChatWindow *)),SLOT(onMessageChatWindowDestroyed(IMessageChatWindow *)));
		}
	}

	plugin = APluginManager->pluginInterface("IMessageProcessor").value(0,NULL);
	if (plugin)
	{
		FMessageProcessor = qobject_cast<IMessageProcessor *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("ISipPhone").value(0,NULL);
	if (plugin)
	{
		FSipPhone = qobject_cast<ISipPhone *>(plugin->instance());
		if (FSipPhone)
		{
			connect(FSipPhone->instance(),SIGNAL(accountInserted(const QUuid &)),SLOT(onSipAccountInserted(const QUuid &)));
			connect(FSipPhone->instance(),SIGNAL(accountRemoved(const QUuid &)),SLOT(onSipAccountRemoved(const QUuid &)));
		}
	}

	plugin = APluginManager->pluginInterface("IServiceDiscovery").value(0,NULL);
	if (plugin)
	{
		FDiscovery = qobject_cast<IServiceDiscovery *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("IStanzaProcessor").value(0,NULL);
	if (plugin)
	{
		FStanzaProcessor = qobject_cast<IStanzaProcessor *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("IMainWindowPlugin").value(0,NULL);
	if (plugin)
	{
		FMainWindowPlugin = qobject_cast<IMainWindowPlugin *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("INotifications").value(0,NULL);
	if (plugin)
	{
		FNotifications = qobject_cast<INotifications *>(plugin->instance());
		if (FNotifications)
		{
			connect(FNotifications->instance(),SIGNAL(notificationActivated(int)),SLOT(onNotificationActivated(int)));
			connect(FNotifications->instance(),SIGNAL(notificationRemoved(int)),SLOT(onNotificationRemoved(int)));
		}
	}

	plugin = APluginManager->pluginInterface("IMessageStyles").value(0,NULL);
	if (plugin)
	{
		FMessageStyles = qobject_cast<IMessageStyles *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("IOptionsManager").value(0,NULL);
	if (plugin)
	{
		FOptionsManager = qobject_cast<IOptionsManager *>(plugin->instance());
	}

	connect(Options::instance(),SIGNAL(optionsOpened()),SLOT(onOptionsOpened()));
	connect(Options::instance(),SIGNAL(optionsClosed()),SLOT(onOptionsClosed()));
	connect(Options::instance(),SIGNAL(optionsChanged(const OptionsNode &)),SLOT(onOptionsChanged(const OptionsNode &)));

	return FRosterPlugin!=NULL && FSipPhone!=NULL;
}

bool PhoneManager::initObjects()
{
	XmppError::registerError(NS_INTERNAL_ERROR,IERR_PHONEMANAGER_HISTORY_DB_NOT_CREATED,tr("Failed to create database"));
	XmppError::registerError(NS_INTERNAL_ERROR,IERR_PHONEMANAGER_HISTORY_DB_NOT_OPENED,tr("Failed to open database"));
	XmppError::registerError(NS_INTERNAL_ERROR,IERR_PHONEMANAGER_HISTORY_DB_NOT_COMPATIBLE,tr("Database format is not compatible"));
	XmppError::registerError(NS_INTERNAL_ERROR,IERR_PHONEMANAGER_HISTORY_DB_EXEC_FAILED,tr("Failed to to execute SQL query"));

	if (FSipPhone)
	{
		if (FDiscovery)
		{
			IDiscoFeature phone;
			phone.active = true;
			phone.icon = IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->getIcon(MNI_PHONEMANAGER_CALL);
			phone.var = NS_VACUUM_SIP_PHONE;
			phone.name = tr("SIP Phone");
			phone.description = tr("Voice and video calls over SIP");
			FDiscovery->insertDiscoFeature(phone);
		}

		if (FStanzaProcessor)
		{
			IStanzaHandle callRequest;
			callRequest.handler = this;
			callRequest.order = SHO_DEFAULT;
			callRequest.direction = IStanzaHandle::DirectionIn;
			callRequest.conditions.append(SHC_PHONE_CALL_REQUEST);
			FSHICallRequest = FStanzaProcessor->insertStanzaHandle(callRequest);
		}

		insertCallHandler(PCHO_PHONEMANAGER,this);
	}

	if (FMainWindowPlugin)
	{
		Action *dialerAction = new Action(FMainWindowPlugin->mainWindow()->topToolBarChanger()->toolBar());
		dialerAction->setText("Phone Calls");
		dialerAction->setIcon(RSR_STORAGE_MENUICONS,MNI_PHONEMANAGER_DIALER_DIALOG);
		connect(dialerAction,SIGNAL(triggered()),SLOT(onShowDialerDialogActionTriggered()));
		FMainWindowPlugin->mainWindow()->topToolBarChanger()->insertAction(dialerAction,TBG_MWTTB_PHONECALL);
	}

	if (FNotifications)
	{
		INotificationType notifyType;
		notifyType.order = NTO_PHONEMANAGER_MISSEDCALL;
		notifyType.icon = IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->getIcon(MNI_PHONEMANAGER_CALL_IN);
		notifyType.title = tr("When incoming call not answered");
		notifyType.kindMask = INotification::RosterNotify|INotification::TrayNotify|INotification::TrayAction|INotification::PopupWindow|INotification::AlertWidget|INotification::ShowMinimized|INotification::AutoActivate;
		notifyType.kindDefs = notifyType.kindMask & ~(INotification::AutoActivate);
		FNotifications->registerNotificationType(NNT_PHONEMANAGER_MISSEDCALL,notifyType);
	}

	return true;
}

bool PhoneManager::initSettings()
{
	Options::setDefaultValue(OPV_PHONEMANAGER_CALL_TIMEOUT,30000);

	if (FSipPhone && FOptionsManager)
	{
		IOptionsDialogNode dnode = { ONO_PHONEMANAGER_SIPPHONE, OPN_PHONEMANAGER_SIPPHONE, tr("SIP Phone"), MNI_PHONEMANAGER_CALL };
		FOptionsManager->insertOptionsDialogNode(dnode);
		FOptionsManager->insertOptionsHolder(this);
	}
	return true;
}

bool PhoneManager::startPlugin()
{
	FCallHistoryWorker->start();
	return true;
}

bool PhoneManager::stanzaReadWrite(int AHandleId, const Jid &AStreamJid, Stanza &AStanza, bool &AAccept)
{
	if (AHandleId == FSHICallRequest)
	{
		QDomElement queryElem = AStanza.firstElement("query",NS_VACUUM_SIP_PHONE);
		QDomElement actionElem = queryElem.firstChildElement("action");
		QString actionType = actionElem.attribute("type");
		if (actionType == "request")
		{
			AAccept = true;
			QString callId = queryElem.attribute("cid");
			QString remoteUri = actionElem.text();
			bool withVideo = QVariant(actionElem.attribute("video")).toBool();

			LOG_STRM_INFO(AStreamJid,QString("Phone call request received, from=%1, uri=%2, video=%3, call=%4").arg(AStanza.from(),remoteUri).arg(withVideo).arg(callId));

			if (callId.isEmpty())
			{
				LOG_STRM_WARNING(AStreamJid,QString("Invalid phone call request received from=%1: Call id is empty").arg(AStanza.from()));
				Stanza error = FStanzaProcessor->makeReplyError(AStanza,XmppStanzaError::EC_BAD_REQUEST);
				FStanzaProcessor->sendStanzaOut(AStreamJid,error);
			}
			else if (findCallById(callId) != NULL)
			{
				LOG_STRM_WARNING(AStreamJid,QString("Invalid phone call request received from=%1, call=%2: Duplicate call id").arg(AStanza.from(),callId));
				Stanza error = FStanzaProcessor->makeReplyError(AStanza,XmppStanzaError::EC_CONFLICT);
				FStanzaProcessor->sendStanzaOut(AStreamJid,error);
			}
			else if (!processIncomingCall(AStreamJid,callId,AStanza.from(),remoteUri,withVideo))
			{
				LOG_STRM_WARNING(AStreamJid,QString("Failed to process phone call request from=%1, call=%2").arg(AStanza.from(),callId));
				Stanza error = FStanzaProcessor->makeReplyError(AStanza,XmppStanzaError::EC_NOT_ACCEPTABLE);
				FStanzaProcessor->sendStanzaOut(AStreamJid,error);
			}
			else
			{
				LOG_STRM_INFO(AStreamJid,QString("Phone call request accepted from=%1, call=%2").arg(AStanza.from(),callId));
				Stanza result = FStanzaProcessor->makeReplyResult(AStanza);
				FStanzaProcessor->sendStanzaOut(AStreamJid,result);
			}
		}
	}
	return false;
}

QMultiMap<int, IOptionsWidget *> PhoneManager::optionsWidgets(const QString &ANodeId, QWidget *AParent)
{
	QMultiMap<int, IOptionsWidget *> widgets;
	if (ANodeId == OPN_PHONEMANAGER_SIPPHONE)
	{
		widgets.insert(OWO_PHONEMANAGER_SIPPHONE, new SipAccountOptionsWidget(AParent));
	}
	return widgets;
}

bool PhoneManager::phoneCallReceive(int AOrder, IPhoneCall *ACall)
{
	if (AOrder == PCHO_PHONEMANAGER)
	{
		QSet<IPhoneCall *> activeCalls = (phoneCalls(true).toSet() -= ACall);
		if (activeCalls.isEmpty())
		{
			LOG_STRM_INFO(ACall->streamJid(),QString("Phone call received from=%1, call=%2").arg(ACall->contactJid().full(),ACall->callId()));
			return showCallWindow(ACall);
		}
		else
		{
			LOG_STRM_WARNING(ACall->streamJid(),QString("Phone call not received from=%1, call=%2: Active calls found").arg(ACall->contactJid().full(),ACall->callId()));
		}
	}
	return false;
}

bool PhoneManager::isCallsEnabled() const
{
	return FSipPhone!=NULL && !FSipPhone->availAccounts().isEmpty() && !FAvailCallStreams.isEmpty();
}

bool PhoneManager::isAudioCallsAvailable() const
{
	return FSipPhone!=NULL && FSipPhone->isAudioCallsAvailable();
}

bool PhoneManager::isVideoCallsAvailable() const
{
	return FSipPhone!=NULL && FSipPhone->isVideoCallsAvailable();
}

QUuid PhoneManager::defaultSipAccountId() const
{
	static const QUuid uuid = "{48AD5AA2-EC3E-4102-B130-94FCF758A630}";
	return uuid;
}

QList<Jid> PhoneManager::availCallStreams() const
{
	return FAvailCallStreams;
}

QList<IPhoneCall *> PhoneManager::phoneCalls(bool AActiveOnly) const
{
	QList<IPhoneCall *> calls;
	foreach(IPhoneCall *call, FCalls)
	{
		if (!AActiveOnly || call->isActive())
			calls.append(call);
	}
	return calls;
}

IPhoneCall *PhoneManager::newCall(const Jid &AStreamJid, const QString &ANumber)
{
	if (AStreamJid.isValid() && isCallsEnabled() && isValidNumber(ANumber))
	{
		QString callId = QUuid::createUuid().toString();
		LOG_STRM_INFO(AStreamJid,QString("Phone call created as caller, number=%1, call=%2").arg(ANumber,callId));
		PhoneCall *call = new PhoneCall(this,FSipPhone,AStreamJid,callId,ANumber,this);
		appendCall(call);
		return call;
	}
	return NULL;
}

IPhoneCall *PhoneManager::newCall(const Jid &AStreamJid, const QSet<Jid> &AReceivers)
{
	if (FStanzaProcessor && AStreamJid.isValid() && isCallsEnabled() && !AReceivers.isEmpty())
	{
		QString callId = QUuid::createUuid().toString();
		LOG_STRM_INFO(AStreamJid,QString("Phone call created as caller, with=%1, call=%2").arg(AReceivers.toList().value(0).full(),callId));
		PhoneCall *call = new PhoneCall(this,FSipPhone,FStanzaProcessor,AStreamJid,callId,AReceivers,this);
		appendCall(call);
		return call;
	}
	return NULL;
}

bool PhoneManager::isCallsSupported(const Jid &AStreamJid, const Jid &AContactJid) const
{
	return FDiscovery!=NULL && FDiscovery->hasDiscoInfo(AStreamJid,AContactJid) ? FDiscovery->discoInfo(AStreamJid,AContactJid).features.contains(NS_VACUUM_SIP_PHONE) : false;
}

bool PhoneManager::showCallWindow(IPhoneCall *ACall)
{
	QSet<Jid> contacts;
	if (ACall->isNumberCall())
		contacts += findContactByNumber(ACall->streamJid(),ACall->number());
	else if (ACall->contactJid().isValid())
		contacts += ACall->contactJid();
	else if (ACall->role() == IPhoneCall::Caller)
		contacts += ACall->receivers();
	contacts -= Jid::null;

	foreach(const Jid &contactJid, contacts)
	{
		Jid streamJid = ACall->streamJid();
		if (FRosterPlugin!=NULL && !isRosterContact(streamJid,contactJid))
		{
			foreach(const Jid &stream, FNumbers.keys())
			{
				if (isRosterContact(stream,contactJid))
				{
					streamJid = stream;
					break;
				}
			}
		}

		PhoneChatHandler *handler = getChatHandler(streamJid,contactJid);
		if (handler)
			return handler->processPhoneCall(ACall);
	}

	if (FCallWindow.isNull())
	{
		FCallWindow = new PhoneCallWindow(this,ACall);
		WidgetManager::showActivateRaiseWindow(FCallWindow);
		return true;
	}

	return false;
}

void PhoneManager::showDialerDialog()
{
	if (FDialerDialog.isNull())
		FDialerDialog = new PhoneDialerDialog(this);
	WidgetManager::showActivateRaiseWindow(FDialerDialog);
}

QString PhoneManager::saveCallHistoryItem(IPhoneCall *ACall)
{
	if (ACall->state()==IPhoneCall::Disconnected || ACall->state()==IPhoneCall::Aborted)
	{
		IPhoneCallHistoryItem item;
		item.start = ACall->startTime();
		if (ACall->isNumberCall())
			item.with = ACall->number();
		else if (ACall->contactJid().isEmpty())
			item.with = ACall->receivers().toList().value(0).pBare();
		else
			item.with = ACall->contactJid().pBare();
		item.role = ACall->role();
		item.hangupCode = ACall->hangupCode();
		item.duration = ACall->durationTime();
		item.isNumberCall = ACall->isNumberCall();
		return saveCallHistoryItem(ACall->streamJid(),item);
	}
	return QString::null;
}

QString PhoneManager::saveCallHistoryItem(const Jid &AStreamJid, const IPhoneCallHistoryItem &AItem)
{
	if (FCallHistoryProperties.contains(AStreamJid.pBare()))
	{
		CallHistoryTaskSaveItem *task = new CallHistoryTaskSaveItem(AStreamJid,AItem);
		if (FCallHistoryWorker->startTask(task))
		{
			LOG_STRM_DEBUG(AStreamJid,QString("Save phone call history item task started, id=%1, with=%2").arg(task->taskId(),AItem.with));
			return task->taskId();
		}
		else
		{
			LOG_STRM_ERROR(AStreamJid,QString("Failed to start save phone call history item task, with=%1").arg(AItem.with));
		}
	}
	else
	{
		REPORT_ERROR("Failed to save phone call history item: Stream not found");
	}
	return QString::null;
}

QString PhoneManager::loadCallHistoryItems(const Jid &AStreamJid, const IPhoneCallHistoryRequest &ARequest)
{
	if (FCallHistoryProperties.contains(AStreamJid.pBare()))
	{
		CallHistoryTaskLoadItems *task = new CallHistoryTaskLoadItems(AStreamJid,ARequest);
		if (FCallHistoryWorker->startTask(task))
		{
			LOG_STRM_DEBUG(AStreamJid,QString("Load phone call history items task started, id=%1, with=%2").arg(task->taskId(),ARequest.with));
			return task->taskId();
		}
		else
		{
			LOG_STRM_ERROR(AStreamJid,QString("Failed to start load phone call history items task, with=%1").arg(ARequest.with));
		}
	}
	else
	{
		REPORT_ERROR("Failed to load phone call history items: Stream not found");
	}
	return QString::null;
}

QString PhoneManager::removeCallHistoryItems(const Jid &AStreamJid, const IPhoneCallHistoryRequest &ARequest)
{
	if (FCallHistoryProperties.contains(AStreamJid.pBare()))
	{
		CallHistoryTaskRemoveItems *task = new CallHistoryTaskRemoveItems(AStreamJid,ARequest);
		if (FCallHistoryWorker->startTask(task))
		{
			LOG_STRM_DEBUG(AStreamJid,QString("Remove phone call history items task started, id=%1, with=%2").arg(task->taskId(),ARequest.with));
			return task->taskId();
		}
		else
		{
			LOG_STRM_ERROR(AStreamJid,QString("Failed to start remove phone call history items task, with=%1").arg(ARequest.with));
		}
	}
	else
	{
		REPORT_ERROR("Failed to remove phone call history items: Stream not found");
	}
	return QString::null;
}

void PhoneManager::showCallHistoryWindow()
{
	if (FCallHistoryWindow.isNull())
		FCallHistoryWindow = new CallHistoryWindow(this,FPresencePlugin,FMessageProcessor);
	WidgetManager::showActivateRaiseWindow(FCallHistoryWindow);
}

Jid PhoneManager::callContactJid(IPhoneCall *ACall) const
{
	if (ACall->isNumberCall())
		return findContactByNumber(ACall->streamJid(),ACall->number());
	else if (!ACall->contactJid().isEmpty())
		return ACall->contactJid();
	else
		return ACall->receivers().toList().value(0);
}

QString PhoneManager::callContactName(IPhoneCall *ACall) const
{
	Jid contactJid = callContactJid(ACall);
	if (!contactJid.isEmpty())
	{
		IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->findRoster(ACall->streamJid()) : NULL;
		IRosterItem ritem = roster!=NULL ? roster->rosterItem(contactJid) : IRosterItem();
		return !ritem.name.isEmpty() ? ritem.name : contactJid.uBare();
	}
	else if (ACall->isNumberCall())
	{
		return formattedNumber(ACall->number());
	}
	return QString::null;
}

QString PhoneManager::callContactName(const Jid &AStreamJid, const IPhoneCallHistoryItem &AItem) const
{
	Jid contactJid = AItem.isNumberCall ? findContactByNumber(AStreamJid,AItem.with) : Jid(AItem.with);
	if (!contactJid.isEmpty())
	{
		IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->findRoster(AStreamJid) : NULL;
		IRosterItem ritem = roster!=NULL ? roster->rosterItem(contactJid) : IRosterItem();
		return !ritem.name.isEmpty() ? ritem.name : contactJid.uBare();
	}
	else if (AItem.isNumberCall)
	{
		return formattedNumber(AItem.with);
	}
	return AItem.with;
}

QImage PhoneManager::callContactAvatarImage(IPhoneCall *ACall, const QSize &AMaxSize, bool AGray) const
{
	QImage avatar;
	if (FAvatars)
	{
		QString hash = FAvatars->avatarHash(callContactJid(ACall));
		avatar = FAvatars->loadAvatarImage(hash,AMaxSize,AGray);
		if (avatar.isNull())
			avatar = FAvatars->emptyAvatarImage(AMaxSize,AGray);
	}
	return avatar;
}

QList<Jid> PhoneManager::numbersStreams() const
{
	return FNumbers.keys();
}

bool PhoneManager::isNumbersReady(const Jid &AStreamJid) const
{
	return FNumbers.contains(AStreamJid) && (FPrivateStorage==NULL || FPrivateStorage->isLoaded(AStreamJid,"storage",NS_STORAGE_PHONENUMBERS));
}

bool PhoneManager::isValidNumber(const QString &ANumber) const
{
	QString number = normalizedNumber(ANumber);

	if (number.length() < 2)
		return false;

	if (number.startsWith('+') && number.length()<12)
		return false;
	
	for (int i=0; i<number.length(); i++)
		if (!number.at(i).isDigit() && (i>0 || number.at(i)!='+'))
			return false;

	return true;
}

QString PhoneManager::formattedNumber(const QString &ANumber) const
{
	QString number = normalizedNumber(ANumber);
	if (number.length() >= 12)
	{
		number.insert(2," (");
		number.insert(7,") ");
		number.insert(12,"-");
		number.insert(15,"-");
	}
	return number;
}

QString PhoneManager::normalizedNumber(const QString &ANumber) const
{
	QString number;
	for (int i=0; i<ANumber.length(); i++)
	{
		QChar ch = ANumber.at(i);
		if (ch.isDigit() || ch.isLetter())
			number += ch;
		else if (i==0 && ch=='+')
			number += ch;
	}
	if (number.length()==11 && (number.startsWith('8') || number.startsWith('7')))
	{
		number.remove(0,1);
		number.prepend("+7");
	}
	else if (number.length()==10 && !number.startsWith('+'))
	{
		number.prepend("+7");
	}
	return number;
}

QHash<Jid, QStringList> PhoneManager::streamNumbers(const Jid &AStreamJid) const
{
	return FNumbers.value(AStreamJid);
}

QStringList PhoneManager::contactNumbers(const Jid &AStreamJid, const Jid &AContactJid) const
{
	return FNumbers.value(AStreamJid).value(AContactJid.bare());
}

bool PhoneManager::isRosterContact(const Jid &AStreamJid, const Jid &AContactJid) const
{
	IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->findRoster(AStreamJid) : NULL;
	return roster!=NULL ? roster->rosterItem(AContactJid).isValid : false;
}

Jid PhoneManager::findContactByNumber(const Jid &AStreamJid, const QString &ANumber) const
{
	QString number = normalizedNumber(ANumber);
	QHash<Jid,QStringList> numbers = FNumbers.value(AStreamJid);
	for (QHash<Jid,QStringList>::const_iterator it=numbers.constBegin(); it!=numbers.constEnd(); ++it)
		if (it->contains(number))
			return it.key();
	return Jid::null;
}

bool PhoneManager::appendContactNumber(const Jid &AStreamJid, const Jid &AContactJid, const QString &ANumber)
{
	if (isNumbersReady(AStreamJid) && isValidNumber(ANumber) && isRosterContact(AStreamJid,AContactJid))
	{
		LOG_STRM_INFO(AStreamJid,QString("Adding phone number to contact jid=%1, number=%2").arg(AContactJid.bare(),ANumber));
		QString number = normalizedNumber(ANumber);
		QStringList &numbers = FNumbers[AStreamJid][AContactJid.bare()];
		if (!numbers.contains(number))
		{
			removeContactNumber(AStreamJid,findContactByNumber(AStreamJid,ANumber),ANumber);
			numbers.append(number);
			startSaveNumbersToStorage(AStreamJid);
			emit numbersChanged(AStreamJid,AContactJid.bare());
		}
		return true;
	}
	else
	{
		LOG_STRM_ERROR(AStreamJid,QString("Failed to add phone number to contact jid=%1, number=%2").arg(AContactJid.bare(),ANumber));
	}
	return false;
}

bool PhoneManager::removeContactNumber(const Jid &AStreamJid, const Jid &AContactJid, const QString &ANumber)
{
	if (isNumbersReady(AStreamJid) && AContactJid.isValid())
	{
		LOG_STRM_INFO(AStreamJid,QString("Removing phone number from contact jid=%1, number=%1").arg(AContactJid.bare(),ANumber));
		QString number = !ANumber.isNull() ? normalizedNumber(ANumber) : QString::null;
		if (!number.isNull() && FNumbers.value(AStreamJid).value(AContactJid.bare()).contains(number))
		{
			QStringList &numbers = FNumbers[AStreamJid][AContactJid.bare()];
			if (numbers.count() > 1)
				numbers.removeAll(number);
			else
				FNumbers[AStreamJid].remove(AContactJid.bare());
			startSaveNumbersToStorage(AStreamJid);
			emit numbersChanged(AStreamJid,AContactJid.bare());
			return true;
		}
		else if (number.isNull() && FNumbers.value(AStreamJid).contains(AContactJid.bare()))
		{
			FNumbers[AStreamJid].remove(AContactJid.bare());
			startSaveNumbersToStorage(AStreamJid);
			emit numbersChanged(AStreamJid,AContactJid.bare());
			return true;
		}
	}
	return false;
}

QMultiMap<int,IPhoneCallHandler *> PhoneManager::callHandlers() const
{
	return FCallHandlers;
}

void PhoneManager::insertCallHandler(int AOrder, IPhoneCallHandler *AHandler)
{
	if (AHandler && !FCallHandlers.contains(AOrder,AHandler))
	{
		LOG_DEBUG(QString("Phone call handler inserted, order=%1, handler=%2").arg(AOrder).arg((quint64)AHandler));
		FCallHandlers.insertMulti(AOrder,AHandler);
	}
}

void PhoneManager::removeCallHandler(int AOrder, IPhoneCallHandler *AHandler)
{
	if (FCallHandlers.contains(AOrder,AHandler))
	{
		LOG_DEBUG(QString("Phone call handler removed, order=%1, handler=%2").arg(AOrder).arg((quint64)AHandler));
		FCallHandlers.remove(AOrder,AHandler);
	}
}

void PhoneManager::appendCall(IPhoneCall *ACall)
{
	if (ACall && !FCalls.contains(ACall))
	{
		FCalls.append(ACall);
		connect(ACall->instance(),SIGNAL(stateChanged()),SLOT(onPhoneCallStateChanged()));
		connect(ACall->instance(),SIGNAL(mediaChanged()),SLOT(onPhoneCallMediaChanged()));
		connect(ACall->instance(),SIGNAL(callDestroyed()),SLOT(onPhoneCallDestroyed()));
		emit callCreated(ACall);
	}
}

void PhoneManager::removeCall(IPhoneCall *ACall)
{
	if (FCalls.contains(ACall))
	{
		FCalls.removeAll(ACall);
		emit callDestroyed(ACall);
	}
}

IPhoneCall *PhoneManager::findCallById(const QString &ACallId) const
{
	foreach(IPhoneCall *call, FCalls)
		if (call->callId() == ACallId)
			return call;
	return NULL;
}

bool PhoneManager::processIncomingCall(const Jid &AStreamJid, const QString &ACallId, const Jid &AContactJid, const QString &ARemoteUri, bool AWithVideo)
{
	bool callReceived = false;
	if (FStanzaProcessor && isCallsEnabled())
	{
		LOG_STRM_INFO(AStreamJid,QString("Phone call created as receiver, uri=%1, call=%2").arg(ARemoteUri,ACallId));
		PhoneCall *call = new PhoneCall(this,FSipPhone,FStanzaProcessor,AStreamJid,ACallId,AContactJid,ARemoteUri,AWithVideo,this);
		appendCall(call);

		for (QMultiMap<int,IPhoneCallHandler *>::const_iterator it=FCallHandlers.constBegin(); !callReceived && it!=FCallHandlers.constEnd(); ++it)
			callReceived = it.value()->phoneCallReceive(it.key(),call);

		if (!callReceived)
		{
			LOG_STRM_INFO(AStreamJid,QString("Incoming phone call not received, uri=%1, call=%2").arg(ARemoteUri,ACallId));
			call->hangupCall(IPhoneCall::Busy);
			call->destroyCall(0);
		}
		else
		{
			LOG_STRM_INFO(AStreamJid,QString("Incoming phone call received, uri=%1, call=%2").arg(ARemoteUri,ACallId));
		}
	}
	else if (FStanzaProcessor)
	{
		LOG_STRM_WARNING(AStreamJid,QString("Failed to process incoming call, uri=%1, call=%2: Calls is not enabled").arg(ARemoteUri,ACallId));
	}
	return callReceived;
}

void PhoneManager::showCallNotification(IPhoneCall *ACall)
{
	Jid contactJid;
	if (ACall->isNumberCall())
		contactJid = findContactByNumber(ACall->streamJid(),ACall->number());
	else if (ACall->contactJid().isValid())
		contactJid = ACall->contactJid();
	else
		contactJid = ACall->receivers().toList().value(0);

	QString userNick;
	if (contactJid.isValid())
		userNick = FMessageStyles!=NULL ? FMessageStyles->contactName(ACall->streamJid(),contactJid) : contactJid.uNode();
	else if (ACall->isNumberCall())
		userNick = formattedNumber(ACall->number());

	QString callIconId = ACall->role()==IPhoneCall::Caller ? MNI_PHONEMANAGER_CALL_OUT : MNI_PHONEMANAGER_CALL_IN;

	switch (ACall->hangupCode())
	{
	case IPhoneCall::Busy:
		showFinishedCallNotification(ACall,callIconId,tr("%1 is now talking. Call later.").arg(userNick));
		break;
	case IPhoneCall::Decline:
	case IPhoneCall::Declined:
	case IPhoneCall::RemotelyDeclined:
		if (ACall->durationTime() > 0)
		{
			showFinishedCallNotification(ACall,callIconId,tr("Call finished (duration: %1).").arg(ACall->durationString()));
		}
		else if (ACall->role()==IPhoneCall::Receiver && ACall->hangupCode()==IPhoneCall::Declined)
		{
			showMissedCallNotification(ACall);
			showFinishedCallNotification(ACall,callIconId,tr("Missed call."));
		}
		else
		{
			showFinishedCallNotification(ACall,callIconId,tr("Call canceled."));
		}
		break;
	case IPhoneCall::NotAnswered:
		if (ACall->role() == IPhoneCall::Receiver)
		{
			showMissedCallNotification(ACall);
			showFinishedCallNotification(ACall,callIconId,tr("Missed call."));
		}
		else
		{
			showFinishedCallNotification(ACall,callIconId,tr("Call not accepted."));
		}
		break;
	case IPhoneCall::RemotelyAccepted:
		showFinishedCallNotification(ACall,callIconId,tr("Call accepted."));
		break;
	case IPhoneCall::ErrorOccured:
		showFinishedCallNotification(ACall,MNI_PHONEMANAGER_CALL_ERROR,tr("Call failed. Try to call later."));
		break;
	default:
		break;
	}
}

void PhoneManager::showMissedCallNotification(IPhoneCall *ACall)
{
	if (FNotifications)
	{
		INotification notify;
		notify.kinds =  FNotifications->enabledTypeNotificationKinds(NNT_PHONEMANAGER_MISSEDCALL);
		if (notify.kinds > 0)
		{
			foreach(int notifyId, FCallNotifications.keys())
			{
				CallNotifyData data = FCallNotifications.value(notifyId);
				if (data.streamJid==ACall->streamJid())
				{
					if (!data.isNumber && !ACall->isNumberCall() && data.contactJid.pBare()==ACall->contactJid().pBare())
						FNotifications->removeNotification(notifyId);
					else if (data.isNumber && ACall->isNumberCall() && data.number==ACall->number())
						FNotifications->removeNotification(notifyId);
				}
			}

			notify.typeId = NNT_PHONEMANAGER_MISSEDCALL;
			notify.data.insert(NDR_ICON,IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->getIcon(MNI_PHONEMANAGER_CALL_IN));
			notify.data.insert(NDR_TOOLTIP,tr("Missed call from %1").arg(FNotifications->contactName(ACall->streamJid(),ACall->contactJid())));
			notify.data.insert(NDR_STREAM_JID,ACall->streamJid().full());
			notify.data.insert(NDR_CONTACT_JID,ACall->contactJid().full());
			notify.data.insert(NDR_ROSTER_ORDER,RNO_MISSEDCALL);
			notify.data.insert(NDR_ROSTER_FLAGS,IRostersNotify::Blink|IRostersNotify::AllwaysVisible|IRostersNotify::HookClicks);
			notify.data.insert(NDR_POPUP_NOTICE, tr("Missed Call"));
			notify.data.insert(NDR_POPUP_TITLE,FNotifications->contactName(ACall->streamJid(),ACall->contactJid()));
			notify.data.insert(NDR_POPUP_IMAGE,FNotifications->contactAvatar(ACall->contactJid()));
			notify.data.insert(NDR_POPUP_TIMEOUT,0);

			PhoneChatHandler *chatHandler = findChatHandler(ACall->streamJid(),ACall->contactJid());
			if (chatHandler)
			{
				notify.data.insert(NDR_ALERT_WIDGET,(qint64)chatHandler->messageWindow()->instance());
				notify.data.insert(NDR_SHOWMINIMIZED_WIDGET,(qint64)chatHandler->messageWindow()->instance());
			}

			Action *callAction = new Action(this);
			callAction->setText(tr("Call Back"));
			callAction->setIcon(RSR_STORAGE_MENUICONS,MNI_PHONEMANAGER_CALL_ANSWER);
			connect(callAction,SIGNAL(triggered()),SLOT(onCallNotificationActionTriggered()));
			notify.actions.append(callAction);

			Action *chatAction = new Action(this);
			chatAction->setText(tr("Chat"));
			connect(chatAction,SIGNAL(triggered()),SLOT(onChatNotificationActionTriggered()));
			notify.actions.append(chatAction);

			CallNotifyData data;
			data.isNumber = ACall->isNumberCall();
			data.streamJid = ACall->streamJid();
			data.contactJid = ACall->contactJid();
			data.number = ACall->number();

			int notifyId = FNotifications->appendNotification(notify);
			FCallNotifications.insert(notifyId,data);

			callAction->setData(ADR_NOTIFY_ID,notifyId);
			chatAction->setData(ADR_NOTIFY_ID,notifyId);
		}
	}
}

void PhoneManager::showFinishedCallNotification(IPhoneCall *ACall, const QString &AIconId, const QString &ANotify)
{
	PhoneChatHandler *chatHandler = findChatHandler(ACall);
	IMessageChatWindow *window = chatHandler!=NULL ? chatHandler->messageWindow() : NULL;
	if (window && window->viewWidget())
	{
		IMessageContentOptions options;
		options.kind = IMessageContentOptions::KindStatus;
		options.type |= IMessageContentOptions::TypeNotification;
		options.direction = IMessageContentOptions::DirectionIn;
		options.time = QDateTime::currentDateTime();
		options.timeFormat = FMessageStyles!=NULL ? FMessageStyles->timeFormat(options.time) : QString::null;

		QUrl iconFile = QUrl::fromLocalFile(IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->fileFullName(AIconId));
		QString html = QString("<span><img src='%1'/> %2</span>").arg(iconFile.toString()).arg(Qt::escape(ANotify));

		window->viewWidget()->appendHtml(html,options);
	}
}

void PhoneManager::startSaveNumbersToStorage(const Jid &AStreamJid)
{
	if (FPrivateStorage)
	{
		FSaveNumbersStreams += AStreamJid;
		FSaveNumbersTimer.start(STORAGE_SAVE_TIMEOUT);
	}
}

bool PhoneManager::saveNumbersToStorage(const Jid &AStreamJid)
{
	if (FPrivateStorage && isNumbersReady(AStreamJid))
	{
		QDomDocument doc;
		QDomElement itemsElem = doc.appendChild(doc.createElementNS(NS_STORAGE_PHONENUMBERS,"storage")).toElement();
		saveNumbersToXml(itemsElem,streamNumbers(AStreamJid));
		if (!FPrivateStorage->saveData(AStreamJid,itemsElem).isEmpty())
		{
			LOG_STRM_DEBUG(AStreamJid,"Save phone numbers to storage request sent");
			return true;
		}
		else
		{
			LOG_STRM_ERROR(AStreamJid,"Failed to send save phone numbers to storage request");
		}
	}
	else if (FPrivateStorage)
	{
		LOG_STRM_ERROR(AStreamJid,"Failed to save phone numbers to storage: Stream is not ready");
	}
	return false;
}

QString PhoneManager::phoneStreamFilePath(const Jid &AStreamJid, const QString &AFileName) const
{
	bool noError = true;

	QDir dir(FPluginManager->homePath());
	if (!dir.exists(DIR_PHONE))
		noError &= dir.mkdir(DIR_PHONE);
	noError &= dir.cd(DIR_PHONE);

	QString streamDir = Jid::encode(AStreamJid.pBare());
	if (!dir.exists(streamDir))
		noError &= dir.mkdir(streamDir);
	noError &= dir.cd(streamDir);

	return noError ? dir.absoluteFilePath(AFileName) : QString::null;
}

QHash<Jid, QStringList> PhoneManager::loadNumbersFromXml(const QDomElement &AElement) const
{
	QHash<Jid, QStringList> numbers;
	QDomElement itemElem = AElement.firstChildElement("item");
	while (!itemElem.isNull())
	{
		QStringList itemNumbers;
		Jid itemJid = itemElem.attribute("jid");

		QDomElement numberElem = itemElem.firstChildElement("number");
		while (!numberElem.isNull())
		{
			itemNumbers.append(numberElem.text());
			numberElem = numberElem.nextSiblingElement("number");
		}

		if (itemJid.isValid() && !itemNumbers.isEmpty())
			numbers.insert(itemJid,itemNumbers);

		itemElem = itemElem.nextSiblingElement("item");
	}
	return numbers;
}

void PhoneManager::saveNumbersToXml(QDomElement &AElement, const QHash<Jid, QStringList> &ANumbers) const
{
	for (QHash<Jid, QStringList>::const_iterator it=ANumbers.constBegin(); it!=ANumbers.constEnd(); ++it)
	{
		if (!it->isEmpty())
		{
			QDomElement itemElem = AElement.appendChild(AElement.ownerDocument().createElement("item")).toElement();
			itemElem.setAttribute("jid",it.key().pBare());
			foreach(const QString &number, it.value())
			{
				QDomElement numberElem = itemElem.appendChild(itemElem.ownerDocument().createElement("number")).toElement();
				numberElem.appendChild(numberElem.ownerDocument().createTextNode(number));
			}
		}
	}
}

QHash<Jid, QStringList> PhoneManager::loadNumbersFromFile(const QString &AFileName) const
{
	QHash<Jid, QStringList> numbers;

	QFile file(AFileName);
	if (file.open(QIODevice::ReadOnly))
	{
		QString xmlError;
		QDomDocument doc;
		if (doc.setContent(&file,true,&xmlError))
		{
			numbers = loadNumbersFromXml(doc.documentElement());
		}
		else
		{
			REPORT_ERROR(QString("Failed to load phone numbers from file content: %1").arg(xmlError));
			file.remove();
		}
	}
	else if (file.exists())
	{
		REPORT_ERROR(QString("Failed to load phone numbers from file: %1").arg(file.errorString()));
	}
	return numbers;
}

void PhoneManager::saveNumbersToFile(const QString &AFileName, const QHash<Jid, QStringList> &ANumbers) const
{
	QFile file(AFileName);
	if (file.open(QIODevice::WriteOnly|QIODevice::Truncate))
	{
		QDomDocument doc;
		QDomElement itemsElem = doc.appendChild(doc.createElementNS(NS_STORAGE_PHONENUMBERS,"storage")).toElement();
		saveNumbersToXml(itemsElem,ANumbers);
		file.write(doc.toByteArray());
		file.close();
	}
	else
	{
		REPORT_ERROR(QString("Failed to save phone numbers to file: %1").arg(file.errorString()));
	}
}

void PhoneManager::updateStreamNumbers(const Jid &AStreamJid, const QHash<Jid, QStringList> &ANumbers)
{
	if (FNumbers.contains(AStreamJid))
	{
		QSet<Jid> changedItems;
		QSet<Jid> removeItems = FNumbers.value(AStreamJid).keys().toSet();

		QHash<Jid, QStringList> &numbers = FNumbers[AStreamJid];
		for (QHash<Jid, QStringList>::const_iterator it=ANumbers.constBegin(); it!=ANumbers.constEnd(); ++it)
		{
			if (!it->isEmpty())
			{
				if (numbers.value(it.key()) != it.value())
				{
					changedItems += it.key();
					numbers.insert(it.key(),it.value());
				}
				removeItems -= it.key();
			}
		}

		foreach(const Jid &itemJid, removeItems)
			numbers.remove(itemJid);

		foreach(const Jid &itemJid, changedItems+removeItems)
			emit numbersChanged(AStreamJid,itemJid);
	}
}

PhoneChatHandler *PhoneManager::findChatHandler(IPhoneCall *ACall) const
{
	if (ACall != NULL)
	{
		foreach(PhoneChatHandler *handler, FChatHandlers)
			if (handler->phoneCall() == ACall)
				return handler;
	}
	return NULL;
}

PhoneChatHandler *PhoneManager::findChatHandler(const Jid &AStreamJid, const Jid &AContactJid) const
{
	foreach(PhoneChatHandler *handler, FChatHandlers)
	{
		QMultiMap<Jid,Jid> addressesMap = handler->messageWindow()->address()->availAddresses(AContactJid.resource().isEmpty()); 
		if (addressesMap.contains(AStreamJid,AContactJid))
			return handler;
	}
	return NULL;
}

PhoneChatHandler *PhoneManager::getChatHandler(const Jid &AStreamJid, const Jid &AContactJid)
{
	PhoneChatHandler *handler = findChatHandler(AStreamJid,AContactJid);
	if (handler == NULL)
	{
		if (FMessageProcessor->createMessageWindow(AStreamJid,AContactJid,Message::Chat,IMessageHandler::SM_ASSIGN))
			handler = findChatHandler(AStreamJid,AContactJid);
	}
	return handler;
}

void PhoneManager::onRosterAdded(IRoster *ARoster)
{
	CallHistoryTaskOpenDatabase *task = new CallHistoryTaskOpenDatabase(ARoster->streamJid(),phoneStreamFilePath(ARoster->streamJid(),FILE_CALLSHISTORY));
	if (FCallHistoryWorker->startTask(task))
	{
		LOG_STRM_DEBUG(ARoster->streamJid(),"Phone calls history database open task started");
		FPluginManager->delayShutdown();
	}
	else
	{
		LOG_STRM_ERROR(ARoster->streamJid(),"Failed to start phone calls history database open task");
	}

	FNumbers.insert(ARoster->streamJid(),loadNumbersFromFile(phoneStreamFilePath(ARoster->streamJid(),FILE_NUMBERS)));
	emit numbersStreamsChanged();
}

void PhoneManager::onRosterOpened(IRoster *ARoster)
{
	FAvailCallStreams.append(ARoster->streamJid());
	if (FAvailCallStreams.count() == 1)
	{
		if (FSipPhone && !FSipPhone->availAccounts().isEmpty())
		{
			LOG_INFO("Phone calls enabled");
			emit callsEnableChanged();
		}
	}

	emit numbersReadyChanged(ARoster->streamJid(),true);
}

void PhoneManager::onRosterClosed(IRoster *ARoster)
{
	FAvailCallStreams.removeAll(ARoster->streamJid());
	if (FAvailCallStreams.count() == 0)
	{
		if (FSipPhone && !FSipPhone->availAccounts().isEmpty())
		{
			LOG_INFO("Phone calls disabled");
			emit callsEnableChanged();
		}
	}

	emit numbersReadyChanged(ARoster->streamJid(),false);
}

void PhoneManager::onRosterRemoved(IRoster *ARoster)
{
	CallHistoryTaskCloseDatabase *task = new CallHistoryTaskCloseDatabase(ARoster->streamJid());
	if (FCallHistoryWorker->startTask(task))
	{
		LOG_STRM_DEBUG(ARoster->streamJid(),"Phone calls history database close task started");
		FPluginManager->delayShutdown();
	}
	else
	{
		LOG_STRM_ERROR(ARoster->streamJid(),"Failed to start phone calls history database close task");
	}

	saveNumbersToFile(phoneStreamFilePath(ARoster->streamJid(),FILE_NUMBERS),FNumbers.take(ARoster->streamJid()));
	emit numbersStreamsChanged();
}

void PhoneManager::onRosterStreamJidChanged(IRoster *ARoster, const Jid &ABefore)
{
	QHash<Jid, QStringList> numbers = FNumbers.take(ABefore);
	FNumbers.insert(ARoster->streamJid(),numbers);
	emit numbersStreamsChanged();
}

void PhoneManager::onRosterItemReceived(IRoster *ARoster, const IRosterItem &AItem, const IRosterItem &ABefore)
{
	Q_UNUSED(ABefore);
	if (AItem.subscription == SUBSCRIPTION_REMOVE)
		removeContactNumber(ARoster->streamJid(),AItem.itemJid);
}

void PhoneManager::onPrivateStorateOpened(const Jid &AStreamJid)
{
	if (!FPrivateStorage->loadData(AStreamJid,"storage",NS_STORAGE_PHONENUMBERS).isEmpty())
		LOG_STRM_INFO(AStreamJid,"Phone numbers load request sent");
	else
		LOG_STRM_WARNING(AStreamJid,"Failed to send load phone numbers request");
}

void PhoneManager::onPrivateDataLoaded(const QString &AId, const Jid &AStreamJid, const QDomElement &AElement)
{
	Q_UNUSED(AId);
	if (AElement.namespaceURI() == NS_STORAGE_PHONENUMBERS)
	{
		LOG_STRM_INFO(AStreamJid,"Phone numbers loaded from storage");
		updateStreamNumbers(AStreamJid,loadNumbersFromXml(AElement));
	}
}

void PhoneManager::onPrivateDataChanged(const Jid &AStreamJid, const QString &ATagName, const QString &ANamespace)
{
	Q_UNUSED(ATagName);
	if (ANamespace == NS_STORAGE_PHONENUMBERS)
	{
		if (!FPrivateStorage->loadData(AStreamJid,"storage",NS_STORAGE_PHONENUMBERS).isEmpty())
			LOG_STRM_INFO(AStreamJid,"Phone numbers reload request sent");
		else
			LOG_STRM_WARNING(AStreamJid,"Failed to send reload phone numbers request");
	}
}

void PhoneManager::onMessageChatWindowActivated()
{
	IMessageChatWindow *window = qobject_cast<IMessageChatWindow *>(sender());
	if (window)
	{
		for (QMap<int,CallNotifyData>::const_iterator it=FCallNotifications.constBegin(); it!=FCallNotifications.constEnd(); ++it)
		{
			Jid contactJid = !it->isNumber ? it->contactJid : findContactByNumber(it->streamJid,it->number);
			PhoneChatHandler *chatHandler = findChatHandler(it->streamJid,contactJid);
			if (chatHandler && chatHandler->messageWindow()==window)
			{
				FNotifications->removeNotification(it.key());
				break;
			}
		}
	}
}

void PhoneManager::onMessageChatWindowCreated(IMessageChatWindow *AWindow)
{
	PhoneChatHandler *handler = new PhoneChatHandler(this,AWindow);
	connect(handler,SIGNAL(showCallNotification(IPhoneCall *)),SLOT(onShowCallNotification(IPhoneCall *)));
	FChatHandlers.insert(AWindow,handler);

	connect(AWindow->instance(),SIGNAL(tabPageActivated()),SLOT(onMessageChatWindowActivated()));
}

void PhoneManager::onMessageChatWindowDestroyed(IMessageChatWindow *AWindow)
{
	FChatHandlers.remove(AWindow);
}

void PhoneManager::onPhoneCallDestroyed()
{
	IPhoneCall *call = qobject_cast<PhoneCall *>(sender());
	if (call)
	{
		LOG_STRM_INFO(call->streamJid(),QString("Phone call destroyed, call=%1").arg(call->callId()));
		removeCall(call);
	}
}

void PhoneManager::onPhoneCallStateChanged()
{
	IPhoneCall *call = qobject_cast<PhoneCall *>(sender());
	if (call)
		emit callStateChanged(call);
}

void PhoneManager::onPhoneCallMediaChanged()
{
	IPhoneCall *call = qobject_cast<PhoneCall *>(sender());
	if (call)
		emit callMediaChanged(call);
}

void PhoneManager::onSaveNumbersTimerTimeout()
{
	for (QSet<Jid>::iterator it=FSaveNumbersStreams.begin(); it!=FSaveNumbersStreams.end(); )
	{
		if (saveNumbersToStorage(*it))
			it = FSaveNumbersStreams.erase(it);
		else
			it++;
	}
}

void PhoneManager::onShowDialerDialogActionTriggered()
{
	showDialerDialog();
}

void PhoneManager::onShowCallNotification(IPhoneCall *ACall)
{
	showCallNotification(ACall);
}

void PhoneManager::onCallHistoryTaskFinished(CallHistoryTask *ATask)
{
	if (!ATask->hasError())
	{
		switch(ATask->type())
		{
		case CallHistoryTask::OpenDatabase:
			{
				CallHistoryTaskOpenDatabase *task = static_cast<CallHistoryTaskOpenDatabase *>(ATask);
				FPluginManager->continueShutdown();
				FCallHistoryProperties.insert(task->streamJid().pBare(),task->databaseProperties());
				LOG_STRM_DEBUG(task->streamJid(),QString("Phone call history database opened, id=%1").arg(task->taskId()));
			}
			break;
		case CallHistoryTask::CloseDatabase:
			{
				CallHistoryTaskCloseDatabase *task = static_cast<CallHistoryTaskCloseDatabase *>(ATask);
				FPluginManager->continueShutdown();
				FCallHistoryProperties.remove(task->streamJid().pBare());
				LOG_STRM_DEBUG(task->streamJid(),QString("Phone call history database closed, id=%1").arg(task->taskId()));
			}
			break;
		case CallHistoryTask::SaveHistoryItem:
			{
				CallHistoryTaskSaveItem *task = static_cast<CallHistoryTaskSaveItem *>(ATask);
				LOG_STRM_DEBUG(task->streamJid(),QString("Phone call history item saved, id=%1").arg(task->taskId()));
				emit callHistoryItemSaved(task->taskId(),task->savedItem());
			}
			break;
		case CallHistoryTask::LoadHistoryItems:
			{
				CallHistoryTaskLoadItems *task =static_cast<CallHistoryTaskLoadItems *>(ATask);
				LOG_STRM_DEBUG(task->streamJid(),QString("Phone call history items loaded, id=%1").arg(task->taskId()));
				emit callHistoryItemsLoaded(task->taskId(),task->loadedItems());
			}
			break;
		case CallHistoryTask::RemoveHistoryItems:
			{
				CallHistoryTaskRemoveItems *task = static_cast<CallHistoryTaskRemoveItems *>(ATask);
				LOG_STRM_DEBUG(task->streamJid(),QString("Phone call history items removed, id=%1").arg(task->taskId()));
				emit callHistoryItemsRemoved(task->taskId(),task->removeRequest());
			}
			break;
		}
	}
	else
	{
		LOG_STRM_ERROR(ATask->streamJid(),QString("Failed to execute phone call history task, id=%1: %2").arg(ATask->taskId(),ATask->error().errorMessage()));
		emit callHistoryRequestFailed(ATask->taskId(),ATask->error());
	}
	delete ATask;
}

void PhoneManager::onNotificationActivated(int ANotifyId)
{
	if (FCallNotifications.contains(ANotifyId))
	{
		CallNotifyData data = FCallNotifications.value(ANotifyId);

		PhoneChatHandler *chatHandler = findChatHandler(data.streamJid,data.contactJid);
		if (chatHandler)
			chatHandler->messageWindow()->showTabPage();

		FNotifications->removeNotification(ANotifyId);
	}
}

void PhoneManager::onNotificationRemoved(int ANotifyId)
{
	FCallNotifications.remove(ANotifyId);
}

void PhoneManager::onCallNotificationActionTriggered()
{
	Action *action = qobject_cast<Action *>(sender());
	int notifyId = action!=NULL ? action->data(ADR_NOTIFY_ID).toInt() : -1;
	if (FCallNotifications.contains(notifyId))
	{
		CallNotifyData data = FCallNotifications.value(notifyId);

		bool calling = false;
		IPhoneCall *call = !data.isNumber ? newCall(data.streamJid,QSet<Jid>()<<data.contactJid) : newCall(data.streamJid,data.number);
		if (call)
		{
			if (showCallWindow(call))
			{
				call->startCall();
				calling = true;
			}
			else
			{
				call->destroyCall(0);
			}
		}

		if (calling)
			FNotifications->removeNotification(notifyId);
		else
			FNotifications->activateNotification(notifyId);
	}
}

void PhoneManager::onChatNotificationActionTriggered()
{
	Action *action = qobject_cast<Action *>(sender());
	int notifyId = action!=NULL ? action->data(ADR_NOTIFY_ID).toInt() : -1;
	if (FCallNotifications.contains(notifyId))
		FNotifications->activateNotification(notifyId);
}

void PhoneManager::onUpdateDefaultSipAccount()
{
	if (FSipPhone)
	{
		ISipAccountConfig config;
		config.userid = Options::node(OPV_PHONEMANAGER_SIP_LOGIN).value().toString();
		config.password = Options::decrypt(Options::node(OPV_PHONEMANAGER_SIP_PASSWORD).value().toByteArray()).toString();
		config.serverHost = Options::node(OPV_PHONEMANAGER_SIP_SERVERHOST).value().toString();
		config.serverPort = Options::node(OPV_PHONEMANAGER_SIP_SERVERPORT).value().toUInt();
		config.proxyHost = Options::node(OPV_PHONEMANAGER_SIP_PROXYHOST).value().toString();
		config.proxyPort = Options::node(OPV_PHONEMANAGER_SIP_PROXYPORT).value().toUInt();

		LOG_INFO(QString("Updating default SIP account, id=%1").arg(defaultSipAccountId().toString()));

		if (FSipPhone->availAccounts().contains(defaultSipAccountId()))
		{
			if (config.userid.isEmpty() || !FSipPhone->updateAccount(defaultSipAccountId(),config))
				FSipPhone->removeAccount(defaultSipAccountId());
		}
		else if (!config.userid.isEmpty())
		{
			FSipPhone->insertAccount(defaultSipAccountId(),config);
		}
	}
}

void PhoneManager::onSipAccountInserted(const QUuid &AAccountId)
{
	Q_UNUSED(AAccountId);
	if (FSipPhone->availAccounts().count() == 1)
	{
		if (!FAvailCallStreams.isEmpty())
		{
			LOG_INFO("Phone calls enabled");
			emit callsEnableChanged();
		}
	}
}

void PhoneManager::onSipAccountRemoved(const QUuid &AAccountId)
{
	Q_UNUSED(AAccountId);
	if (FSipPhone->availAccounts().count() == 0)
	{
		if (!FAvailCallStreams.isEmpty())
		{
			LOG_INFO("Phone calls disabled");
			emit callsEnableChanged();
		}
	}
}

void PhoneManager::onOptionsOpened()
{
	onOptionsChanged(Options::node(OPV_PHONEMANAGER_SIP_LOGIN));
}

void PhoneManager::onOptionsClosed()
{
	if (FSipPhone)
		FSipPhone->removeAccount(defaultSipAccountId());
}

void PhoneManager::onOptionsChanged(const OptionsNode &ANode)
{
	if (FSipPhone && Options::node(OPV_PHONEMANAGER_SIP_ROOT).isChildNode(ANode))
		FSipAccountTimer.start(100);
}

Q_EXPORT_PLUGIN2(plg_phonemanager, PhoneManager)
