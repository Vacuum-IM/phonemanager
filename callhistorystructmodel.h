#ifndef CALLHISTORYSTRUCTMODEL_H
#define CALLHISTORYSTRUCTMODEL_H

#include <QStandardItem>
#include <QStandardItemModel>
#include <interfaces/iphonemanager.h>
#include <utils/advanceditemmodel.h>

class CallHistoryStructModel :
	public AdvancedItemModel
{
	Q_OBJECT;
public:
	enum ItemTypes {
		TypeYear,
		TypeMonth,
		TypeOther
	};
	enum ItemRoles {
		RoleType = Qt::UserRole,
		RoleStreams,
		RoleYear,
		RoleMonth,
		RoleStart,
		RoleEnd,
		RoleSort
	};
public:
	CallHistoryStructModel(IPhoneManager *APhoneManager, QObject *AParent=NULL);
	~CallHistoryStructModel();
	//QAbstractItemModel
	void fetchMore(const QModelIndex &AParent);
	bool canFetchMore(const QModelIndex &AParent) const;
	bool hasChildren(const QModelIndex &AParent = QModelIndex()) const;
	//CallHistoryStructModel
	void loadCallHistoryStructure(const QList<Jid> &AStreams);
protected:
	bool requestYearItems(const Jid &AStreamJid);
	QStandardItem *findYearItem(int AYear) const;
	QStandardItem *getYearItem(const Jid &AStreamJid, int AYear);
	void updateYearItems(const Jid &AStreamJid, const QList<IPhoneCallHistoryItem> &AItems);
protected:
	bool requestMonthItems(const Jid &AStreamJid, int AYear);
	QStandardItem *findMonthItem(int AYear, int AMonth) const;
	QStandardItem *getMonthItem(const Jid &AStreamJid, int AYear, int AMonth);
	void updateMonthItems(const Jid &AStreamJid, const QList<IPhoneCallHistoryItem> &AItems);
protected slots:
	void onCallHistoryRequestFailed(const QString &AId, const XmppError &AError);
	void onCallHistoryItemsLoaded(const QString &AId, const QList<IPhoneCallHistoryItem> &AItems);
private:
	IPhoneManager *FPhoneManager;
private:
	QMap<QString,Jid> FYearRequests;
	QMap<QString,Jid> FMonthRequests;
	QMap<int, QList<Jid> > FFetchMonthStreams;
};

#endif // CALLHISTORYSTRUCTMODEL_H
