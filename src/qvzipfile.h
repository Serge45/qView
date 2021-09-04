#ifndef QVZIPFILE_H
#define QVZIPFILE_H

#include "qvarchivefile.h"
#include <QFileInfoList>
#include <QScopedPointer>
#include <QStringList>

class QVZipFile : public QVArchiveFile
{
public:
    using IndexType = long;
    explicit QVZipFile(const QString &filepath);
    ~QVZipFile();
    bool isValid() const override;
    QString getFilePath() const override { return filepath; }
    const QStringList &listEntries() const override;
    QFileInfoList fileInfoList() const;
    QByteArray read(IndexType idx) const override;
    QByteArray read(const QString& entryName) const override;
    qint64 readTo(QIODevice &device, const QString &entryName) const;
    qint64 readTo(QIODevice &device, IndexType idx) const;

private:
    class Impl;
    QScopedPointer<Impl> pImpl;
    QString filepath;
};

#endif // QVZIPFILE_H
