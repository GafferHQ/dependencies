/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef DOWNLOAD_MANAGER_DELEGATE_QT_H
#define DOWNLOAD_MANAGER_DELEGATE_QT_H

#include "content/public/browser/download_manager_delegate.h"

#include <QtCore/qcompilerdetection.h> // Needed for Q_DECL_OVERRIDE

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
class DownloadItem;
class WebContents;
}

namespace QtWebEngineCore {
class BrowserContextAdapter;
class DownloadManagerDelegateInstance;
class DownloadTargetHelper;

class DownloadManagerDelegateQt
        : public content::DownloadManagerDelegate
        , public content::DownloadItem::Observer
{
public:
    DownloadManagerDelegateQt(BrowserContextAdapter *contextAdapter);
    ~DownloadManagerDelegateQt();
    void GetNextId(const content::DownloadIdCallback& callback) Q_DECL_OVERRIDE;

    bool DetermineDownloadTarget(content::DownloadItem* item,
                                 const content::DownloadTargetCallback& callback) Q_DECL_OVERRIDE;

    void GetSaveDir(content::BrowserContext* browser_context,
                    base::FilePath* website_save_dir,
                    base::FilePath* download_save_dir,
                    bool* skip_dir_check) Q_DECL_OVERRIDE;

    void cancelDownload(quint32 downloadId);

    // Inherited from content::DownloadItem::Observer
    void OnDownloadUpdated(content::DownloadItem *download) Q_DECL_OVERRIDE;
    void OnDownloadDestroyed(content::DownloadItem *download) Q_DECL_OVERRIDE;

private:
    void cancelDownload(const content::DownloadTargetCallback& callback);
    BrowserContextAdapter *m_contextAdapter;

    uint64 m_currentId;

    friend class DownloadManagerDelegateInstance;
    DISALLOW_COPY_AND_ASSIGN(DownloadManagerDelegateQt);
};

} // namespace QtWebEngineCore

#endif //DOWNLOAD_MANAGER_DELEGATE_QT_H
