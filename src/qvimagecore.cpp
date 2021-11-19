#include "qvimagecore.h"
#include "qvapplication.h"
#include "qvzipfile.h"
#include "qvrarfile.h"
#include <random>
#include <QMessageBox>
#include <QDir>
#include <QUrl>
#include <QSettings>
#include <QCollator>
#include <QtConcurrent/QtConcurrentRun>
#include <QPixmapCache>
#include <QIcon>
#include <QGuiApplication>
#include <QScreen>

QVImageCore::QVImageCore(QObject *parent) : QObject(parent)
{  
    isLoopFoldersEnabled = true;
    preloadingMode = 1;
    sortMode = 0;
    sortDescending = false;

    randomSortSeed = 0;

    currentRotation = 0;

    QPixmapCache::setCacheLimit(51200);

    connect(&loadedMovie, &QMovie::updated, this, &QVImageCore::animatedFrameChanged);

    connect(&loadFutureWatcher, &QFutureWatcher<ReadData>::finished, this, [this](){
        loadPixmap(loadFutureWatcher.result(), false);
    });

    fileChangeRateTimer = new QTimer(this);
    fileChangeRateTimer->setSingleShot(true);
    fileChangeRateTimer->setInterval(60);

    largestDimension = 0;
    const auto screenList = QGuiApplication::screens();
    for (auto const &screen : screenList)
    {
        int largerDimension;
        if (screen->size().height() > screen->size().width())
        {
            largerDimension = screen->size().height();
        }
        else
        {
            largerDimension = screen->size().width();
        }

        if (largerDimension > largestDimension)
        {
            largestDimension = largerDimension;
        }
    }

    // Connect to settings signal
    connect(&qvApp->getSettingsManager(), &SettingsManager::settingsUpdated, this, &QVImageCore::settingsUpdated);
    settingsUpdated();
}

void QVImageCore::loadFile(const QString &fileName)
{
    if (loadFutureWatcher.isRunning() || fileChangeRateTimer->isActive())
        return;

    QString sanitaryFileName = fileName;

    //sanitize file name if necessary
    QUrl sanitaryUrl = QUrl(fileName);
    if (sanitaryUrl.isLocalFile())
        sanitaryFileName = sanitaryUrl.toLocalFile();

    QFileInfo fileInfo(sanitaryFileName);
    sanitaryFileName = fileInfo.absoluteFilePath();

    // Pause playing movie because it feels better that way
    setPaused(true);

    currentFileDetails.isLoadRequested = true;
    currentArchiveFile.reset();
    currentLoadedArchiveEntry.reset();

    if (QStringList{"zip", "cbz"}.count(fileInfo.suffix().toLower())) {
        QScopedPointer<QVArchiveFile> zipFile(new QVZipFile(fileName));
        qSwap(currentArchiveFile, zipFile);
        return loadArchiveFile(*currentArchiveFile, 0);
    } else if (QStringList{"cbr"}.count(fileInfo.suffix().toLower())) {
        QScopedPointer<QVArchiveFile> zipFile(new QVRarFile(fileName));
        qSwap(currentArchiveFile, zipFile);
        return loadArchiveFile(*currentArchiveFile, 0);
    }

    //check if cached already before loading the long way
    auto previouslyRecordedFileSize = qvApp->getPreviouslyRecordedFileSize(sanitaryFileName);
    auto *cachedPixmap = new QPixmap();
    if (QPixmapCache::find(sanitaryFileName, cachedPixmap) &&
        !cachedPixmap->isNull() &&
        previouslyRecordedFileSize == fileInfo.size())
    {
        QSize previouslyRecordedImageSize = qvApp->getPreviouslyRecordedImageSize(sanitaryFileName);
        ReadData readData = {
            matchCurrentRotation(*cachedPixmap),
            fileInfo,
            previouslyRecordedImageSize
        };
        loadPixmap(readData, true);
    }
    else
    {
        loadFutureWatcher.setFuture(QtConcurrent::run(this, &QVImageCore::readFile, sanitaryFileName, false));
    }
    delete cachedPixmap;
}

