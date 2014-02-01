#ifndef CALLINFOWIDGET_H
#define CALLINFOWIDGET_H

#include <QWidget>
#include <QVariant>
#include "ui_callinfowidget.h"

class CallInfoWidget :
	public QFrame
{
	Q_OBJECT;
public:
	CallInfoWidget(QWidget *AParent);
	~CallInfoWidget();
	void setName(const QString &AName);
	void setInfo(const QString &AInfo);
	void setAvatar(const QVariant &AAvatar);
	void setError(const QString &AError);
private:
	Ui::CallInfoWidget ui;
};

#endif // CALLINFOWIDGET_H
