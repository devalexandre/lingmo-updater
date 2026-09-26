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

#include "updatemanager.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QLocale>
#include <QRegularExpression>

#include <unistd.h>

static const QString PacmanLock = QStringLiteral("/var/lib/pacman/db.lck");
static constexpr int MaxLogSize = 512 * 1024;

// Updating these asks for a restart to take effect
static bool needsReboot(const QString &name)
{
    return name == QLatin1String("linux") || name.startsWith(QLatin1String("linux-"))
        || name == QLatin1String("systemd") || name.startsWith(QLatin1String("systemd-"))
        || name == QLatin1String("glibc") || name == QLatin1String("lingmo-core");
}

static QProcessEnvironment cEnvironment()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    return env;
}

static QString checkupdatesDb()
{
    // checkupdates' own default, spelled out so pacman -Si can read the fresh sync dbs
    return QDir::tempPath() + QStringLiteral("/checkup-db-%1/").arg(getuid());
}

UpdateManager::UpdateManager(QObject *parent)
    : QObject(parent)
    , m_fakeMode(qEnvironmentVariable("LINGMO_UPDATER_FAKE"))
{
    if (m_fakeMode == QLatin1String("1"))
        m_fakeMode = QStringLiteral("ok");

    // pacman can print a lot: repaint the log view at most ~6 times per second
    m_logTimer.setSingleShot(true);
    m_logTimer.setInterval(150);
    connect(&m_logTimer, &QTimer::timeout, this, &UpdateManager::logChanged);
}

QString UpdateManager::fakeScript() const
{
    QFile file(QStringLiteral(":/fake/fake-pacman.sh"));
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(file.readAll());
}

QString UpdateManager::lastCheckText() const
{
    if (!m_lastCheck.isValid())
        return QString();
    return QLocale().toString(m_lastCheck.time(), QLocale::ShortFormat);
}

QString UpdateManager::updatesKey() const
{
    QStringList parts;
    for (const QVariant &v : m_packages) {
        const QVariantMap pkg = v.toMap();
        parts << pkg.value(QStringLiteral("name")).toString() + QLatin1Char('=')
                     + pkg.value(QStringLiteral("newVersion")).toString();
    }
    return parts.join(QLatin1Char(' '));
}

void UpdateManager::setState(State state, const QString &error)
{
    m_state = state;
    m_errorMessage = error;
    Q_EMIT stateChanged();
}

void UpdateManager::setStep(qreal progress, const QString &text)
{
    if (qFuzzyCompare(progress + 2, m_progress + 2) && text == m_stepText)
        return;
    m_progress = progress;
    m_stepText = text;
    Q_EMIT progressChanged();
}

void UpdateManager::check()
{
    if (m_process)
        return; // a check or an update is already running

    setState(Checking);
    setStep(-1, tr("Checking for updates…"));

    m_process = new QProcess(this);
    m_process->setProcessEnvironment(cEnvironment());
    if (fakeMode()) {
        m_process->setProgram(QStringLiteral("sh"));
        m_process->setArguments({QStringLiteral("-c"), fakeScript(), QStringLiteral("sh"),
                                 QStringLiteral("check"), m_fakeMode});
    } else {
        QProcessEnvironment env = cEnvironment();
        env.insert(QStringLiteral("CHECKUPDATES_DB"), checkupdatesDb());
        m_process->setProcessEnvironment(env);
        m_process->setProgram(QStringLiteral("checkupdates"));
        m_process->setArguments({QStringLiteral("--nocolor")});
    }
    connect(m_process, &QProcess::finished, this, &UpdateManager::onCheckFinished);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        m_process->deleteLater();
        m_process = nullptr;
        m_lastCheck = QDateTime::currentDateTime();
        Q_EMIT lastCheckChanged();
        setState(CheckFailed, tr("The checkupdates tool was not found. Install the pacman-contrib package."));
        Q_EMIT checkFinished();
    });
    m_process->start();
}

