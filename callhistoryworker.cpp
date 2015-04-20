#include "callhistoryworker.h"

#include <QSqlQuery>
#include <QStringList>
#include <QMetaObject>
#include <QMutexLocker>
#include <definitions/phonemanager/internalerrors.h>
#include <utils/logger.h>

#define DATABASE_STRUCTURE_VERSION     1
#define DATABASE_COMPATIBLE_VERSION    1

// CallHistoryTask
quint32 CallHistoryTask::FTaskCount = 0;
CallHistoryTask::CallHistoryTask(const Jid &AStreamJid, Type AType)
{
	FType = AType;
	FStreamJid = AStreamJid;
	FTaskId = QString("PhoneCallHistoryTask_%1").arg(++FTaskCount);

	setAutoDelete(false);
}

CallHistoryTask::Type CallHistoryTask::type() const
{
	return FType;
}

Jid CallHistoryTask::streamJid() const
{
	return FStreamJid;
}

QString CallHistoryTask::taskId() const
{
	return FTaskId;
}

bool CallHistoryTask::hasError() const
{
	return !FError.isNull();
}

XmppError CallHistoryTask::error() const
{
	return FError;
}

QString CallHistoryTask::databaseConnection() const
{
	return QString("PhoneCallHistoryDatabase-%1").arg(FStreamJid.pBare());
}

void CallHistoryTask::setSQLError(const QSqlError &AError)
{
	QString text = !AError.databaseText().isEmpty() ? AError.databaseText() : AError.driverText();
	FError = XmppError(IERR_PHONEMANAGER_HISTORY_DB_EXEC_FAILED,text);
	Logger::reportError("CallHistoryTask",QString("Failed to execute SQL command: %1").arg(text),false);
}


// CallHistoryTaskOpenDatabase
CallHistoryTaskOpenDatabase::CallHistoryTaskOpenDatabase(const Jid &AStreamJid, const QString &ADatabasePath) : CallHistoryTask(AStreamJid,OpenDatabase)
{
	FDatabasePath = ADatabasePath;
}

QMap<QString,QString> CallHistoryTaskOpenDatabase::databaseProperties() const
{
	return FProperties;
}

void CallHistoryTaskOpenDatabase::run()
{
	QString connection = databaseConnection();
	if (!QSqlDatabase::contains(connection))
	{
		bool initialized = false;

		// QSqlDatabase should be destroyed before removing connection
		{
			QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE",connection);
			db.setDatabaseName(FDatabasePath);

			if (!db.isValid())
				FError = XmppError(IERR_PHONEMANAGER_HISTORY_DB_NOT_CREATED,db.lastError().driverText());
			else if (!db.open())
				FError = XmppError(IERR_PHONEMANAGER_HISTORY_DB_NOT_OPENED,db.lastError().driverText());
			else if (!initializeDatabase(db))
				db.close();
			else
				initialized = true;
		}

		if (!initialized)
		{
			QSqlDatabase::removeDatabase(connection);
			Logger::reportError("CallHistoryTaskOpenDatabase",QString("Failed to initialize phone call history database: %1").arg(FError.errorMessage()),false);
		}
	}

}

bool CallHistoryTaskOpenDatabase::initializeDatabase(QSqlDatabase &ADatabase)
{
	QSqlQuery query(ADatabase);

	if (ADatabase.tables().contains("properties"))
	{
		if (query.exec("SELECT property, value FROM properties"))
		{
			while (query.next())
				FProperties.insert(query.value(0).toString(),query.value(1).toString());
		}
		else
		{
			setSQLError(query.lastError());
			return false;
		}
	}

	int structureVersion = FProperties.value("StructureVersion").toInt();
	int compatibleVersion = FProperties.value("CompatibleVersion").toInt();
	if (structureVersion < DATABASE_STRUCTURE_VERSION)
	{
		static const struct { QString query; int compatible; } databaseUpdates[] = 
		{
			{
				"CREATE TABLE properties ("
					"  property         TEXT,"
					"  value            TEXT"
					");"

					"CREATE TABLE history ("
					"  start            DATETIME NOT NULL,"
					"  with             TEXT NOT NULL,"
					"  role             INTEGER NOT NULL,"
					"  hangup_code      INTEGER NOT NULL,"
					"  duration         INTEGER NOT NULL,"
					"  is_number_call   BOOLEAN NOT NULL"
					");"

					"CREATE UNIQUE INDEX properties_property ON properties ("
					"  property         ASC"
					");"

					"CREATE UNIQUE INDEX history_start_with ON history ("
					"  start         DESC,"
					"  with          ASC"
					");"

					"INSERT INTO properties(property,value) VALUES('StructureVersion','1');"
					"INSERT INTO properties(property,value) VALUES('CompatibleVersion','1');"
					, 1
			} 
		};

		ADatabase.transaction();
		QSqlQuery query(ADatabase);
		for (int i=structureVersion; i<DATABASE_STRUCTURE_VERSION; i++)
		{
			QStringList commands = databaseUpdates[i].query.split(';',QString::SkipEmptyParts);
			foreach(const QString &command, commands)
			{
				if (!query.exec(command))
				{
					setSQLError(query.lastError());
					ADatabase.rollback();
					return false;
				}
			}
		}
		ADatabase.commit();

		FProperties.insert("StructureVersion",QString::number(DATABASE_STRUCTURE_VERSION));
		FProperties.insert("CompatibleVersion",QString::number(databaseUpdates[DATABASE_STRUCTURE_VERSION-1].compatible));
	}
	else if (compatibleVersion > DATABASE_STRUCTURE_VERSION)
	{
		FError = XmppError(IERR_PHONEMANAGER_HISTORY_DB_NOT_COMPATIBLE);
		return false;
	}

	return true;
}


