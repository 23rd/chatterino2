// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/buttons/InitUpdateButton.hpp"

#include "Application.hpp"
#include "singletons/Updates.hpp"
#include "widgets/buttons/SvgButton.hpp"
#include "widgets/dialogs/UpdateDialog.hpp"

namespace chatterino {

void initUpdateButton(SvgButton &button, const std::function<void()> &relayout,
                      pajlada::Signals::SignalHolder &signalHolder)
{
    button.hide();

    // show update prompt when clicking the button
    QObject::connect(&button, &Button::leftClicked, [&button, relayout] {
        auto *dialog = new UpdateDialog();

        auto globalPoint = button.mapToGlobal(
            QPoint(int(-100 * button.scale()), button.height()));

        // Make sure that update dialog will not go off left edge of screen
        if (globalPoint.x() < 0)
        {
            globalPoint.setX(0);
        }

        dialog->moveTo(globalPoint, widgets::BoundsChecking::DesiredPosition);
        dialog->show();
        dialog->raise();

        // We can safely ignore the signal connection because the dialog will always
        // be destroyed before the button is destroyed, since it is destroyed on focus loss
        //
        // The button is either attached to a Notebook, or a Window frame
        std::ignore = dialog->dismissed.connect([&button, relayout]() {
            button.hide();
            relayout();
        });

        //        handle.reset(dialog);
        //        dialog->closing.connect([&handle] { handle.release(); });
    });

    // update image when state changes
    auto updateChange = [&button, relayout](auto) {
        const auto &updates = getApp()->getUpdates();
        button.setVisible(updates.shouldShowUpdateButton());

        // One icon, tinted by state: red when something went wrong, green once
        // the update is downloaded and only a restart is left, and the theme's
        // own colour while it is merely available or downloading.
        std::optional<QColor> tint;
        if (updates.isError())
        {
            tint = QColor(255, 92, 92);
        }
        else if (updates.getStatus() == Updates::UpdateReady)
        {
            tint = QColor(88, 190, 110);
        }
        button.setColor(tint);

        button.setToolTip([&]() -> QString {
            switch (updates.getStatus())
            {
                case Updates::UpdateReady:
                    return QStringLiteral(
                               "Version %1 is ready - it installs when you "
                               "close Chatterino")
                        .arg(updates.getOnlineVersion());
                case Updates::Downloading: {
                    auto progress = updates.getDownloadProgress();
                    return progress >= 0
                               ? QStringLiteral("Downloading the update... %1%")
                                     .arg(progress)
                               : QStringLiteral("Downloading the update...");
                }
                case Updates::UpdateAvailable:
                    return QStringLiteral("Version %1 is available")
                        .arg(updates.getOnlineVersion());
                default:
                    return QStringLiteral("Update");
            }
        }());

        relayout();
    };

    updateChange(getApp()->getUpdates().getStatus());

    signalHolder.managedConnect(getApp()->getUpdates().statusUpdated,
                                [updateChange](auto status) {
                                    updateChange(status);
                                });
}

}  // namespace chatterino
