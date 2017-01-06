/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#include "qerrormessage.h"

#ifndef QT_NO_ERRORMESSAGE

#include "qapplication.h"
#include "qcheckbox.h"
#include "qlabel.h"
#include "qlayout.h"
#include "qmessagebox.h"
#include "qpushbutton.h"
#include "qstringlist.h"
#include "qtextedit.h"
#include "qdialog_p.h"
#include "qpixmap.h"
#include "qmetaobject.h"
#include "qthread.h"
#include "qqueue.h"
#include "qset.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef Q_WS_WINCE
extern bool qt_wince_is_mobile();    //defined in qguifunctions_wince.cpp
extern bool qt_wince_is_high_dpi();  //defined in qguifunctions_wince.cpp

#include "qguifunctions_wince.h"
#endif

#if defined(QT_SOFTKEYS_ENABLED)
#include <qaction.h>
#endif
#ifdef Q_WS_S60
#include "private/qt_s60_p.h"
#endif

QT_BEGIN_NAMESPACE

class QErrorMessagePrivate : public QDialogPrivate
{
    Q_DECLARE_PUBLIC(QErrorMessage)
public:
    QPushButton * ok;
    QCheckBox * again;
    QTextEdit * errors;
    QLabel * icon;
#ifdef QT_SOFTKEYS_ENABLED
    QAction *okAction;
#endif
    QQueue<QPair<QString, QString> > pending;
    QSet<QString> doNotShow;
    QSet<QString> doNotShowType;
    QString currentMessage;
    QString currentType;

    bool nextPending();
    void retranslateStrings();
};

class QErrorMessageTextView : public QTextEdit
{
public:
    QErrorMessageTextView(QWidget *parent)
        : QTextEdit(parent) { setReadOnly(true); }

    virtual QSize minimumSizeHint() const;
    virtual QSize sizeHint() const;
};

QSize QErrorMessageTextView::minimumSizeHint() const
{
#ifdef Q_WS_WINCE
    if (qt_wince_is_mobile())
         if (qt_wince_is_high_dpi())
            return QSize(200, 200);
         else
             return QSize(100, 100);
    else
      return QSize(70, 70);
#else
    return QSize(50, 50);
#endif
}

QSize QErrorMessageTextView::sizeHint() const
{
#ifdef Q_WS_WINCE
    if (qt_wince_is_mobile())
         if (qt_wince_is_high_dpi())
            return QSize(400, 200);
         else
             return QSize(320, 120);
    else
      return QSize(300, 100);
#else

#ifdef Q_WS_S60
    const int smallerDimension = qMin(S60->screenHeightInPixels, S60->screenWidthInPixels);
    // In S60 layout data, error messages seem to be one third of the screen height (in portrait) minus two.
    return QSize(smallerDimension, smallerDimension/3-2);
#else
    return QSize(250, 75);
#endif //Q_WS_S60
#endif //Q_WS_WINCE
}

/*!
    \class QErrorMessage

    \brief The QErrorMessage class provides an error message display dialog.

    \ingroup standard-dialog

    An error message widget consists of a text label and a checkbox. The
    checkbox lets the user control whether the same error message will be
    displayed again in the future, typically displaying the text,
    "Show this message again" translated into the appropriate local
    language.

    For production applications, the class can be used to display messages which
    the user only needs to see once. To use QErrorMessage like this, you create
    the dialog in the usual way, and show it by calling the showMessage() slot or
    connecting signals to it.

    The static qtHandler() function installs a message handler
    using qInstallMsgHandler() and creates a QErrorMessage that displays
    qDebug(), qWarning() and qFatal() messages. This is most useful in
    environments where no console is available to display warnings and
    error messages.

    In both cases QErrorMessage will queue pending messages and display
    them in order, with each new message being shown as soon as the user
    has accepted the previous message. Once the user has specified that a
    message is not to be shown again it is automatically skipped, and the
    dialog will show the next appropriate message in the queue.

    The \l{dialogs/standarddialogs}{Standard Dialogs} example shows
    how to use QErrorMessage as well as other built-in Qt dialogs.

    \img qerrormessage.png

    \sa QMessageBox, QStatusBar::showMessage(), {Standard Dialogs Example}
*/

