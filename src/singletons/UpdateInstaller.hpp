// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QByteArray>
#include <QString>

#include <atomic>
#include <functional>
#include <memory>

namespace chatterino {

class Paths;

/// Applies updates without asking the user anything.
///
/// A running process can't overwrite its own installation, so the work is split
/// in two halves:
///
///  1. While the app runs, the new build is downloaded and unpacked into a
///     staging directory (`<misc>/update-staging`).
///  2. On quit, a small detached script is handed the staged payload. It waits
///     for this process to disappear and only then swaps the payload over the
///     installation directory.
///
/// The next launch therefore starts the new version, and the user only ever had
/// to restart the app.
///
/// Only tagged release builds do this - see CHATTERINO_FORK_RELEASE in
/// cmake/ForkVersion.cmake - so a local build never overwrites itself.
class UpdateInstaller
{
public:
    explicit UpdateInstaller(const Paths &paths);

    /// Whether this build and installation can update itself silently.
    ///
    /// False for development builds, nightlies, Flatpak, platforms without an
    /// implementation (Linux), and installations we can't write to (e.g. an
    /// installer-managed directory under Program Files).
    static bool isSupported();

    /// Downloads `url` and unpacks it into the staging directory.
    ///
    /// Runs in the background; any previously staged payload is replaced.
    /// `onFinished`, when given, is invoked on the GUI thread with whether the
    /// update ended up staged - silent checks ignore it, the update button in
    /// settings uses it to report failures.
    /// `onProgress` reports 0-100, or -1 while the total size is unknown.
    void stage(const QString &url, const QString &version,
               std::function<void(bool)> onFinished = {},
               std::function<void(int)> onProgress = {});

    /// The version currently sitting in the staging directory, or an empty
    /// string if nothing is staged.
    QString stagedVersion() const;

    /// Hands the staged payload to the detached swap script.
    ///
    /// Call this while quitting: the script waits for this process to exit
    /// before touching anything. Does nothing when nothing is staged.
    void applyStagedUpdate();

    /// Deletes the staging directory.
    void discardStaged();

private:
    /// Where a staged update lives on disk.
    ///
    /// Passed around by value so the unpacking, which runs on a worker thread,
    /// never has to reach back into this object - it may well outlive it when
    /// the app is closed mid-download.
    struct Layout {
        QString staging;
        QString payload;
        QString versionFile;
    };

    /// Root of the installation to be replaced: the .app bundle on macOS, the
    /// directory holding the executable on Windows.
    static QString installRoot();

    /// Writes the downloaded archive out and unpacks it. Blocking, so it runs
    /// on a worker thread.
    static bool stageDownload(const Layout &layout, const QByteArray &data,
                              const QString &url, const QString &version);

    /// Unpacks the downloaded archive into `layout.payload`. Blocking.
    static bool unpack(const Layout &layout, const QString &archivePath);

    static void discardStaged(const Layout &layout);

    /// Writes the swap script and returns its path, or an empty string.
    QString writeSwapScript() const;

    Layout layout() const;

    const Paths &paths;

    /// Guards against staging twice, e.g. when a check fires again while a
    /// download is still running. Shared with, and cleared by, the worker
    /// thread, which may outlive this object.
    std::shared_ptr<std::atomic<bool>> staging_ =
        std::make_shared<std::atomic<bool>>(false);
};

}  // namespace chatterino
