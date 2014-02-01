#ifndef EDITNUMBERSWIDGET_H
#define EDITNUMBERSWIDGET_H

#include <QFrame>
#include <interfaces/iphonemanager.h>
#include "ui_editnumberswidget.h"

class EditNumbersWidget : 
	public QFrame
{
	Q_OBJECT;
public:
	EditNumbersWidget(IPhoneManager *APhoneManager, QWidget *AParent);
	~EditNumbersWidget();
	QString currentNumber() const;
	bool setCurrentNumber(const QString &ANumber);
	void setNumbersEditable(bool AEditable);
	void setAvailNumbers(const QStringList &ANumbers);
signals:
	void closeRequested();
	void appendRequested(const QString &ANumber);
	void removeRequested(const QString &ANumber);
protected slots:
	void onAppendNumberButtonClicked();
	void onRemoveNumberButtonClicked();
	void onSaveNumberButtonClicked();
	void onCancelNumberButtonClicked();
protected slots:
	void onNumberTextChanged(const QString &AText);
private:
	Ui::EditNumbersWidget ui;
private:
	IPhoneManager *FPhoneManager;
private:
	bool FNumbersEditable;
};

#endif // EDITNUMBERSWIDGET_H
