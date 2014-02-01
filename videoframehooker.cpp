#include "videoframehooker.h"

#include <QStyle>
#include <QTimer>
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <definitions/resources.h>
#include <definitions/phonemanager/menuicons.h>
#include <utils/iconstorage.h>
#include "videoframe.h"

static const struct { 
	Qt::Corner corner;
	qreal rotateAngel;
	Qt::Alignment align;
	Qt::CursorShape cursor;
} Corners[] = {
	{ Qt::TopLeftCorner,     0.0,   Qt::AlignLeft|Qt::AlignTop,     Qt::SizeFDiagCursor },
	{ Qt::TopRightCorner,    90.0,  Qt::AlignRight|Qt::AlignTop,    Qt::SizeBDiagCursor },
	{ Qt::BottomLeftCorner,  270.0, Qt::AlignLeft|Qt::AlignBottom,  Qt::SizeBDiagCursor }, 
	{ Qt::BottomRightCorner, 180.0, Qt::AlignRight|Qt::AlignBottom, Qt::SizeFDiagCursor }
};

VideoFrameHooker::VideoFrameHooker(VideoFrame *AParent) : QWidget(AParent)
{
	setMouseTracking(true);

	FMoved = false;
	FClicked = false;
	FCursorCorner = -1;
	FDoubleClickTime = QDateTime::currentDateTime();

	FVideoFrame = AParent;
	connect(FVideoFrame,SIGNAL(stateChanged()),SLOT(onVideoFrameStateChanged()));

	QIcon icon = IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->getIcon(MNI_PHONEMANAGER_VIDEO_RESIZE);
	FResizeIcon = icon.pixmap(icon.availableSizes().value(0));

	FWaitMovie = new QMovie(this);
	FWaitMovie->setFileName(IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->fileFullName(MNI_PHONEMANAGER_VIDEO_WAIT));
	connect(FWaitMovie,SIGNAL(frameChanged(int)),SLOT(onWaitMovieFrameChanged(int)));

	onVideoFrameStateChanged();
}

VideoFrameHooker::~VideoFrameHooker()
{

}

void VideoFrameHooker::enterEvent(QEvent *AEvent)
{
	if (FVideoFrame->isResizeEnabled())
		update();
	QWidget::enterEvent(AEvent);
}

void VideoFrameHooker::leaveEvent(QEvent *AEvent)
{
	if (FVideoFrame->isResizeEnabled())
		update();
	FCursorCorner = -1;
	setCursor(Qt::ArrowCursor);
	QWidget::leaveEvent(AEvent);
}

void VideoFrameHooker::mouseMoveEvent(QMouseEvent *AEvent)
{
	static const QSize cornerSize = QSize(10,10);

	if (FPressedPos.isNull())
	{
		FCursorCorner = -1;
		for (int i=0; FCursorCorner<0 && i<4; i++)
			if ((Corners[i].align & FVideoFrame->alignment())==0 && QStyle::alignedRect(Qt::LeftToRight,Corners[i].align,cornerSize,rect()).contains(AEvent->pos()))
				FCursorCorner = i;

		if (FVideoFrame->isResizeEnabled() && FCursorCorner>=0)
			setCursor(Corners[FCursorCorner].cursor);
		else if (FVideoFrame->isMoveEnabled())
			setCursor(Qt::OpenHandCursor);
		else
			setCursor(Qt::ArrowCursor);
	}
	else if (FVideoFrame->isResizeEnabled() && FCursorCorner>=0)
	{
		FMoved = true;
		emit resizeTo(Corners[FCursorCorner].corner,mapToParent(AEvent->pos()));
	}
	else if (FVideoFrame->isMoveEnabled() && FMoved)
	{
		emit moveTo(mapToParent(AEvent->pos())-FPressedPos);
	}
	else if ((FGlobalPressed-AEvent->globalPos()).manhattanLength() >= qApp->startDragDistance())
	{
		FMoved = true;
	}

	QWidget::mouseMoveEvent(AEvent);
}

