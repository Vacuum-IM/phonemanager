#include "callcontrolwidget.h"

#include <QTime>
#include <definitions/resources.h>
#include <definitions/phonemanager/menuicons.h>
#include <utils/iconstorage.h>

CallControlWidget::CallControlWidget(IPhoneCall *ACall, QWidget *AParent) : QFrame(AParent)
{
	ui.setupUi(this);
	
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.tlbReject,MNI_PHONEMANAGER_CALL_HANGUP);
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.lblTimeIcon,MNI_PHONEMANAGER_CALL_TIMER,0,0,"pixmap");

	FVideoPreviewEnabled = true;
	FVideoPlaybackEnabled = false;

	FCall = ACall;
	connect(FCall->instance(),SIGNAL(stateChanged()),SLOT(onCallStateChanged()));
	connect(FCall->instance(),SIGNAL(mediaChanged()),SLOT(onCallMediaChanged()));
	connect(FCall->instance(),SIGNAL(callDestroyed()),SLOT(onCallDestroyed()));

	connect(ui.tlbReject,SIGNAL(clicked()),SLOT(onRejectButtonClicked()));

	connect(ui.tlbCameraPreview,SIGNAL(clicked(bool)),SLOT(onPreviewEnableClicked(bool)));
	connect(ui.tlbCameraEnable,SIGNAL(clicked(bool)),SLOT(onCameraEnableClicked(bool)));
	connect(ui.tlbMicrophoneEnable,SIGNAL(clicked(bool)),SLOT(onMicrophoneEnableClicked(bool)));
	connect(ui.sdrSpeakerVolume,SIGNAL(valueChanged(int)),SLOT(onSpeakerVolumeChanged(int)));

	connect(ui.tlbDialerVisible,SIGNAL(toggled(bool)),SLOT(onDialerVisibleToggled(bool)));
	connect(ui.tlbCameraPreview,SIGNAL(toggled(bool)),SLOT(onPreviewEnableToggled(bool)));
	connect(ui.tlbCameraEnable,SIGNAL(toggled(bool)),SLOT(onCameraEnableToggled(bool)));
	connect(ui.tlbMicrophoneEnable,SIGNAL(toggled(bool)),SLOT(onMicrophoneEnableToggled(bool)));

	FDurationTimer.setInterval(1000);
	FDurationTimer.setSingleShot(false);
	connect(&FDurationTimer,SIGNAL(timeout()),SLOT(onDurationTimerTimeout()));

	ui.tlbDialerVisible->setVisible(ACall->isNumberCall());
	ui.tlbCameraEnable->setVisible(!ACall->isNumberCall());
	ui.tlbCameraPreview->setVisible(!ACall->isNumberCall());

	onCallStateChanged();
	onCallMediaChanged();
	onCameraEnableToggled(isMediaEnabled(IPhoneCall::VideoCapture));

	onDurationTimerTimeout();
	onDialerVisibleToggled(ui.tlbDialerVisible->isChecked());
	onPreviewEnableToggled(ui.tlbCameraPreview->isChecked());
	onCameraEnableToggled(ui.tlbCameraEnable->isChecked());
	onMicrophoneEnableToggled(ui.tlbMicrophoneEnable->isChecked());
}

CallControlWidget::~CallControlWidget()
{

}

bool CallControlWidget::isDialerEnabled() const
{
	return ui.tlbDialerVisible->isChecked();
}

void CallControlWidget::setDialerEnabled(bool AEnabled)
{
	ui.tlbDialerVisible->setChecked(AEnabled);
}

bool CallControlWidget::isPreviewEnabled() const
{
	return ui.tlbCameraPreview->isChecked();
}

void CallControlWidget::setPreviewEnabled(bool AEnabled)
{
	if (!AEnabled)
		ui.tlbCameraPreview->setChecked(false);
	else if (ui.tlbCameraEnable->isEnabled() && ui.tlbCameraEnable->isChecked())
		ui.tlbCameraPreview->setChecked(true);
}

bool CallControlWidget::isMediaEnabled(IPhoneCall::Media AMedia) const
{
	switch (AMedia)
	{
	case IPhoneCall::AudioCapture:
		return ui.tlbMicrophoneEnable->isChecked();
	case IPhoneCall::AudioPlayback:
		return ui.sdrSpeakerVolume->isEnabled();
	case IPhoneCall::VideoCapture:
		return ui.tlbCameraEnable->isChecked();
	case IPhoneCall::VideoPlayback:
		return FVideoPlaybackEnabled;
	default:
		break;
	}
	return false;
}

void CallControlWidget::setMediaEnabled(IPhoneCall::Media AMedia, bool AEnabled)
{
	switch (AMedia)
	{
	case IPhoneCall::AudioCapture:
		if (ui.tlbMicrophoneEnable->isEnabled())
			ui.tlbMicrophoneEnable->setChecked(AEnabled);
		break;
	case IPhoneCall::VideoCapture:
		if (ui.tlbCameraEnable->isEnabled())
			ui.tlbCameraEnable->setChecked(AEnabled);
		break;
	case IPhoneCall::VideoPlayback:
		if (FVideoPlaybackEnabled != AEnabled)
		{
			FVideoPlaybackEnabled = AEnabled;
			if (FCall)
				FCall->setMediaProperty(IPhoneCall::VideoPlayback,IPhoneCall::Enabled,AEnabled);
			emit mediaEnableChanged(IPhoneCall::VideoPlayback,AEnabled);
		}
	default:
		break;
	}
}

