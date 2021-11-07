#include "qvzipfile.h"
#include <QMap>
#include <QImageReader>
#include "3rdparty/zip/src/zip.h"
#include <algorithm>

class QVZipFile::Impl {
public:
    using EntryNameToZipEntryIndex = QMap<QString, QVZipFile::IndexType>;
    explicit Impl(const QString &path)
    {
        zipHandle = zip_open(path.toStdString().c_str(), 0, 'r');
    }

    ~Impl()
    {
        if (zipHandle) {
            zip_close(zipHandle);
        }
    }

    zip_t *zipHandle{nullptr};
    QStringList fileNameList;
    EntryNameToZipEntryIndex entryToIndex;
};

QVZipFile::QVZipFile(const QString &filepath)
    : QVArchiveFile(), pImpl(new Impl(filepath)), filepath(filepath)
{

}

bool QVZipFile::isValid() const
{
    return pImpl->zipHandle;
}

QVZipFile::~QVZipFile() = default;

const QStringList &QVZipFile::listEntries() const
{
    const auto numEntries = zip_entries_total(pImpl->zipHandle);

    if (pImpl->fileNameList.size()) {
        return pImpl->fileNameList;
    }

    for (IndexType i = 0; i < numEntries; ++i) {
        zip_entry_openbyindex(pImpl->zipHandle, i);

        if (!zip_entry_isdir(pImpl->zipHandle)) {
            const auto *name = zip_entry_name(pImpl->zipHandle);
            QFileInfo entryInfo(name);

            if (entryInfo.baseName().size()
                    && QImageReader::supportedImageFormats().count(entryInfo.suffix().toLower().toUtf8())) {
                pImpl->fileNameList.push_back(name);
                pImpl->entryToIndex[entryInfo.filePath()] = i;
            }
        }

        zip_entry_close(pImpl->zipHandle);
    }

    return pImpl->fileNameList;
}

QByteArray QVZipFile::read(QVZipFile::IndexType index) const
{
    index = pImpl->entryToIndex[pImpl->fileNameList[index]];
    zip_entry_openbyindex(pImpl->zipHandle, index);
    const auto numBytes = zip_entry_size(pImpl->zipHandle);
    QByteArray bytes(numBytes, Qt::Initialization::Uninitialized);
    zip_entry_noallocread(pImpl->zipHandle, bytes.data(), bytes.size());
    zip_entry_close(pImpl->zipHandle);
    return bytes;
}

QByteArray QVZipFile::read(const QString &entryName) const
{
    Q_ASSERT(pImpl->entryToIndex.count(entryName));
    const auto idx = pImpl->entryToIndex[entryName];
    zip_entry_openbyindex(pImpl->zipHandle, idx);
    const auto numBytes = zip_entry_size(pImpl->zipHandle);
    QByteArray bytes(numBytes, Qt::Initialization::Uninitialized);
    zip_entry_noallocread(pImpl->zipHandle, bytes.data(), bytes.size());
    zip_entry_close(pImpl->zipHandle);
    return bytes;
}

qint64 QVZipFile::entryNumBytes(QVZipFile::IndexType idx) const
{
    Q_ASSERT(pImpl->zipHandle);
    zip_entry_openbyindex(pImpl->zipHandle, idx);
    const auto numBytes = zip_entry_size(pImpl->zipHandle);
    zip_entry_close(pImpl->zipHandle);
    return numBytes;
}
