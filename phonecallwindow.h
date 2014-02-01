#ifndef PHONECALLWINDOW_H
#define PHONECALLWINDOW_H

#include <QWidget>
#include <interfaces/iphonemanager.h>

class PhoneCallWindow : 
	public QWidget
{
	Q_OBJECT;
public:
	PhoneCallWindow(IPhoneManager *APhoneManager, IPhoneCall *ACall, QWidget *AParent = NULL);
	~PhoneCallWindow();
protected:
	void saveWindowGeometry();
	void restoreWindowGeometry();
protected:
	void closeEvent(QCloseEvent *AEvent);
protected slots:
	void onPhoneCallWidgetDestroyed();
private:
	IPhoneCall *FCall;
	bool FIsNumberCall;
};

#endif // PHONECALLWINDOW_H
