/****************************************************************************
**
** Copyright (C) 2011 - 2012 Research In Motion <blackberry-qt@qnx.com>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

// #define QBBCLIPBOARD_DEBUG

#ifndef QT_NO_CLIPBOARD

#include "qbbclipboard.h"

#include <QDebug>
#include <QMimeData>
#include <QStringList>

#include <clipboard/clipboard.h>
#include <errno.h>

QT_BEGIN_NAMESPACE

// null terminated array
static const char *typeList[] = {"text/html", "text/plain", "image/png", "image/jpeg", "application/x-color", 0};

static QByteArray readClipboardBuff(const char *type)
{
    char *pbuffer;
    if (is_clipboard_format_present(type) == 0) {
        int size = get_clipboard_data(type, &pbuffer);
        if (size != -1 && pbuffer) {
            const QByteArray result = QByteArray(pbuffer, size);
            free(pbuffer);
            return result;
        }
    }

    return QByteArray();
}

class QBBClipboard::MimeData : public QMimeData
{
    Q_OBJECT
public:
    MimeData(QBBClipboard *clipboard)
        : QMimeData(),
          m_clipboard(clipboard),
          m_userMimeData(0)
    {
        Q_ASSERT(clipboard);

        for (int i = 0; typeList[i] != 0; ++i) {
            m_formatsToCheck << QString::fromUtf8(typeList[i]);
        }
    }

    ~MimeData()
    {
        delete m_userMimeData;
    }

    void addFormatToCheck(const QString &format) {
        m_formatsToCheck << format;

#if defined(QBBCLIPBOARD_DEBUG)
        qDebug() << Q_FUNC_INFO << "formats=" << m_formatsToCheck;
#endif
    }

    bool hasFormat(const QString &mimetype) const
    {
        const bool result = is_clipboard_format_present(mimetype.toUtf8().constData()) == 0;
#if defined(QBBCLIPBOARD_DEBUG)
        qDebug() << Q_FUNC_INFO << "mimetype=" << mimetype << "result=" << result;
#endif
        return result;
    }

    QStringList formats() const
    {
        QStringList result;

        Q_FOREACH (const QString &format, m_formatsToCheck) {
            if (is_clipboard_format_present(format.toUtf8().constData()) == 0)
                result << format;
        }

#if defined(QBBCLIPBOARD_DEBUG)
        qDebug() << Q_FUNC_INFO << "result=" << result;
#endif
        return result;
    }

    void setUserMimeData(QMimeData *userMimeData)
    {
        delete m_userMimeData;
        m_userMimeData = userMimeData;

        // system clipboard API doesn't allow detection of changes by other applications
        // simulate an owner change through delayed invocation
        // basically transfer ownership of data to the system clipboard once event processing resumes
        if (m_userMimeData)
            QMetaObject::invokeMethod(this, "releaseOwnership", Qt::QueuedConnection);
    }

    QMimeData *userMimeData()
    {
        return m_userMimeData;
    }

protected:
    QVariant retrieveData(const QString &mimetype, QVariant::Type preferredType) const
    {
#if defined(QBBCLIPBOARD_DEBUG)
        qDebug() << Q_FUNC_INFO << "mimetype=" << mimetype << "preferredType=" << preferredType;
#endif
        if (is_clipboard_format_present(mimetype.toUtf8().constData()) != 0)
            return QMimeData::retrieveData(mimetype, preferredType);

        const QByteArray data = readClipboardBuff(mimetype.toUtf8().constData());
        return qVariantFromValue(data);
    }

private Q_SLOTS:
    void releaseOwnership()
    {
        if (m_userMimeData) {
#if defined(QBBCLIPBOARD_DEBUG)
            qDebug() << Q_FUNC_INFO << "user data formats=" << m_userMimeData->formats() << "system formats=" << formats();
#endif
            delete m_userMimeData;
            m_userMimeData = 0;
            m_clipboard->emitChanged(QClipboard::Clipboard);
        }
    }

private:
    QBBClipboard * const m_clipboard;

    QSet<QString> m_formatsToCheck;
    QMimeData *m_userMimeData;
};

QBBClipboard::QBBClipboard()
    : m_mimeData(new MimeData(this))
{
}

QBBClipboard::~QBBClipboard()
{
    delete m_mimeData;
}

void QBBClipboard::setMimeData(QMimeData *data, QClipboard::Mode mode)
{
    if (mode != QClipboard::Clipboard)
        return;

    if (m_mimeData == data)
        return;

    if (m_mimeData->userMimeData() && m_mimeData->userMimeData() == data)
        return;

    empty_clipboard();

    m_mimeData->clear();
    m_mimeData->setUserMimeData(data);

    if (data == 0)
        return;

    const QStringList formats = data->formats();
#if defined(QBBCLIPBOARD_DEBUG)
    qDebug() << Q_FUNC_INFO << "formats=" << formats;
#endif

    Q_FOREACH (const QString &format, formats) {
        const QByteArray buf = data->data(format);

        if (buf.isEmpty())
            continue;

        int ret = set_clipboard_data(format.toUtf8().data(), buf.size(), buf.data());
#if defined(QBBCLIPBOARD_DEBUG)
        qDebug() << "QBB: set " << format << "to clipboard, size=" << buf.size() << ";ret=" << ret;
#endif
        if (ret)
            m_mimeData->addFormatToCheck(format);
    }

}

QMimeData* QBBClipboard::mimeData(QClipboard::Mode mode)
{
    if (mode != QClipboard::Clipboard)
        return 0;

    if (m_mimeData->userMimeData())
        return m_mimeData->userMimeData();

    m_mimeData->clear();

    return m_mimeData;
}

QT_END_NAMESPACE

#include "qbbclipboard.moc"

#endif //QT_NO_CLIPBOARD
