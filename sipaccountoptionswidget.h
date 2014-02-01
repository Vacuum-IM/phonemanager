#ifndef SIPACCOUNTOPTIONSWIDGET_H
#define SIPACCOUNTOPTIONSWIDGET_H

#include <QWidget>
#include <interfaces/ioptionsmanager.h>
#include "ui_sipaccountoptionswidget.h"

class SipAccountOptionsWidget : 
	public QWidget,
	public IOptionsWidget
{
	Q_OBJECT;
	Q_INTERFACES(IOptionsWidget);
public:
	SipAccountOptionsWidget(QWidget *AParent);
	~SipAccountOptionsWidget();
	virtual QWidget* instance() { return this; }
public slots:
	virtual void apply();
	virtual void reset();
signals:
	void modified();
	void childApply();
	void childReset();
private:
	Ui::SipAccountOptionsWidget ui;
};

#endif // SIPACCOUNTOPTIONSWIDGET_H
