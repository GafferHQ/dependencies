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

#include <QAxObject>
#include <QFile>
#include <QTextStream>
#include <qt_windows.h>

QT_USE_NAMESPACE

int main(int argc, char **argv)
{
    CoInitialize(0);

    enum State {
        Default = 0,
            OutOption
    } state;
    state = Default;
    
    QByteArray outname;
    QByteArray object;
    
    for (int a = 1; a < argc; ++a) {
        QByteArray arg(argv[a]);
        const char first = arg[0];
        switch(state) {
        case Default:
            if (first == '-' || first == '/') {
                arg = arg.mid(1);
                arg.toLower();
                if (arg == "o")
                    state = OutOption;
                else if (arg == "v") {
                    qWarning("dumpdoc: Version 1.0");
                    return 0;
                } else if (arg == "h") {
                    qWarning("dumpdoc Usage:\n\tdumpdoc object [-o <file>]"
                        "              \n\tobject   : object[/subobject]*"
                        "              \n\tsubobject: property\n"
                        "      \nexample:\n\tdumpdoc Outlook.Application/Session/CurrentUser -o outlook.html");
                    return 0;
                }
            } else {
                object = arg;
            }
            break;
        case OutOption:
            outname = arg;
            state = Default;
            break;
            
        default:
            break;
        }
    }
    
    if (object.isEmpty()) {
        qWarning("dumpdoc: No object name provided.\n"
            "         Use -h for help.");
        return -1;
    }
    QFile outfile;
    if (!outname.isEmpty()) {
        outfile.setFileName(QString::fromLatin1(outname.constData()));
        if (!outfile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning("dumpdoc: Could not open output file '%s'", outname.data());
        }
    } else {
        outfile.open(stdout, QIODevice::WriteOnly);
    }
    QTextStream out(&outfile);
    
    QByteArray subobject = object;
    int index = subobject.indexOf('/');
    if (index != -1)
        subobject = subobject.left(index);
    
    QAxObject topobject(QString::fromLatin1(subobject.constData()));
    
    if (topobject.isNull()) {
        qWarning("dumpdoc: Could not instantiate COM object '%s'", subobject.data());
        return -2;
    }
    
    QAxObject *axobject = &topobject;
    while (index != -1 && axobject) {
        index++;
        subobject = object.mid(index);
        if (object.indexOf('/', index) != -1) {
            int oldindex = index;
            index = object.indexOf('/', index);
            subobject = object.mid(oldindex, index-oldindex);	    
        } else {
            index = -1;
        }
        
        axobject = axobject->querySubObject(subobject);
    }
    if (!axobject || axobject->isNull()) {
        qWarning("dumpdoc: Subobject '%s' does not exist in '%s'", subobject.data(), object.data());
        return -3;
    }
    
    QString docu = axobject->generateDocumentation();
    out << docu;
    return 0;
}
