#include "callinfowidget.h"

#include <QMovie>
#include <QImageReader>
#include <QTextDocument>

CallInfoWidget::CallInfoWidget(QWidget *AParent) : QFrame(AParent)
{
	ui.setupUi(this);
	ui.lblError->setVisible(false);
}

CallInfoWidget::~CallInfoWidget()
{

}

void CallInfoWidget::setName(const QString &AName)
{
	ui.lblName->setText(AName);
}

void CallInfoWidget::setInfo(const QString &AInfo)
{
	ui.lblInfo->setText(AInfo);
}

void CallInfoWidget::setAvatar(const QVariant &AAvatar)
{
	if (ui.lblAvatar->movie())
	{
		ui.lblAvatar->movie()->deleteLater();
		ui.lblAvatar->setMovie(NULL);
	}
	
	switch (AAvatar.type())
	{
	case QVariant::String:
		{
			QMovie *movie = new QMovie(AAvatar.toString(),QByteArray(),ui.lblAvatar);
			QSize size = QImageReader(AAvatar.toString()).size();
			size.scale(ui.lblAvatar->maximumSize(),Qt::KeepAspectRatio);
			movie->setScaledSize(size);
			ui.lblAvatar->setMovie(movie);
			movie->start();
		}
		break;
	case QVariant::Pixmap:
		ui.lblAvatar->setPixmap(AAvatar.value<QPixmap>());
		break;
	case QVariant::Image:
		ui.lblAvatar->setPixmap(QPixmap::fromImage(AAvatar.value<QImage>()));
		break;
	default:
		ui.lblAvatar->clear();
	}
}

void CallInfoWidget::setError(const QString &AError)
{
	ui.lblError->setText(Qt::escape(AError));
	ui.lblError->setVisible(!AError.isEmpty());
}
