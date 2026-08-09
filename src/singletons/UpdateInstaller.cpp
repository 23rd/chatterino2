// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "singletons/UpdateInstaller.hpp"

#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"
#include "common/QLogging.hpp"
#include "common/Version.hpp"
#include "singletons/Paths.hpp"
#include "util/CombinePath.hpp"
#include "util/PostToThread.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QtConcurrent>
#include <QUrl>

#ifdef USEWINSDK
#    include <Windows.h>
#endif

namespace {

using namespace chatterino;

/// Release archives are on the order of a hundred megabytes.
constexpr int DOWNLOAD_TIMEOUT_MS = 15 * 60 * 1000;
constexpr int PROCESS_TIMEOUT_MS = 10 * 60 * 1000;

/// Runs `program` to completion, returning whether it exited cleanly.
bool runProcess(const QString &program, const QStringList &arguments)
{
    QProcess process;
    process.start(program, arguments);

    if (!process.waitForStarted(30000))
    {
        qCWarning(chatterinoUpdate)
            << "failed to start" << program << process.errorString();
        return false;
    }

    if (!process.waitForFinished(PROCESS_TIMEOUT_MS))
    {
        qCWarning(chatterinoUpdate) << program << "timed out";
        process.kill();
        process.waitForFinished(5000);
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    {
        qCWarning(chatterinoUpdate)
            << program << "exited with" << process.exitCode()
            << process.readAllStandardError().trimmed();
        return false;
    }

    return true;
}

bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    if (!file.open(QIODevice::Truncate | QIODevice::WriteOnly))
    {
        qCWarning(chatterinoUpdate)
            << "failed to open" << path << file.errorString();
        return false;
    }

    if (file.write(contents) != contents.size())
    {
        qCWarning(chatterinoUpdate)
            << "failed to write" << path << file.errorString();
        return false;
    }

    file.close();
    return true;
}

}  // namespace

namespace chatterino {

UpdateInstaller::UpdateInstaller(const Paths &paths_)
    : paths(paths_)
{
}

bool UpdateInstaller::isSupported()
{
#if CHATTERINO_FORK_RELEASE && (defined(Q_OS_MACOS) || defined(Q_OS_WIN))
    const auto &version = Version::instance();
    if (version.isNightly() || version.isFlatpak())
    {
        return false;
    }

    const auto root = installRoot();
    if (root.isEmpty())
    {
        return false;
    }

    // The swap replaces the install root itself, so what has to be writable is
    // the directory containing it. On Windows that also rules out an
    // installer-managed copy under Program Files, where we'd need elevation.
    return QFileInfo(QFileInfo(root).absolutePath()).isWritable();
#else
    return false;
#endif
}

QString UpdateInstaller::installRoot()
{
    const auto appDir = QCoreApplication::applicationDirPath();

#ifdef Q_OS_MACOS
    // <bundle>/Contents/MacOS/chatterino -> <bundle>
    QDir dir(appDir);
    if (!dir.cdUp() || !dir.cdUp())
    {
        return {};
    }

    auto bundle = dir.absolutePath();
    if (!bundle.endsWith(QStringLiteral(".app")))
    {
        // Not running from a bundle, e.g. a raw build directory.
        return {};
    }
    return bundle;
#else
    return appDir;
#endif
}

UpdateInstaller::Layout UpdateInstaller::layout() const
{
    auto staging = combinePath(this->paths.miscDirectory, "update-staging");
    return {
        .staging = staging,
        .payload = combinePath(staging, "payload"),
        .versionFile = combinePath(staging, "version"),
    };
}

QString UpdateInstaller::stagedVersion() const
{
    const auto layout = this->layout();

    QFile file(layout.versionFile);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }

    auto version = QString::fromUtf8(file.readAll()).trimmed();
    if (version.isEmpty() || !QFileInfo::exists(layout.payload))
    {
        return {};
    }

    return version;
}

