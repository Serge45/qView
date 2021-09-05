#ifndef QVIMAGEINFO_H
#define QVIMAGEINFO_H
#include <QBuffer>
#include <QFileInfo>
#include <QImageReader>
#include <QMimeDatabase>
#include <QMimeType>
#include <QSize>
#include <QString>
#include <QDateTime>
#include "qvarchivefile.h"

struct QVImageInfo {
    enum class ArchiveType {
        LocalImageFile,
        LocalZipFile
    };

    static QVImageInfo createFromFileInfo(const QFileInfo &fileInfo) {
        QVImageInfo imageInfo;
        imageInfo.archivePath = fileInfo.filePath();
        imageInfo.entryPath = fileInfo.filePath();
        imageInfo.numBytes = fileInfo.size();
        imageInfo.archiveType = ArchiveType::LocalImageFile;
        QMimeDatabase db;
        imageInfo.mimeType = db.mimeTypeForFile(fileInfo.absoluteFilePath());
        QImageReader reader(fileInfo.absoluteFilePath());
        imageInfo.imageSize = reader.size();
        imageInfo.numFrames = reader.imageCount();
        imageInfo.modifiedDateTime = fileInfo.lastModified();
        return imageInfo;
    }

    static QVImageInfo createFromArchiveEntry(QByteArray &data, const QVArchiveFile &file, const QString &entryPath) {
        QVImageInfo imageInfo;
        imageInfo.archivePath = file.getFilePath();
        imageInfo.entryPath = entryPath;
        imageInfo.numBytes = data.size();
        imageInfo.archiveType = ArchiveType::LocalZipFile;
        QMimeDatabase db;
        imageInfo.mimeType = db.mimeTypeForData(data);
        QBuffer buffer(&data);

        if (buffer.open(QIODevice::ReadOnly)) {
            QImageReader reader(&buffer);
            imageInfo.imageSize = reader.size();
            imageInfo.numFrames = reader.imageCount();
            buffer.close();
        }

        QFileInfo archiveFileInfo(file.getFilePath());
        imageInfo.modifiedDateTime = archiveFileInfo.lastModified();
        return imageInfo;
    }

    QString archivePath;
    QString entryPath;
    QMimeType mimeType;
    QDateTime modifiedDateTime;
    QSize imageSize{0, 0};
    qint64 numBytes{0};
    qint64 numFrames{0};
    ArchiveType archiveType{ArchiveType::LocalImageFile};
};

#endif // QVIMAGEINFO_H
