/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the utils of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
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
** $QT_END_LICENSE$
**
****************************************************************************/

#include <qbytearray.h>
#include <qlist.h>
#include <qmap.h>
#include <qfile.h>

static QByteArray fileVersion;

static QList<QByteArray> glyphNames;
static QList<ushort> glyphNameOffsets;
static QMap<ushort, ushort> uni_to_agl_map; // sort by code point for bsearch

static void readGlyphList()
{
    qDebug("Reading aglfn.txt:");
    QFile f("data/aglfn.txt");
    if (!f.exists())
        qFatal("Couldn't find aglfn.txt");

    f.open(QFile::ReadOnly);

    glyphNames.append(".notdef");
    glyphNameOffsets.append(0);
    uni_to_agl_map.insert(0, 0);
    while (!f.atEnd()) {
        QByteArray line;
        line.resize(1024);
        int len = f.readLine(line.data(), 1024);
        line.resize(len-1);

        int comment = line.indexOf('#');
        if (comment != -1) {
            if (fileVersion.isEmpty()) {
                int commentTableVersion = line.indexOf("Table version:", comment);
                if (commentTableVersion != -1)
                    fileVersion = line.mid(commentTableVersion + 15).trimmed();
            }
            line = line.left(comment);
        }
        line = line.trimmed();

        if (line.isEmpty())
            continue;

        QList<QByteArray> l = line.split(';');
        Q_ASSERT(l.size() == 3);

        bool ok;
        ushort codepoint = l[0].toUShort(&ok, 16);
        Q_ASSERT(ok);
        QByteArray glyphName = l[1].trimmed();

        int glyphIndex = glyphNames.indexOf(glyphName);
        if (glyphIndex == -1) {
            glyphIndex = glyphNames.size();
            glyphNameOffsets.append(glyphNameOffsets.last() + glyphNames.last().size() + 1);
            glyphNames.append(glyphName);
        }
        uni_to_agl_map.insert(codepoint, glyphIndex);
    }

    qDebug("    %d unique glyph names found", glyphNames.size());
}

static QByteArray createGlyphList()
{
    qDebug("createGlyphList:");

    QByteArray out;

    out += "static const char glyph_names[] =\n\"";
    int linelen = 2;
    for (int i = 0; i < glyphNames.size(); ++i) {
        if (linelen + glyphNames.at(i).size() + 2 >= 80) {
            linelen = 2;
            out += "\"\n\"";
        }
        linelen += glyphNames.at(i).size() + 2;
        out += glyphNames.at(i) + "\\0";
    }
    if (out.endsWith("\""))
        out.chop(2);
    out += "\"\n;\n\n";

    out += "struct AGLEntry {\n"
           "    unsigned short uc;\n"
           "    unsigned short index;\n"
           "};\n"
           "\n"
           "inline bool operator<(unsigned short uc, AGLEntry entry)\n"
           "{ return uc < entry.uc; }\n"
           "inline bool operator<(AGLEntry entry, unsigned short uc)\n"
           "{ return entry.uc < uc; }\n"
           "\n"
           "static const AGLEntry unicode_to_agl_map[] = {";

    int i = 0;
    QMap<ushort, ushort>::const_iterator it = uni_to_agl_map.constBegin();
    while (it != uni_to_agl_map.constEnd()) {
        if (i++ % 4 == 0)
            out += "\n   ";
        out += " { 0x" + QByteArray::number(it.key(), 16).rightJustified(4, '0') + ", ";
        out += QByteArray::number(glyphNameOffsets.at(it.value())).leftJustified(4, ' ') + " },";
        ++it;
    }
    out.chop(1);
    out += "\n};\n\n";

    out += "enum { unicode_to_agl_map_size = sizeof(unicode_to_agl_map) / sizeof(unicode_to_agl_map[0]) };\n\n";

    qDebug("    Glyph names list uses : %d bytes", glyphNameOffsets.last() + glyphNames.last().size() + 1);
    qDebug("    Unicode to Glyph names map uses : %d bytes", uni_to_agl_map.size()*4);

    return out;
}


int main(int, char **)
{
    readGlyphList();

    QByteArray header =
        "/****************************************************************************\n"
        "**\n"
        "** Copyright (C) 2015 The Qt Company Ltd.\n"
        "** Contact: http://www.qt.io/licensing/\n"
        "**\n"
        "** This file is part of the utils of the Qt Toolkit.\n"
        "**\n"
        "** $QT_BEGIN_LICENSE:LGPL21$\n"
        "** Commercial License Usage\n"
        "** Licensees holding valid commercial Qt licenses may use this file in\n"
        "** accordance with the commercial license agreement provided with the\n"
        "** Software or, alternatively, in accordance with the terms contained in\n"
        "** a written agreement between you and The Qt Company. For licensing terms\n"
        "** and conditions see http://www.qt.io/terms-conditions. For further\n"
        "** information use the contact form at http://www.qt.io/contact-us.\n"
        "**\n"
        "** GNU Lesser General Public License Usage\n"
        "** Alternatively, this file may be used under the terms of the GNU Lesser\n"
        "** General Public License version 2.1 or version 3 as published by the Free\n"
        "** Software Foundation and appearing in the file LICENSE.LGPLv21 and\n"
        "** LICENSE.LGPLv3 included in the packaging of this file. Please review the\n"
        "** following information to ensure the GNU Lesser General Public License\n"
        "** requirements will be met: https://www.gnu.org/licenses/lgpl.html and\n"
        "** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.\n"
        "**\n"
        "** As a special exception, The Qt Company gives you certain additional\n"
        "** rights. These rights are described in The Qt Company LGPL Exception\n"
        "** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.\n"
        "**\n"
        "** $QT_END_LICENSE$\n"
        "**\n"
        "****************************************************************************/\n\n";

    QByteArray note =
        "/* This file is autogenerated from the Adobe Glyph List database" +
        (!fileVersion.isEmpty() ? " v" + fileVersion : "") + ". Do not edit */\n\n";

    QFile f("../../src/gui/text/qfontsubset_agl.cpp");
    f.open(QFile::WriteOnly|QFile::Truncate);
    f.write(header);
    f.write(note);
    f.write("namespace {\n\n");
    f.write(createGlyphList());
    f.write("}\n");
    f.close();
}
