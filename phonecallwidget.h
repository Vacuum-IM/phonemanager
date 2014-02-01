#ifndef PHONECALLWIDGET_H
#define PHONECALLWIDGET_H

#include <QSound>
#include <QFrame>
#include <interfaces/iphonemanager.h>
#include "videoframe.h"
#include "dialerwidget.h"
#include "phonecalllayout.h"
#include "callinfowidget.h"
#include "callcontrolwidget.h"

class PhoneCallWidget :
	public QFrame
{
	Q_OBJECT;
public:
	PhoneCallWidget(IPhoneManager *APhoneManager, IPhoneCall *ACall, QWidget *AParent);
	~PhoneCallWidget();
	IPhoneCall *call() const;
signals:
	void widgetDestroyed();
protected:
	void destroyWidgetLater(int ATimeout);
	void playSound(const QString &ASoundKey, int ALoops = 0);
protected slots:
	void onCallStateChanged();
	void onCallDestroyed();
protected slots:
	void onDialerButtonClicked(const QChar &AChar);
protected slots:
	void onDialerEnableChanged(bool AEnabled);
	void onVideoPreviewEnableChanged(bool AEnabled);
	void onMediaEnableChanged(IPhoneCall::Media AMedia, bool AEnabled);
private:
	IPhoneCall *FCall;
	IPhoneManager *FPhoneManager;
private:
	QSound *FSound;
	PhoneCallLayout *FLayout;
	VideoFrame *FLocalVideo;
	VideoFrame *FRemoteVideo;
	QWidget *FDialerWidgetLayer;
	CallInfoWidget *FInfoWidget;
	CallControlWidget *FControlWidget;
};

#endif // PHONECALLWIDGET_H
