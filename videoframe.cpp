#include "videoframe.h"

#include <QPainter>
#include <QVariant>
#include <QPalette>
#include <QVBoxLayout>
#include "videoframehooker.h"

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


VideoFrame::VideoFrame(QWidget *AParent) : QFrame(AParent)
{
	FMoveEnabled = false;
	FResizeEnabled = false;
	FVideoWidget = NULL;
	FMinimumSize = QSize(50,50);
	FMaximumSize = QSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
	FAlignment = Qt::AlignRight|Qt::AlignBottom;

	FLayout = new QStackedLayout(this);
	FLayout->setMargin(0);
	FLayout->setStackingMode(QStackedLayout::StackAll);

	VideoFrameHooker *hooker = new VideoFrameHooker(this);
	connect(hooker,SIGNAL(singleClicked()),SIGNAL(singleClicked()));
	connect(hooker,SIGNAL(doubleClicked()),SIGNAL(doubleClicked()));
	connect(hooker,SIGNAL(moveTo(const QPoint &)),SLOT(onVideoFrameHookerMoveTo(const QPoint &)));
	connect(hooker,SIGNAL(resizeTo(Qt::Corner, const QPoint &)),SLOT(onVideoFrameHookerResizeTo(Qt::Corner, const QPoint &)));
	FLayout->addWidget(hooker);
}

VideoFrame::~VideoFrame()
{

}

bool VideoFrame::isNull() const
{
	return FVideoWidget==NULL;
}

bool VideoFrame::isMoveEnabled() const
{
	return FMoveEnabled;
}

void VideoFrame::setMoveEnabled(bool AEnabled)
{
	if (FMoveEnabled != AEnabled)
	{
		FMoveEnabled = AEnabled;
		emit stateChanged();
	}
}

bool VideoFrame::isResizeEnabled() const
{
	return FResizeEnabled;
}

void VideoFrame::setResizeEnabled(bool AEnabled)
{
	if (FResizeEnabled != AEnabled)
	{
		FResizeEnabled = AEnabled;
		emit stateChanged();
	}
}

Qt::Alignment VideoFrame::alignment() const
{
	return FAlignment;
}

void VideoFrame::setAlignment(Qt::Alignment AAlign)
{
	if (FAlignment != AAlign)
	{
		FAlignment = AAlign;
		emit stateChanged();
	}
}

QSize VideoFrame::minimumVideoSize() const
{
	return FMinimumSize;
}

void VideoFrame::setMinimumVideoSize(const QSize &ASize)
{
	if (FMinimumSize != ASize)
	{
		FMinimumSize = ASize;
		updateGeometry();
		emit stateChanged();
	}
}

QSize VideoFrame::maximumVideoSize() const
{
	return FMaximumSize;
}

void VideoFrame::setMaximumVideoSize(const QSize &ASize)
{
	if (FMaximumSize != ASize)
	{
		FMaximumSize = ASize;
		updateGeometry();
		emit stateChanged();
	}
}

QImage VideoFrame::nullImage() const
{
	return FNullImage;
}

void VideoFrame::setNullImage(const QImage &AImage)
{
	if (FNullImage != AImage)
	{
		FNullImage = AImage;
		emit stateChanged();
	}
}

QWidget *VideoFrame::videoWidget() const
{
	return FVideoWidget;
}

void VideoFrame::setVideoWidget(QWidget *AWidget)
{
	if (FVideoWidget != AWidget)
	{
		if (FVideoWidget)
		{
			FVideoWidget->hide();
			FLayout->removeWidget(FVideoWidget);
		}

		FVideoWidget = AWidget;

		if (FVideoWidget)
		{
			FLayout->addWidget(FVideoWidget);
			FVideoWidget->show();
		}

		emit stateChanged();
	}
}

QSize VideoFrame::sizeHint() const
{
	if (FVideoWidget)
		return FVideoWidget->sizeHint();
	if (!FNullImage.isNull())
		return FNullImage.size();
	return FMinimumSize;
}

QSize VideoFrame::minimumSizeHint() const
{
	return FMinimumSize;
}

void VideoFrame::paintEvent(QPaintEvent *AEvent)
{
	QFrame::paintEvent(AEvent);

	QPainter p(this);
	QRect clipRect = rect().adjusted(lineWidth(),lineWidth(),-lineWidth(),-lineWidth());
	if (frameShape() != QFrame::NoFrame)
		p.setClipRect(clipRect,Qt::IntersectClip);
	p.fillRect(clipRect, Qt::black);
}

void VideoFrame::onVideoFrameHookerMoveTo(const QPoint &APos)
{
	emit moveTo(mapToParent(APos));
}

void VideoFrame::onVideoFrameHookerResizeTo(Qt::Corner ACorner, const QPoint &APos)
{
	emit resizeTo(ACorner,mapToParent(APos));
}
