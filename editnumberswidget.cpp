#include "editnumberswidget.h"

#include <QClipboard>
#include <QMessageBox>
#include <definitions/resources.h>
#include <definitions/phonemanager/menuicons.h>
#include <utils/iconstorage.h>

EditNumbersWidget::EditNumbersWidget(IPhoneManager *APhoneManager, QWidget *AParent) : QFrame(AParent)
{
	ui.setupUi(this);
	FPhoneManager = APhoneManager;

	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.tlbAppend,MNI_PHONEMANAGER_NUMBER_APPEND);
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.tlbRemove,MNI_PHONEMANAGER_NUMBER_REMOVE);
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.tlbSave,MNI_PHONEMANAGER_NUMBER_SAVE);
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.tlbCancel,MNI_PHONEMANAGER_NUMBER_CANCEL);

	connect(ui.cbtClose,SIGNAL(clicked()),SIGNAL(closeRequested()));
	connect(ui.tlbAppend,SIGNAL(clicked()),SLOT(onAppendNumberButtonClicked()));
	connect(ui.tlbRemove,SIGNAL(clicked()),SLOT(onRemoveNumberButtonClicked()));
	connect(ui.tlbSave,SIGNAL(clicked()),SLOT(onSaveNumberButtonClicked()));
	connect(ui.tlbCancel,SIGNAL(clicked()),SLOT(onCancelNumberButtonClicked()));
	connect(ui.lneNumber,SIGNAL(textChanged(const QString &)),SLOT(onNumberTextChanged(const QString &)));

	ui.wdtView->setVisible(true);
	ui.wdtEdit->setVisible(false);

	setNumbersEditable(false);
}

EditNumbersWidget::~EditNumbersWidget()
{

}

QString EditNumbersWidget::currentNumber() const
{
	return ui.cmbNumbers->itemData(ui.cmbNumbers->currentIndex()).toString();
}

bool EditNumbersWidget::setCurrentNumber(const QString &ANumber)
{
	int index = ui.cmbNumbers->findData(FPhoneManager->normalizedNumber(ANumber));
	if (index >= 0)
	{
		ui.cmbNumbers->setCurrentIndex(index);
		return true;
	}
	return false;
}

void EditNumbersWidget::setNumbersEditable(bool AEditable)
{
	FNumbersEditable = AEditable;

	ui.tlbAppend->setEnabled(FNumbersEditable);
	ui.tlbRemove->setEnabled(FNumbersEditable);

	onNumberTextChanged(ui.lneNumber->text());
}

void EditNumbersWidget::setAvailNumbers(const QStringList &ANumbers)
{
	QString number = currentNumber();

	ui.cmbNumbers->clear();
	for (int i=0; i<ANumbers.count(); i++)
	{
		QString number = ANumbers.at(i);
		ui.cmbNumbers->addItem(FPhoneManager->formattedNumber(number),number);
	}
	setCurrentNumber(number);
}

void EditNumbersWidget::onAppendNumberButtonClicked()
{
	ui.wdtView->setVisible(false);
	ui.wdtEdit->setVisible(true);

	QString number = QApplication::clipboard()->text();
	if (FPhoneManager->isValidNumber(number))
		ui.lneNumber->setText(FPhoneManager->formattedNumber(number));
	else
		ui.lneNumber->clear();
	ui.lneNumber->setFocus();
}

void EditNumbersWidget::onRemoveNumberButtonClicked()
{
	QString number = currentNumber();
	if (!number.isEmpty())
	{
		int button = QMessageBox::question(this,tr("Remove Phone Number"),
			tr("Are you sure you want to delete the number <b>%1</b>?").arg(FPhoneManager->formattedNumber(number)),
			QMessageBox::Yes|QMessageBox::No);
		if (button == QMessageBox::Yes)
		{
			emit removeRequested(number);
		}
	}
}

void EditNumbersWidget::onSaveNumberButtonClicked()
{
	QString number = ui.lneNumber->text();
	if (FPhoneManager->isValidNumber(number))
	{
		emit appendRequested(ui.lneNumber->text().trimmed());
		onCancelNumberButtonClicked();
	}
}

void EditNumbersWidget::onCancelNumberButtonClicked()
{
	ui.wdtView->setVisible(true);
	ui.wdtEdit->setVisible(false);
}

void EditNumbersWidget::onNumberTextChanged(const QString &AText)
{
	ui.tlbSave->setEnabled(FNumbersEditable && FPhoneManager->isValidNumber(AText));
}
