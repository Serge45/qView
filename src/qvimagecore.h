#ifndef QVIMAGECORE_H
#define QVIMAGECORE_H

#include "qvarchivefile.h"
#include <QBuffer>
#include <QObject>
#include <QImageReader>
#include <QPixmap>
#include <QMovie>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QTimer>
#include <QCache>
#include <QScopedPointer>
#include "qvimageinfo.h"

class QVImageCore : public QObject
{
    Q_OBJECT

public:
    enum class ScaleMode
    {
       normal,
       width,
       height
    };
    Q_ENUM(ScaleMode)

    struct FileDetails
    {
        QFileInfo fileInfo;
        QFileInfoList folderFileInfoList;
        int loadedIndexInFolder = -1;
        bool isLoadRequested = false;
        bool isPixmapLoaded = false;
        bool isMovieLoaded = false;
        QSize baseImageSize;
        QSize loadedPixmapSize;
    };

    struct ReadData
    {
        QPixmap pixmap;
        QFileInfo fileInfo;
        QSize size;
    };

    explicit QVImageCore(QObject *parent = nullptr);

    void loadFile(const QString &fileName);
    void loadArchiveFile(QVArchiveFile &archiveFile, std::size_t idx);
    void loadArchiveFile(QVArchiveFile &archiveFile, const QString &entryPath);
    ReadData readFromIODevice(QIODevice *device,
                              const QString &archiveName);
    ReadData readFile(const QString &fileName, bool forCache);
    void loadPixmap(const ReadData &readData, bool fromCache);
    void closeImage();
    void updateFolderInfo();
    void requestCaching();
    void requestCacheArchiveEntry(const QString &entryPath);
    void requestCachingFile(const QString &filePath);
    void addToCache(const ReadData &readImageAndFileInfo);

    void settingsUpdated();

    void jumpToNextFrame();
    void setPaused(bool desiredState);
    void setSpeed(int desiredSpeed);

    void rotateImage(int rotation);
    QImage matchCurrentRotation(const QImage &imageToRotate);
    QPixmap matchCurrentRotation(const QPixmap &pixmapToRotate);

    QPixmap scaleExpensively(const int desiredWidth, const int desiredHeight, const ScaleMode mode = ScaleMode::normal);
    QPixmap scaleExpensively(const QSize desiredSize, const ScaleMode mode = ScaleMode::normal);

    //returned const reference is read-only
    const QPixmap& getLoadedPixmap() const {return loadedPixmap; }
    const QMovie& getLoadedMovie() const {return loadedMovie; }
    const FileDetails& getCurrentFileDetails() const {return currentFileDetails; }
    int getCurrentRotation() const {return currentRotation; }
    bool archiveMode() const { return currentArchiveFile; }
    QVArchiveFile *archiveFile() { return currentArchiveFile.get(); }
    const QVArchiveFile *archiveFile() const { return currentArchiveFile.data(); }
    QBuffer *loadedArchiveEntry() { return currentLoadedArchiveEntry.get(); }
    const QBuffer *loadedArchiveEntry() const { return currentLoadedArchiveEntry.data(); }
    const QVImageInfo &archiveImageInfo(const QString& entryPath) const { return *cachedEntryInfo.find(entryPath);}

signals:
    void animatedFrameChanged(QRect rect);

    void updateLoadedPixmapItem();

    void fileChanged();

    void readError(int errorNum, const QString &errorString, const QString &fileName);

private:
    QPixmap loadedPixmap;
    QMovie loadedMovie;

    FileDetails currentFileDetails;
    int currentRotation;

    QFutureWatcher<ReadData> loadFutureWatcher;

    bool isLoopFoldersEnabled;
    int preloadingMode;
    int sortMode;
    bool sortDescending;

    QPair<QString, uint> lastDirInfo;
    unsigned randomSortSeed;

    QStringList lastFilesPreloaded;

    QTimer *fileChangeRateTimer;

    int largestDimension;

    QScopedPointer<QVArchiveFile> currentArchiveFile;
    QScopedPointer<QBuffer> currentLoadedArchiveEntry;
    QMap<QString, QVImageInfo> cachedEntryInfo;
};

#endif // QVIMAGECORE_H