void UpdateInstaller::discardStaged()
{
    discardStaged(this->layout());
}

void UpdateInstaller::discardStaged(const Layout &layout)
{
    QDir staging(layout.staging);
    if (staging.exists() && !staging.removeRecursively())
    {
        qCWarning(chatterinoUpdate)
            << "failed to remove staging directory" << layout.staging;
    }
}

void UpdateInstaller::stage(const QString &url, const QString &version,
                            std::function<void(bool)> onFinished,
                            std::function<void(int)> onProgress)
{
    // Always answers on the GUI thread, so callers can touch widgets.
    auto finish = [onFinished](bool ok) {
        if (onFinished)
        {
            postToThread([onFinished, ok] {
                onFinished(ok);
            });
        }
    };

    if (!isSupported())
    {
        finish(false);
        return;
    }

    if (*this->staging_)
    {
        // A download is already running; it will report on its own.
        return;
    }

    if (this->stagedVersion() == version)
    {
        qCDebug(chatterinoUpdate) << version << "is already staged";
        finish(true);
        return;
    }

    *this->staging_ = true;
    qCDebug(chatterinoUpdate)
        << "downloading update" << version << "from" << url;

    // Everything the background work needs is copied in, so nothing here
    // reaches back into this object - closing the app mid-download must not
    // leave a worker thread holding a dangling pointer.
    const auto layout = this->layout();
    auto staging = this->staging_;

    NetworkRequest(url)
        .timeout(DOWNLOAD_TIMEOUT_MS)
        .onProgress([onProgress](qint64 received, qint64 total) {
            if (!onProgress)
            {
                return;
            }
            // The server doesn't always announce a length; -1 tells the UI to
            // say "downloading" without pretending to know how far along it is.
            int percent =
                total > 0 ? static_cast<int>((received * 100) / total) : -1;
            postToThread([onProgress, percent] {
                onProgress(percent);
            });
        })
        .onError([staging, version, finish](const NetworkResult &result) {
            qCWarning(chatterinoUpdate) << "failed to download update"
                                        << version << result.formatError();
            *staging = false;
            finish(false);
        })
        .onSuccess([staging, layout, url, version,
                    finish](const NetworkResult &result) {
            if (result.status() != 200)
            {
                qCWarning(chatterinoUpdate) << "failed to download update"
                                            << version << result.formatError();
                *staging = false;
                finish(false);
                return;
            }

            // Unpacking shells out and moves hundreds of megabytes around, so
            // keep it off the GUI thread.
            std::ignore =
                QtConcurrent::run([staging, layout, data = result.getData(),
                                   url, version, finish] {
                    bool ok = stageDownload(layout, data, url, version);
                    *staging = false;
                    finish(ok);
                });
        })
        .execute();
}

bool UpdateInstaller::stageDownload(const Layout &layout,
                                    const QByteArray &data, const QString &url,
                                    const QString &version)
{
    discardStaged(layout);

    if (!QDir().mkpath(layout.staging))
    {
        qCWarning(chatterinoUpdate) << "failed to create" << layout.staging;
        return false;
    }

    auto suffix = QFileInfo(QUrl(url).path()).suffix();
    if (suffix.isEmpty())
    {
        suffix = QStringLiteral("archive");
    }
    auto archivePath =
        combinePath(layout.staging, QStringLiteral("update.") + suffix);

    if (!writeFile(archivePath, data))
    {
        discardStaged(layout);
        return false;
    }

    if (!unpack(layout, archivePath))
    {
        qCWarning(chatterinoUpdate) << "failed to unpack update" << version;
        discardStaged(layout);
        return false;
    }

    QFile::remove(archivePath);

    // Written last on purpose: stagedVersion() only reports a version once the
    // payload beside it is complete, so an interrupted download stays invisible.
    if (!writeFile(layout.versionFile, version.toUtf8()))
    {
        discardStaged(layout);
        return false;
    }

    qCDebug(chatterinoUpdate)
        << "staged update" << version << "- it will be applied on quit";
    return true;
}