void QVImageCore::loadArchiveFile(QVArchiveFile &archiveFile, std::size_t idx)
{
    if (loadFutureWatcher.isRunning() || fileChangeRateTimer->isActive())
        return;

    setPaused(true);
    currentFileDetails.isLoadRequested = true;

    if (!archiveFile.isValid()) {
        emit readError(static_cast<int>(QVArchiveFile::ErrorCode::invalidArchive),
                       tr("Invalid archive"),
                       archiveFile.getFilePath());
        return;
    }

    const auto &fileList = archiveFile.listEntries();

    if (idx >= static_cast<std::size_t>(fileList.size())) {
        emit readError(static_cast<int>(QVArchiveFile::ErrorCode::noAvailableEntry),
                       tr("No supported entry"),
                       archiveFile.getFilePath());
        return;
    }

    Q_ASSERT(idx < static_cast<std::size_t>(fileList.size()));
    auto entryPath = fileList[idx];

    QPixmap cachedPixmap;
    currentLoadedArchiveEntry.reset(new QBuffer);
    currentLoadedArchiveEntry->open(QIODevice::ReadWrite);
    archiveFile.readTo(*currentLoadedArchiveEntry, static_cast<QVZipFile::IndexType>(idx));
    currentLoadedArchiveEntry->seek(0);

    if (QPixmapCache::find(QFileInfo(entryPath).absoluteFilePath(), &cachedPixmap)) {
        loadPixmap({cachedPixmap,
                    QFileInfo(entryPath),
                    cachedPixmap.size()},
                    true);
        return;
    }

    auto readData = readFromIODevice(currentLoadedArchiveEntry.get(), fileList[static_cast<QVZipFile::IndexType>(idx)]);
    currentLoadedArchiveEntry->seek(0);
    loadPixmap(readData, false);
}

void QVImageCore::loadArchiveFile(QVArchiveFile &archiveFile, const QString &entryPath)
{
    if (loadFutureWatcher.isRunning() || fileChangeRateTimer->isActive())
        return;

    setPaused(true);
    currentFileDetails.isLoadRequested = true;

    Q_ASSERT(archiveFile.isValid());

    QPixmap cachedPixmap;
    currentLoadedArchiveEntry.reset(new QBuffer);
    currentLoadedArchiveEntry->open(QIODevice::ReadWrite);
    archiveFile.readTo(*currentLoadedArchiveEntry, entryPath);
    currentLoadedArchiveEntry->seek(0);

    if (QPixmapCache::find(QFileInfo(entryPath).absoluteFilePath(), &cachedPixmap)) {
        loadPixmap({cachedPixmap,
                    QFileInfo(entryPath),
                    cachedPixmap.size()},
                    true);
        return;
    }

    auto readData = readFromIODevice(currentLoadedArchiveEntry.get(), entryPath);
    currentLoadedArchiveEntry->seek(0);
    loadPixmap(readData, false);
}

QVImageCore::ReadData QVImageCore::readFromIODevice(QIODevice *device,
                              const QString &archiveName)
{
    QImageReader imageReader(device);
    imageReader.setDecideFormatFromContent(true);
    imageReader.setAutoTransform(true);
    auto readPixmap = QPixmap::fromImageReader(&imageReader);

    return {
        readPixmap,
        QFileInfo(archiveName),
        imageReader.size()
    };
}

QVImageCore::ReadData QVImageCore::readFile(const QString &fileName, bool forCache)
{
    QImageReader imageReader;
    imageReader.setDecideFormatFromContent(true);
    imageReader.setAutoTransform(true);

    imageReader.setFileName(fileName);

    QPixmap readPixmap;
    if (imageReader.format() == "svg" || imageReader.format() == "svgz")
    {
        // Render vectors into a high resolution
        QIcon icon;
        icon.addFile(fileName);
        readPixmap = icon.pixmap(largestDimension);
        // If this fails, try reading the normal way so that a proper error message is given
        if (readPixmap.isNull())
            readPixmap = QPixmap::fromImageReader(&imageReader);
    }
    else
    {
        readPixmap = QPixmap::fromImageReader(&imageReader);
    }

    ReadData readData = {
        readPixmap,
        QFileInfo(fileName),
        imageReader.size(),
    };
    // Only error out when not loading for cache
    if (readPixmap.isNull() && !forCache)
    {
        emit readError(imageReader.error(), imageReader.errorString(), readData.fileInfo.fileName());
    }

    return readData;
}

