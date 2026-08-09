// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "singletons/UpdateInstaller.hpp"

#include <pajlada/signals/scoped-connection.hpp>
#include <pajlada/signals/signal.hpp>
#include <QString>

#include <memory>
#include <vector>

namespace chatterino {

class Paths;
class Settings;
class Modes;

/**
 * To check for updates, use the `checkForUpdates` method.
 * The class by itself does not start any automatic updates.
 */
class Updates
{
    const Paths &paths;
    const Modes &modes;

public:
    Updates(const Modes &modes_, const Paths &paths_, Settings &settings);

    enum Status {
        None,
        Searching,
        UpdateAvailable,
        NoUpdateAvailable,
        SearchFailed,
        Downloading,
        /// Downloaded and unpacked; it gets swapped in once Chatterino closes.
        UpdateReady,
        DownloadFailed,
        WriteFileFailed,
        MissingPortableUpdater,
        RunUpdaterFailed,
    };

    static bool isDowngradeOf(const QString &online, const QString &current);

    /**
     * @brief Delete old files that belong to the update process
     */
    void deleteOldFiles();

    void checkForUpdates();
    const QString &getCurrentVersion() const;
    const QString &getOnlineVersion() const;
    void installUpdates();
    Status getStatus() const;

    /// Hands a silently downloaded update to the swap helper, which applies it
    /// once this process is gone. Call this while quitting; it does nothing
    /// when no update was staged.
    void applyStagedUpdate();

    /// Drops a staged update that this build already is, i.e. one that was
    /// applied on the previous quit. Call this at startup.
    void discardAppliedUpdate();

    /// How far along the download is, 0-100, or -1 when the size is unknown.
    /// Only meaningful while the status is Downloading.
    int getDownloadProgress() const;

    static QString portableUpdaterPath(const Paths &paths);

    bool shouldShowUpdateButton() const;
    bool isError() const;
    bool isDowngrade() const;

    /// Generates the string that the update dialog will show.
    QString buildUpdateAvailableText() const;

    pajlada::Signals::Signal<Status> statusUpdated;

private:
    QString currentVersion_;
    QString onlineVersion_;
    Status status_ = None;
    bool isDowngrade_{};

    QString updateExe_;
    QString updatePortable_;
    QString updateGuideLink_;

    UpdateInstaller installer_;

    void setStatus_(Status status);

    /// Stores the percentage and notifies listeners without changing status.
    void setDownloadProgress_(int percent);

    /// Starts a background download of `onlineVersion_` and reports its state
    /// through the status signal. Used both by the silent check and by the
    /// update button.
    void stageUpdate_();

    /// Starts a silent background download of `onlineVersion_`, if this build
    /// and the user's settings allow it.
    void stageSilentUpdate();

    int downloadProgress_ = -1;

    std::vector<std::unique_ptr<pajlada::Signals::ScopedConnection>>
        managedConnections;
};

}  // namespace chatterino
