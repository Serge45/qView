#ifndef QVARCHIVEFILE_H
#define QVARCHIVEFILE_H
#include <QString>
#include <QByteArray>
#include <QFileInfoList>

class QIODevice;

class QVArchiveFile {
public:
    enum class ErrorCode : unsigned {
        ok = 0,
        noAvailableEntry,
        invalidArchive
    };
    using IndexType = long;
    QVArchiveFile() = default;
    QVArchiveFile(const QVArchiveFile &) = delete;
    QVArchiveFile(QVArchiveFile &&) = delete;
    QVArchiveFile &operator=(const QVArchiveFile &) = delete;
    QVArchiveFile &operator=(QVArchiveFile &&) = delete;
    virtual ~QVArchiveFile() = default;
    virtual bool isValid() const = 0;
    virtual QString getFilePath() const = 0;
    virtual const QStringList &listEntries() const = 0;
    virtual QByteArray read(IndexType idx) const = 0;
    virtual QByteArray read(const QString& entryName) const = 0;
    virtual qint64 entryNumBytes(IndexType idx) const = 0;
    qint64 entryNumBytes(const QString &entryName) const {
        const auto &entries = listEntries();
        const auto entryIdx = entries.indexOf(entryName);

        if (entryIdx >= 0) {
            return entryNumBytes(entryIdx);
        }

        return -1;
    }

    qint64 numEntries() const { return listEntries().size(); }

    const QFileInfoList &fileInfoList() const {
        using std::begin;
        using std::end;

        const auto &fileList = listEntries();

        if (cachedFileInfoList.size() == fileList.size()) {
            return cachedFileInfoList;
        }

        cachedFileInfoList.clear();

        std::transform(begin(fileList), end(fileList), std::back_inserter(cachedFileInfoList), [] (auto fileName) {
            return QFileInfo(fileName);
        });

        return cachedFileInfoList;
    }

    qint64 readTo(QIODevice &device, IndexType idx) const {
        const auto bytes = read(idx);
        return device.write(bytes);
    }

    qint64 readTo(QIODevice &device, const QString &entryPath) const {
        const auto bytes = read(entryPath);
        return device.write(bytes);
    }

private:
    mutable QFileInfoList cachedFileInfoList;
};

#endif // QVARCHIVEFILE_H
