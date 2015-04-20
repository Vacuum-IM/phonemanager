#ifndef CALLHISTORYWINDOW_H
#define CALLHISTORYWINDOW_H

#include <QMainWindow>
#include <interfaces/iphonemanager.h>
#include <interfaces/ipresencemanager.h>
#include <interfaces/imessageprocessor.h>
#include "callhistoryviewwidget.h"
#include "callhistorystructmodel.h"
#include "ui_callhistorywindow.h"

class CallHistoryWindow :
	public QMainWindow
{
	Q_OBJECT;
public:
	CallHistoryWindow(IPhoneManager *APhoneManager, IPresenceManager *APresenceManager, IMessageProcessor *AMessageProcessor, QWidget *AParent = NULL);
	~CallHistoryWindow();
	void updateHistoryView() const;
	void updateHistoryStruct() const;
protected:
	void createStaticItems();
protected slots:
	void onSearchStart();
	void onClearHistoryButtonClicked();
	void onHistoryViewItemDoubleClicked(const QModelIndex &AIndex);
	void onCurrentStructRowChanged(const QModelIndex &ACurrent, const QModelIndex &APrevious);
private:
	Ui::CallHistoryWindow ui;
private:
	IPhoneManager *FPhoneManager;
	IPresenceManager *FPresenceManager;
	IMessageProcessor *FMessageProcessor;
private:
	CallHistoryStructModel *FHistoryModel;
	CallHistoryViewWidget *FHistoryWidget;
};

#endif // CALLHISTORYWINDOW_H
