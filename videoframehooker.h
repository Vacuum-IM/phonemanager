#ifndef VIDEOFRAMEHOOKER_H
#define VIDEOFRAMEHOOKER_H

#include <QMovie>
#include <QWidget>
#include <QDateTime>

class VideoFrame;
class VideoFrameHooker : 
	public QWidget
{
	Q_OBJECT;
public:
	VideoFrameHooker(VideoFrame *AParent);
	~VideoFrameHooker();
signals:
	void singleClicked();
	void doubleClicked();
	void moveTo(const QPoint &APos);
	void resizeTo(Qt::Corner ACorner, const QPoint &APos);
protected:
	void enterEvent(QEvent *AEvent);
	void leaveEvent(QEvent *AEvent);
	void mouseMoveEvent(QMouseEvent *AEvent);
	void mousePressEvent(QMouseEvent *AEvent);
	void mouseReleaseEvent(QMouseEvent *AEvent);
	void mouseDoubleClickEvent(QMouseEvent *AEvent);
	void paintEvent(QPaintEvent *AEvent);
protected slots:
	void onVideoFrameStateChanged();
	void onEmitSingleClicked();
	void onWaitMovieFrameChanged(int AFrameNumber);
private:
	bool FMoved;
	bool FClicked;
	int FCursorCorner;
	QPoint FPressedPos;
	QPoint FGlobalPressed;
	QDateTime FDoubleClickTime;
private:
	QMovie *FWaitMovie;
	QPixmap FResizeIcon;
	VideoFrame *FVideoFrame;
};

#endif // VIDEOFRAMEHOOKER_H
