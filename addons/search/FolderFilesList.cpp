/*   Kate search plugin
 *
 * Copyright (C) 2013 by Kåre Särs <kare.sars@iki.fi>
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
 * along with this program in a file called COPYING; if not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "FolderFilesList.h"

#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>
#include <QDebug>
#include <QMimeDatabase>
#include <QMimeType>

FolderFilesList::FolderFilesList(QObject *parent)
    : QThread(parent)
{
}

FolderFilesList::~FolderFilesList()
{
    m_cancelSearch = true;
    wait();
}

void FolderFilesList::run()
{
    m_files.clear();

    QFileInfo folderInfo(m_folder);
    checkNextItem(folderInfo);

    if (m_cancelSearch)
        m_files.clear();
}

void FolderFilesList::generateList(const QString &folder, bool recursive, bool hidden, bool symlinks, bool binary, const QString &types, const QString &excludes)
{
    m_cancelSearch = false;
    m_folder = folder;
    if (!m_folder.endsWith(QLatin1Char('/'))) {
        m_folder += QLatin1Char('/');
    }
    m_recursive = recursive;
    m_hidden = hidden;
    m_symlinks = symlinks;
    m_binary = binary;

    m_types.clear();
    const auto typesList = types.split(QLatin1Char(','), QString::SkipEmptyParts);
    for (const QString &type : typesList) {
        m_types << type.trimmed();
    }
    if (m_types.isEmpty()) {
        m_types << QStringLiteral("*");
    }

    QStringList tmpExcludes = excludes.split(QLatin1Char(','));
    m_excludeList.clear();
    for (int i = 0; i < tmpExcludes.size(); i++) {
        QRegExp rx(tmpExcludes[i].trimmed());
        rx.setPatternSyntax(QRegExp::Wildcard);
        m_excludeList << rx;
    }

    m_time.restart();
    start();
}

QStringList FolderFilesList::fileList()
{
    return m_files;
}

void FolderFilesList::cancelSearch()
{
    m_cancelSearch = true;
}

void FolderFilesList::checkNextItem(const QFileInfo &item)
{
    if (m_cancelSearch) {
        return;
    }
    if (m_time.elapsed() > 100) {
        m_time.restart();
        emit searching(item.absoluteFilePath());
    }
    if (item.isFile()) {
        if (!m_binary) {
            QMimeType mimeType = QMimeDatabase().mimeTypeForFile(item);
            if (!mimeType.inherits(QStringLiteral("text/plain"))) {
                return;
            }
        }
        m_files << item.canonicalFilePath();
    } else {
        QDir currentDir(item.absoluteFilePath());

        if (!currentDir.isReadable()) {
            qDebug() << currentDir.absolutePath() << "Not readable";
            return;
        }

        QDir::Filters filter = QDir::Files | QDir::NoDotAndDotDot | QDir::Readable;
        if (m_hidden)
            filter |= QDir::Hidden;
        if (m_recursive)
            filter |= QDir::AllDirs;
        if (!m_symlinks)
            filter |= QDir::NoSymLinks;

        // sort the items to have an deterministic order!
        const QFileInfoList currentItems = currentDir.entryInfoList(m_types, filter, QDir::Name | QDir::LocaleAware);

        bool skip;
        for (const auto &currentItem : currentItems) {
            skip = false;
            for (const auto &regex : qAsConst(m_excludeList)) {
                QString matchString = currentItem.filePath();
                if (currentItem.filePath().startsWith(m_folder)) {
                    matchString = currentItem.filePath().mid(m_folder.size());
                }
                if (regex.exactMatch(matchString)) {
                    skip = true;
                    break;
                }
            }
            if (!skip) {
                checkNextItem(currentItem);
            }
        }
    }
}
