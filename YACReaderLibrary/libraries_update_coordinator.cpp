
#include "libraries_update_coordinator.h"

#include "library_creator.h"
#include "yacreader_libraries.h"
#include "yacreader_global.h"

LibrariesUpdateCoordinator::LibrariesUpdateCoordinator(QSettings *settings, YACReaderLibraries &libraries, QObject *parent)
    : QObject(parent), libraries(libraries), timer(new QTimer(this))
{
    libraries.load();

    this->settings = settings;

    timer->setInterval(1000 * 60);
    timer->start();
    connect(timer, &QTimer::timeout, this, &LibrariesUpdateCoordinator::checkUpdatePolicy);

    elapsedTimer.start();

    if (settings->value(UPDATE_LIBRARIES_AT_STARTUP, false).toBool()) {
        updateLibraries();
    }
}

void LibrariesUpdateCoordinator::checkUpdatePolicy()
{
    if (settings->value(UPDATE_LIBRARIES_PERIODICALLY, false).toBool()) {
        auto variant = settings->value(UPDATE_LIBRARIES_PERIODICALLY_INTERVAL, static_cast<typename std::underlying_type<YACReader::LibrariesUpdateInterval>::type>(YACReader::LibrariesUpdateInterval::Hours2));

        bool itIsDue = false;
        auto interval = static_cast<YACReader::LibrariesUpdateInterval>(variant.toInt());
        switch (interval) {
        case YACReader::LibrariesUpdateInterval::Minutes30:
            itIsDue = elapsedTimer.elapsed() >= 1000 * 60 * 30;
            break;
        case YACReader::LibrariesUpdateInterval::Hourly:
            itIsDue = elapsedTimer.elapsed() >= 1000 * 60 * 60;
            break;
        case YACReader::LibrariesUpdateInterval::Hours2:
            itIsDue = elapsedTimer.elapsed() >= 1000 * 60 * 60 * 2;
            break;
        case YACReader::LibrariesUpdateInterval::Hours4:
            itIsDue = elapsedTimer.elapsed() >= 1000 * 60 * 60 * 4;
            break;
        case YACReader::LibrariesUpdateInterval::Hours8:
            itIsDue = elapsedTimer.elapsed() >= 1000 * 60 * 60 * 8;
            break;
        case YACReader::LibrariesUpdateInterval::Hours12:
            itIsDue = elapsedTimer.elapsed() >= 1000 * 60 * 60 * 12;
            break;
        case YACReader::LibrariesUpdateInterval::Daily:
            itIsDue = elapsedTimer.elapsed() >= 1000 * 60 * 60 * 24;
            break;
        }

        if (itIsDue) {
            elapsedTimer.restart();
            updateLibraries();
            return;
        }
    }

    if (settings->value(UPDATE_LIBRARIES_AT_CERTAIN_TIME, false).toBool()) {
        QTime time = settings->value(UPDATE_LIBRARIES_AT_CERTAIN_TIME_TIME, "00:00").toTime();
        QTime currentTime = QTime::currentTime();

        if (currentTime.hour() == time.hour() && currentTime.minute() == time.minute()) {
            updateLibraries();
            return;
        }
    }
}

void LibrariesUpdateCoordinator::updateLibraries()
{
    startUpdate();
}

void LibrariesUpdateCoordinator::startUpdate()
{
    if (updateFuture.valid() && updateFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        return;
    }

    updateFuture = std::async(std::launch::async, [this] {
        emit updateStarted();
        for (auto library : libraries.getLibraries()) {
            updateLibrary(library.getPath());
        }
        emit updateEnded();
    });
}

void LibrariesUpdateCoordinator::updateLibrary(const QString &path)
{
    QDir pathDir(path);
    if (!pathDir.exists()) {
        return;
    }

    QEventLoop eventLoop;
    LibraryCreator *libraryCreator = new LibraryCreator(settings);
    QString cleanPath = QDir::cleanPath(pathDir.absolutePath());

    libraryCreator->updateLibrary(cleanPath, QDir::cleanPath(pathDir.absolutePath() + "/.yacreaderlibrary"));

    connect(libraryCreator, &LibraryCreator::finished, &eventLoop, &QEventLoop::quit);
    connect(libraryCreator, &LibraryCreator::finished, libraryCreator, &QObject::deleteLater);

    libraryCreator->start();
    eventLoop.exec();
}
