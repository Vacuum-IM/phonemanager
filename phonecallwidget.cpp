#include "phonecallwidget.h"

#include <QFile>
#include <definitions/resources.h>
#include <definitions/phonemanager/menuicons.h>
#include <definitions/phonemanager/soundfiles.h>
#include <utils/filestorage.h>
#include <utils/iconstorage.h>

static const QString PhoneCallWidgetStyleSheet = 
"QFrame                                          "
"{                                                "
"  background-color: transparent;                 "
"}                                                "
"QFrame#frmDialerLayer                            "
"{                                                "
"  background-color: rgb(231,234,236);            "
"}                                                " 
"QLabel                                           "
"{                                                "
"  color: lightgray;                              "
"}                                                "
"PhoneCallWidget                                  "
"{                                                "
"  background-repeat: repeat-xy;                  "
"  background-image: url(%1);                     "
"}                                                "
"CallControlWidget                                "
"{                                                "
"  background-color: rgba(22, 22, 22, 100);       "
"}                                                "
"CallControlWidget QToolButton                    "
"{                                                "
"  padding: 0px;                                  "
"  border-image: none;                            "
"  border: 3px solid transparent;                 "
"  background-color: transparent;                 "
"}                                                "
"CallControlWidget QToolButton:hover:enabled      "
"{                                                "
"  border-image: url(%2);                         "
"}                                                "
"CallControlWidget QToolButton:pressed:enabled    "
"{                                                "
"  border-image: url(%3);                         "
"}                                                "
"CallControlWidget QSlider::handle                "
"{                                                "
"  image: url(%4);                                "
"}                                                "
"CallControlWidget QSlider#sdrSpeakerVolume       "
"{                                                "
"  max-width: 100px;                              "
"}                                                "
"CallInfoWidget QLabel#lblName                    "
"{                                                "
"  font-size: 16px;                               "
"  font-weight: 100;                              "
"}                                                "
"CallInfoWidget QLabel#lblError                   "
"{                                                "
"  min-height: 30px;                              "
"  border-bottom: 1px solid darkred;              "
"  background-color: rgba(22, 22, 22, 100);       "
"}                                                "
;

PhoneCallWidget::PhoneCallWidget(IPhoneManager *APhoneManager, IPhoneCall *ACall, QWidget *AParent) : QFrame(AParent)
{
	setFrameShape(StyledPanel);

	setStyleSheet(PhoneCallWidgetStyleSheet
		.arg(IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->fileFullName(MNI_PHONEMANAGER_BACKGROUND))
		.arg(IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->fileFullName(MNI_PHONEMANAGER_BUTTON_HOVER))
		.arg(IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->fileFullName(MNI_PHONEMANAGER_BUTTON_PRESSED))
		.arg(IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->fileFullName(MNI_PHONEMANAGER_VOLUME_HANDLE)));

	FSound = NULL;
	FLocalVideo = NULL;
	FRemoteVideo = NULL;
	FDialerWidgetLayer = NULL;
	FPhoneManager = APhoneManager;

	FCall = ACall;
	connect(FCall->instance(),SIGNAL(stateChanged()),SLOT(onCallStateChanged()));
	connect(FCall->instance(),SIGNAL(callDestroyed()),SLOT(onCallDestroyed()));

	FLayout = new PhoneCallLayout(this);
	setLayout(FLayout);

	FInfoWidget = new CallInfoWidget(this);
	FInfoWidget->setAvatar(FPhoneManager->callContactAvatarImage(FCall));
	FInfoWidget->setName(!FCall->isNumberCall() ? FPhoneManager->callContactName(FCall) : FPhoneManager->formattedNumber(FCall->number()));
	FLayout->setInfoWidget(FInfoWidget);

	FControlWidget = new CallControlWidget(ACall,this);
	connect(FControlWidget,SIGNAL(dialerEnableChanged(bool)),SLOT(onDialerEnableChanged(bool)));
	connect(FControlWidget,SIGNAL(previewEnableChanged(bool)),SLOT(onVideoPreviewEnableChanged(bool)));
	connect(FControlWidget,SIGNAL(mediaEnableChanged(IPhoneCall::Media,bool)),SLOT(onMediaEnableChanged(IPhoneCall::Media,bool)));
	FLayout->setControlWidget(FControlWidget);

	onCallStateChanged();
	onDialerEnableChanged(FControlWidget->isDialerEnabled());
	onVideoPreviewEnableChanged(FControlWidget->isPreviewEnabled());
	onMediaEnableChanged(IPhoneCall::VideoPlayback,FControlWidget->isMediaEnabled(IPhoneCall::VideoPlayback));
}

PhoneCallWidget::~PhoneCallWidget()
{
	if (FCall)
		FCall->destroyCall(0);
	playSound(QString::null);
	emit widgetDestroyed();
}

IPhoneCall *PhoneCallWidget::call() const
{
	return FCall;
}

void PhoneCallWidget::destroyWidgetLater(int ATimeout)
{
	FPhoneManager->saveCallHistoryItem(FCall);

	FInfoWidget->setAvatar(FPhoneManager->callContactAvatarImage(FCall,QSize(),true));

	FControlWidget->setPreviewEnabled(false);
	FControlWidget->setMediaEnabled(IPhoneCall::VideoPlayback,false);

	QTimer::singleShot(ATimeout,this,SLOT(deleteLater()));
}

