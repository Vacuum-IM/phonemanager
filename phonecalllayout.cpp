#include "phonecalllayout.h"

#include <QStyle>
#include <utils/options.h>
#include <utils/widgetmanager.h>

PhoneCallLayout::PhoneCallLayout(QWidget *AParent) : QLayout(AParent)
{
	FLocalMargin = 0;
	FLocalStickDelta = 5;

	FCtrlVisible = true;
	FCtrlVisiblePerc = 100;

	FCtrlWidget = NULL;
	FInfoWidget = NULL;
	FDialerWidget = NULL;
	FLocalVideo = NULL;
	FRemoteVideo = NULL;

	connect(&FCtrlAnimation,SIGNAL(valueChanged(const QVariant &)),SLOT(onControlsVisibilityPercentChanged(const QVariant &)));
}

PhoneCallLayout::~PhoneCallLayout()
{

}

int PhoneCallLayout::count() const
{
	return FLayoutItems.count();
}

void PhoneCallLayout::addItem(QLayoutItem *AItem)
{
	FLayoutItems.append(AItem);
}

QLayoutItem *PhoneCallLayout::itemAt(int AIndex) const
{
	return FLayoutItems.value(AIndex);
}

QLayoutItem *PhoneCallLayout::takeAt(int AIndex)
{
	return AIndex>=0 && AIndex<FLayoutItems.count() ? FLayoutItems.takeAt(AIndex) : NULL;
}

QSize PhoneCallLayout::sizeHint() const
{
	QSize hint(0,0);
	if (FCtrlWidget)
		hint = hint.expandedTo(FCtrlWidget->sizeHint());
	if (FInfoWidget || FLocalVideo || FRemoteVideo)
		hint = hint.expandedTo(QSize(450,250));
	return hint;
}

QSize PhoneCallLayout::minimumSize() const
{
	QSize hint(260,200);

	if (FInfoWidget)
		hint = hint.expandedTo(FInfoWidget->minimumSizeHint());

	if (FLocalVideo)
		hint = hint.expandedTo(FLocalVideo->minimumVideoSize());

	if (FRemoteVideo)
		hint = hint.expandedTo(FRemoteVideo->minimumVideoSize());

	if (FCtrlWidget)
	{
		QSize controlsSize = FCtrlWidget->sizeHint();
		hint.rheight() += controlsSize.height() + spacing();
		hint.rwidth() = qMax(controlsSize.width(),hint.width());
	}

	return hint;
}

void PhoneCallLayout::setGeometry(const QRect &ARect)
{
	QLayout::setGeometry(ARect);
	if (!ARect.isEmpty())
	{
		FVideoRect = ARect;
		if (FCtrlWidget)
		{
			QSize controlsSize = QSize(FVideoRect.width(),FCtrlWidget->sizeHint().height());
			QRect controlRect = QStyle::alignedRect(Qt::LeftToRight,Qt::AlignBottom|Qt::AlignHCenter,controlsSize,FVideoRect);
			controlRect.moveTop((controlRect.top()-FVideoRect.bottom())*(int)FCtrlVisiblePerc/100 + FVideoRect.bottom() + 1);
			FCtrlWidget->setGeometry(controlRect);
			FVideoRect.adjust(0,0,0,-controlRect.height());
		}

		if (FDialerWidget)
		{
			QSize dialerSize = FDialerWidget->minimumSizeHint();
			dialerSize.setHeight(FVideoRect.height());
			QRect dialerRect = QStyle::alignedRect(Qt::LeftToRight,Qt::AlignLeft|Qt::AlignVCenter,dialerSize,FVideoRect);
			FDialerWidget->setGeometry(dialerRect);
			FVideoRect.adjust(dialerRect.width(),0,0,0);
		}

		if (FInfoWidget)
		{
			FInfoWidget->setGeometry(FVideoRect);
		}

		if (FLocalVideo)
		{
			QRect localRect = adjustLocalVideoSize(FLocalVideo->geometry());
			localRect =	adjustLocalVideoPosition(localRect);
			FLocalVideo->setGeometry(localRect);
			FLocalVideo->setMaximumVideoSize(FVideoRect.size()/2);
			FLocalVideo->setAlignment(geometryAlignment(localRect));
		}

		if (FRemoteVideo)
		{
			QSize remoteSize = FRemoteVideo->sizeHint();
			remoteSize.scale(FVideoRect.size(),Qt::KeepAspectRatio);
			QRect remoteRect = QStyle::alignedRect(Qt::LeftToRight,remoteVideoAlignment(),remoteSize,FVideoRect);
			FRemoteVideo->setGeometry(adjustRemoteVideoPosition(remoteRect));
		}
	}
}