void QVImageCore::loadPixmap(const ReadData &readData, bool fromCache)
{
    // Do this first so we can keep folder info even when loading errored files
    currentFileDetails.fileInfo = readData.fileInfo;
    updateFolderInfo();

    // Reset file change rate timer
    fileChangeRateTimer->start();

    if (readData.pixmap.isNull())
        return;

    loadedPixmap = matchCurrentRotation(readData.pixmap);

    // Set file details
    currentFileDetails.isPixmapLoaded = true;
    currentFileDetails.baseImageSize = readData.size;
    currentFileDetails.loadedPixmapSize = loadedPixmap.size();
    if (currentFileDetails.baseImageSize == QSize(-1, -1))
    {
        qInfo() << "QImageReader::size gave an invalid size for " + currentFileDetails.fileInfo.fileName() + ", using size from loaded pixmap";
        currentFileDetails.baseImageSize = currentFileDetails.loadedPixmapSize;
    }

    // If this image isnt originally from the cache, add it to the cache
    if (!fromCache)
        addToCache(readData);

    // Animation detection
    loadedMovie.stop();

    if (currentArchiveFile) {
        loadedMovie.setDevice(currentLoadedArchiveEntry.get());
    } else {
        loadedMovie.setFileName(currentFileDetails.fileInfo.absoluteFilePath());
    }

    // APNG workaround
    if (loadedMovie.format() == "png")
        loadedMovie.setFormat("apng");

    currentFileDetails.isMovieLoaded = loadedMovie.isValid() && loadedMovie.frameCount() != 1;

    if (currentFileDetails.isMovieLoaded)
        loadedMovie.start();
    else if (auto device = loadedMovie.device())
        device->close();

    emit fileChanged();

    requestCaching();
}

void QVImageCore::closeImage()
{
    loadedPixmap = QPixmap();
    loadedMovie.stop();
    loadedMovie.setFileName("");
    currentFileDetails = {
        QFileInfo(),
        currentFileDetails.folderFileInfoList,
        currentFileDetails.loadedIndexInFolder,
        false,
        false,
        false,
        QSize(),
        QSize()
    };

    emit fileChanged();
}

void QVImageCore::updateFolderInfo()
{
    if (!currentFileDetails.fileInfo.isFile() && !currentArchiveFile)
        return;

    QPair<QString, uint> dirInfo = {currentFileDetails.fileInfo.absoluteDir().path(),
                                    currentFileDetails.fileInfo.dir().count()};
    // If the current folder changed since the last image, assign a new seed for random sorting
    if (lastDirInfo != dirInfo)
    {
        randomSortSeed = std::chrono::system_clock::now().time_since_epoch().count();
    }
    lastDirInfo = dirInfo;

    QDir::SortFlags sortFlags = QDir::NoSort;

    // Deal with sort flags
    switch (sortMode) {
    case 1: {
        sortFlags = QDir::Time;
        break;
    }
    case 2: {
        sortFlags = QDir::Size;
        break;
    }
    case 3: {
        sortFlags = QDir::Type;
        break;
    }
    }

    if (sortDescending)
        sortFlags.setFlag(QDir::Reversed, true);

    if (currentArchiveFile) {
        // Currently we don't support sortFlags for archive file
        currentFileDetails.folderFileInfoList = currentArchiveFile->fileInfoList();
    } else {
        currentFileDetails.folderFileInfoList = currentFileDetails.fileInfo.dir().entryInfoList(qvApp->getFilterList(), QDir::Files, sortFlags);
    }

    // For more special types of sorting
    if (sortMode == 0) // Natural sorting
    {
        QCollator collator;
        collator.setNumericMode(true);
        std::sort(currentFileDetails.folderFileInfoList.begin(),
                  currentFileDetails.folderFileInfoList.end(),
                  [&collator, this](const QFileInfo &file1, const QFileInfo &file2)
        {
            if (sortDescending)
                return collator.compare(file1.filePath(), file2.filePath()) > 0;
            else
                return collator.compare(file1.filePath(), file2.filePath()) < 0;
        });
    }
    else if (sortMode == 4) // Random sorting
    {
        std::shuffle(currentFileDetails.folderFileInfoList.begin(), currentFileDetails.folderFileInfoList.end(), std::default_random_engine(randomSortSeed));
    }

    if (currentArchiveFile) {
        const auto currentIdx = std::find_if(currentFileDetails.folderFileInfoList.begin(),
                                             currentFileDetails.folderFileInfoList.end(), [this](const QFileInfo &info) {
                                                return info.filePath() == currentFileDetails.fileInfo.filePath();
                                             }) - currentFileDetails.folderFileInfoList.begin();
        currentFileDetails.loadedIndexInFolder = currentIdx;
    } else {
        // Set current file index variable
        currentFileDetails.loadedIndexInFolder = currentFileDetails.folderFileInfoList.indexOf(currentFileDetails.fileInfo);
    }
}