void PhoneCallWidget::playSound(const QString &ASoundKey, int ALoops)
{
	QString soundFile = FileStorage::staticStorage(RSR_STORAGE_SOUNDS)->fileFullName(ASoundKey);
	if (FSound==NULL || FSound->isFinished() || FSound->fileName()!=soundFile)
	{
		if(FSound)
		{
			FSound->stop();
			delete FSound;
			FSound = NULL;
		}
		if (!soundFile.isEmpty() && QFile::exists(soundFile) && QSound::isAvailable())
		{
			FSound = new QSound(soundFile);
			FSound->setLoops(ALoops);
			FSound->play();
		}
	}
}

void PhoneCallWidget::onCallStateChanged()
{
	switch (FCall->state())
	{
	case IPhoneCall::Inited:
	case IPhoneCall::Calling:
		FInfoWidget->setInfo(tr("Calling..."));
		playSound(SDF_PHONEMANAGER_CALL_RING_OUT,-1);
		break;
	case IPhoneCall::Ringing:
		if (FCall->role() == IPhoneCall::Receiver)
			playSound(SDF_PHONEMANAGER_CALL_RING_IN,-1);
		else if (FCall->isNumberCall())
			playSound(QString::null);
		break;
	case IPhoneCall::Connecting:
		FInfoWidget->setInfo(tr("Connecting..."));
		playSound(QString::null);
		break;
	case IPhoneCall::Confirmed:
		FInfoWidget->setInfo(QString::null);
		playSound(QString::null);
		break;
	case IPhoneCall::Disconnected:
		switch (FCall->hangupCode())
		{
		case IPhoneCall::Busy:
			FInfoWidget->setInfo(tr("Caller is busy."));
			playSound(SDF_PHONEMANAGER_CALL_RING_BUSY,5);
			break;
		case IPhoneCall::Declined:
			if (FCall->durationTime() > 0)
				FInfoWidget->setInfo(tr("Call finished."));
			else
				FInfoWidget->setInfo(tr("Call declined."));
			playSound(QString::null);
			break;
		case IPhoneCall::NotAnswered:
			FInfoWidget->setInfo(tr("Call not accepted."));
			playSound(QString::null);
			break;
		case IPhoneCall::RemotelyAccepted:
			FInfoWidget->setInfo(tr("Call accepted."));
			playSound(QString::null);
			break;
		case IPhoneCall::RemotelyDeclined:
			FInfoWidget->setInfo(tr("Call declined."));
			playSound(QString::null);
			break;
		default:
			FInfoWidget->setInfo(tr("Call finished."));
			playSound(QString::null);
		}
		destroyWidgetLater(2000);
		break;
	case IPhoneCall::Aborted:
		FInfoWidget->setInfo(tr("Call failed. Try to call later."));
		FInfoWidget->setError(FCall->hangupText());
		playSound(QString::null);
		destroyWidgetLater(5000);
		break;
	}
}

void PhoneCallWidget::onCallDestroyed()
{
	FCall = NULL;
	deleteLater();
}

void PhoneCallWidget::onDialerButtonClicked(const QChar &AChar)
{
	if (FCall)
	{
		char digits[] = { AChar.toAscii(), '\0' };
		FCall->sendDtmf(digits);
	}
}

void PhoneCallWidget::onDialerEnableChanged(bool AEnabled)
{
	if (AEnabled && FDialerWidgetLayer==NULL)
	{
		FDialerWidgetLayer = new QFrame(this);
		FDialerWidgetLayer->setObjectName("frmDialerLayer");

		DialerWidget *dialer = new DialerWidget(FDialerWidgetLayer);
		dialer->setTextInputEnabled(false);
		connect(dialer,SIGNAL(numberClicked(const QChar &)),SLOT(onDialerButtonClicked(const QChar &)));

		QVBoxLayout *dialerLayout = new QVBoxLayout(FDialerWidgetLayer);
		dialerLayout->setContentsMargins(20,5,20,5);
		dialerLayout->addStretch();
		dialerLayout->addWidget(dialer);
		dialerLayout->addStretch();

		FLayout->setDialerWidget(FDialerWidgetLayer);
	}
	else if (!AEnabled && FDialerWidgetLayer!=NULL)
	{
		FLayout->setDialerWidget(NULL);
		FDialerWidgetLayer->deleteLater();
		FDialerWidgetLayer = NULL;
	}
}

void PhoneCallWidget::onVideoPreviewEnableChanged(bool AEnabled)
{
	if (FCall && AEnabled && FLocalVideo==NULL)
	{
		FLocalVideo = new VideoFrame(this);
		FLocalVideo->setResizeEnabled(true);
		FLocalVideo->setMoveEnabled(true);
		FLocalVideo->setFrameShape(QLabel::Box);
		FLocalVideo->setVideoWidget(FCall->getVideoPreviewWidget(FLocalVideo));
		FLayout->setLocalVideoFrame(FLocalVideo);
	}
	else if (!AEnabled && FLocalVideo!=NULL)
	{
		FLayout->setLocalVideoFrame(NULL);
		FLocalVideo->deleteLater();
		FLocalVideo = NULL;
	}
}

void PhoneCallWidget::onMediaEnableChanged(IPhoneCall::Media AMedia, bool AEnabled)
{
	if (AMedia == IPhoneCall::VideoPlayback)
	{
		if (FCall && AEnabled && FRemoteVideo==NULL)
		{
			FRemoteVideo = new VideoFrame(this);
			FRemoteVideo->setVideoWidget(FCall->getVideoPlaybackWidget(FRemoteVideo));
			FLayout->setRemoteVideoFrame(FRemoteVideo);

			FLayout->setInfoWidget(NULL);
		}
		else if (!AEnabled && FRemoteVideo!=NULL)
		{
			FLayout->setRemoteVideoFrame(NULL);
			FRemoteVideo->deleteLater();
			FRemoteVideo = NULL;

			FLayout->setInfoWidget(FInfoWidget);
		}
	}
}