int PhoneCallLayout::locaVideoMargin() const
{
	return FLocalMargin;
}

void PhoneCallLayout::setLocalVideoMargin(int AMargin)
{
	if (FLocalMargin!=AMargin && 0<=AMargin && AMargin<=FLocalStickDelta)
	{
		FLocalMargin = AMargin;
		update();
	}
}

bool PhoneCallLayout::isControlsVisible() const
{
	return FCtrlVisible;
}

void PhoneCallLayout::setControlsVisible(bool AVisible)
{
	if (FCtrlVisible != AVisible)
	{
		FCtrlVisible = AVisible;
		FCtrlAnimation.stop();
		FCtrlAnimation.setEasingCurve(AVisible ? QEasingCurve::OutBack : QEasingCurve::InBack);
		FCtrlAnimation.setStartValue(FCtrlVisiblePerc);
		FCtrlAnimation.setEndValue(AVisible ? 100.0 : 0.0);
		FCtrlAnimation.setDuration(300 * qAbs((FCtrlAnimation.startValue().toInt() - FCtrlAnimation.endValue().toInt())) / 100);
		FCtrlAnimation.start();
	}
}

void PhoneCallLayout::setControlWidget(QWidget *AControls)
{
	if (FCtrlWidget != AControls)
	{
		if (FCtrlWidget)
		{
			removeWidget(FCtrlWidget);
			FCtrlWidget->hide();
		}

		FCtrlWidget = AControls;

		if (FCtrlWidget)
		{
			addWidget(FCtrlWidget);
			updateWidgetsZOrder();
		}

		FCtrlVisiblePerc = FCtrlVisible ? 100.0 : 0.0;
		invalidate();
	}
}

void PhoneCallLayout::setDialerWidget(QWidget *ADialer)
{
	if (FDialerWidget != ADialer)
	{
		if (FDialerWidget)
		{
			removeWidget(FDialerWidget);
			FDialerWidget->hide();
		}

		FDialerWidget = ADialer;

		if (FDialerWidget)
		{
			addWidget(FDialerWidget);
			updateWidgetsZOrder();
			FDialerWidget->show();
		}

		invalidate();
	}
}

void PhoneCallLayout::setInfoWidget(QWidget *AInfoWidget)
{
	if (FInfoWidget != AInfoWidget)
	{
		if (FInfoWidget)
		{
			removeWidget(FInfoWidget);
			FInfoWidget->hide();
		}

		FInfoWidget = AInfoWidget;

		if (FInfoWidget)
		{
			addWidget(FInfoWidget);
			updateWidgetsZOrder();
			FInfoWidget->show();
		}

		invalidate();
	}
}

void PhoneCallLayout::setLocalVideoFrame(VideoFrame *AFrame)
{
	if (FLocalVideo != AFrame)
	{
		if (FLocalVideo)
		{
			FLocalVideo->disconnect(this);
			saveLocalVideoGeometry();
			removeWidget(FLocalVideo);
			FLocalVideo->hide();
		}
		
		FLocalVideo = AFrame;

		if (FLocalVideo)
		{
			connect(FLocalVideo,SIGNAL(moveTo(const QPoint &)),SLOT(onLocalVideoMove(const QPoint &)));
			connect(FLocalVideo,SIGNAL(resizeTo(Qt::Corner, const QPoint &)),SLOT(onLocalVideoResize(Qt::Corner, const QPoint &)));

			addWidget(FLocalVideo);
			updateWidgetsZOrder();
			restoreLocalVideoGeometry();
			FLocalVideo->show();
		}

		invalidate();
	}
}

