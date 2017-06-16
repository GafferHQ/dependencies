/****************************************************************************
**
** Copyright (C) 2015 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "file_picker_controller.h"
#include "type_conversion.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"

#include <QFileInfo>
#include <QDir>
#include <QVariant>
#include <QStringList>

namespace QtWebEngineCore {

FilePickerController::FilePickerController(FileChooserMode mode, content::WebContents *contents, const QString &defaultFileName, const QStringList &acceptedMimeTypes, QObject *parent)
    : QObject(parent)
    , m_defaultFileName(defaultFileName)
    , m_acceptedMimeTypes(acceptedMimeTypes)
    , m_contents(contents)
    , m_mode(mode)
{
}

void FilePickerController::accepted(const QStringList &files)
{
    FilePickerController::filesSelectedInChooser(files, m_contents);
}

void FilePickerController::accepted(const QVariant &files)
{
    QStringList stringList;

    if (files.canConvert(QVariant::StringList)) {
        stringList = files.toStringList();
    } else if (files.canConvert<QList<QUrl> >()) {
        Q_FOREACH (const QUrl &url, files.value<QList<QUrl> >())
            stringList.append(url.toLocalFile());
    } else {
        qWarning("An unhandled type '%s' was provided in FilePickerController::accepted(QVariant)", files.typeName());
    }

    FilePickerController::filesSelectedInChooser(stringList, m_contents);
}

void FilePickerController::rejected()
{
    FilePickerController::filesSelectedInChooser(QStringList(), m_contents);
}

static QStringList listRecursively(const QDir &dir)
{
    QStringList ret;
    QFileInfoList infoList(dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden));
    Q_FOREACH (const QFileInfo &fileInfo, infoList) {
        if (fileInfo.isDir()) {
            ret.append(fileInfo.absolutePath() + QStringLiteral("/.")); // Match chromium's behavior. See chrome/browser/file_select_helper.cc
            ret.append(listRecursively(QDir(fileInfo.absoluteFilePath())));
        } else
            ret.append(fileInfo.absoluteFilePath());
    }
    return ret;
}

ASSERT_ENUMS_MATCH(FilePickerController::Open, content::FileChooserParams::Open)
ASSERT_ENUMS_MATCH(FilePickerController::OpenMultiple, content::FileChooserParams::OpenMultiple)
ASSERT_ENUMS_MATCH(FilePickerController::UploadFolder, content::FileChooserParams::UploadFolder)
ASSERT_ENUMS_MATCH(FilePickerController::Save, content::FileChooserParams::Save)

void FilePickerController::filesSelectedInChooser(const QStringList &filesList, content::WebContents *contents)
{
    content::RenderViewHost *rvh = contents->GetRenderViewHost();
    Q_ASSERT(rvh);
    QStringList files(filesList);
    if (this->m_mode == UploadFolder && !filesList.isEmpty()
            && QFileInfo(filesList.first()).isDir()) // Enumerate the directory
        files = listRecursively(QDir(filesList.first()));
    rvh->FilesSelectedInChooser(toVector<content::FileChooserFileInfo>(files), static_cast<content::FileChooserParams::Mode>(this->m_mode));
}

QStringList FilePickerController::acceptedMimeTypes()
{
    return m_acceptedMimeTypes;
}

FilePickerController::FileChooserMode FilePickerController::mode()
{
    return m_mode;
}

QString FilePickerController::defaultFileName()
{
    return m_defaultFileName;
}

} // namespace
