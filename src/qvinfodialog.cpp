#include "qvinfodialog.h"
#include "ui_qvinfodialog.h"
#include <QDateTime>
#include <QMimeDatabase>

static int getGcd (int a, int b) {
    return (b == 0) ? a : getGcd(b, a%b);
}

QVInfoDialog::QVInfoDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QVInfoDialog)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint | Qt::CustomizeWindowHint));
    setFixedSize(0, 0);
}

QVInfoDialog::~QVInfoDialog()
{
    delete ui;
}

void QVInfoDialog::updateInfo()
{
    QLocale locale = QLocale::system();

    const QString entryName = QFileInfo(imageInfo.entryPath).fileName();
    ui->nameLabel->setText(entryName);
    ui->typeLabel->setText(imageInfo.mimeType.name());
    ui->locationLabel->setText(imageInfo.archivePath);

    ui->sizeLabel->setText(tr("%1 (%2 bytes)").arg(formatBytes(imageInfo.numBytes), locale.toString(imageInfo.numBytes)));
    ui->modifiedLabel->setText(imageInfo.modifiedDateTime.toString(locale.dateTimeFormat()));

    const auto width = imageInfo.imageSize.width();
    const auto height = imageInfo.imageSize.height();
    //this is just math to figure the megapixels and then round it to the tenths place
    const double megapixels = static_cast<double>(qRound(((static_cast<double>((width*height))))/1000000 * 10 + 0.5)) / 10 ;
    ui->dimensionsLabel->setText(tr("%1 x %2 (%3 MP)").arg(QString::number(width), QString::number(height), QString::number(megapixels)));
    int gcd = getGcd(imageInfo.imageSize.width(), imageInfo.imageSize.height());

    if (gcd != 0)
        ui->ratioLabel->setText(QString::number(width / gcd) + ":" + QString::number(height / gcd));

    if (imageInfo.numFrames != 0)
    {
        ui->framesLabel2->show();
        ui->framesLabel->show();
        ui->framesLabel->setText(QString::number(imageInfo.numFrames));
    }
    else
    {
        ui->framesLabel2->hide();
        ui->framesLabel->hide();
    }
}