void PhoneCallLayout::setRemoteVideoFrame(VideoFrame *AFrame)
{
	if (FRemoteVideo != AFrame)
	{
		if (FRemoteVideo)
		{
			removeWidget(FRemoteVideo);
			FRemoteVideo->hide();
		}

		FRemoteVideo = AFrame;

		if (FRemoteVideo)
		{
			updateWidgetsZOrder();
			addWidget(FRemoteVideo);
			FRemoteVideo->show();
		}

		invalidate();
	}
}

void PhoneCallLayout::saveLocalVideoGeometry()
{
	if (FLocalVideo && !FLocalScale.isNull())
	{
		Options::setFileValue(FLocalScale,"phonemanager.phonecallwindow.layout.localvideo.scale");
		Options::setFileValue((int)FLocalVideo->alignment(),"phonemanager.phonecallwindow.layout.localvideo.alignment");
	}
}

void PhoneCallLayout::restoreLocalVideoGeometry()
{
	if (FLocalVideo)
	{
		FLocalScale = Options::fileValue("phonemanager.phonecallwindow.layout.localvideo.scale").toRectF();
		if (FLocalScale.isNull())
		{
			FLocalScale = QRectF(0.75,0.75,0.25,0.25);
			FLocalVideo->setAlignment(Qt::AlignRight|Qt::AlignBottom);
		}
		else
		{
			FLocalVideo->setAlignment((Qt::Alignment)Options::fileValue("phonemanager.phonecallwindow.layout.localvideo.alignment").toInt());
		}
	}
}

void PhoneCallLayout::invalidate()
{
	update();
}

void PhoneCallLayout::updateWidgetsZOrder()
{
	if (FRemoteVideo)
		FRemoteVideo->raise();
	if (FInfoWidget)
		FInfoWidget->raise();
	if (FDialerWidget)
		FDialerWidget->raise();
	if (FLocalVideo)
		FLocalVideo->raise();
	if (FCtrlWidget)
		FCtrlWidget->raise();
}

void PhoneCallLayout::updateLocalVideoGeometryScale()
{
	if (FLocalVideo && FLocalVideo->isVisible() && FVideoRect.width()>0 && FVideoRect.height()>0)
	{
		QRect localRect = FLocalVideo->geometry();
		FLocalScale.setLeft((qreal)localRect.left()/FVideoRect.width());
		FLocalScale.setRight((qreal)localRect.right()/FVideoRect.width());
		FLocalScale.setTop((qreal)localRect.top()/FVideoRect.height());
		FLocalScale.setBottom((qreal)localRect.bottom()/FVideoRect.height());
	}
}

Qt::Alignment PhoneCallLayout::remoteVideoAlignment() const
{
	if (FLocalVideo && FLocalVideo->isVisible())
	{
		Qt::Alignment remoteAlign = 0;
		Qt::Alignment localAlign = FLocalVideo->alignment();

		if (localAlign & Qt::AlignLeft)
			remoteAlign |= Qt::AlignRight;
		else if (localAlign & Qt::AlignRight)
			remoteAlign |= Qt::AlignLeft;
		else
			remoteAlign |= Qt::AlignHCenter;

		if (localAlign & Qt::AlignTop)
			remoteAlign |= Qt::AlignBottom;
		else if (localAlign & Qt::AlignBottom)
			remoteAlign |= Qt::AlignTop;
		else
			remoteAlign |= Qt::AlignVCenter;

		return remoteAlign;
	}
	return Qt::AlignCenter;
}

Qt::Alignment PhoneCallLayout::geometryAlignment(const QRect &AGeometry) const
{
	Qt::Alignment align = 0;

	int leftDelta = AGeometry.left() - FVideoRect.left();
	int topDelta = AGeometry.top() - FVideoRect.top();
	int rightDelta = FVideoRect.right() - AGeometry.right();
	int bottomDelta = FVideoRect.bottom() - AGeometry.bottom();

	if (leftDelta < FLocalStickDelta)
		align |= Qt::AlignLeft;
	else if (rightDelta < FLocalStickDelta)
		align |= Qt::AlignRight;
	if (topDelta < FLocalStickDelta)
		align |= Qt::AlignTop;
	else if (bottomDelta < FLocalStickDelta)
		align |= Qt::AlignBottom;
	return align;
}