void UpdateManager::onCheckFinished(int exitCode, QProcess::ExitStatus status)
{
    const QString out = QString::fromUtf8(m_process->readAllStandardOutput());
    const QString err = QString::fromUtf8(m_process->readAllStandardError());
    m_process->deleteLater();
    m_process = nullptr;

    m_lastCheck = QDateTime::currentDateTime();
    Q_EMIT lastCheckChanged();

    if (status != QProcess::NormalExit || (exitCode != 0 && exitCode != 2)) {
        qWarning() << "checkupdates failed:" << exitCode << err.trimmed();
        QString message;
        if (err.contains(QLatin1String("Cannot fetch updates")))
            message = tr("Could not reach the package servers. Check your internet connection and try again.");
        else
            message = tr("Could not check for updates.") + QLatin1Char(' ') + err.trimmed();
        setState(CheckFailed, message);
        Q_EMIT checkFinished();
        return;
    }

    static const QRegularExpression re(QStringLiteral("^(\\S+)\\s+(\\S+)\\s+->\\s+(\\S+)"));
    QVariantList packages;
    const QStringList lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QRegularExpressionMatch m = re.match(line.trimmed());
        if (!m.hasMatch())
            continue;
        packages << QVariantMap{
            {QStringLiteral("name"), m.captured(1)},
            {QStringLiteral("oldVersion"), m.captured(2)},
            {QStringLiteral("newVersion"), m.captured(3)},
        };
    }

    m_packages = packages;
    m_downloadSize.clear();
    Q_EMIT packagesChanged();
    Q_EMIT downloadSizeChanged();
    setStep(-1, QString());
    setState(m_packages.isEmpty() ? UpToDate : Available);
    Q_EMIT checkFinished();

    if (!m_packages.isEmpty())
        queryDownloadSize();
}

void UpdateManager::queryDownloadSize()
{
    QStringList names;
    for (const QVariant &v : std::as_const(m_packages))
        names << v.toMap().value(QStringLiteral("name")).toString();

    auto *proc = new QProcess(this);
    proc->setProcessEnvironment(cEnvironment());
    if (fakeMode()) {
        proc->setProgram(QStringLiteral("sh"));
        proc->setArguments(QStringList{QStringLiteral("-c"), fakeScript(), QStringLiteral("sh"),
                                       QStringLiteral("info")} + names);
    } else {
        proc->setProgram(QStringLiteral("pacman"));
        proc->setArguments(QStringList{QStringLiteral("-Si"), QStringLiteral("--dbpath"), checkupdatesDb()}
                           + names);
    }
    connect(proc, &QProcess::finished, this, [this, proc, names](int exitCode) {
        proc->deleteLater();
        if (exitCode != 0 || m_state != Available)
            return;
        // "Name : foo" ... "Download Size : 1.23 MiB"; a package in several repos counts once
        static const QRegularExpression field(QStringLiteral("^(Name|Download Size)\\s*:\\s*(.+)$"));
        static const QRegularExpression size(QStringLiteral("^([0-9.]+)\\s*(B|KiB|MiB|GiB|TiB)$"));
        static const QHash<QString, double> units{{QStringLiteral("B"), 1.0},
                                                  {QStringLiteral("KiB"), 1024.0},
                                                  {QStringLiteral("MiB"), 1024.0 * 1024},
                                                  {QStringLiteral("GiB"), 1024.0 * 1024 * 1024},
                                                  {QStringLiteral("TiB"), 1024.0 * 1024 * 1024 * 1024}};
        QHash<QString, double> sizes;
        QString name;
        const QStringList lines = QString::fromUtf8(proc->readAllStandardOutput()).split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            const QRegularExpressionMatch m = field.match(line.trimmed());
            if (!m.hasMatch())
                continue;
            if (m.captured(1) == QLatin1String("Name")) {
                name = m.captured(2).trimmed();
                continue;
            }
            const QRegularExpressionMatch s = size.match(m.captured(2).trimmed());
            if (s.hasMatch() && !name.isEmpty() && !sizes.contains(name))
                sizes.insert(name, s.captured(1).toDouble() * units.value(s.captured(2)));
        }
        double total = 0;
        for (double bytes : std::as_const(sizes))
            total += bytes;
        if (total <= 0)
            return;
        m_downloadSize = QLocale().formattedDataSize(qint64(total), 1);
        Q_EMIT downloadSizeChanged();
    });
    proc->start();
}

