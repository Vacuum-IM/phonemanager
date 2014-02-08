#include "incomingcalldialog.h"

#include <QFile>
#include <definitions/resources.h>
#include <definitions/phonemanager/soundfiles.h>
#include <definitions/phonemanager/menuicons.h>
#include <utils/iconstorage.h>
#include <utils/filestorage.h>
#include <utils/logger.h>
#include <utils/menu.h>

static const QString IncomingCallDialogStyleSheet =
"QLabel#lblName                                                         "
"{                                                                      "
"  font-size: 16px;                                                     "
"}                                                                      "
"QLabel#lblInfo                                                         "
"{                                                                      "
"  color: gray;                                                         "
"}                                                                      "
"QToolButton#tlbAccept, QToolButton#tlbAcceptVideo                      "
"{                                                                      "
"  color: white;                                                        "
"  min-height: 26px;                                                    "
"  border-width: 4px;                                                   "
"  border-image: url(%1);                                               "
"}                                                                      "
"QToolButton#tlbAccept:hover, QToolButton#tlbAcceptVideo:hover          "
"{                                                                      "
"  border-image: url(%2);                                               "
"}                                                                      "
"QToolButton#tlbAccept:pressed, QToolButton#tlbAcceptVideo:pressed      "
"{                                                                      "
"  border-image: url(%3);                                               "
"}                                                                      "
"QToolButton#tlbReject                                                  "
"{                                                                      "
"  color: white;                                                        "
"  min-height: 26px;                                                    "
"  border-width: 4px;                                                   "
"  border-image: url(%4);                                               "
"}                                                                      "
"QToolButton#tlbReject:hover                                            "
"{                                                                      "
"  border-image: url(%5);                                               "
"}                                                                      "
"QToolButton#tlbReject:pressed                                          "
"{                                                                      "
"  border-image: url(%6);                                               "
"}                                                                      "
;

IncomingCallDialog::IncomingCallDialog(IPhoneManager *APhoneManager, IPhoneCall *ACall, QWidget *AParent) : QDialog(AParent,Qt::WindowStaysOnTopHint)
{
	REPORT_VIEW;
	ui.setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose,true);

	setStyleSheet(IncomingCallDialogStyleSheet
		.arg(IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->fileFullName(MNI_PHONEMANAGER_BUTTON_ACCEPT))
		.arg(IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->fileFullName(MNI_PHONEMANAGER_BUTTON_ACCEPT_HOVER))
		.arg(IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->fileFullName(MNI_PHONEMANAGER_BUTTON_ACCEPT_PRESSED))
		.arg(IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->fileFullName(MNI_PHONEMANAGER_BUTTON_REJECT))
		.arg(IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->fileFullName(MNI_PHONEMANAGER_BUTTON_REJECT_HOVER))
		.arg(IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->fileFullName(MNI_PHONEMANAGER_BUTTON_REJECT_PRESSED)));

	FSound = NULL;
	FPhoneManager = APhoneManager;

	FCall = ACall;
	connect(FCall->instance(),SIGNAL(stateChanged()),SLOT(onCallStateChanged()));

	ui.lblInfo->setText(tr("Incoming Call"));
	ui.lblName->setText(FPhoneManager->callContactName(FCall));
	ui.lblAvatar->setPixmap(QPixmap::fromImage(FPhoneManager->callContactAvatarImage(FCall)));

	Action *audioCallAction = new Action(ui.tlbAccept);
	audioCallAction->setText(tr("Answer"));
	audioCallAction->setIcon(RSR_STORAGE_MENUICONS,MNI_PHONEMANAGER_MICROPHONE_ENABLED);
	connect(audioCallAction,SIGNAL(triggered()),SLOT(onAudioCallActionTriggered()));
	ui.tlbAccept->setDefaultAction(audioCallAction);

	if (FCall->isVideoCallRequested() && FCall->mediaProperty(IPhoneCall::VideoCapture,IPhoneCall::Avail).toBool())
	{
		Action *videoCallAction = new Action(ui.tlbAcceptVideo);
		videoCallAction->setText(tr("Answer with Video"));
		videoCallAction->setIcon(RSR_STORAGE_MENUICONS,MNI_PHONEMANAGER_CAMERA_ENABLED);
		connect(videoCallAction,SIGNAL(triggered()),SLOT(onVideoCallActionTriggered()));
		ui.tlbAcceptVideo->setDefaultAction(videoCallAction);
	}
	else
	{
		ui.tlbAcceptVideo->setVisible(false);
	}

	connect(ui.tlbReject,SIGNAL(clicked()),SLOT(onRejectButtonClicked()));
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.tlbReject,MNI_PHONEMANAGER_CALL_REJECT);

	onCallStateChanged();
	playSound(SDF_PHONEMANAGER_CALL_RING_IN, -1);
	setResult(IncomingCallDialog::Rejected);
}

IncomingCallDialog::~IncomingCallDialog()
{
	playSound(QString::null);
	emit dialogDestroyed();
}

IPhoneCall *IncomingCallDialog::call() const
{
	return FCall;
}

void IncomingCallDialog::playSound(const QString &ASoundKey, int ALoops)
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

void IncomingCallDialog::onCallStateChanged()
{
	if (FCall->state() > IPhoneCall::Ringing)
		done(IncomingCallDialog::CallStateChanged);
}

void IncomingCallDialog::onAudioCallActionTriggered()
{
	done(IncomingCallDialog::AcceptedNoVideo);
}

void IncomingCallDialog::onVideoCallActionTriggered()
{
	done(IncomingCallDialog::AcceptedWithVideo);
}

void IncomingCallDialog::onRejectButtonClicked()
{
	done(IncomingCallDialog::Rejected);
}