QRect PhoneCallLayout::adjustRemoteVideoPosition(const QRect &AGeometry) const
{
	QRect newGeometry = AGeometry;
	if (FLocalVideo && FLocalVideo->isVisible() && FLocalVideo->alignment()!=0 && !AGeometry.intersects(FLocalVideo->geometry()))
	{
		newGeometry = QStyle::alignedRect(Qt::LeftToRight,Qt::AlignCenter,newGeometry.size(),FVideoRect);
		QRect intersectRect = newGeometry.intersect(FLocalVideo->geometry());
		if (intersectRect.width()>0 || intersectRect.height()>0)
		{
			Qt::Alignment localAlign = FLocalVideo->alignment();
			if (localAlign & Qt::AlignTop)
				newGeometry.moveTop(newGeometry.top()+qMin(intersectRect.height(),FVideoRect.bottom()-newGeometry.bottom()));
			else if (localAlign & Qt::AlignBottom)
				newGeometry.moveBottom(newGeometry.bottom()-qMin(intersectRect.height(),newGeometry.top()-FVideoRect.top()));
			if (localAlign & Qt::AlignLeft)
				newGeometry.moveLeft(newGeometry.left()+qMin(intersectRect.width(),FVideoRect.right()-newGeometry.right()));
			else if (localAlign & Qt::AlignRight)
				newGeometry.moveRight(newGeometry.right()-qMin(intersectRect.width(),newGeometry.left()-FVideoRect.left()));
		}
	}
	return newGeometry;
}

QRect PhoneCallLayout::adjustLocalVideoSize(const QRect &AGeometry) const
{
	QRect newGeometry = AGeometry;
	if (!FLocalScale.isNull())
	{
		newGeometry.setLeft(qRound(FVideoRect.width()*FLocalScale.left()));
		newGeometry.setRight(qRound(FVideoRect.width()*FLocalScale.right()));
		newGeometry.setTop(qRound(FVideoRect.height()*FLocalScale.top()));
		newGeometry.setBottom(qRound(FVideoRect.height()*FLocalScale.bottom()));

		if (newGeometry.width() < FLocalVideo->minimumVideoSize().width())
		{
			int delta = FLocalVideo->minimumVideoSize().width() - newGeometry.width();
			newGeometry.setLeft(newGeometry.left() - delta/2);
			newGeometry.setRight(newGeometry.right() + delta - delta/2);
		}
		else if (newGeometry.width() > FLocalVideo->maximumVideoSize().width())
		{
			int delta = newGeometry.width() - FLocalVideo->maximumVideoSize().width();
			newGeometry.setLeft(newGeometry.left() + delta/2);
			newGeometry.setRight(newGeometry.right() - delta + delta/2);
		}
		if (newGeometry.height() < FLocalVideo->minimumVideoSize().height())
		{
			int delta = FLocalVideo->minimumVideoSize().height() - newGeometry.height();
			newGeometry.setTop(newGeometry.top() - delta/2);
			newGeometry.setBottom(newGeometry.bottom() + delta - delta/2);
		}
		else if (newGeometry.height() > FLocalVideo->maximumVideoSize().height())
		{
			int delta = newGeometry.height() - FLocalVideo->maximumVideoSize().height();
			newGeometry.setTop(newGeometry.top() + delta/2);
			newGeometry.setBottom(newGeometry.bottom() - delta + delta/2);
		}

		QSize videoSize = FLocalVideo->sizeHint();
		int newWidth = qRound(newGeometry.height()*((qreal)videoSize.width()/videoSize.height()));
		int newHeight = qRound(newGeometry.width()*((qreal)videoSize.height()/videoSize.width()));
		newWidth = qMin(qMax(newWidth,FLocalVideo->minimumVideoSize().width()),FLocalVideo->maximumVideoSize().width());
		newHeight = qMin(qMax(newHeight,FLocalVideo->minimumVideoSize().height()),FLocalVideo->maximumVideoSize().height());

		if (newGeometry.width()-newWidth > 1)
			newGeometry.setWidth(newWidth);
		else if (newGeometry.height()-newHeight > 1)
			newGeometry.setHeight(newHeight);
	}
	return newGeometry;
}

