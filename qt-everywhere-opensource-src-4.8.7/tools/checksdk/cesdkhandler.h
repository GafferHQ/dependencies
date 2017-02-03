/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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
#ifndef CE_SDK_HANDLER_INCL
#define CE_SDK_HANDLER_INCL

#include <QStringList>
#include <QDir>

QT_USE_NAMESPACE

#define VCINSTALL_MACRO "$(VCInstallDir)"
#define VSINSTALL_MACRO "$(VSInstallDir)"

class CeSdkInfo
{
public:
    CeSdkInfo();
    inline QString                  name();
    inline QString                  binPath();
    inline QString                  includePath();
    inline QString                  libPath();
    QStringList                     environment();
    inline bool                     isValid();
    inline int                      majorVersion();
    inline int                      minorVersion();
    inline bool                     isSupported();
private:
    friend class                    CeSdkHandler;
    QString                         m_name;
    QString                         m_bin;
    QString                         m_include;
    QString                         m_lib;
    int                             m_major;
    int                             m_minor;
};

inline QString CeSdkInfo::name(){ return m_name; }
inline QString CeSdkInfo::binPath(){ return m_bin; }
inline QString CeSdkInfo::includePath(){ return m_include; }
inline QString CeSdkInfo::libPath(){ return m_lib; }
inline bool CeSdkInfo::isValid(){ return !m_name.isEmpty() && !m_bin.isEmpty() && !m_include.isEmpty() && !m_lib.isEmpty(); }
inline int CeSdkInfo::majorVersion(){ return m_major; }
inline int CeSdkInfo::minorVersion(){ return m_minor; }
inline bool CeSdkInfo::isSupported() { return m_major >= 5; }

class CeSdkHandler
{
public:
                                    CeSdkHandler();
    bool                            parse();
    inline QList<CeSdkInfo>         listAll() const;
    CeSdkInfo                       find(const QString &name);
private:
    inline QString                  fixPaths(QString path) const;
    QList<CeSdkInfo>                m_list;
    QString                         VCInstallDir;
    QString                         VSInstallDir;
};

inline QList<CeSdkInfo> CeSdkHandler::listAll() const
{
    return m_list;
}

inline QString CeSdkHandler::fixPaths(QString path) const
{
    QString str = QDir::toNativeSeparators(QDir::cleanPath(path.replace(QLatin1String(VCINSTALL_MACRO), VCInstallDir).replace(QLatin1String(VSINSTALL_MACRO), VSInstallDir).replace(QLatin1String("$(PATH)"), QLatin1String("%PATH%"))));
    if (str.endsWith(QLatin1Char(';')))
        str.truncate(str.length() - 1);
    return str;
}

#endif