bool UpdateInstaller::unpack(const Layout &layout, const QString &archivePath)
{
    const auto &payload = layout.payload;

#ifdef Q_OS_MACOS
    // Mount the disk image, copy the bundle out, unmount. `cp -a` rather than
    // Qt's file copies because a bundle is full of symlinks.
    const auto mountPoint = combinePath(layout.staging, "mnt");
    if (!QDir().mkpath(mountPoint))
    {
        return false;
    }

    if (!runProcess("/usr/bin/hdiutil",
                    {"attach", archivePath, "-nobrowse", "-readonly",
                     "-noautoopen", "-mountpoint", mountPoint}))
    {
        return false;
    }

    bool copied = false;
    const auto bundles =
        QDir(mountPoint)
            .entryList({"*.app"}, QDir::Dirs | QDir::NoDotAndDotDot);
    if (bundles.isEmpty())
    {
        qCWarning(chatterinoUpdate) << "no .app found in the disk image";
    }
    else
    {
        // payload must not exist yet: it becomes the bundle itself.
        copied = runProcess(
            "/bin/cp",
            {"-a", combinePath(mountPoint, bundles.first()), payload});
    }

    runProcess("/usr/bin/hdiutil", {"detach", mountPoint, "-quiet"});
    QDir(mountPoint).removeRecursively();

    return copied;
#elif defined(Q_OS_WIN)
    // bsdtar has shipped with Windows since 10 1803 and reads .zip.
    const auto extracted = combinePath(layout.staging, "extract");
    if (!QDir().mkpath(extracted))
    {
        return false;
    }

    if (!runProcess("tar", {"-xf", QDir::toNativeSeparators(archivePath), "-C",
                            QDir::toNativeSeparators(extracted)}))
    {
        return false;
    }

    // The release zip wraps everything in a single Chatterino2/ directory;
    // unwrap it so the payload holds exactly what belongs at the install root.
    QDir extractedDir(extracted);
    const auto entries =
        extractedDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
    auto root = extracted;
    if (entries.size() == 1 &&
        QFileInfo(combinePath(extracted, entries.first())).isDir())
    {
        root = combinePath(extracted, entries.first());
    }

    if (!QDir().rename(root, payload))
    {
        qCWarning(chatterinoUpdate)
            << "failed to move" << root << "to" << payload;
        return false;
    }

    QDir(extracted).removeRecursively();
    return true;
#else
    (void)layout;
    (void)archivePath;
    (void)payload;
    return false;
#endif
}

