#ifndef DIALERWIDGET_H
#define DIALERWIDGET_H

#include <QWidget>
#include <QSignalMapper>
#include "ui_dialerwidget.h"

class DialerWidget : 
	public QFrame
{
	Q_OBJECT;
public:
	DialerWidget(QWidget *AParent);
	~DialerWidget();
	QString number() const;
	void setNumber(const QString &ANumber);
	bool isTextInputEnabled() const;
	void setTextInputEnabled(bool AEnabled);
signals:
	void numberChanged();
	void numberClicked(const QChar &AChar);
protected:
	QString convertTextToNumber(const QString &AText) const;
protected slots:
	void onNumberTextChanged(const QString &AText);
	void onNumberButtonMapped(const QString &AText);
private:
	Ui::DialerWidget ui;
private:
	QSignalMapper FNumberMapper;
};

#endif // DIALERWIDGET_H