QRect PhoneCallLayout::adjustLocalVideoPosition(const QRect &AGeometry) const
{
	QRect availRect = FVideoRect;
	availRect.adjust(FLocalMargin,FLocalMargin,-FLocalMargin,-FLocalMargin);
	return WidgetManager::alignRect(AGeometry,availRect,FLocalVideo->alignment());
}

QRect PhoneCallLayout::correctLocalVideoPosition(const QRect &AGeometry) const
{
	int leftDelta = AGeometry.left() - FVideoRect.left();
	int topDelta = AGeometry.top() - FVideoRect.top();
	int rightDelta = FVideoRect.right() - AGeometry.right();
	int bottomDelta = FVideoRect.bottom() - AGeometry.bottom();

	QPoint newTopLeft = AGeometry.topLeft();
	if (leftDelta < FLocalStickDelta)
		newTopLeft.rx() += FLocalMargin-leftDelta;
	if (topDelta < FLocalStickDelta)
		newTopLeft.ry() += FLocalMargin-topDelta;
	if (rightDelta < FLocalStickDelta)
		newTopLeft.rx() -= FLocalMargin-rightDelta;
	if (bottomDelta < FLocalStickDelta)
		newTopLeft.ry() -= FLocalMargin-bottomDelta;

	QRect newGeometry = AGeometry;
	newGeometry.moveTo(newTopLeft);

	return newGeometry;
}

QRect PhoneCallLayout::correctLocalVideoSize(Qt::Corner ACorner, const QRect &AGeometry) const
{
	int leftDelta = AGeometry.left() - FVideoRect.left();
	int topDelta = AGeometry.top() - FVideoRect.top();
	int rightDelta = FVideoRect.right() - AGeometry.right();
	int bottomDelta = FVideoRect.bottom() - AGeometry.bottom();

	QRect newGeometry = AGeometry;
	if (leftDelta<FLocalStickDelta && (ACorner==Qt::TopLeftCorner || ACorner==Qt::BottomLeftCorner))
		newGeometry.setLeft(newGeometry.left()+FLocalMargin-leftDelta);
	if (topDelta < FLocalStickDelta && (ACorner==Qt::TopLeftCorner || ACorner==Qt::TopRightCorner))
		newGeometry.setTop(newGeometry.top()+FLocalMargin-topDelta);
	if (rightDelta<FLocalStickDelta && (ACorner==Qt::TopRightCorner || ACorner==Qt::BottomRightCorner))
		newGeometry.setRight(newGeometry.right()-FLocalMargin+rightDelta);
	if (bottomDelta<FLocalStickDelta && (ACorner==Qt::BottomLeftCorner || ACorner==Qt::BottomRightCorner))
		newGeometry.setBottom(newGeometry.bottom()-FLocalMargin+bottomDelta);

	if (newGeometry.width() < FLocalVideo->minimumVideoSize().width())
	{
		if (ACorner==Qt::TopLeftCorner || ACorner==Qt::BottomLeftCorner)
			newGeometry.setLeft(newGeometry.left()-(FLocalVideo->minimumVideoSize().width()-newGeometry.width()));
		else if (ACorner==Qt::TopRightCorner || ACorner==Qt::BottomRightCorner)
			newGeometry.setRight(newGeometry.right()+(FLocalVideo->minimumVideoSize().width()-newGeometry.width()));
	}
	else if (newGeometry.width() > FLocalVideo->maximumVideoSize().width())
	{
		if (ACorner==Qt::TopLeftCorner || ACorner==Qt::BottomLeftCorner)
			newGeometry.setLeft(newGeometry.left()+(newGeometry.width()-FLocalVideo->maximumVideoSize().width()));
		else if (ACorner==Qt::TopRightCorner || ACorner==Qt::BottomRightCorner)
			newGeometry.setRight(newGeometry.right()-(newGeometry.width()-FLocalVideo->maximumVideoSize().width()));
	}
	if (newGeometry.height() < FLocalVideo->minimumVideoSize().height())
	{
		if (ACorner==Qt::TopLeftCorner || ACorner==Qt::TopRightCorner)
			newGeometry.setTop(newGeometry.top()-(FLocalVideo->minimumVideoSize().height()-newGeometry.height()));
		else if (ACorner==Qt::BottomLeftCorner || ACorner==Qt::BottomRightCorner)
			newGeometry.setBottom(newGeometry.bottom()+(FLocalVideo->minimumVideoSize().height()-newGeometry.height()));
	}
	else if (newGeometry.height() > FLocalVideo->maximumVideoSize().height())
	{
		if (ACorner==Qt::TopLeftCorner || ACorner==Qt::TopRightCorner)
			newGeometry.setTop(newGeometry.top()+(newGeometry.height()-FLocalVideo->maximumVideoSize().height()));
		else if (ACorner==Qt::BottomLeftCorner || ACorner==Qt::BottomRightCorner)
			newGeometry.setBottom(newGeometry.bottom()-(newGeometry.height()-FLocalVideo->maximumVideoSize().height()));
	}

	QSize videoSize = FLocalVideo->sizeHint();
	int newWidth = qRound(newGeometry.height()*((qreal)videoSize.width()/videoSize.height()));
	int newHeight = qRound(newGeometry.width()*((qreal)videoSize.height()/videoSize.width()));
	newWidth = qMin(qMax(newWidth,FLocalVideo->minimumVideoSize().width()),FLocalVideo->maximumVideoSize().width());
	newHeight = qMin(qMax(newHeight,FLocalVideo->minimumVideoSize().height()),FLocalVideo->maximumVideoSize().height());

	if (newGeometry.width()-newWidth > 1)
	{
		if (ACorner==Qt::TopLeftCorner || ACorner==Qt::BottomLeftCorner)
			newGeometry.setLeft(newGeometry.left()+(newGeometry.width()-newWidth));
		else if (ACorner==Qt::TopRightCorner || ACorner==Qt::BottomRightCorner)
			newGeometry.setRight(newGeometry.right()-(newGeometry.width()-newWidth));
	}
	else if (newGeometry.height()-newHeight > 1)
	{
		if (ACorner==Qt::TopLeftCorner || ACorner==Qt::TopRightCorner)
			newGeometry.setTop(newGeometry.top()+(newGeometry.height()-newHeight));
		else if (ACorner==Qt::BottomLeftCorner || ACorner==Qt::BottomRightCorner)
			newGeometry.setBottom(newGeometry.bottom()-(newGeometry.height()-newHeight));
	}

	return newGeometry;
}