static QErrorMessage * qtMessageHandler = 0;

static void deleteStaticcQErrorMessage() // post-routine
{
    if (qtMessageHandler) {
        delete qtMessageHandler;
        qtMessageHandler = 0;
    }
}

static bool metFatal = false;

static void jump(QtMsgType t, const char * m)
{
    if (!qtMessageHandler)
        return;

    QString rich;

    switch (t) {
    case QtDebugMsg:
    default:
        rich = QErrorMessage::tr("Debug Message:");
        break;
    case QtWarningMsg:
        rich = QErrorMessage::tr("Warning:");
        break;
    case QtFatalMsg:
        rich = QErrorMessage::tr("Fatal Error:");
    }
    rich = QString::fromLatin1("<p><b>%1</b></p>").arg(rich);
    rich += Qt::convertFromPlainText(QLatin1String(m), Qt::WhiteSpaceNormal);

    // ### work around text engine quirk
    if (rich.endsWith(QLatin1String("</p>")))
        rich.chop(4);

    if (!metFatal) {
        if (QThread::currentThread() == qApp->thread()) {
            qtMessageHandler->showMessage(rich);
        } else {
            QMetaObject::invokeMethod(qtMessageHandler,
                                      "showMessage",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, rich));
        }
        metFatal = (t == QtFatalMsg);
    }
}


/*!
    Constructs and installs an error handler window with the given \a
    parent.
*/

QErrorMessage::QErrorMessage(QWidget * parent)
    : QDialog(*new QErrorMessagePrivate, parent)
{
    Q_D(QErrorMessage);
    QGridLayout * grid = new QGridLayout(this);
    d->icon = new QLabel(this);
#ifndef QT_NO_MESSAGEBOX
    d->icon->setPixmap(QMessageBox::standardIcon(QMessageBox::Information));
    d->icon->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
#endif
#ifdef Q_WS_S60
    //In Symbian, messagebox icons are in LtR UIs on right. Thus, layout needs to switch icon and text columns.
    const int preferredIconColumn = (QApplication::layoutDirection() == Qt::LeftToRight) ? 1 : 0;
    const int preferredTextColumn = (QApplication::layoutDirection() == Qt::LeftToRight) ? 0 : 1;
#else
    const int preferredIconColumn = 0;
    const int preferredTextColumn = 1;
#endif
    grid->addWidget(d->icon, 0, preferredIconColumn, Qt::AlignTop);
    d->errors = new QErrorMessageTextView(this);
    grid->addWidget(d->errors, 0, preferredTextColumn);
    d->again = new QCheckBox(this);
    d->again->setChecked(true);
    grid->addWidget(d->again, 1, preferredTextColumn, Qt::AlignTop);
    d->ok = new QPushButton(this);
#ifdef QT_SOFTKEYS_ENABLED
    d->okAction = new QAction(d->ok);
    d->okAction->setSoftKeyRole(QAction::PositiveSoftKey);
    connect(d->okAction, SIGNAL(triggered()), this, SLOT(accept()));
    addAction(d->okAction);
#endif


#if defined(Q_WS_WINCE) || defined(Q_WS_S60)
    d->ok->setFixedSize(0,0);
#endif
    connect(d->ok, SIGNAL(clicked()), this, SLOT(accept()));
    d->ok->setFocus();
    grid->addWidget(d->ok, 2, 0, 1, 2, Qt::AlignCenter);
    grid->setColumnStretch(preferredTextColumn, 42);
    grid->setRowStretch(0, 42);
    d->retranslateStrings();
}


/*!
    Destroys the error message dialog.
*/

