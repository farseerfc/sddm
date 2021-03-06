/***************************************************************************
* Copyright (c) 2015 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
* Copyright (c) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the
* Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
***************************************************************************/

#include "SessionModel.h"

#include "Configuration.h"

#include <QDir>
#include <QFile>
#include <QList>
#include <QProcessEnvironment>
#include <QTextStream>

#include <memory>

namespace SDDM {
    class Session {
    public:
        SessionModel::SessionType type;
        QString directory;
        QString file;
        QString name;
        QString exec;
        QString comment;
    };

    typedef std::shared_ptr<Session> SessionPtr;

    class SessionModelPrivate {
    public:
        int lastIndex { 0 };
        QList<SessionPtr> sessions;
    };

    SessionModel::SessionModel(QObject *parent) : QAbstractListModel(parent), d(new SessionModelPrivate()) {
        populate(SessionModel::X11Session, mainConfig.XDisplay.SessionDir.get());
        populate(SessionModel::WaylandSession, mainConfig.WaylandDisplay.SessionDir.get());
    }

    SessionModel::~SessionModel() {
        delete d;
    }

    QHash<int, QByteArray> SessionModel::roleNames() const {
        // set role names
        QHash<int, QByteArray> roleNames;
        roleNames[DirectoryRole] = "directory";
        roleNames[FileRole] = "file";
        roleNames[NameRole] = "name";
        roleNames[ExecRole] = "exec";
        roleNames[CommentRole] = "comment";

        return roleNames;
    }

    const int SessionModel::lastIndex() const {
        return d->lastIndex;
    }

    int SessionModel::rowCount(const QModelIndex &parent) const {
        return d->sessions.length();
    }

    QVariant SessionModel::data(const QModelIndex &index, int role) const {
        if (index.row() < 0 || index.row() >= d->sessions.count())
            return QVariant();

        // get session
        SessionPtr session = d->sessions[index.row()];

        // return correct value
        if (role == DirectoryRole)
            return session->directory;
        if (role == FileRole)
            return session->file;
        else if (role == NameRole)
            return session->name;
        else if (role == ExecRole)
            return session->exec;
        else if (role == CommentRole)
            return session->comment;

        // return empty value
        return QVariant();
    }

    void SessionModel::populate(SessionModel::SessionType type, const QString &path) {
        // read session files
        QDir dir(path);
        dir.setNameFilters(QStringList() << "*.desktop");
        dir.setFilter(QDir::Files);
        // read session
        foreach(const QString &session, dir.entryList()) {
            QFile inputFile(dir.absoluteFilePath(session));
            if (!inputFile.open(QIODevice::ReadOnly))
                continue;
            SessionPtr si { new Session { type, path, session, "", "", "" } };
            QTextStream in(&inputFile);
            bool execAllowed = true;
            while (!in.atEnd()) {
                QString line = in.readLine();
                if (line.startsWith("Name=")) {
                    if (type == WaylandSession)
                        si->name = tr("%1 (Wayland)").arg(line.mid(5));
                    else
                        si->name = line.mid(5);
                }
                if (line.startsWith("Exec="))
                    si->exec = line.mid(5);
                if (line.startsWith("Comment="))
                    si->comment = line.mid(8);
                if (line.startsWith("TryExec=")) {
                    QString tryExecBin = line.mid(8);
                    QFileInfo fi(tryExecBin);
                    if (fi.isAbsolute()) {
                        if (!fi.exists() || !fi.isExecutable())
                            execAllowed = false;
                    } else {
                        execAllowed = false;
                        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
                        QString envPath = env.value("PATH");
                        QStringList pathList = envPath.split(':');
                        foreach(const QString &path, pathList) {
                            QDir pathDir(path);
                            fi.setFile(pathDir, tryExecBin);
                            if (fi.exists() && fi.isExecutable()) {
                                execAllowed = true;
                                break;
                            }
                        }
                    }
                }
            }
            // add to sessions list
            if (execAllowed)
                d->sessions.push_back(si);
            // close file
            inputFile.close();
        }
        // add failsafe session
        if (type == X11Session)
            d->sessions << SessionPtr { new Session {type, path, "failsafe", "Failsafe", "failsafe", "Failsafe Session"} };
        // find out index of the last session
        for (int i = 0; i < d->sessions.size(); ++i) {
            if (d->sessions.at(i)->file == stateConfig.Last.Session.get()) {
                d->lastIndex = i;
                break;
            }
        }
    }
}