void VideoFrameHooker::mousePressEvent(QMouseEvent *AEvent)
{
	if (AEvent->button() == Qt::LeftButton)
	{
		FPressedPos = AEvent->pos();
		FGlobalPressed = AEvent->globalPos();
		if (FVideoFrame->isMoveEnabled() && FCursorCorner<0)
		{
			FPressedPos = mapToParent(AEvent->pos()) - geometry().topLeft();
			setCursor(Qt::ClosedHandCursor);
		}
		else if (!FVideoFrame->isResizeEnabled() || FCursorCorner<0)
		{
			QWidget::mousePressEvent(AEvent);
		}
		FClicked = FDoubleClickTime.msecsTo(QDateTime::currentDateTime())>qApp->doubleClickInterval();
	}
	else
	{
		QWidget::mousePressEvent(AEvent);
	}
}

void VideoFrameHooker::mouseReleaseEvent(QMouseEvent *AEvent)
{
	if (!FPressedPos.isNull() && AEvent->button()==Qt::LeftButton)
	{
		if (FVideoFrame->isMoveEnabled() && FCursorCorner<0)
			setCursor(Qt::OpenHandCursor);
		if (FClicked && !FMoved)
			QTimer::singleShot(qApp->doubleClickInterval(),this,SLOT(onEmitSingleClicked()));
		FMoved = false;
		FPressedPos = QPoint();
	}
	QWidget::mouseReleaseEvent(AEvent);
}

void VideoFrameHooker::mouseDoubleClickEvent(QMouseEvent *AEvent)
{
	Q_UNUSED(AEvent);
	emit doubleClicked();
	FClicked = false;
	FDoubleClickTime = QDateTime::currentDateTime();
}

void VideoFrameHooker::paintEvent(QPaintEvent *AEvent)
{
	Q_UNUSED(AEvent);

	QPainter p(this);
	p.fillRect(rect(), Qt::transparent);

	if (FVideoFrame->isNull())
	{
		if (!FVideoFrame->nullImage().isNull())
		{
			QSize availSize = rect().size();
			QSize imageSize = FVideoFrame->nullImage().size();
			if (imageSize.height()>availSize.height() || imageSize.width()>availSize.width())
				imageSize.scale(availSize,Qt::KeepAspectRatio);
			QRect imageRect = QStyle::alignedRect(Qt::LeftToRight,Qt::AlignCenter,imageSize,rect());
			p.drawImage(imageRect, FVideoFrame->nullImage());
		}
		else if (FVideoFrame->isMoveEnabled())
		{
			QRect waitRect = QStyle::alignedRect(Qt::LeftToRight,Qt::AlignCenter,FWaitMovie->currentPixmap().size(),rect());
			p.drawPixmap(waitRect, FWaitMovie->currentPixmap());
		}
	}

	if (FVideoFrame->isResizeEnabled() && underMouse())
	{
		QRect resizeRect = QRect(rect().topLeft(),FResizeIcon.size());
		resizeRect.moveCenter(QPoint(0,0));
		for (int i=0; i<4; i++)
		{
			if ((FVideoFrame->alignment() & Corners[i].align) == 0)
			{
				p.save();
				p.translate(QStyle::alignedRect(Qt::LeftToRight,Corners[i].align,FResizeIcon.size(),rect()).center());
				p.rotate(Corners[i].rotateAngel);
				p.drawPixmap(resizeRect,FResizeIcon);
				p.restore();
			}
		}
	}
}

void VideoFrameHooker::onVideoFrameStateChanged()
{
	if (FVideoFrame->isNull() && FVideoFrame->isMoveEnabled() && FVideoFrame->nullImage().isNull())
		FWaitMovie->start();
	else
		FWaitMovie->stop();
	update();
}

void VideoFrameHooker::onEmitSingleClicked()
{
	if (FClicked)
		emit singleClicked();
}

void VideoFrameHooker::onWaitMovieFrameChanged(int AFrameNumber)
{
	Q_UNUSED(AFrameNumber);
	update();
}