QErrorMessage::~QErrorMessage()
{
    if (this == qtMessageHandler) {
        qtMessageHandler = 0;
        QtMsgHandler tmp = qInstallMsgHandler(0);
        // in case someone else has later stuck in another...
        if (tmp != jump)
            qInstallMsgHandler(tmp);
    }
}


/*! \reimp */

void QErrorMessage::done(int a)
{
    Q_D(QErrorMessage);
    if (!d->again->isChecked() && !d->currentMessage.isEmpty() && d->currentType.isEmpty()) {
        d->doNotShow.insert(d->currentMessage);
    }
    if (!d->again->isChecked() && !d->currentType.isEmpty()) {
        d->doNotShowType.insert(d->currentType);
    }
    d->currentMessage.clear();
    d->currentType.clear();
    if (!d->nextPending()) {
        QDialog::done(a);
        if (this == qtMessageHandler && metFatal)
            exit(1);
    }
}


/*!
    Returns a pointer to a QErrorMessage object that outputs the
    default Qt messages. This function creates such an object, if there
    isn't one already.
*/

QErrorMessage * QErrorMessage::qtHandler()
{
    if (!qtMessageHandler) {
        qtMessageHandler = new QErrorMessage(0);
        qAddPostRoutine(deleteStaticcQErrorMessage); // clean up
        qtMessageHandler->setWindowTitle(QApplication::applicationName());
        qInstallMsgHandler(jump);
    }
    return qtMessageHandler;
}


/*! \internal */

bool QErrorMessagePrivate::nextPending()
{
    while (!pending.isEmpty()) {
        QPair<QString,QString> pendingMessage = pending.dequeue();
        QString message = pendingMessage.first;
        QString type = pendingMessage.second;
        if (!message.isEmpty() && ((type.isEmpty() && !doNotShow.contains(message)) || (!type.isEmpty() && !doNotShowType.contains(type)))) {
#ifndef QT_NO_TEXTHTMLPARSER
            errors->setHtml(message);
#else
            errors->setPlainText(message);
#endif
            currentMessage = message;
            currentType = type;
            return true;
        }
    }
    return false;
}


/*!
    Shows the given message, \a message, and returns immediately. If the user
    has requested for the message not to be shown again, this function does
    nothing.

    Normally, the message is displayed immediately. However, if there are
    pending messages, it will be queued to be displayed later.
*/

void QErrorMessage::showMessage(const QString &message)
{
    Q_D(QErrorMessage);
    if (d->doNotShow.contains(message))
        return;
    d->pending.enqueue(qMakePair(message,QString()));
    if (!isVisible() && d->nextPending())
        show();
}

/*!
    \since 4.5
    \overload

    Shows the given message, \a message, and returns immediately. If the user
    has requested for messages of type, \a type, not to be shown again, this
    function does nothing.

    Normally, the message is displayed immediately. However, if there are
    pending messages, it will be queued to be displayed later.

    \sa showMessage()
*/

void QErrorMessage::showMessage(const QString &message, const QString &type)
{
    Q_D(QErrorMessage);
    if (d->doNotShow.contains(message) && d->doNotShowType.contains(type))
        return;
     d->pending.push_back(qMakePair(message,type));
    if (!isVisible() && d->nextPending())
        show();
}

/*!
    \reimp
*/
void QErrorMessage::changeEvent(QEvent *e)
{
    Q_D(QErrorMessage);
    if (e->type() == QEvent::LanguageChange) {
        d->retranslateStrings();
    }
    QDialog::changeEvent(e);
}

void QErrorMessagePrivate::retranslateStrings()
{
    again->setText(QErrorMessage::tr("&Show this message again"));
    ok->setText(QErrorMessage::tr("&OK"));
#ifdef QT_SOFTKEYS_ENABLED
    okAction->setText(ok->text());
#endif
}

/*!
    \fn void QErrorMessage::message(const QString & message)

    Use showMessage(\a message) instead.
*/

QT_END_NAMESPACE

#endif // QT_NO_ERRORMESSAGE
