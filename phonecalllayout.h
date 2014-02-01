#ifndef PHONECALLLAYOUT_H
#define PHONECALLLAYOUT_H

#include <QRectF>
#include <QLayout>
#include <QWidget>
#include <QPropertyAnimation>
#include "videoframe.h"

class SimpleAnimation : 
	public QVariantAnimation
{
	Q_OBJECT;
protected:
	virtual void updateCurrentValue(const QVariant &AValue) { 
		Q_UNUSED(AValue); 
	}
};

class PhoneCallLayout : 
	public QLayout
{
	Q_OBJECT;
public:
	PhoneCallLayout(QWidget *AParent);
	~PhoneCallLayout();
	// QLayout
	int count() const;
	void addItem(QLayoutItem *AItem);
	QLayoutItem *itemAt(int AIndex) const;
	QLayoutItem *takeAt(int AIndex);
	// QLayoutItem
	QSize sizeHint() const;
	QSize minimumSize() const;
	void setGeometry(const QRect &ARect);
	// PhoneCallLayout
	int locaVideoMargin() const;
	void setLocalVideoMargin(int AMargin);
	bool isControlsVisible() const;
	void setControlsVisible(bool AVisible);
	void setControlWidget(QWidget *AControls);
	void setDialerWidget(QWidget *ADialer);
	void setInfoWidget(QWidget *AInfoWidget);
	void setLocalVideoFrame(VideoFrame *AFrame);
	void setRemoteVideoFrame(VideoFrame *AFrame);
	void saveLocalVideoGeometry();
	void restoreLocalVideoGeometry();
protected:
	// QLayoutItem
	void invalidate();
	// PhoneCallLayout
	void updateWidgetsZOrder();
	void updateLocalVideoGeometryScale();
	Qt::Alignment remoteVideoAlignment() const;
	Qt::Alignment geometryAlignment(const QRect &AGeometry) const;
	QRect adjustRemoteVideoPosition(const QRect &AGeometry) const;
	QRect adjustLocalVideoSize(const QRect &AGeometry) const;
	QRect adjustLocalVideoPosition(const QRect &AGeometry) const;
	QRect correctLocalVideoPosition(const QRect &AGeometry) const;
	QRect correctLocalVideoSize(Qt::Corner ACorner, const QRect &AGeometry) const;
protected slots:
	void onLocalVideoMove(const QPoint &APos);
	void onLocalVideoResize(Qt::Corner ACorner, const QPoint &APos);
	void onControlsVisibilityPercentChanged(const QVariant &AValue);
private:
	int FLocalMargin;
	int FLocalStickDelta;
	QRectF FLocalScale;
	QWidget *FInfoWidget;
	QWidget *FDialerWidget;
	VideoFrame *FLocalVideo;
	VideoFrame *FRemoteVideo;
private:
	bool FCtrlVisible;
	double FCtrlVisiblePerc;
	QWidget *FCtrlWidget;
	SimpleAnimation FCtrlAnimation;
private:
	QRect FVideoRect;
	QList<QLayoutItem *> FLayoutItems;
};

#endif // PHONECALLLAYOUT_H
