#ifndef CALLHISTORYVIEWWIDGET_H
#define CALLHISTORYVIEWWIDGET_H

#include <QSet>
#include <QListView>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <interfaces/iphonemanager.h>
#include <utils/advanceditemdelegate.h>

class CallHistoryViewWidget :
	public QListView
{
	Q_OBJECT;
public:
	enum ItemRoles {
		RoleStream = Qt::UserRole,
		RoleStart,
		RoleWith,
		RoleRole,
		RoleHangupCode,
		RoleDuration,
		RoleIsNumberCall,
		RoleDelegateItems
	};
public:
	CallHistoryViewWidget(IPhoneManager *APhoneManager, QWidget *AParent);
	~CallHistoryViewWidget();
	void setFilterFixedString(const QString &APattern);
	IPhoneCallHistoryItem modelIndexToHistoryItem(const QModelIndex &AIndex) const;
	void showCallHistoryRequest(const QList<Jid> &AStreams, const IPhoneCallHistoryRequest &ARequest);
protected:
	QVariant createDelegateItems(const IPhoneCallHistoryItem &AItem) const;
	void updateItems(const Jid &AStreamJid, const QList<IPhoneCallHistoryItem> &AItems);
protected slots:
	void onCallHistoryRequestFailed(const QString &AId, const XmppError &AError);
	void onCallHistoryItemsLoaded(const QString &AId, const QList<IPhoneCallHistoryItem> &AItems);
private:
	IPhoneManager *FPhoneManager;
private:
	bool FWithGrouped;
	Qt::SortOrder FSortOrder;
	QStandardItemModel *FItemModel;
	QSortFilterProxyModel *FSortFilter;
	QMap<QString, Jid> FHistoryRequests;
	QMap<Jid, QSet<QStandardItem *> > FCurItems;
};

#endif // CALLHISTORYVIEWWIDGET_H