// CallHistoryTaskCloseDatabase
CallHistoryTaskCloseDatabase::CallHistoryTaskCloseDatabase(const Jid &AStreamJid) : CallHistoryTask(AStreamJid,CloseDatabase)
{

}

void CallHistoryTaskCloseDatabase::run()
{
	QString connection = databaseConnection();
	if (QSqlDatabase::contains(connection))
		QSqlDatabase::removeDatabase(connection);
}


//CallHistoryTaskSaveItem
CallHistoryTaskSaveItem::CallHistoryTaskSaveItem(const Jid &AStreamJid, const IPhoneCallHistoryItem &AItem) : CallHistoryTask(AStreamJid,SaveHistoryItem)
{
	FItem = AItem;
}

IPhoneCallHistoryItem CallHistoryTaskSaveItem::savedItem() const
{
	return FItem;
}

void CallHistoryTaskSaveItem::run()
{
	QSqlDatabase db = QSqlDatabase::database(databaseConnection());
	if (db.isOpen())
	{
		QSqlQuery query(db);
		if (query.prepare("INSERT INTO history(start,with,role,hangup_code,duration,is_number_call) VALUES (?,?,?,?,?,?)"))
		{
			query.addBindValue(FItem.start);
			query.addBindValue(FItem.with);
			query.addBindValue(FItem.role);
			query.addBindValue(FItem.hangupCode);
			query.addBindValue(FItem.duration);
			query.addBindValue(FItem.isNumberCall);

			if (!query.exec())
				setSQLError(query.lastError());
		}
		else
		{
			setSQLError(query.lastError());
		}
	}
	else
	{
		FError = XmppError(IERR_PHONEMANAGER_HISTORY_DB_NOT_OPENED);
	}
}


// CallHistoryTaskLoadItems
CallHistoryTaskLoadItems::CallHistoryTaskLoadItems(const Jid &AStreamJid, const IPhoneCallHistoryRequest &ARequest) : CallHistoryTask(AStreamJid,LoadHistoryItems)
{
	FRequest = ARequest;
}

QList<IPhoneCallHistoryItem> CallHistoryTaskLoadItems::loadedItems() const
{
	return FItems;
}

void CallHistoryTaskLoadItems::run()
{
	QSqlDatabase db = QSqlDatabase::database(databaseConnection());
	if (db.isOpen())
	{
		QVariantList bindValues;
		QString command = "SELECT %1, with, role, hangup_code, duration, is_number_call FROM history";
		if (FRequest.grouping == IPhoneCallHistoryRequest::NoGrouping)
			command = command.arg("start");
		else if (FRequest.order == Qt::AscendingOrder)
			command = command.arg("MIN(start)");
		else
			command = command.arg("MAX(start)");

		QStringList conditions;
		if (FRequest.start.isValid())
		{
			conditions.append("(start >= ?)");
			bindValues.append(FRequest.start);
		}
		if (FRequest.end.isValid())
		{
			conditions.append("(start <= ?)");
			bindValues.append(FRequest.end);
		}
		if (!FRequest.with.isEmpty())
		{
			conditions.append("(with LIKE ?)");
			bindValues.append(FRequest.with);
		}
		if (FRequest.filtering & IPhoneCallHistoryRequest::FilterNumberCalls)
		{
			conditions.append("(is_number_call = ?)");
			bindValues.append(true); //-V601
		}
		if (FRequest.filtering & IPhoneCallHistoryRequest::FilterDirectCalls)
		{
			conditions.append("(is_number_call = ?)");
			bindValues.append(false);
		}
		if (!conditions.isEmpty())
		{
			command += QString (" WHERE %1").arg(conditions.join(" AND "));
		}

		switch (FRequest.grouping)
		{
		case IPhoneCallHistoryRequest::GroupByWith:
			command += " GROUP BY with";
			break;
		case IPhoneCallHistoryRequest::GroupByYear:
			command += " GROUP BY strftime('%Y',start)";
			break;
		case IPhoneCallHistoryRequest::GroupByMonth:
			command += " GROUP BY strftime('%m',start)";
			break;
		case IPhoneCallHistoryRequest::GroupByDay:
			command += " GROUP BY strftime('%d',start)";
			break;
		default:
			break;
		}

		switch (FRequest.sorting)
		{
		case IPhoneCallHistoryRequest::SortByStart:
			command += QString(" ORDER BY start %1").arg(FRequest.order==Qt::AscendingOrder ? "ASC" : "DESC");
			break;
		default:
			break;
		}

		if (FRequest.maxItems > 0)
			command += QString(" LIMIT %1").arg(FRequest.maxItems);

		QSqlQuery query(db);
		if (query.prepare(command))
		{
			foreach(const QVariant &value, bindValues)
				query.addBindValue(value);

			if (query.exec())
			{
				while (query.next())
				{
					IPhoneCallHistoryItem item;
					item.start = query.value(0).toDateTime();
					item.with = query.value(1).toString();
					item.role = query.value(2).toInt();
					item.hangupCode = query.value(3).toInt();
					item.duration = query.value(4).toInt();
					item.isNumberCall = query.value(5).toBool();
					FItems.append(item);
				}
			}
			else
			{
				setSQLError(query.lastError());
			}
		}
		else
		{
			setSQLError(query.lastError());
		}
	}
	else
	{
		FError = XmppError(IERR_PHONEMANAGER_HISTORY_DB_NOT_OPENED);
	}
}

