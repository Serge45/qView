#ifndef QVZIPFILE_H
#define QVZIPFILE_H

#include "qvarchivefile.h"
#include <QFileInfoList>
#include <QScopedPointer>
#include <QStringList>

class QVZipFile : public QVArchiveFile
{
public:
    explicit QVZipFile(const QString &filepath);
    ~QVZipFile();
    bool isValid() const override;
    QString getFilePath() const override { return filepath; }
    const QStringList &listEntries() const override;
    QByteArray read(IndexType idx) const override;
    QByteArray read(const QString& entryName) const override;
    qint64 entryNumBytes(IndexType idx) const override;

private:
    class Impl;
    QScopedPointer<Impl> pImpl;
    QString filepath;
};

#endif // QVZIPFILE_H
