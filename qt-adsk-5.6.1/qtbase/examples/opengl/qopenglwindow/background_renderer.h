/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef FRAGMENT_TOY_H
#define FRAGMENT_TOY_H

#include <QObject>
#include <QFile>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions>

class FragmentToy : public QObject, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    FragmentToy(const QString &fragmentSource, QObject *parent = 0);

    void draw(const QSize &windowSize);

private:
    void fileChanged(const QString &path);
    bool m_recompile_shaders;
#ifndef QT_NO_FILESYSTEMWATCHER
    QFileSystemWatcher m_watcher;
#endif
    QString m_fragment_file;
    QDateTime m_fragment_file_last_modified;

    QScopedPointer<QOpenGLShaderProgram> m_program;
    QScopedPointer<QOpenGLShader> m_vertex_shader;
    QScopedPointer<QOpenGLShader> m_fragment_shader;
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vertex_buffer;
    GLuint m_vertex_coord_pos;
};

#endif //FRAGMENT_TOY_H
