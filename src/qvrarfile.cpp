#include "qvrarfile.h"
#include <QImageReader>
#include <QString>
#include <QFileInfo>
#include <QMap>
#include "3rdparty/dmc_unrar/dmc_unrar.c"

class QVRarFile::Impl {
public:
    explicit Impl(const QString &archivePath)
        :archivePath(archivePath)
    {
        lastReturnCode = dmc_unrar_archive_init(&archive);
        lastReturnCode = dmc_unrar_archive_open_path(&archive, archivePath.toStdString().c_str());
    }

    ~Impl()
    {
        dmc_unrar_archive_close(&archive);
    }

    QString getEntryName(dmc_unrar_size_t idx)
    {
        auto size = dmc_unrar_get_filename(&archive, idx, nullptr, 0);

        if (!size) {
            return QString();
        }

        std::string filename(size, 0);
        size = dmc_unrar_get_filename(&archive, idx, &filename[0], size);
        dmc_unrar_unicode_make_valid_utf8(&filename[0]);
        return QString::fromUtf8(filename.data());
    }

    bool isEntryDirectory(dmc_unrar_size_t idx)
    {
        return dmc_unrar_file_is_directory(&archive, idx);
    }

    bool isSupportedEntry(dmc_unrar_size_t idx)
    {
        return dmc_unrar_file_is_supported(&archive, idx) == DMC_UNRAR_OK;
    }

    QString archivePath;
    QStringList entryNames;
    QMap<QString, dmc_unrar_size_t> entryToIndex;
    dmc_unrar_archive archive;
    dmc_unrar_return lastReturnCode;
};

QVRarFile::QVRarFile(const QString &filepath)
    : pImpl(new Impl(filepath)), filepath(filepath)
{

}

bool QVRarFile::isValid() const
{
    return pImpl->lastReturnCode == DMC_UNRAR_OK;
}

const QStringList &QVRarFile::listEntries() const
{
    if (pImpl->entryNames.size()) {
        return pImpl->entryNames;
    }

    const auto numEntries = dmc_unrar_get_file_count(&pImpl->archive);

    for (dmc_unrar_size_t i = 0; i < numEntries; ++i) {
        if (pImpl->isEntryDirectory(i) || !pImpl->isSupportedEntry(i)) {
            continue;
        }

        const auto entryName = pImpl->getEntryName(i);
        QFileInfo entryInfo(entryName);

        if (entryInfo.baseName().size()
                && QImageReader::supportedImageFormats().count(entryInfo.suffix().toLower().toUtf8())) {
            pImpl->entryNames.push_back(entryName);
            pImpl->entryToIndex[entryInfo.filePath()] = i;
        }
    }

    return pImpl->entryNames;
}

QByteArray QVRarFile::read(QVArchiveFile::IndexType idx) const
{
    const auto indexInArchive = pImpl->entryToIndex.find(pImpl->entryNames[idx]).value();
    const auto *unrarFile = dmc_unrar_get_file_stat(&pImpl->archive, indexInArchive);
    QByteArray bytes(unrarFile->uncompressed_size, 0);
    pImpl->lastReturnCode = dmc_unrar_extract_file_to_mem(&pImpl->archive, indexInArchive, bytes.data(), bytes.size(), nullptr, true);
    return bytes;
}

QByteArray QVRarFile::read(const QString &entryName) const
{
    const auto indexInArchive = pImpl->entryToIndex.find(entryName).value();
    const auto *unrarFile = dmc_unrar_get_file_stat(&pImpl->archive, indexInArchive);
    QByteArray bytes(unrarFile->uncompressed_size, 0);
    pImpl->lastReturnCode = dmc_unrar_extract_file_to_mem(&pImpl->archive, indexInArchive, bytes.data(), bytes.size(), nullptr, true);
    return bytes;
}

qint64 QVRarFile::entryNumBytes(QVArchiveFile::IndexType idx) const
{
    const auto indexInArchive = pImpl->entryToIndex.find(pImpl->entryNames[idx]).value();
    const auto *unrarFile = dmc_unrar_get_file_stat(&pImpl->archive, indexInArchive);
    return unrarFile->uncompressed_size;
}
