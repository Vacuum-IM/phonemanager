#include "callhistorystructmodel.h"

#include <QColor>
#include <QWidget>

CallHistoryStructModel::CallHistoryStructModel(IPhoneManager *APhoneManager, QObject *AParent) : AdvancedItemModel(AParent)
{
	FPhoneManager = APhoneManager;
	connect(FPhoneManager->instance(),SIGNAL(callHistoryRequestFailed(const QString &, const XmppError &)),
		SLOT(onCallHistoryRequestFailed(const QString &, const XmppError &)));
	connect(FPhoneManager->instance(),SIGNAL(callHistoryItemsLoaded(const QString &, const QList<IPhoneCallHistoryItem> &)),
		SLOT(onCallHistoryItemsLoaded(const QString &, const QList<IPhoneCallHistoryItem> &)));

	setColumnCount(1);
	setSortRole(RoleSort);
}

CallHistoryStructModel::~CallHistoryStructModel()
{

}

void CallHistoryStructModel::fetchMore(const QModelIndex &AParent)
{
	if (AParent.data(RoleType).toInt() == TypeYear)
	{
		int year = AParent.data(RoleYear).toInt();
		QList<Jid> streams =  FFetchMonthStreams.take(year);
		foreach(const Jid &streamJid, streams)
			requestMonthItems(streamJid,year);
	}
}

bool CallHistoryStructModel::canFetchMore(const QModelIndex &AParent) const
{
	return AParent.data(RoleType).toInt()==TypeYear && !FFetchMonthStreams.value(AParent.data(RoleYear).toInt()).isEmpty();
}

bool CallHistoryStructModel::hasChildren(const QModelIndex &AParent) const
{
	return AParent.data(RoleType).toInt()==TypeYear || QStandardItemModel::hasChildren(AParent);
}

void CallHistoryStructModel::loadCallHistoryStructure(const QList<Jid> &AStreams)
{
	FYearRequests.clear();
	FMonthRequests.clear();
	FFetchMonthStreams.clear();

	QList<QStandardItem *> yearItems;
	for (int row=0; row<invisibleRootItem()->rowCount(); row++)
	{
		QStandardItem *item = invisibleRootItem()->child(row);
		if (item->data(RoleType).toInt() == TypeYear)
			yearItems.append(item);
	}

	foreach(QStandardItem *item, yearItems)
		qDeleteAll(takeRow(item->row()));

	foreach(const Jid &streamJid, AStreams)
		requestYearItems(streamJid);
}

bool CallHistoryStructModel::requestYearItems(const Jid &AStreamJid)
{
	IPhoneCallHistoryRequest request;
	request.grouping = IPhoneCallHistoryRequest::GroupByYear;

	QString id = FPhoneManager->loadCallHistoryItems(AStreamJid,request);
	if (!id.isEmpty())
	{
		FYearRequests.insert(id,AStreamJid);
		return true;
	}
	return false;
}

QStandardItem *CallHistoryStructModel::findYearItem(int AYear) const
{
	QStandardItem *root = invisibleRootItem();
	for (int row=0; row<root->rowCount(); row++)
	{
		QStandardItem *yearItem = root->child(row);
		if (yearItem->data(RoleType).toInt()==TypeYear && yearItem->data(RoleYear).toInt()==AYear)
			return yearItem;
	}
	return NULL;
}

QStandardItem *CallHistoryStructModel::getYearItem(const Jid &AStreamJid, int AYear)
{
	QStandardItem *yearItem = findYearItem(AYear);
	if (yearItem == NULL)
	{
		yearItem = new QStandardItem(QString::number(AYear));
		yearItem->setData(TypeYear,RoleType);
		yearItem->setData(AYear,RoleYear);
		yearItem->setData(AYear,RoleSort);
		yearItem->setData(QDateTime(QDate(AYear,1,1),QTime(0,0,0)),RoleStart);
		yearItem->setData(QDateTime(QDate(AYear,12,31),QTime(23,59,59,999)),RoleEnd);
		appendRow(yearItem);
	}

	QStringList streams = yearItem->data(RoleStreams).toStringList();
	if (!streams.contains(AStreamJid.pBare()))
	{
		FFetchMonthStreams[AYear].append(AStreamJid);
		streams.append(AStreamJid.pBare());
		yearItem->setData(streams,RoleStreams);
	}

	return yearItem;
}

