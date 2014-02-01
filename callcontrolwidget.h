#ifndef CALLCONTROLWIDGET_H
#define CALLCONTROLWIDGET_H

#include <QTimer>
#include <QFrame>
#include <interfaces/iphonemanager.h>
#include "ui_callcontrolwidget.h"

class CallControlWidget : 
	public QFrame
{
	Q_OBJECT;
public:
	CallControlWidget(IPhoneCall *ACall, QWidget *AParent);
	~CallControlWidget();
	bool isDialerEnabled() const;
	void setDialerEnabled(bool AEnabled);
	bool isPreviewEnabled() const;
	void setPreviewEnabled(bool AEnabled);
	bool isMediaEnabled(IPhoneCall::Media AMedia) const;
	void setMediaEnabled(IPhoneCall::Media AMedia, bool AEnabled);
signals:
	void dialerEnableChanged(bool AEnabled);
	void previewEnableChanged(bool AEnabled);
	void mediaEnableChanged(IPhoneCall::Media AMedia, bool AEnabled);
protected slots:
	void onCallStateChanged();
	void onCallMediaChanged();
	void onCallDestroyed();
protected slots:
	void onRejectButtonClicked();
protected slots:
	void onPreviewEnableClicked(bool AChecked);
	void onCameraEnableClicked(bool AChecked);
	void onMicrophoneEnableClicked(bool AChecked);
	void onSpeakerVolumeChanged(int AValue);
protected slots:
	void onDialerVisibleToggled(bool AChecked);
	void onPreviewEnableToggled(bool AChecked);
	void onCameraEnableToggled(bool AChecked);
	void onMicrophoneEnableToggled(bool AChecked);
protected slots:
	void onDurationTimerTimeout();
private:
	Ui::CallControlWidget ui;
private:
	IPhoneCall *FCall;
	QTimer FDurationTimer;
	bool FVideoPreviewEnabled;
	bool FVideoPlaybackEnabled;
};

#endif // CALLCONTROLWIDGET_H
