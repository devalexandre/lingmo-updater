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

#include "updaterapp.h"
#include "updatemanager.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFontDatabase>
#include <QFontInfo>
#include <QIcon>
#include <QMenu>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QFile>
#include <QRegularExpression>
#include <QSysInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSystemTrayIcon>

#include <KWindowSystem>
#include <KX11Extras>

using namespace std::chrono_literals;

static const QString NotifyService = QStringLiteral("org.freedesktop.Notifications");
static const QString NotifyPath = QStringLiteral("/org/freedesktop/Notifications");

static QIcon updateIcon()
{
    return QIcon::fromTheme(QStringLiteral("system-software-update"),
                            QIcon::fromTheme(QStringLiteral("update-high")));
}

// pacman's output reads best in a monospaced font; fontconfig's "monospace" alias
// isn't always one
static QString fixedFontFamily()
{
    const QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (QFontInfo(font).fixedPitch())
        return font.family();
    const QStringList families = QFontDatabase::families();
    for (const char *name : {"Noto Sans Mono", "DejaVu Sans Mono", "Hack", "Liberation Mono", "Source Code Pro"}) {
        if (families.contains(QLatin1String(name)))
            return QLatin1String(name);
    }
    return font.family();
}

namespace {
// Where pacman gets [archlingmo] from, per /etc/pacman.conf
QString archlingmoDatabase()
{
    QFile conf(QStringLiteral("/etc/pacman.conf"));
    if (!conf.open(QIODevice::ReadOnly))
        return {};
    bool inSection = false;
    while (!conf.atEnd()) {
        const QString line = QString::fromUtf8(conf.readLine()).trimmed();
        if (line.startsWith(QLatin1Char('['))) {
            inSection = line == QLatin1String("[archlingmo]");
            continue;
        }
        static const QRegularExpression server(QStringLiteral("^Server\\s*=\\s*(\\S+)"));
        const auto match = server.match(line);
        if (inSection && match.hasMatch()) {
            QString url = match.captured(1);
            url.replace(QLatin1String("$arch"), QSysInfo::currentCpuArchitecture());
            url.replace(QLatin1String("$repo"), QLatin1String("archlingmo"));
            return url + QLatin1String("/archlingmo.db");
        }
    }
    return {};
}
}

UpdaterApp::UpdaterApp(bool resident, QObject *parent)
    : QObject(parent)
    , m_resident(resident)
    , m_manager(new UpdateManager(this))
    , m_tray(new QSystemTrayIcon(this))
    , m_menu(new QMenu)
{
    m_tray->setIcon(updateIcon());
    m_tray->setContextMenu(m_menu);
    m_menu->addAction(tr("Open Updates"), this, &UpdaterApp::show);
    m_menu->addAction(tr("Check Again"), this, &UpdaterApp::checkNow);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            show();
    });

    connect(m_manager, &UpdateManager::stateChanged, this, &UpdaterApp::updateTray);
    connect(m_manager, &UpdateManager::packagesChanged, this, &UpdaterApp::updateTray);
    connect(m_manager, &UpdateManager::checkFinished, this, &UpdaterApp::onCheckFinished);
    connect(m_manager, &UpdateManager::updateFinished, this, &UpdaterApp::onUpdateFinished);

    QDBusConnection::sessionBus().connect(QString(), NotifyPath, NotifyService, QStringLiteral("ActionInvoked"),
                                          this, SLOT(onNotificationAction(uint, QString)));

    if (m_resident) {
        // First check right after login, once the network is up, then every 3 hours
        bool ok = false;
        int delay = qEnvironmentVariableIntValue("LINGMO_UPDATER_DELAY", &ok);
        if (!ok)
            delay = 30;
        QTimer::singleShot(std::chrono::seconds(delay), m_manager, &UpdateManager::check);
        m_timer.setInterval(3h);
        connect(&m_timer, &QTimer::timeout, m_manager, &UpdateManager::check);
        m_timer.start();

        // Lingmo updates show up within minutes of being published: the repository
        // database is tiny to ask about, so it's watched much more often than the
        // full check runs
        m_repoDb = archlingmoDatabase();
        if (!m_repoDb.isEmpty()) {
            m_network = new QNetworkAccessManager(this);
            m_repoTimer.setInterval(10min);
            connect(&m_repoTimer, &QTimer::timeout, this, &UpdaterApp::pollRepo);
            m_repoTimer.start();
            pollRepo();
        }

        QDBusConnection::systemBus().connect(QStringLiteral("org.freedesktop.login1"),
                                             QStringLiteral("/org/freedesktop/login1"),
                                             QStringLiteral("org.freedesktop.login1.Manager"),
                                             QStringLiteral("PrepareForSleep"),
                                             this, SLOT(onPrepareForSleep(bool)));
    }
}

