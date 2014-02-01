#include "callhistoryviewwidget.h"

#include <definitions/resources.h>
#include <definitions/phonemanager/menuicons.h>
#include <utils/iconstorage.h>

class HistoryViewSortFilterProxyModel :
	public QSortFilterProxyModel
{
public:
	HistoryViewSortFilterProxyModel(QObject *AParent = NULL) : QSortFilterProxyModel(AParent) {}
	bool filterAcceptsRow(int ASourceRow, const QModelIndex &ASourceParent) const {
		QRegExp regexp = filterRegExp();
		if (!regexp.isEmpty())
		{
			QModelIndex index = sourceModel()->index(ASourceRow,0,ASourceParent);
			return regexp.indexIn(index.data(Qt::DisplayRole).toString())>=0 || regexp.indexIn(index.data(CallHistoryViewWidget::RoleWith).toString())>=0;
		}
		return true;
	}
};

CallHistoryViewWidget::CallHistoryViewWidget(IPhoneManager *APhoneManager, QWidget *AParent) : QListView(AParent)
{
	FPhoneManager = APhoneManager;
	connect(FPhoneManager->instance(),SIGNAL(callHistoryRequestFailed(const QString &, const XmppError &)),
		SLOT(onCallHistoryRequestFailed(const QString &, const XmppError &)));
	connect(FPhoneManager->instance(),SIGNAL(callHistoryItemsLoaded(const QString &, const QList<IPhoneCallHistoryItem> &)),
		SLOT(onCallHistoryItemsLoaded(const QString &, const QList<IPhoneCallHistoryItem> &)));

	AdvancedItemDelegate *advDelegate = new AdvancedItemDelegate(this);
	advDelegate->setItemsRole(RoleDelegateItems);
	setItemDelegate(advDelegate);

	FItemModel = new QStandardItemModel(this);
	FItemModel->setColumnCount(1);
	
	FSortFilter = new HistoryViewSortFilterProxyModel(this);
	FSortFilter->setSortRole(RoleStart);
	FSortFilter->setSourceModel(FItemModel);
	setModel(FSortFilter);

	setUniformItemSizes(true);
	setAlternatingRowColors(true);
	setEditTriggers(QAbstractItemView::NoEditTriggers);
}

CallHistoryViewWidget::~CallHistoryViewWidget()
{

}

void CallHistoryViewWidget::setFilterFixedString(const QString &APattern)
{
	FSortFilter->setFilterFixedString(APattern);
}

IPhoneCallHistoryItem CallHistoryViewWidget::modelIndexToHistoryItem(const QModelIndex &AIndex) const
{
	IPhoneCallHistoryItem historyItem;
	historyItem.start = AIndex.data(RoleStart).toDateTime();
	historyItem.with = AIndex.data(RoleWith).toString();
	historyItem.role = AIndex.data(RoleRole).toInt();
	historyItem.hangupCode = AIndex.data(RoleHangupCode).toInt();
	historyItem.duration = AIndex.data(RoleDuration).toInt();
	historyItem.isNumberCall = AIndex.data(RoleIsNumberCall).toInt();
	return historyItem;
}

void CallHistoryViewWidget::showCallHistoryRequest(const QList<Jid> &AStreams, const IPhoneCallHistoryRequest &ARequest)
{
	FCurItems.clear();
	FHistoryRequests.clear();

	QList<QStandardItem *> oldItems;
	for (int row=0; row<FItemModel->rowCount(); row++)
	{
		QStandardItem *modelItem = FItemModel->item(row);
		Jid streamJid = modelItem->data(RoleStream).toString();
		if (AStreams.contains(streamJid))
			FCurItems[streamJid] += modelItem;
		else
			oldItems.append(modelItem);
	}

	foreach(QStandardItem *oldItem, oldItems)
		qDeleteAll(FItemModel->takeRow(oldItem->row()));

	FSortOrder = ARequest.order;
	FWithGrouped = ARequest.grouping==IPhoneCallHistoryRequest::GroupByWith;
	foreach(const Jid &streamJid, AStreams)
	{
		QString id = FPhoneManager->loadCallHistoryItems(streamJid,ARequest);
		if (!id.isEmpty())
			FHistoryRequests.insert(id,streamJid);
	}
}

