#ifndef VIDEOFRAME_H
#define VIDEOFRAME_H

#include <QFrame>
#include <QStackedLayout>

class VideoFrame : 
	public QFrame
{
	Q_OBJECT;
public:
	VideoFrame(QWidget *AParent);
	~VideoFrame();
	bool isNull() const;
	bool isMoveEnabled() const;
	void setMoveEnabled(bool AEnabled);
	bool isResizeEnabled() const;
	void setResizeEnabled(bool AEnabled);
	Qt::Alignment alignment() const;
	void setAlignment(Qt::Alignment AAlign);
	QSize minimumVideoSize() const;
	void setMinimumVideoSize(const QSize &ASize);
	QSize maximumVideoSize() const;
	void setMaximumVideoSize(const QSize &ASize);
	QImage nullImage() const;
	void setNullImage(const QImage &AImage);
	QWidget *videoWidget() const;
	void setVideoWidget(QWidget *AWidget);
signals:
	void stateChanged();
	void singleClicked();
	void doubleClicked();
	void moveTo(const QPoint &APos);
	void resizeTo(Qt::Corner ACorner, const QPoint &APos);
public:
	QSize sizeHint() const;
	QSize minimumSizeHint() const;
protected:
	void paintEvent(QPaintEvent *AEvent);
protected slots:
	void onVideoFrameHookerMoveTo(const QPoint &APos);
	void onVideoFrameHookerResizeTo(Qt::Corner ACorner, const QPoint &APos);
private:
	bool FMoveEnabled;
	bool FResizeEnabled;
	QImage FNullImage;
	QSize FMinimumSize;
	QSize FMaximumSize;
	QWidget *FVideoWidget;
	Qt::Alignment FAlignment;
private:
	QStackedLayout *FLayout;
};

#endif // VIDEOFRAME_H
