#include "phonecallwindow.h"

#include <QVBoxLayout>
#include <definitions/resources.h>
#include <definitions/phonemanager/menuicons.h>
#include <utils/widgetmanager.h>
#include <utils/iconstorage.h>
#include <utils/options.h>
#include "phonecallwidget.h"

PhoneCallWindow::PhoneCallWindow(IPhoneManager *APhoneManager, IPhoneCall *ACall, QWidget *AParent) : QWidget(AParent)
{
	setAttribute(Qt::WA_DeleteOnClose,true);
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(this,MNI_PHONEMANAGER_CALL,0,0,"windowIcon");

	FCall = ACall;
	FIsNumberCall = ACall->isNumberCall();

	setLayout(new QVBoxLayout);
	layout()->setMargin(0);

	PhoneCallWidget *callWidget = new PhoneCallWidget(APhoneManager,ACall,this);
	connect(callWidget,SIGNAL(destroyed(QObject *)),SLOT(onPhoneCallWidgetDestroyed()));
	layout()->addWidget(callWidget);

	setWindowTitle(tr("Call with %1").arg(APhoneManager->callContactName(ACall)));
	restoreWindowGeometry();
}

PhoneCallWindow::~PhoneCallWindow()
{
}

void PhoneCallWindow::saveWindowGeometry()
{
	Options::setFileValue(saveGeometry(),"phonemanager.phonecallwindow.geometry",FIsNumberCall ? "number-call" : "direct-call");
}

void PhoneCallWindow::restoreWindowGeometry()
{
	if (!restoreGeometry(Options::fileValue("phonemanager.phonecallwindow.geometry",FIsNumberCall ? "number-call" : "direct-call").toByteArray()))
		setGeometry(WidgetManager::alignGeometry(FCall->isNumberCall() ? minimumSizeHint() : sizeHint(), this));
}

void PhoneCallWindow::closeEvent(QCloseEvent *AEvent)
{
	saveWindowGeometry();
	QWidget::closeEvent(AEvent);
}

void PhoneCallWindow::onPhoneCallWidgetDestroyed()
{
	close();
}
