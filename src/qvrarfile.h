#ifndef QVRARFILE_H
#define QVRARFILE_H

#include <QString>
#include "qvarchivefile.h"

class QVRarFile : public QVArchiveFile
{
public:
    explicit QVRarFile(const QString &filepath);
    ~QVRarFile() override = default;
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

#endif // QVRARFILE_H
