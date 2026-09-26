/*
 * Copyright (C) 2026 LingmoOS Team.
 *
 * Author:     devalexandre <alexandre@dev2learn.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QIcon>
#include <QLocale>
#include <QQmlEngine>
#include <QSettings>
#include <QStandardPaths>
#include <QTranslator>

#include "updatemanager.h"
#include "updaterapp.h"

static const QString DBusService = QStringLiteral("com.lingmo.Updater");
static const QString DBusPath = QStringLiteral("/Updater");

// The Lingmo platform theme doesn't always hand the icon theme over at startup:
// use the one set in Lingmo Settings.
static void applyIconTheme()
{
    const QString current = QIcon::themeName();
    if (!current.isEmpty() && current != QLatin1String("hicolor"))
        return;
    QSettings theme(QStringLiteral("lingmoos"), QStringLiteral("theme"));
    const bool dark = theme.value(QStringLiteral("DarkMode"), false).toBool();
    const QStringList candidates = {
        theme.value(dark ? QStringLiteral("DarkIconTheme") : QStringLiteral("IconTheme")).toString(),
        dark ? QStringLiteral("Crule-dark") : QStringLiteral("Crule"),
        QStringLiteral("breeze"),
    };
    for (const QString &name : candidates) {
        if (!name.isEmpty()
            && !QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                       QStringLiteral("icons/%1/index.theme").arg(name)).isEmpty()) {
            QIcon::setThemeName(name);
            break;
        }
    }
    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("lingmo-updater"));
    app.setApplicationVersion(QStringLiteral(LINGMO_UPDATER_VERSION));
    app.setDesktopFileName(QStringLiteral("lingmo-updater"));
    app.setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Lingmo system updater"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption background(QStringLiteral("background"),
                                  QStringLiteral("Run as the session daemon: check periodically, no window"));
    parser.addOption(background);
    parser.process(app);
    applyIconTheme();
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("system-software-update")));

    // Installed location first, then next to the binary (staged/relocated installs)
    QTranslator translator;
    for (const QString &dir : {QStringLiteral(TRANSLATIONS_DIR),
                               app.applicationDirPath() + QStringLiteral("/../share/lingmo-updater/translations")}) {
        if (translator.load(QLocale(), QStringLiteral("lingmo-updater"), QStringLiteral("_"), dir)) {
            app.installTranslator(&translator);
            break;
        }
    }

    // One instance per session: launching it again just opens the window
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(DBusService)) {
        if (!parser.isSet(background)) {
            QDBusInterface iface(DBusService, DBusPath, DBusService, bus);
            iface.call(QStringLiteral("show"));
        }
        return 0;
    }

    qmlRegisterUncreatableType<UpdateManager>("Lingmo.Updater", 1, 0, "UpdateManager",
                                              QStringLiteral("Use the updater context property"));

    UpdaterApp updater(parser.isSet(background));
    bus.registerObject(DBusPath, &updater, QDBusConnection::ExportAllSlots);
    if (!parser.isSet(background))
        updater.show();

    return app.exec();
}