void QVImageCore::requestCaching()
{
    if (preloadingMode == 0)
    {
        QPixmapCache::clear();
        return;
    }

    int preloadingDistance = 1;

    if (preloadingMode > 1)
        preloadingDistance = 4;

    QStringList filesToPreload;
    for (int i = currentFileDetails.loadedIndexInFolder-preloadingDistance; i <= currentFileDetails.loadedIndexInFolder+preloadingDistance; i++)
    {
        int index = i;

        // Don't try to cache the currently loaded image
        if (index == currentFileDetails.loadedIndexInFolder)
            continue;

        //keep within index range
        if (isLoopFoldersEnabled)
        {
            if (index > currentFileDetails.folderFileInfoList.length()-1)
                index = index-(currentFileDetails.folderFileInfoList.length());
            else if (index < 0)
                index = index+(currentFileDetails.folderFileInfoList.length());
        }

        //if still out of range after looping, just cancel the cache for this index
        if (index > currentFileDetails.folderFileInfoList.length()-1 || index < 0 || currentFileDetails.folderFileInfoList.isEmpty())
            continue;

        if (archiveMode()) {
            QString entryPath = currentFileDetails.folderFileInfoList[index].filePath();
            filesToPreload.append(entryPath);
            requestCacheArchiveEntry(entryPath);
        } else {
            QString filePath = currentFileDetails.folderFileInfoList[index].absoluteFilePath();
            filesToPreload.append(filePath);
            requestCachingFile(filePath);
        }
    }
    lastFilesPreloaded = filesToPreload;

}

void QVImageCore::requestCacheArchiveEntry(const QString &entryPath)
{
    //check if image is already loaded or requested
    if (QPixmapCache::find(entryPath, nullptr) || lastFilesPreloaded.contains(entryPath))
        return;

    //check if too big for caching
    QSharedPointer<QByteArray> sharedData(new QByteArray);
    QSharedPointer<QBuffer> buffer(new QBuffer(sharedData.get()));
    buffer->open(QBuffer::ReadWrite);
    archiveFile()->readTo(*buffer, entryPath);
    QImageReader newImageReader(buffer.get());
    QTransform transform;
    transform.rotate(currentRotation);
    if (((newImageReader.size().width()*newImageReader.size().height()*32)/8)/1000 > QPixmapCache::cacheLimit()/2)
        return;

    auto *cacheFutureWatcher = new QFutureWatcher<ReadData>();
    connect(cacheFutureWatcher, &QFutureWatcher<ReadData>::finished, this, [cacheFutureWatcher, sharedData, buffer, this]() mutable {
        buffer.clear();
        sharedData.clear();
        addToCache(cacheFutureWatcher->result());
        cacheFutureWatcher->deleteLater();
    });
    buffer->seek(0);
    cacheFutureWatcher->setFuture(QtConcurrent::run(this, &QVImageCore::readFromIODevice, buffer.get(), entryPath));
}

void QVImageCore::requestCachingFile(const QString &filePath)
{
    //check if image is already loaded or requested
    if (QPixmapCache::find(filePath, nullptr) || lastFilesPreloaded.contains(filePath))
        return;

    //check if too big for caching
    //This size check is probably inefficient and could be replaced
    QImageReader newImageReader(filePath);
    QTransform transform;
    transform.rotate(currentRotation);
    if (((newImageReader.size().width()*newImageReader.size().height()*32)/8)/1000 > QPixmapCache::cacheLimit()/2)
        return;

    auto *cacheFutureWatcher = new QFutureWatcher<ReadData>();
    connect(cacheFutureWatcher, &QFutureWatcher<ReadData>::finished, this, [cacheFutureWatcher, this](){
        addToCache(cacheFutureWatcher->result());
        cacheFutureWatcher->deleteLater();
    });
    cacheFutureWatcher->setFuture(QtConcurrent::run(this, &QVImageCore::readFile, filePath, true));
}

