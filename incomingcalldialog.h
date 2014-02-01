#ifndef INCOMINGCALLDIALOG_H
#define INCOMINGCALLDIALOG_H

#include <QSound>
#include <QDialog>
#include <interfaces/iphonemanager.h>
#include "ui_incomingcalldialog.h"

class IncomingCallDialog : 
	public QDialog
{
	Q_OBJECT;
public:
	enum DialogResult {
		Rejected,
		AcceptedNoVideo,
		AcceptedWithVideo,
		CallStateChanged,
	};
public:
	IncomingCallDialog(IPhoneManager *APhoneManager, IPhoneCall *ACall, QWidget *AParent=NULL);
	~IncomingCallDialog();
	IPhoneCall *call() const;
signals:
	void dialogDestroyed();
protected:
	void playSound(const QString &ASoundKey, int ALoops = 0);
protected slots:
	void onCallStateChanged();
	void onAudioCallActionTriggered();
	void onVideoCallActionTriggered();
	void onRejectButtonClicked();
private:
	Ui::IncomingCallDialog ui;
private:
	IPhoneCall *FCall;
	IPhoneManager *FPhoneManager;
private:
	QSound *FSound;
};

#endif // INCOMINGCALLDIALOG_H
