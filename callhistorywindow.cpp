#include "callhistorywindow.h"

#include <QMessageBox>
#include <definitions/resources.h>
#include <definitions/phonemanager/menuicons.h>
#include <utils/iconstorage.h>

CallHistoryWindow::CallHistoryWindow(IPhoneManager *APhoneManager, IPresencePlugin *APresencePlugin, IMessageProcessor *AMessageProcessor, QWidget *AParent) : QMainWindow(AParent)
{
	ui.setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose,true);
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(this,MNI_PHONEMANAGER_DIALER_DIALOG,0,0,"windowIcon");

	ui.sprSplitter->setStretchFactor(0,1);
	ui.sprSplitter->setStretchFactor(1,5);

	FPhoneManager = APhoneManager;
	FPresencePlugin = APresencePlugin;
	FMessageProcessor = AMessageProcessor;

	ui.wdtHistoryItems->setLayout(new QVBoxLayout);
	ui.wdtHistoryItems->layout()->setMargin(0);
	FHistoryWidget = new CallHistoryViewWidget(FPhoneManager,ui.wdtHistoryItems);
	connect(FHistoryWidget,SIGNAL(doubleClicked(const QModelIndex &)),SLOT(onHistoryViewItemDoubleClicked(const QModelIndex &)));
	ui.wdtHistoryItems->layout()->addWidget(FHistoryWidget);

	FHistoryModel = new CallHistoryStructModel(FPhoneManager,this);

	ui.trvHistoryStruct->setModel(FHistoryModel);
	connect(ui.trvHistoryStruct->selectionModel(),SIGNAL(currentRowChanged(const QModelIndex &, const QModelIndex &)),
		SLOT(onCurrentStructRowChanged(const QModelIndex &, const QModelIndex &)));

	connect(ui.sleSearch,SIGNAL(searchStart()),SLOT(onSearchStart()));

	connect(ui.pbtClearHistory,SIGNAL(clicked()),SLOT(onClearHistoryButtonClicked()));
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.pbtClearHistory,MNI_PHONEMANAGER_CALLHISTORTY_CLEAR);

	createStaticItems();
	updateHistoryStruct();
}

CallHistoryWindow::~CallHistoryWindow()
{

}

void CallHistoryWindow::updateHistoryView() const
{
	QModelIndex index = ui.trvHistoryStruct->selectionModel()->currentIndex();

	IPhoneCallHistoryRequest request;
	request.order = Qt::DescendingOrder;
	if (!ui.sleSearch->text().isEmpty())
		request.with = "%"+ui.sleSearch->text()+"%";
	request.start = index.data(CallHistoryStructModel::RoleStart).toDateTime();
	request.end = index.data(CallHistoryStructModel::RoleEnd).toDateTime();

	QList<Jid> streams;
	foreach(const QString &streamJid, index.data(CallHistoryStructModel::RoleStreams).toStringList())
		streams.append(streamJid);

	FHistoryWidget->showCallHistoryRequest(streams,request);
}

void CallHistoryWindow::updateHistoryStruct() const
{
	FHistoryModel->loadCallHistoryStructure(FPhoneManager->numbersStreams());
}