void UpdaterApp::onPrepareForSleep(bool sleeping)
{
    if (sleeping)
        return;
    // The network needs a moment to come back after resume
    QTimer::singleShot(30s, m_manager, &UpdateManager::check);
}

void UpdaterApp::pollRepo()
{
    QNetworkRequest request{QUrl(m_repoDb)};
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    QNetworkReply *reply = m_network->head(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return;
        QByteArray stamp = reply->rawHeader("ETag");
        if (stamp.isEmpty())
            stamp = reply->rawHeader("Last-Modified");
        if (stamp.isEmpty())
            return;
        // The first answer is only the baseline: login already runs a full check
        const bool changed = !m_repoStamp.isEmpty() && stamp != m_repoStamp;
        m_repoStamp = stamp;
        if (changed)
            m_manager->check();
    });
}

UpdaterApp::~UpdaterApp()
{
    delete m_engine;
    delete m_menu;
}

void UpdaterApp::show()
{
    if (!m_engine) {
        m_engine = new QQmlApplicationEngine(this);
        m_engine->rootContext()->setContextProperty(QStringLiteral("updater"), m_manager);
        m_engine->rootContext()->setContextProperty(QStringLiteral("fixedFontFamily"), fixedFontFamily());
        m_engine->load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
        if (m_engine->rootObjects().isEmpty()) {
            qWarning("Failed to load the updater window");
            return;
        }
        m_window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().first());
        if (m_window) {
            connect(m_window, &QWindow::visibleChanged, this, [this](bool visible) {
                if (!visible)
                    maybeQuit();
            });
        }
    }
    if (!m_window)
        return;

    m_window->show();
    m_window->raise();
    m_window->requestActivate();
    if (KWindowSystem::isPlatformX11())
        KX11Extras::forceActiveWindow(m_window->winId());

    if (m_manager->state() == UpdateManager::Idle)
        m_manager->check();
}

void UpdaterApp::checkNow()
{
    m_manager->check();
}

bool UpdaterApp::windowVisible() const
{
    return m_window && m_window->isVisible();
}

void UpdaterApp::maybeQuit()
{
    // Opened from the launcher without the session daemon: leave once there's nothing to do
    if (!m_resident && !windowVisible() && m_manager->state() != UpdateManager::Updating
        && m_manager->state() != UpdateManager::Checking)
        QApplication::quit();
}

void UpdaterApp::updateTray()
{
    const auto state = m_manager->state();
    const int count = m_manager->count();
    const bool pending = count > 0
        && (state == UpdateManager::Available || state == UpdateManager::UpdateFailed);

    if (state == UpdateManager::Updating) {
        m_tray->setToolTip(tr("Updating the system…"));
    } else if (pending) {
        m_tray->setToolTip(tr("%n update(s) available", nullptr, count));
    }
    m_tray->setVisible(pending || state == UpdateManager::Updating);
}

void UpdaterApp::onCheckFinished()
{
    if (m_manager->state() != UpdateManager::Available)
        return;
    // One notification per new set of updates, and none while the window is in front
    const QString key = m_manager->updatesKey();
    if (key == m_notifiedKey)
        return;
    m_notifiedKey = key;
    if (windowVisible())
        return;
    const int count = m_manager->count();
    notify(tr("%n update(s) available", nullptr, count),
           tr("Keep your system secure and up to date."), true);
}

void UpdaterApp::onUpdateFinished(bool success)
{
    // A cancelled authorization needs no notification: the user just did it
    if (!windowVisible() && m_manager->state() != UpdateManager::Available) {
        if (success)
            notify(tr("System updated"),
                   m_manager->rebootRecommended() ? tr("Restart the computer to finish the update.")
                                                  : tr("All updates were installed."),
                   false);
        else
            notify(tr("Update failed"), m_manager->errorMessage(), true);
    }
    maybeQuit();
}

void UpdaterApp::notify(const QString &summary, const QString &body, bool withAction)
{
    QDBusInterface iface(NotifyService, NotifyPath, NotifyService, QDBusConnection::sessionBus());
    QStringList actions;
    if (withAction)
        actions << QStringLiteral("default") << tr("Update") << QStringLiteral("update") << tr("Update");
    const QVariantMap hints{{QStringLiteral("desktop-entry"), QStringLiteral("lingmo-updater")}};
    QDBusReply<uint> reply = iface.call(QStringLiteral("Notify"), QStringLiteral("lingmo-updater"),
                                        m_notificationId, QStringLiteral("system-software-update"),
                                        summary, body, actions, hints, -1);
    if (reply.isValid())
        m_notificationId = reply.value();
    else
        qWarning() << "Notification failed:" << reply.error().message();
}

void UpdaterApp::onNotificationAction(uint id, const QString &action)
{
    if (id != m_notificationId || id == 0)
        return;
    if (action == QLatin1String("default") || action == QLatin1String("update"))
        show();
}
