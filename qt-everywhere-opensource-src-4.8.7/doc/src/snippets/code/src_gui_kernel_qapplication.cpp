/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the documentation of the Qt Toolkit.
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

//! [0]
int main(int argc, char **argv)
{
#ifdef Q_WS_X11
    bool useGUI = getenv("DISPLAY") != 0;
#else
    bool useGUI = true;
#endif
    QApplication app(argc, argv, useGUI);

    if (useGUI) {
       // start GUI version
       ...
    } else {
       // start non-GUI version
       ...
    }
    return app.exec();
}
//! [0]


//! [1]
QApplication::setStyle(new QWindowsStyle);
//! [1]


//! [2]
int main(int argc, char *argv[])
{
    QApplication::setColorSpec(QApplication::ManyColor);
    QApplication app(argc, argv);
    ...
    return app.exec();
}
//! [2]


//! [3]
QSize MyWidget::sizeHint() const
{
    return QSize(80, 25).expandedTo(QApplication::globalStrut());
}
//! [3]


//! [4]
void showAllHiddenTopLevelWidgets()
{
    foreach (QWidget *widget, QApplication::topLevelWidgets()) {
        if (widget->isHidden())
            widget->show();
    }
}
//! [4]


//! [5]
void updateAllWidgets()
{
    foreach (QWidget *widget, QApplication::allWidgets())
        widget->update();
}
//! [5]


//! [6]
int main(int argc, char *argv[])
{
    QApplication::setDesktopSettingsAware(false);
    QApplication app(argc, argv);
    ...
    return app.exec();
}
//! [6]


//! [7]
if ((startPos - currentPos).manhattanLength() >=
        QApplication::startDragDistance())
    startTheDrag();
//! [7]


//! [8]
void MyApplication::commitData(QSessionManager& manager)
{
    if (manager.allowsInteraction()) {
        int ret = QMessageBox::warning(
                    mainWindow,
                    tr("My Application"),
                    tr("Save changes to document?"),
                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

        switch (ret) {
        case QMessageBox::Save:
            manager.release();
            if (!saveDocument())
                manager.cancel();
            break;
        case QMessageBox::Discard:
            break;
        case QMessageBox::Cancel:
        default:
            manager.cancel();
        }
    } else {
        // we did not get permission to interact, then
        // do something reasonable instead
    }
}
//! [8]


//! [9]
appname -session id
//! [9]


//! [10]
foreach (const QString &command, mySession.restartCommand())
    do_something(command);
//! [10]


//! [11]
foreach (const QString &command, mySession.discardCommand())
    do_something(command);
//! [11]


//! [12]
QWidget *widget = qApp->widgetAt(x, y);
if (widget)
    widget = widget->window();
//! [12]


//! [13]
QWidget *widget = qApp->widgetAt(point);
if (widget)
    widget = widget->window();
//! [13]