void CallHistoryStructModel::updateYearItems(const Jid &AStreamJid, const QList<IPhoneCallHistoryItem> &AItems)
{
	foreach(const IPhoneCallHistoryItem &item, AItems)
		getYearItem(AStreamJid,item.start.date().year());
	sort(0,Qt::DescendingOrder);
}

bool CallHistoryStructModel::requestMonthItems(const Jid &AStreamJid, int AYear)
{
	IPhoneCallHistoryRequest request;
	request.grouping = IPhoneCallHistoryRequest::GroupByMonth;
	request.start = QDateTime(QDate(AYear,1,1),QTime(0,0,0));
	request.end = QDateTime(QDate(AYear,12,31),QTime(23,59,59,999));

	QString id = FPhoneManager->loadCallHistoryItems(AStreamJid,request);
	if (!id.isEmpty())
	{
		FMonthRequests.insert(id,AStreamJid);
		return true;
	}
	return false;
}

QStandardItem *CallHistoryStructModel::findMonthItem(int AYear, int AMonth) const
{
	QStandardItem *yearItem = findYearItem(AYear);
	if (yearItem)
	{
		for (int row=0; row<yearItem->rowCount(); row++)
		{
			QStandardItem *monthItem = yearItem->child(row);
			if (monthItem->data(RoleType).toInt()==TypeMonth && monthItem->data(RoleMonth).toInt()==AMonth)
				return monthItem;
		}
	}
	return NULL;
}

QStandardItem *CallHistoryStructModel::getMonthItem(const Jid &AStreamJid, int AYear, int AMonth)
{
	QStandardItem *monthItem = findMonthItem(AYear,AMonth);
	if (monthItem == NULL)
	{
		monthItem = new QStandardItem(QDate::longMonthName(AMonth));
		monthItem->setData(TypeMonth,RoleType);
		monthItem->setData(AMonth,RoleMonth);
		monthItem->setData(12-AMonth,RoleSort);
		monthItem->setData(QDateTime(QDate(AYear,AMonth,1),QTime(0,0,0)),RoleStart);
		monthItem->setData(QDateTime(QDate(AYear,AMonth,QDate(AYear,AMonth,1).daysInMonth()),QTime(23,59,59,999)),RoleEnd);
		getYearItem(AStreamJid,AYear)->appendRow(monthItem);
	}

	QStringList streams = monthItem->data(RoleStreams).toStringList();
	if (!streams.contains(AStreamJid.pBare()))
	{
		streams.append(AStreamJid.pBare());
		monthItem->setData(streams,RoleStreams);
	}

	return monthItem;
}

void CallHistoryStructModel::updateMonthItems(const Jid &AStreamJid, const QList<IPhoneCallHistoryItem> &AItems)
{
	foreach(const IPhoneCallHistoryItem &item, AItems)
		getMonthItem(AStreamJid,item.start.date().year(),item.start.date().month());
	sort(0,Qt::DescendingOrder);
}

void CallHistoryStructModel::onCallHistoryRequestFailed(const QString &AId, const XmppError &AError)
{
	Q_UNUSED(AError);
	static const QList<IPhoneCallHistoryItem> emptyItems;
	if (FYearRequests.contains(AId))
		updateYearItems(FYearRequests.take(AId),emptyItems);
	else if (FMonthRequests.contains(AId))
		updateMonthItems(FMonthRequests.take(AId),emptyItems);
}

void CallHistoryStructModel::onCallHistoryItemsLoaded(const QString &AId, const QList<IPhoneCallHistoryItem> &AItems)
{
	if (FYearRequests.contains(AId))
		updateYearItems(FYearRequests.take(AId),AItems);
	else if (FMonthRequests.contains(AId))
		updateMonthItems(FMonthRequests.take(AId),AItems);
}
