/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

#ifndef QSTATICTEXT_H
#define QSTATICTEXT_H

#include <QtCore/qsize.h>
#include <QtCore/qstring.h>
#include <QtCore/qmetatype.h>

#include <QtGui/qtransform.h>
#include <QtGui/qfont.h>
#include <QtGui/qtextoption.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Gui)

class QStaticTextPrivate;
class Q_GUI_EXPORT QStaticText
{    
public:
    enum PerformanceHint {
        ModerateCaching,
        AggressiveCaching
    };

    QStaticText();
    QStaticText(const QString &text);
    QStaticText(const QStaticText &other);
    ~QStaticText();

    void setText(const QString &text);
    QString text() const;

    void setTextFormat(Qt::TextFormat textFormat);
    Qt::TextFormat textFormat() const;

    void setTextWidth(qreal textWidth);
    qreal textWidth() const;

    void setTextOption(const QTextOption &textOption);
    QTextOption textOption() const;

    QSizeF size() const;

    void prepare(const QTransform &matrix = QTransform(), const QFont &font = QFont());

    void setPerformanceHint(PerformanceHint performanceHint);
    PerformanceHint performanceHint() const;

    QStaticText &operator=(const QStaticText &);
    bool operator==(const QStaticText &) const;
    bool operator!=(const QStaticText &) const;

private:
    void detach();

    QExplicitlySharedDataPointer<QStaticTextPrivate> data;
    friend class QStaticTextPrivate;
};

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QStaticText)

QT_END_HEADER

#endif // QSTATICTEXT_H
