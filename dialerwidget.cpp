#include "dialerwidget.h"

#include <definitions/resources.h>
#include <definitions/phonemanager/menuicons.h>
#include <utils/iconstorage.h>

static const QString DialerWidgetStyleSheet =
"QToolButton           "
"{                     "
"  min-width: 50px;    "
"  min-height: 20px;   "
"}                     "
;

DialerWidget::DialerWidget(QWidget *AParent) : QFrame(AParent)
{
	ui.setupUi(this);
	setStyleSheet(DialerWidgetStyleSheet);

	FNumberMapper.setMapping(ui.tlbNumber_1,"1");
	connect(ui.tlbNumber_1,SIGNAL(clicked()),&FNumberMapper,SLOT(map()));
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.tlbNumber_1,MNI_PHONEMANAGER_DIALER_1);

	FNumberMapper.setMapping(ui.tlbNumber_2,"2");
	connect(ui.tlbNumber_2,SIGNAL(clicked()),&FNumberMapper,SLOT(map()));
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.tlbNumber_2,MNI_PHONEMANAGER_DIALER_2);

	FNumberMapper.setMapping(ui.tlbNumber_3,"3");
	connect(ui.tlbNumber_3,SIGNAL(clicked()),&FNumberMapper,SLOT(map()));
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.tlbNumber_3,MNI_PHONEMANAGER_DIALER_3);

	FNumberMapper.setMapping(ui.tlbNumber_4,"4");
	connect(ui.tlbNumber_4,SIGNAL(clicked()),&FNumberMapper,SLOT(map()));
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.tlbNumber_4,MNI_PHONEMANAGER_DIALER_4);

	FNumberMapper.setMapping(ui.tlbNumber_5,"5");
	connect(ui.tlbNumber_5,SIGNAL(clicked()),&FNumberMapper,SLOT(map()));
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.tlbNumber_5,MNI_PHONEMANAGER_DIALER_5);

	FNumberMapper.setMapping(ui.tlbNumber_6,"6");
	connect(ui.tlbNumber_6,SIGNAL(clicked()),&FNumberMapper,SLOT(map()));
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.tlbNumber_6,MNI_PHONEMANAGER_DIALER_6);

	FNumberMapper.setMapping(ui.tlbNumber_7,"7");
	connect(ui.tlbNumber_7,SIGNAL(clicked()),&FNumberMapper,SLOT(map()));
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.tlbNumber_7,MNI_PHONEMANAGER_DIALER_7);

	FNumberMapper.setMapping(ui.tlbNumber_8,"8");
	connect(ui.tlbNumber_8,SIGNAL(clicked()),&FNumberMapper,SLOT(map()));
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.tlbNumber_8,MNI_PHONEMANAGER_DIALER_8);

	FNumberMapper.setMapping(ui.tlbNumber_9,"9");
	connect(ui.tlbNumber_9,SIGNAL(clicked()),&FNumberMapper,SLOT(map()));
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.tlbNumber_9,MNI_PHONEMANAGER_DIALER_9);

	FNumberMapper.setMapping(ui.tlbNumber_10,"*");
	connect(ui.tlbNumber_10,SIGNAL(clicked()),&FNumberMapper,SLOT(map()));
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.tlbNumber_10,MNI_PHONEMANAGER_DIALER_10);

	FNumberMapper.setMapping(ui.tlbNumber_11,"0");
	connect(ui.tlbNumber_11,SIGNAL(clicked()),&FNumberMapper,SLOT(map()));
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.tlbNumber_11,MNI_PHONEMANAGER_DIALER_11);

	FNumberMapper.setMapping(ui.tlbNumber_12,"#");
	connect(ui.tlbNumber_12,SIGNAL(clicked()),&FNumberMapper,SLOT(map()));
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(ui.tlbNumber_12,MNI_PHONEMANAGER_DIALER_12);

	connect(&FNumberMapper,SIGNAL(mapped(const QString &)),SLOT(onNumberButtonMapped(const QString &)));

	connect(ui.lneNumber,SIGNAL(textChanged(const QString &)),SLOT(onNumberTextChanged(const QString &)));
}

DialerWidget::~DialerWidget()
{

}

QString DialerWidget::number() const
{
	return ui.lneNumber->text();
}

void DialerWidget::setNumber(const QString &ANumber)
{
	ui.lneNumber->setText(ANumber);
}

bool DialerWidget::isTextInputEnabled() const
{
	return !ui.lneNumber->isReadOnly();
}

void DialerWidget::setTextInputEnabled(bool AEnabled)
{
	ui.lneNumber->setReadOnly(!AEnabled);
}

QString DialerWidget::convertTextToNumber(const QString &AText) const
{
	static const QString SubsStringKey   = "0123456789abcdefghijklmnopqrstuvwxyz *#+-()";
	static const QString SubsStringValue = "012345678922233344455566677778889999 *#+-()";

	QString newNumber = AText;
	for (int i=0; i<newNumber.length(); )
	{
		int index = SubsStringKey.indexOf(newNumber.at(i).toLower());
		if (index >= 0)
		{
			newNumber[i] = SubsStringValue[index];
			i++;
		}
		else
		{
			newNumber.remove(i,1);
		}
	}
	return newNumber;
}

void DialerWidget::onNumberTextChanged(const QString &AText)
{
	QString newNumber = convertTextToNumber(AText);
	if (newNumber != AText)
	{
		int pos = ui.lneNumber->cursorPosition() - (AText.length()-newNumber.length());
		ui.lneNumber->setText(newNumber);
		ui.lneNumber->setCursorPosition(pos);
	}
	else
	{
		emit numberChanged();
	}
}

void DialerWidget::onNumberButtonMapped(const QString &AText)
{
	ui.lneNumber->insert(AText);
	ui.lneNumber->setFocus();
	emit numberClicked(AText.at(0));
}
