#ifndef PHONEDIALERDIALOG_H
#define PHONEDIALERDIALOG_H

#include <QDialog>
#include <QPointer>
#include <interfaces/iphonemanager.h>
#include "ui_phonedialerdialog.h"
#include "callhistoryviewwidget.h"

class PhoneDialerDialog : 
	public QDialog
{
	Q_OBJECT;
public:
	PhoneDialerDialog(IPhoneManager *APhoneManager, QWidget *AParent = NULL);
	~PhoneDialerDialog();
protected:
	bool isCallEnabled() const;
protected slots:
	void updateCallHistory();
	void updateCallButtonState();
protected slots:
	void onDialerNumberChanged();
	void onCallButtonClicked();
	void onPhoneCallDestroyed();
	void onLabelLinkActivated(const QString &ALink);
	void onHistoryViewItemClicked(const QModelIndex &AIndex);
	void onHistoryViewItemDoubleClicked(const QModelIndex &AIndex);
private:
	Ui::PhoneDialerDialog ui;
private:
	IPhoneManager *FPhoneManager;
	CallHistoryViewWidget *FHistoryWidget;
};

#endif // PHONEDIALERDIALOG_H
