#ifndef CALLHISTORYWORKER_H
#define CALLHISTORYWORKER_H

#include <QQueue>
#include <QMutex>
#include <QThread>
#include <QVariant>
#include <QRunnable>
#include <QSqlError>
#include <QSqlDatabase>
#include <QWaitCondition>
#include <interfaces/iphonemanager.h>

class CallHistoryTask :
	public QRunnable
{
	friend class CallHistoryWorker;
public:
	enum Type {
		OpenDatabase,
		CloseDatabase,
		SaveHistoryItem,
		LoadHistoryItems,
		RemoveHistoryItems
	};
public:
	CallHistoryTask(const Jid &AStreamJid, Type AType);
	Type type() const;
	Jid streamJid() const;
	QString taskId() const;
	bool hasError() const;
	XmppError error() const;
protected:
	QString databaseConnection() const;
	void setSQLError(const QSqlError &AError);
protected:
	Type FType;
	Jid FStreamJid;
	QString FTaskId;
	XmppError FError;
private:
	static quint32 FTaskCount;
};

class CallHistoryTaskOpenDatabase :
	public CallHistoryTask
{
public:
	CallHistoryTaskOpenDatabase(const Jid &AStreamJid, const QString &ADatabasePath);
	QMap<QString,QString> databaseProperties() const;
protected:
	void run();
private:
	bool initializeDatabase(QSqlDatabase &ADatabase);
private:
	QString FDatabasePath;
	QMap<QString,QString> FProperties;
};

class CallHistoryTaskCloseDatabase :
	public CallHistoryTask
{
public:
	CallHistoryTaskCloseDatabase(const Jid &AStreamJid);
protected:
	void run();
};

class CallHistoryTaskSaveItem :
	public CallHistoryTask
{
public:
	CallHistoryTaskSaveItem(const Jid &AStreamJid, const IPhoneCallHistoryItem &AItem);
	IPhoneCallHistoryItem savedItem() const;
protected:
	void run();
private:
	IPhoneCallHistoryItem FItem;
};

class CallHistoryTaskLoadItems :
	public CallHistoryTask
{
public:
	CallHistoryTaskLoadItems(const Jid &AStreamJid, const IPhoneCallHistoryRequest &ARequest);
	QList<IPhoneCallHistoryItem> loadedItems() const;
protected:
	void run();
private:
	IPhoneCallHistoryRequest FRequest;
	QList<IPhoneCallHistoryItem> FItems;
};

class CallHistoryTaskRemoveItems :
	public CallHistoryTask
{
public:
	CallHistoryTaskRemoveItems(const Jid &AStreamJid, const IPhoneCallHistoryRequest &ARequest);
	IPhoneCallHistoryRequest removeRequest() const;
protected:
	void run();
private:
	IPhoneCallHistoryRequest FRequest;
};

class CallHistoryWorker : 
	public QThread
{
	Q_OBJECT;
public:
	CallHistoryWorker(QObject *AParent);
	bool startTask(CallHistoryTask *ATask);
public slots:
	void quit();
signals:
	void taskFinished(CallHistoryTask *ATask);
protected:
	void run();
private:
	bool FQuit;
	QMutex FMutex;
	QWaitCondition FTaskReady;
	QQueue<CallHistoryTask *> FTasks;
};

#endif // CALLHISTORYWORKER_H