void PhoneCallLayout::onLocalVideoMove(const QPoint &APos)
{
	if (!FVideoRect.isEmpty())
	{
		QRect newGeometry = FLocalVideo->geometry();
		newGeometry.moveTo(APos);
		newGeometry = correctLocalVideoPosition(newGeometry);

		FLocalVideo->setGeometry(newGeometry);
		FLocalVideo->setAlignment(geometryAlignment(newGeometry));

		updateLocalVideoGeometryScale();
		update();
	}
}

void PhoneCallLayout::onLocalVideoResize(Qt::Corner ACorner, const QPoint &APos)
{
	if (!FVideoRect.isEmpty())
	{
		QRect newGeometry = FLocalVideo->geometry();
		if (ACorner == Qt::TopLeftCorner)
			newGeometry.setTopLeft(APos);
		else if (ACorner == Qt::TopRightCorner)
			newGeometry.setTopRight(APos);
		else if (ACorner == Qt::BottomLeftCorner)
			newGeometry.setBottomLeft(APos);
		else if (ACorner == Qt::BottomRightCorner)
			newGeometry.setBottomRight(APos);
		newGeometry = correctLocalVideoSize(ACorner,newGeometry);

		FLocalVideo->setGeometry(newGeometry);
		FLocalVideo->setAlignment(geometryAlignment(newGeometry));

		updateLocalVideoGeometryScale();
		update();
	}
}

void PhoneCallLayout::onControlsVisibilityPercentChanged(const QVariant &AValue)
{
	FCtrlVisiblePerc = AValue.toDouble();
	update();
}