void QVImageCore::addToCache(const ReadData &readData)
{
    if (readData.pixmap.isNull())
        return;

    QPixmapCache::insert(readData.fileInfo.absoluteFilePath(), readData.pixmap);

    auto *size = new qint64(archiveMode() ?
                            currentArchiveFile->entryNumBytes(readData.fileInfo.filePath()) :
                            readData.fileInfo.size());
    qvApp->setPreviouslyRecordedFileSize(readData.fileInfo.absoluteFilePath(), size);
    qvApp->setPreviouslyRecordedImageSize(readData.fileInfo.absoluteFilePath(), new QSize(readData.size));
}

void QVImageCore::jumpToNextFrame()
{
    if (currentFileDetails.isMovieLoaded)
        loadedMovie.jumpToNextFrame();
}

void QVImageCore::setPaused(bool desiredState)
{
    if (currentFileDetails.isMovieLoaded)
        loadedMovie.setPaused(desiredState);
}

void QVImageCore::setSpeed(int desiredSpeed)
{
    if (currentFileDetails.isMovieLoaded)
        loadedMovie.setSpeed(desiredSpeed);
}

void QVImageCore::rotateImage(int rotation)
{
        currentRotation += rotation;

        // normalize between 360 and 0
        currentRotation = (currentRotation % 360 + 360) % 360;
        QTransform transform;

        QImage transformedImage;
        if (currentFileDetails.isMovieLoaded)
        {
            transform.rotate(currentRotation);
            transformedImage = loadedMovie.currentImage().transformed(transform);
        }
        else
        {
            transform.rotate(rotation);
            transformedImage = loadedPixmap.toImage().transformed(transform);
        }

        loadedPixmap.convertFromImage(transformedImage);

        currentFileDetails.loadedPixmapSize = QSize(loadedPixmap.width(), loadedPixmap.height());
        emit updateLoadedPixmapItem();
}

QImage QVImageCore::matchCurrentRotation(const QImage &imageToRotate)
{
    if (!currentRotation)
        return imageToRotate;

    QTransform transform;
    transform.rotate(currentRotation);
    return imageToRotate.transformed(transform);
}

QPixmap QVImageCore::matchCurrentRotation(const QPixmap &pixmapToRotate)
{
    if (!currentRotation)
        return pixmapToRotate;

    return QPixmap::fromImage(matchCurrentRotation(pixmapToRotate.toImage()));
}

QPixmap QVImageCore::scaleExpensively(const int desiredWidth, const int desiredHeight, const ScaleMode mode)
{
    return scaleExpensively(QSize(desiredWidth, desiredHeight), mode);
}

QPixmap QVImageCore::scaleExpensively(const QSize desiredSize, const ScaleMode mode)
{
    if (!currentFileDetails.isPixmapLoaded)
        return QPixmap();

    QSize size = QSize(loadedPixmap.width(), loadedPixmap.height());
    size.scale(desiredSize, Qt::KeepAspectRatio);

    QPixmap relevantPixmap;
    if (!currentFileDetails.isMovieLoaded)
    {
        relevantPixmap = loadedPixmap;
    }
    else
    {
        relevantPixmap = loadedMovie.currentPixmap();
        relevantPixmap = matchCurrentRotation(relevantPixmap);
    }

    switch (mode) {
    case ScaleMode::normal:
    {
        relevantPixmap = relevantPixmap.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        break;
    }
    case ScaleMode::width:
    {
        relevantPixmap = relevantPixmap.scaledToWidth(desiredSize.width(), Qt::SmoothTransformation);
        break;
    }
    case ScaleMode::height:
    {
        relevantPixmap = relevantPixmap.scaledToHeight(desiredSize.height(), Qt::SmoothTransformation);
        break;
    }
    }

    return relevantPixmap;
}


void QVImageCore::settingsUpdated()
{
    auto &settingsManager = qvApp->getSettingsManager();

    //loop folders
    isLoopFoldersEnabled = settingsManager.getBoolean("loopfoldersenabled");

    //preloading mode
    preloadingMode = settingsManager.getInteger("preloadingmode");
    switch (preloadingMode) {
    case 1:
    {
        QPixmapCache::setCacheLimit(51200);
        break;
    }
    case 2:
    {
        QPixmapCache::setCacheLimit(204800);
        break;
    }
    }

    //sort mode
    sortMode = settingsManager.getInteger("sortmode");

    //sort ascending
    sortDescending = settingsManager.getBoolean("sortdescending");

    //update folder info to re-sort
    updateFolderInfo();
}