QString UpdateInstaller::writeSwapScript() const
{
    const auto target = installRoot();
    if (target.isEmpty())
    {
        return {};
    }

    const auto layout = this->layout();
    const auto pid = QString::number(QCoreApplication::applicationPid());

#ifdef Q_OS_MACOS
    const auto script = QStringLiteral(R"(#!/bin/sh
# Generated by Chatterino. Swaps a staged update in once Chatterino has exited.
PID="%1"
TARGET="%2"
PAYLOAD="%3"
STAGING="%4"

# Wait up to two minutes for Chatterino to go away.
tries=0
while kill -0 "$PID" 2>/dev/null; do
    if [ "$tries" -ge 600 ]; then
        exit 1
    fi
    tries=$((tries + 1))
    sleep 0.2
done

BACKUP="$TARGET.old"
rm -rf "$BACKUP"
mv "$TARGET" "$BACKUP" || exit 1

if ! cp -a "$PAYLOAD" "$TARGET"; then
    # Put the old build back so the user is never left without an app.
    rm -rf "$TARGET"
    mv "$BACKUP" "$TARGET"
    exit 1
fi
rm -rf "$BACKUP"

# The bundle came out of a downloaded disk image and its signature no longer
# matches after being copied. Without clearing quarantine and re-signing ad-hoc
# macOS refuses to launch it on Apple Silicon.
xattr -dr com.apple.quarantine "$TARGET" 2>/dev/null
codesign --force --deep --sign - "$TARGET" 2>/dev/null

rm -rf "$STAGING"
)")
                            .arg(pid, target, layout.payload, layout.staging);

    const auto scriptPath = combinePath(layout.staging, "apply-update.sh");
#elif defined(Q_OS_WIN)
    const auto script =
        QStringLiteral("@echo off\r\n"
                       "rem Generated by Chatterino. Swaps a staged update "
                       "in once Chatterino has exited.\r\n"
                       "setlocal\r\n"
                       "set \"PID=%1\"\r\n"
                       "set \"TARGET=%2\"\r\n"
                       "set \"PAYLOAD=%3\"\r\n"
                       "set \"STAGING=%4\"\r\n"
                       "\r\n"
                       "rem Wait up to two minutes for Chatterino to go "
                       "away.\r\n"
                       "set /a tries=0\r\n"
                       ":wait\r\n"
                       "tasklist /FI \"PID eq %PID%\" /NH 2>nul | find "
                       "\"%PID%\" >nul || goto gone\r\n"
                       "set /a tries+=1\r\n"
                       "if %tries% GEQ 120 exit /b 1\r\n"
                       "ping -n 2 127.0.0.1 >nul\r\n"
                       "goto wait\r\n"
                       ":gone\r\n"
                       "\r\n"
                       "rem Merge rather than mirror: in portable mode the "
                       "settings live in this same\r\n"
                       "rem directory and must survive. Robocopy only "
                       "treats 8 and up as failure.\r\n"
                       "robocopy \"%PAYLOAD%\" \"%TARGET%\" /E /MOVE /R:2 "
                       "/W:1 /NFL /NDL /NJH /NJS >nul\r\n"
                       "if %ERRORLEVEL% GEQ 8 exit /b 1\r\n"
                       "\r\n"
                       "rem This script lives inside the staging directory "
                       "and cmd holds it open, so\r\n"
                       "rem the directory itself can't go yet. Drop the "
                       "marker instead - that alone\r\n"
                       "rem makes the staged update invisible, and the next "
                       "start sweeps the rest.\r\n"
                       "del /q \"%STAGING%\\version\" >nul 2>&1\r\n"
                       "rmdir /s /q \"%PAYLOAD%\" >nul 2>&1\r\n")
            .arg(pid, QDir::toNativeSeparators(target),
                 QDir::toNativeSeparators(layout.payload),
                 QDir::toNativeSeparators(layout.staging));

    const auto scriptPath = combinePath(layout.staging, "apply-update.bat");
#else
    const QString script;
    const QString scriptPath;
    return {};
#endif

    if (!writeFile(scriptPath, script.toUtf8()))
    {
        return {};
    }

    return scriptPath;
}

void UpdateInstaller::applyStagedUpdate()
{
    const auto version = this->stagedVersion();
    if (version.isEmpty())
    {
        return;
    }

    const auto script = this->writeSwapScript();
    if (script.isEmpty())
    {
        return;
    }

    bool started = false;

#ifdef Q_OS_MACOS
    started = QProcess::startDetached("/bin/sh", {script});
#elif defined(Q_OS_WIN)
    // Configured on an instance rather than through the static overload so the
    // helper can be told not to flash up a console window.
    QProcess process;
    process.setProgram("cmd.exe");
    process.setArguments({"/c", QDir::toNativeSeparators(script)});
#    ifdef USEWINSDK
    process.setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments *args) {
            args->flags |= CREATE_NO_WINDOW;
        });
#    endif
    started = process.startDetached();
#endif

    if (started)
    {
        qCDebug(chatterinoUpdate)
            << "handed update" << version << "to" << script;
    }
    else
    {
        qCWarning(chatterinoUpdate) << "failed to start" << script;
    }
}

}  // namespace chatterino