void UpdateManager::update()
{
    if (m_process || m_packages.isEmpty())
        return;

    m_log.clear();
    m_pending.clear();
    m_upgraded.clear();
    m_downloadTotal = 0;
    m_downloaded = 0;
    m_inHooks = false;
    m_rebootRecommended = false;
    Q_EMIT logChanged();

    if (!fakeMode() && QFile::exists(PacmanLock)) {
        setState(UpdateFailed, tr("Another program is installing or updating software. Wait for it to finish and try again."));
        appendLog(tr("The package database is locked (%1).").arg(PacmanLock) + QLatin1Char('\n'));
        Q_EMIT updateFinished(false);
        return;
    }

    setState(Updating);
    setStep(-1, tr("Waiting for authorization…"));

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_process->setStandardInputFile(QProcess::nullDevice());
    m_process->setProcessEnvironment(cEnvironment());
    if (fakeMode()) {
        m_process->setProgram(QStringLiteral("sh"));
        m_process->setArguments({QStringLiteral("-c"), fakeScript(), QStringLiteral("sh"),
                                 QStringLiteral("update"), m_fakeMode});
        appendLog(QStringLiteral("$ [fake] pkexec pacman -Syu --noconfirm\n"));
    } else {
        m_process->setProgram(QStringLiteral("pkexec"));
        m_process->setArguments({QStringLiteral("/usr/bin/pacman"), QStringLiteral("-Syu"),
                                 QStringLiteral("--noconfirm")});
        appendLog(QStringLiteral("$ pkexec pacman -Syu --noconfirm\n"));
    }
    connect(m_process, &QProcess::readyReadStandardOutput, this, &UpdateManager::onUpdateOutput);
    connect(m_process, &QProcess::finished, this, &UpdateManager::onUpdateFinished);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        m_process->deleteLater();
        m_process = nullptr;
        setStep(-1, QString());
        setState(UpdateFailed, tr("Could not start pkexec. Make sure polkit is installed."));
        Q_EMIT updateFinished(false);
    });
    m_process->start();
}

void UpdateManager::appendLog(const QString &text)
{
    m_log += text;
    if (m_log.size() > MaxLogSize)
        m_log = m_log.right(MaxLogSize * 3 / 4);
    if (!m_logTimer.isActive())
        m_logTimer.start();
}

void UpdateManager::onUpdateOutput()
{
    QString chunk = QString::fromUtf8(m_process->readAllStandardOutput());
    chunk.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    m_pending += chunk;
    const int end = m_pending.lastIndexOf(QLatin1Char('\n'));
    if (end < 0)
        return;
    const QString complete = m_pending.left(end + 1);
    m_pending = m_pending.mid(end + 1);
    appendLog(complete);
    const QStringList lines = complete.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines)
        parseLine(line.trimmed());
}