QVariant CallHistoryViewWidget::createDelegateItems(const IPhoneCallHistoryItem &AItem) const
{
	QVariant data;
	AdvancedDelegateItems advItems;

	AdvancedDelegateItem decorationItem(AdvancedDelegateItem::DecorationId);
	decorationItem.d->kind = AdvancedDelegateItem::Decoration;
	if (AItem.hangupCode == IPhoneCall::ErrorOccured)
		decorationItem.d->data = IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->getIcon(MNI_PHONEMANAGER_CALL_ERROR);
	else if (AItem.role == IPhoneCall::Caller)
		decorationItem.d->data = IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->getIcon(MNI_PHONEMANAGER_CALL_OUT);
	else
		decorationItem.d->data = IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->getIcon(MNI_PHONEMANAGER_CALL_IN);
	advItems.insert(decorationItem.d->id,decorationItem);

	if (AItem.duration > 0)
	{
		QTime time = QTime(0, 0, 0, 0).addMSecs(AItem.duration);
		AdvancedDelegateItem durationItem(AdvancedDelegateItem::makeId(AdvancedDelegateItem::MiddleCenter,128,600));
		durationItem.d->kind = AdvancedDelegateItem::CustomData;
		durationItem.d->data = time.toString(time.hour()>0 ? "h:mm:ss" : "m:ss");
		durationItem.d->hints.insert(AdvancedDelegateItem::Foreground,palette().color(QPalette::Disabled,QPalette::Text));
		advItems.insert(durationItem.d->id,durationItem);
	}

	AdvancedDelegateItem startItem(AdvancedDelegateItem::makeId(AdvancedDelegateItem::MiddleRight,128,500));
	startItem.d->kind = AdvancedDelegateItem::CustomData;
	QString startData;
	if (AItem.start.date() == QDate::currentDate())
		startData = AItem.start.time().toString("h:mm");
	else if (AItem.start.daysTo(QDateTime::currentDateTime()) == 1)
		startData = tr("Yesterday");
	else if (AItem.start.daysTo(QDateTime::currentDateTime()) <= 7)
		startData = QDate::longDayName(AItem.start.date().dayOfWeek());
	else 
		startData = AItem.start.date().toString("d MMM");
	for (int i=0; i<startData.length(); i++)
	{
		if (startData[i].isLetter())
		{
			startData[i] = startData[i].toUpper();
			break;
		}
	}
	startItem.d->data = startData;
	startItem.d->hints.insert(AdvancedDelegateItem::Foreground,palette().color(QPalette::Disabled,QPalette::Text));
	advItems.insert(startItem.d->id,startItem);

	data.setValue(advItems);
	return data;
}

void CallHistoryViewWidget::updateItems(const Jid &AStreamJid, const QList<IPhoneCallHistoryItem> &AItems)
{
	QSet<int> years;
	QSet<QStandardItem *> oldItems = FCurItems.take(AStreamJid);

	foreach(const IPhoneCallHistoryItem &historyItem, AItems)
	{
		QStandardItem *modelItem = NULL;
		years += historyItem.start.date().year();

		QSet<QString> withList;
		for(QSet<QStandardItem *>::const_iterator it=oldItems.constBegin(); modelItem==NULL && it!=oldItems.constEnd(); ++it)
		{
			QStandardItem *oldItem = *it;
			QString oldItemWith = oldItem->data(RoleWith).toString();
			if (oldItemWith == historyItem.with)
			{
				if (FWithGrouped && !withList.contains(oldItemWith))
				{
					modelItem = oldItem;
					withList += oldItemWith;
				}
				else if(oldItem->data(RoleStart).toDateTime() == historyItem.start)
				{
					modelItem = oldItem;
				}
			}
		}

		if (modelItem == NULL)
		{
			modelItem = new QStandardItem;
			modelItem->setData(AStreamJid.pFull(),RoleStream);
			modelItem->setData(historyItem.with,RoleWith);
			modelItem->setSizeHint(QSize(0,32));

			if (historyItem.hangupCode == IPhoneCall::ErrorOccured)
				modelItem->setForeground(Qt::darkRed);
		
			if (historyItem.isNumberCall)
			{
				QString text = FPhoneManager->callContactName(AStreamJid,historyItem);
				if (text != FPhoneManager->formattedNumber(historyItem.with))
					text += QString(" (%1)").arg(historyItem.with);
				modelItem->setText(text);
			}
			else
			{
				modelItem->setText(FPhoneManager->callContactName(AStreamJid,historyItem));
			}
			modelItem->setToolTip(modelItem->text());

			FItemModel->appendRow(modelItem);
		}
		else
		{
			oldItems -= modelItem;
		}

		modelItem->setData(historyItem.start,RoleStart);
		modelItem->setData(historyItem.role,RoleRole);
		modelItem->setData(historyItem.hangupCode,RoleHangupCode);
		modelItem->setData(historyItem.isNumberCall,RoleIsNumberCall);
		modelItem->setData(createDelegateItems(historyItem),RoleDelegateItems);
	}

	foreach(QStandardItem *item, oldItems)
		qDeleteAll(FItemModel->takeRow(item->row()));

	FSortFilter->sort(0,FSortOrder);
}

void CallHistoryViewWidget::onCallHistoryRequestFailed(const QString &AId, const XmppError &AError)
{
	Q_UNUSED(AError);
	if (FHistoryRequests.contains(AId))
	{
		static const QList<IPhoneCallHistoryItem> emptyItems;
		updateItems(FHistoryRequests.take(AId),emptyItems);
	}
}

void CallHistoryViewWidget::onCallHistoryItemsLoaded(const QString &AId, const QList<IPhoneCallHistoryItem> &AItems)
{
	if (FHistoryRequests.contains(AId))
		updateItems(FHistoryRequests.take(AId),AItems);
}