void CallControlWidget::onCallStateChanged()
{
	bool isSpkAvail = FCall->mediaProperty(IPhoneCall::AudioPlayback,IPhoneCall::Avail).toBool();
	bool isMicAvail = FCall->mediaProperty(IPhoneCall::AudioCapture,IPhoneCall::Avail).toBool();
	bool isCamAvail = FCall->mediaProperty(IPhoneCall::VideoCapture,IPhoneCall::Avail).toBool();

	switch (FCall->state())
	{
	case IPhoneCall::Inited:
	case IPhoneCall::Calling:
	case IPhoneCall::Ringing:
	case IPhoneCall::Connecting:
		ui.tlbReject->setEnabled(true);

		ui.tlbDialerVisible->setEnabled(false);
		ui.tlbCameraEnable->setEnabled(isCamAvail);
		ui.tlbMicrophoneEnable->setEnabled(isMicAvail);
		ui.sdrSpeakerVolume->setEnabled(isSpkAvail);
		break;
	case IPhoneCall::Confirmed:
		ui.tlbReject->setEnabled(true);

		ui.tlbDialerVisible->setEnabled(true);
		ui.tlbCameraEnable->setEnabled(isCamAvail);
		ui.tlbMicrophoneEnable->setEnabled(isMicAvail);
		ui.sdrSpeakerVolume->setEnabled(isSpkAvail);

		FDurationTimer.start();
		break;
	case IPhoneCall::Disconnected:
	case IPhoneCall::Aborted:
		ui.tlbReject->setEnabled(false);

		ui.tlbDialerVisible->setEnabled(false);
		ui.tlbCameraEnable->setEnabled(false);
		ui.tlbCameraPreview->setEnabled(false);
		ui.tlbMicrophoneEnable->setEnabled(false);
		ui.sdrSpeakerVolume->setEnabled(false);

		FDurationTimer.stop();
		break;
	}
}

void CallControlWidget::onCallMediaChanged()
{
	if (FVideoPlaybackEnabled != FCall->mediaProperty(IPhoneCall::VideoPlayback,IPhoneCall::Enabled).toBool())
	{
		FVideoPlaybackEnabled = !FVideoPlaybackEnabled;
		emit mediaEnableChanged(IPhoneCall::VideoPlayback,FVideoPlaybackEnabled);
	}

	ui.tlbCameraEnable->setChecked(FCall->mediaProperty(IPhoneCall::VideoCapture,IPhoneCall::Enabled).toBool());
	ui.tlbMicrophoneEnable->setChecked(FCall->mediaProperty(IPhoneCall::AudioCapture,IPhoneCall::Enabled).toBool());
	ui.sdrSpeakerVolume->setValue(qRound(FCall->mediaProperty(IPhoneCall::AudioPlayback,IPhoneCall::Volume).toFloat()*100));
}

void CallControlWidget::onCallDestroyed()
{
	FCall = NULL;
}

void CallControlWidget::onRejectButtonClicked()
{
	if (FCall)
		FCall->hangupCall(IPhoneCall::Decline);
}

void CallControlWidget::onPreviewEnableClicked(bool AChecked)
{
	FVideoPreviewEnabled = AChecked;
}

void CallControlWidget::onCameraEnableClicked(bool AChecked)
{
	if (FCall)
		FCall->setMediaProperty(IPhoneCall::VideoCapture,IPhoneCall::Enabled,AChecked);
}

void CallControlWidget::onMicrophoneEnableClicked(bool AChecked)
{
	if (FCall)
		FCall->setMediaProperty(IPhoneCall::AudioCapture,IPhoneCall::Enabled,AChecked);
}

void CallControlWidget::onSpeakerVolumeChanged(int AValue)
{
	if (FCall)
		FCall->setMediaProperty(IPhoneCall::AudioPlayback,IPhoneCall::Volume,(float)AValue/100.0);
}

void CallControlWidget::onDialerVisibleToggled(bool AChecked)
{
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.tlbDialerVisible,AChecked ? MNI_PHONEMANAGER_DIALER_ENABLED : MNI_PHONEMANAGER_DIALER_DISABLED);
	emit dialerEnableChanged(AChecked);
}

void CallControlWidget::onPreviewEnableToggled(bool AChecked)
{
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.tlbCameraPreview,AChecked ? MNI_PHONEMANAGER_VIDEOPREVIEW_ENABLED : MNI_PHONEMANAGER_VIDEOPREVIEW_DISABLED);
	emit previewEnableChanged(AChecked);
}

void CallControlWidget::onCameraEnableToggled(bool AChecked)
{
	ui.tlbCameraPreview->setEnabled(AChecked);
	ui.tlbCameraPreview->setChecked(AChecked && FVideoPreviewEnabled);
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.tlbCameraEnable,AChecked ? MNI_PHONEMANAGER_CAMERA_ENABLED : MNI_PHONEMANAGER_CAMERA_DISABLED);
	emit mediaEnableChanged(IPhoneCall::VideoCapture,AChecked);
}

void CallControlWidget::onMicrophoneEnableToggled(bool AChecked)
{
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.tlbMicrophoneEnable,AChecked ? MNI_PHONEMANAGER_MICROPHONE_ENABLED : MNI_PHONEMANAGER_MICROPHONE_DISABLED);
	emit mediaEnableChanged(IPhoneCall::AudioCapture,AChecked);
}

void CallControlWidget::onDurationTimerTimeout()
{
	if (FCall)
		ui.lblTime->setText(FCall->durationString());
}