// CallHistoryTaskRemoveItems
CallHistoryTaskRemoveItems::CallHistoryTaskRemoveItems(const Jid &AStreamJid, const IPhoneCallHistoryRequest &ARequest) : CallHistoryTask(AStreamJid,RemoveHistoryItems)
{
	FRequest = ARequest;
}

IPhoneCallHistoryRequest CallHistoryTaskRemoveItems::removeRequest() const
{
	return FRequest;
}

void CallHistoryTaskRemoveItems::run()
{
	QSqlDatabase db = QSqlDatabase::database(databaseConnection());
	if (db.isOpen())
	{
		QVariantList bindValues;
		QString command = "DELETE FROM history";

		QStringList conditions;
		if (FRequest.start.isValid())
		{
			conditions.append("(start >= ?)");
			bindValues.append(FRequest.start);
		}
		if (FRequest.end.isValid())
		{
			conditions.append("(start <= ?)");
			bindValues.append(FRequest.end);
		}
		if (!FRequest.with.isEmpty())
		{
			conditions.append("(with LIKE ?)");
			bindValues.append(FRequest.with);
		}
		if (FRequest.filtering & IPhoneCallHistoryRequest::FilterNumberCalls)
		{
			conditions.append("(is_number_call = ?)");
			bindValues.append(true);
		}
		if (FRequest.filtering & IPhoneCallHistoryRequest::FilterDirectCalls)
		{
			conditions.append("(is_number_call = ?)");
			bindValues.append(false);
		}
		if (!conditions.isEmpty())
		{
			command += QString (" WHERE %1").arg(conditions.join(" AND "));
		}

		QSqlQuery query(db);
		if (query.prepare(command))
		{
			foreach(const QVariant &value, bindValues)
				query.addBindValue(value);

			if (!query.exec())
				setSQLError(query.lastError());
		}
		else
		{
			setSQLError(query.lastError());
		}
	}
	else
	{
		FError = XmppError(IERR_PHONEMANAGER_HISTORY_DB_NOT_OPENED);
	}
}

// CallHistoryWorker
CallHistoryWorker::CallHistoryWorker(QObject *AParent) : QThread(AParent)
{
	FQuit = false;
	qRegisterMetaType<CallHistoryTask *>("CallHistoryTask *");
}

bool CallHistoryWorker::startTask(CallHistoryTask *ATask)
{
	QMutexLocker locker(&FMutex);
	if (!FQuit)
	{
		FTasks.enqueue(ATask);
		FTaskReady.wakeAll();
		return true;
	}
	return false;
}

void CallHistoryWorker::quit()
{
	QMutexLocker locker(&FMutex);
	FQuit = true;
	FTaskReady.wakeAll();
}

void CallHistoryWorker::run()
{
	QMutexLocker locker(&FMutex);
	while (!FQuit || !FTasks.isEmpty())
	{
		CallHistoryTask *task = !FTasks.isEmpty() ? FTasks.dequeue() : NULL;
		if (task)
		{
			locker.unlock();
			task->run();
			QMetaObject::invokeMethod(this,"taskFinished",Qt::QueuedConnection,Q_ARG(CallHistoryTask *,task));
			locker.relock();
		}
		else
		{
			FTaskReady.wait(locker.mutex());
		}
	}
}