void CallHistoryWindow::createStaticItems()
{
	QStringList streams;
	foreach(const Jid &streamJid, FPhoneManager->numbersStreams())
		streams.append(streamJid.pFull());

	QStandardItem *todayItem = new QStandardItem(tr("Today"));
	todayItem->setData(10000,CallHistoryStructModel::RoleSort);
	todayItem->setData(streams,CallHistoryStructModel::RoleStreams);
	todayItem->setData(CallHistoryStructModel::TypeOther,CallHistoryStructModel::RoleType);
	todayItem->setData(QDateTime(QDate::currentDate(),QTime(0,0,0)),CallHistoryStructModel::RoleStart);
	todayItem->setData(QDateTime(QDate::currentDate(),QTime(23,59,59,999)),CallHistoryStructModel::RoleEnd);
	FHistoryModel->appendRow(todayItem);

	QStandardItem *yesterdayItem = new QStandardItem(tr("Yesterday"));
	yesterdayItem->setData(10100,CallHistoryStructModel::RoleSort);
	yesterdayItem->setData(streams,CallHistoryStructModel::RoleStreams);
	yesterdayItem->setData(CallHistoryStructModel::TypeOther,CallHistoryStructModel::RoleType);
	yesterdayItem->setData(QDateTime(QDate::currentDate().addDays(-1),QTime(0,0,0)),CallHistoryStructModel::RoleStart);
	yesterdayItem->setData(QDateTime(QDate::currentDate().addDays(-1),QTime(23,59,59,999)),CallHistoryStructModel::RoleEnd);
	FHistoryModel->appendRow(yesterdayItem);

	QStandardItem *weakItem = new QStandardItem(tr("Last 7 Days"));
	weakItem->setData(10200,CallHistoryStructModel::RoleSort);
	weakItem->setData(streams,CallHistoryStructModel::RoleStreams);
	weakItem->setData(CallHistoryStructModel::TypeOther,CallHistoryStructModel::RoleType);
	weakItem->setData(QDateTime(QDate::currentDate().addDays(-7),QTime(0,0,0)),CallHistoryStructModel::RoleStart);
	weakItem->setData(QDateTime(QDate::currentDate(),QTime(23,59,59,999)),CallHistoryStructModel::RoleEnd);
	FHistoryModel->appendRow(weakItem);

	QStandardItem *monthItem = new QStandardItem(tr("Last Month"));
	monthItem->setData(10300,CallHistoryStructModel::RoleSort);
	monthItem->setData(streams,CallHistoryStructModel::RoleStreams);
	monthItem->setData(CallHistoryStructModel::TypeOther,CallHistoryStructModel::RoleType);
	monthItem->setData(QDateTime(QDate::currentDate().addMonths(-1),QTime(0,0,0)),CallHistoryStructModel::RoleStart);
	monthItem->setData(QDateTime(QDate::currentDate(),QTime(23,59,59,999)),CallHistoryStructModel::RoleEnd);
	FHistoryModel->appendRow(monthItem);

	ui.trvHistoryStruct->selectionModel()->setCurrentIndex(FHistoryModel->indexFromItem(monthItem),QItemSelectionModel::ClearAndSelect);
}

void CallHistoryWindow::onSearchStart()
{
	FHistoryWidget->setFilterFixedString(ui.sleSearch->text());
}

void CallHistoryWindow::onClearHistoryButtonClicked()
{
	if (QMessageBox::question(this,tr("Clear History"),
		tr("Are you sure you want to delete all of your call history? This operation can not be undone."),
		QMessageBox::Yes|QMessageBox::Cancel) == QMessageBox::Yes)
	{
		IPhoneCallHistoryRequest request;
		foreach(const Jid &streamJid, FPhoneManager->numbersStreams())
			FPhoneManager->removeCallHistoryItems(streamJid,request);
		updateHistoryStruct();
		updateHistoryView();
	}
}

void CallHistoryWindow::onHistoryViewItemDoubleClicked(const QModelIndex &AIndex)
{
	if (FPhoneManager->isCallsEnabled())
	{
		Jid streamJid = AIndex.data(CallHistoryViewWidget::RoleStream).toString();
		IPhoneCallHistoryItem item = FHistoryWidget->modelIndexToHistoryItem(AIndex);

		IPhoneCall *call = NULL;
		if (!item.isNumberCall)
		{
			QSet<Jid> receivers;
			IPresence *presence = FPresencePlugin!=NULL ? FPresencePlugin->findPresence(streamJid) : NULL;
			QList<IPresenceItem> pItems = presence!=NULL ? presence->findItems(item.with) : QList<IPresenceItem>();
			foreach(const IPresenceItem &pitem, pItems)
			{
				if (FPhoneManager->isCallsSupported(streamJid,pitem.itemJid))
					receivers += pitem.itemJid;
			}
			call = FPhoneManager->newCall(streamJid,receivers);
		}
		else
		{
			call = FPhoneManager->newCall(streamJid,item.with);
		}

		if (call)
		{
			if (FPhoneManager->showCallWindow(call))
				call->startCall();
			else
				call->destroyCall(0);
		}
		else if (FMessageProcessor)
		{
			Jid contactJid = item.isNumberCall ? FPhoneManager->findContactByNumber(streamJid,item.with) : Jid(item.with);
			FMessageProcessor->createMessageWindow(streamJid,contactJid,Message::Chat,IMessageHandler::SM_SHOW);
		}
	}
}

void CallHistoryWindow::onCurrentStructRowChanged(const QModelIndex &ACurrent, const QModelIndex &APrevious)
{
	Q_UNUSED(ACurrent); Q_UNUSED(APrevious);
	updateHistoryView();
}