// Maps pacman's (non-interactive) output to an overall progress:
// sync 0-10%, download 10-40%, checks 40-50%, install 50-95%, hooks 95-100%
void UpdateManager::parseLine(const QString &line)
{
    static const QRegularExpression counted(QStringLiteral("^\\((\\d+)/(\\d+)\\)\\s+(.*)$"));
    static const QRegularExpression packagesLine(QStringLiteral("^Packages \\((\\d+)\\)"));

    if (line.startsWith(QLatin1String(":: Synchronizing package databases"))) {
        setStep(0.02, tr("Synchronizing package databases…"));
    } else if (line.startsWith(QLatin1String(":: Starting full system upgrade"))) {
        setStep(0.08, tr("Resolving dependencies…"));
    } else if (const auto m = packagesLine.match(line); m.hasMatch()) {
        m_downloadTotal = m.captured(1).toInt();
    } else if (line.startsWith(QLatin1String(":: Retrieving packages"))) {
        setStep(0.10, tr("Downloading packages…"));
    } else if (line.endsWith(QLatin1String("downloading...")) && m_downloadTotal > 0) {
        m_downloaded = qMin(m_downloaded + 1, m_downloadTotal);
        setStep(0.10 + 0.30 * m_downloaded / m_downloadTotal,
                tr("Downloading packages (%1 of %2)…").arg(m_downloaded).arg(m_downloadTotal));
    } else if (line.startsWith(QLatin1String(":: Processing package changes"))) {
        setStep(0.50, tr("Installing updates…"));
    } else if (line.startsWith(QLatin1String(":: Running post-transaction hooks"))) {
        m_inHooks = true;
        setStep(0.95, tr("Finishing…"));
    } else if (const auto m = counted.match(line); m.hasMatch()) {
        const int i = m.captured(1).toInt();
        const int n = qMax(1, m.captured(2).toInt());
        const QString what = m.captured(3);
        if (m_inHooks) {
            setStep(0.95 + 0.05 * i / n, tr("Finishing…"));
        } else if (what.startsWith(QLatin1String("checking")) || what.startsWith(QLatin1String("loading"))) {
            setStep(qMax(m_progress, 0.40 + 0.10 * i / n * 0.9), tr("Verifying packages…"));
        } else {
            static const QRegularExpression op(
                QStringLiteral("^(upgrading|installing|reinstalling|downgrading|removing)\\s+(\\S+)"));
            if (const auto o = op.match(what); o.hasMatch()) {
                m_upgraded << o.captured(2);
                setStep(0.50 + 0.45 * i / n, tr("Installing updates (%1 of %2)…").arg(i).arg(n));
            }
        }
    }
}

void UpdateManager::onUpdateFinished(int exitCode, QProcess::ExitStatus status)
{
    onUpdateOutput();
    if (!m_pending.isEmpty()) {
        appendLog(m_pending + QLatin1Char('\n'));
        parseLine(m_pending.trimmed());
        m_pending.clear();
    }
    m_process->deleteLater();
    m_process = nullptr;
    m_logTimer.stop();
    Q_EMIT logChanged();

    if (status == QProcess::NormalExit && exitCode == 0) {
        QStringList names = m_upgraded;
        for (const QVariant &v : std::as_const(m_packages))
            names << v.toMap().value(QStringLiteral("name")).toString();
        m_rebootRecommended = std::any_of(names.cbegin(), names.cend(), needsReboot);
        setStep(1.0, QString());
        setState(Updated);
        Q_EMIT updateFinished(true);
        return;
    }

    QString message;
    if (status != QProcess::NormalExit) {
        message = tr("The update was interrupted.");
    } else if (exitCode == 126 || exitCode == 127) {
        // pkexec: 126 = dialog dismissed, 127 = not authorized. Nothing ran: back to the list
        setStep(-1, QString());
        setState(Available, tr("Authentication was cancelled. Nothing was changed."));
        Q_EMIT updateFinished(false);
        return;
    } else if (m_log.contains(QLatin1String("unable to lock database"))) {
        message = tr("Another program is installing or updating software. Wait for it to finish and try again.");
    } else if (m_log.contains(QLatin1String("failed retrieving file"))
               || m_log.contains(QLatin1String("failed to synchronize"))
               || m_log.contains(QLatin1String("Could not resolve host"))
               || m_log.contains(QLatin1String("Connection timed out"))
               || m_log.contains(QLatin1String("failed to retrieve some files"))) {
        message = tr("Could not download the updates. Check your internet connection and try again.");
    } else {
        message = tr("The update failed. Open the details to see what went wrong.");
    }
    setStep(-1, QString());
    setState(UpdateFailed, message);
    Q_EMIT updateFinished(false);
}

void UpdateManager::reboot()
{
    if (fakeMode()) {
        qInfo("[fake] reboot requested");
        return;
    }
    QDBusInterface session(QStringLiteral("com.lingmo.Session"), QStringLiteral("/Session"),
                           QStringLiteral("com.lingmo.Session"), QDBusConnection::sessionBus());
    if (session.isValid()) {
        session.call(QStringLiteral("reboot"));
        return;
    }
    QDBusInterface login1(QStringLiteral("org.freedesktop.login1"), QStringLiteral("/org/freedesktop/login1"),
                          QStringLiteral("org.freedesktop.login1.Manager"), QDBusConnection::systemBus());
    login1.call(QStringLiteral("Reboot"), true);
}
