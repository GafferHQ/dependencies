/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt3Support module of the Qt Toolkit.
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

#include "qplatformdefs.h"
#include "q3process.h"

#ifndef QT_NO_PROCESS

#include "qapplication.h"
#include "q3cstring.h"
#include "q3ptrqueue.h"
#include "qtimer.h"
#include "qregexp.h"
#include "private/q3membuf_p.h"
#include "qt_windows.h"

#ifdef Q_OS_WINCE
#define STARTF_USESTDHANDLES 1
#endif

QT_BEGIN_NAMESPACE

//#define QT_Q3PROCESS_DEBUG

/***********************************************************************
 *
 * Q3ProcessPrivate
 *
 **********************************************************************/
class Q3ProcessPrivate
{
public:
    Q3ProcessPrivate( Q3Process *proc )
    {
	stdinBufRead = 0;
	pipeStdin[0] = 0;
	pipeStdin[1] = 0;
	pipeStdout[0] = 0;
	pipeStdout[1] = 0;
	pipeStderr[0] = 0;
	pipeStderr[1] = 0;
	exitValuesCalculated = false;

	lookup = new QTimer( proc );
	qApp->connect( lookup, SIGNAL(timeout()),
		proc, SLOT(timeout()) );

	pid = 0;
    }

    ~Q3ProcessPrivate()
    {
	reset();
    }

    void reset()
    {
	while ( !stdinBuf.isEmpty() ) {
	    delete stdinBuf.dequeue();
	}
	closeHandles();
	stdinBufRead = 0;
	pipeStdin[0] = 0;
	pipeStdin[1] = 0;
	pipeStdout[0] = 0;
	pipeStdout[1] = 0;
	pipeStderr[0] = 0;
	pipeStderr[1] = 0;
	exitValuesCalculated = false;

	deletePid();
    }

    void closeHandles()
    {
	if( pipeStdin[1] != 0 ) {
	    CloseHandle( pipeStdin[1] );
	    pipeStdin[1] = 0;
	}
	if( pipeStdout[0] != 0 ) {
	    CloseHandle( pipeStdout[0] );
	    pipeStdout[0] = 0;
	}
	if( pipeStderr[0] != 0 ) {
	    CloseHandle( pipeStderr[0] );
	    pipeStderr[0] = 0;
	}
    }

    void deletePid()
    {
	if ( pid ) {
	    CloseHandle( pid->hProcess );
	    CloseHandle( pid->hThread );
	    delete pid;
	    pid = 0;
	}
    }

    void newPid()
    {
	deletePid();
	pid = new PROCESS_INFORMATION;
	memset( pid, 0, sizeof(PROCESS_INFORMATION) );
    }

    Q3Membuf bufStdout;
    Q3Membuf bufStderr;

    Q3PtrQueue<QByteArray> stdinBuf;

    HANDLE pipeStdin[2];
    HANDLE pipeStdout[2];
    HANDLE pipeStderr[2];
    QTimer *lookup;

    PROCESS_INFORMATION *pid;
    uint stdinBufRead;

    bool exitValuesCalculated;
};


/***********************************************************************
 *
 * Q3Process
 *
 **********************************************************************/
void Q3Process::init()
{
    d = new Q3ProcessPrivate( this );
    exitStat = 0;
    exitNormal = false;
}

void Q3Process::reset()
{
    d->reset();
    exitStat = 0;
    exitNormal = false;
    d->bufStdout.clear();
    d->bufStderr.clear();
}

Q3Membuf* Q3Process::membufStdout()
{
    if( d->pipeStdout[0] != 0 )
	socketRead( 1 );
    return &d->bufStdout;
}

Q3Membuf* Q3Process::membufStderr()
{
    if( d->pipeStderr[0] != 0 )
	socketRead( 2 );
    return &d->bufStderr;
}

Q3Process::~Q3Process()
{
    delete d;
}

bool Q3Process::start( QStringList *env )
{
#if defined(QT_Q3PROCESS_DEBUG)
    qDebug( "Q3Process::start()" );
#endif
    reset();

    if ( _arguments.isEmpty() )
	return false;

    // Open the pipes.  Make non-inheritable copies of input write and output
    // read handles to avoid non-closable handles (this is done by the
    // DuplicateHandle() call).
    SECURITY_ATTRIBUTES secAtt = { sizeof( SECURITY_ATTRIBUTES ), NULL, TRUE };
#ifndef Q_OS_WINCE
    // I guess there is no stdin stdout and stderr on Q_OS_WINCE to dup
    // CreatePipe and DupilcateHandle aren't available for Q_OS_WINCE
    HANDLE tmpStdin, tmpStdout, tmpStderr;
    if ( comms & Stdin ) {
	if ( !CreatePipe( &d->pipeStdin[0], &tmpStdin, &secAtt, 0 ) ) {
	    d->closeHandles();
	    return false;
	}
	if ( !DuplicateHandle( GetCurrentProcess(), tmpStdin, GetCurrentProcess(), &d->pipeStdin[1], 0, FALSE, DUPLICATE_SAME_ACCESS ) ) {
	    d->closeHandles();
	    return false;
	}
	if ( !CloseHandle( tmpStdin ) ) {
	    d->closeHandles();
	    return false;
	}
    }
    if ( comms & Stdout ) {
	if ( !CreatePipe( &tmpStdout, &d->pipeStdout[1], &secAtt, 0 ) ) {
	    d->closeHandles();
	    return false;
	}
	if ( !DuplicateHandle( GetCurrentProcess(), tmpStdout, GetCurrentProcess(), &d->pipeStdout[0], 0, FALSE, DUPLICATE_SAME_ACCESS ) ) {
	    d->closeHandles();
	    return false;
	}
	if ( !CloseHandle( tmpStdout ) ) {
	    d->closeHandles();
	    return false;
	}
    }
    if ( comms & Stderr ) {
	if ( !CreatePipe( &tmpStderr, &d->pipeStderr[1], &secAtt, 0 ) ) {
	    d->closeHandles();
	    return false;
	}
	if ( !DuplicateHandle( GetCurrentProcess(), tmpStderr, GetCurrentProcess(), &d->pipeStderr[0], 0, FALSE, DUPLICATE_SAME_ACCESS ) ) {
	    d->closeHandles();
	    return false;
	}
	if ( !CloseHandle( tmpStderr ) ) {
	    d->closeHandles();
	    return false;
	}
    }
    if ( comms & DupStderr ) {
	CloseHandle( d->pipeStderr[1] );
	d->pipeStderr[1] = d->pipeStdout[1];
    }
#endif

    // construct the arguments for CreateProcess()
    QString args;
    QString appName;
    QStringList::Iterator it = _arguments.begin();
    args = *it;
    ++it;
    if ( args.endsWith( QLatin1String(".bat") ) && args.contains( QLatin1Char(' ') ) ) {
	// CreateProcess() seems to have a strange semantics (see also
	// http://www.experts-exchange.com/Programming/Programming_Platforms/Win_Prog/Q_11138647.html):
	// If you start a batch file with spaces in the filename, the first
	// argument to CreateProcess() must be the name of the batchfile
	// without quotes, but the second argument must start with the same
	// argument with quotes included. But if the same approach is used for
	// .exe files, it doesn't work.
	appName = args;
	args = QLatin1Char('"') + args + QLatin1Char('"');
    }
    for ( ; it != _arguments.end(); ++it ) {
	QString tmp = *it;
	// escape a single " because the arguments will be parsed
	tmp.replace( QLatin1Char('\"'), QLatin1String("\\\"") );
	if ( tmp.isEmpty() || tmp.contains( QLatin1Char(' ') ) || tmp.contains( QLatin1Char('\t') ) ) {
	    // The argument must not end with a \ since this would be interpreted
	    // as escaping the quote -- rather put the \ behind the quote: e.g.
	    // rather use "foo"\ than "foo\"
	    QString endQuote( QLatin1String("\"") );
	    int i = tmp.length();
	    while ( i>0 && tmp.at( i-1 ) == QLatin1Char('\\') ) {
		--i;
		endQuote += QLatin1Char('\\');
	    }
        args += QLatin1String(" \"") + tmp.left( i ) + endQuote;
	} else {
	    args += QLatin1Char(' ') + tmp;
	}
    }
#if defined(QT_Q3PROCESS_DEBUG)
    qDebug( "Q3Process::start(): args [%s]", args.latin1() );
#endif

    // CreateProcess()
    bool success;
    d->newPid();

    STARTUPINFOW startupInfo = {
        sizeof( STARTUPINFO ), 0, 0, 0,
        (ulong)CW_USEDEFAULT, (ulong)CW_USEDEFAULT, (ulong)CW_USEDEFAULT, (ulong)CW_USEDEFAULT,
        0, 0, 0,
        STARTF_USESTDHANDLES,
        0, 0, 0,
        d->pipeStdin[0], d->pipeStdout[1], d->pipeStderr[1]
    };
    wchar_t *applicationName;
    if ( appName.isNull() )
        applicationName = 0;
    else
        applicationName = _wcsdup( (wchar_t*)appName.utf16() );
    wchar_t *commandLine = _wcsdup( (wchar_t*)args.utf16() );
    QByteArray envlist;
    if ( env != 0 ) {
        int pos = 0;
        // add PATH if necessary (for DLL loading)
        QByteArray path = qgetenv( "PATH" );
        if ( env->grep( QRegExp(QLatin1String("^PATH="),FALSE) ).empty() && !path.isNull() ) {
            QString tmp = QString::fromLatin1("PATH=%1").arg(QLatin1String(path.constData()));
            uint tmpSize = sizeof(wchar_t) * (tmp.length() + 1);
            envlist.resize( envlist.size() + tmpSize );
            memcpy( envlist.data() + pos, tmp.utf16(), tmpSize );
            pos += tmpSize;
        }
        // add the user environment
        for ( QStringList::Iterator it = env->begin(); it != env->end(); it++ ) {
            QString tmp = *it;
            uint tmpSize = sizeof(wchar_t) * (tmp.length() + 1);
            envlist.resize( envlist.size() + tmpSize );
            memcpy( envlist.data() + pos, tmp.utf16(), tmpSize );
            pos += tmpSize;
        }
        // add the 2 terminating 0 (actually 4, just to be on the safe side)
        envlist.resize( envlist.size()+4 );
        envlist[pos++] = 0;
        envlist[pos++] = 0;
        envlist[pos++] = 0;
        envlist[pos++] = 0;
    }
    success = CreateProcess( applicationName, commandLine,
                            0, 0, TRUE, ( comms == 0 ? CREATE_NEW_CONSOLE : CREATE_NO_WINDOW )
#ifndef Q_OS_WINCE
                            | CREATE_UNICODE_ENVIRONMENT
#endif
                            , env == 0 ? 0 : envlist.data(),
                            (wchar_t*)QDir::toNativeSeparators(workingDir.absPath()).utf16(),
                            &startupInfo, d->pid );

    free( applicationName );
    free( commandLine );

    if  ( !success ) {
	d->deletePid();
	return false;
    }

#ifndef Q_OS_WINCE
    if ( comms & Stdin )
	CloseHandle( d->pipeStdin[0] );
    if ( comms & Stdout )
        CloseHandle( d->pipeStdout[1] );
    if ( (comms & Stderr) && !(comms & DupStderr) )
	CloseHandle( d->pipeStderr[1] );
#endif

    if ( ioRedirection || notifyOnExit ) {
	d->lookup->start( 100 );
    }

    // cleanup and return
    return true;
}

static BOOL QT_WIN_CALLBACK qt_terminateApp( HWND hwnd, LPARAM procId )
{
    DWORD procId_win;
    GetWindowThreadProcessId( hwnd, &procId_win );
    if( procId_win == (DWORD)procId )
	PostMessage( hwnd, WM_CLOSE, 0, 0 );

    return TRUE;
}

void Q3Process::tryTerminate() const
{
    if ( d->pid )
	EnumWindows( qt_terminateApp, (LPARAM)d->pid->dwProcessId );
}

void Q3Process::kill() const
{
    if ( d->pid )
	TerminateProcess( d->pid->hProcess, 0xf291 );
}

bool Q3Process::isRunning() const
{
    if ( !d->pid )
	return false;

    if ( WaitForSingleObject( d->pid->hProcess, 0) == WAIT_OBJECT_0 ) {
	// there might be data to read
	Q3Process *that = (Q3Process*)this;
	that->socketRead( 1 ); // try stdout
	that->socketRead( 2 ); // try stderr
	// compute the exit values
	if ( !d->exitValuesCalculated ) {
	    DWORD exitCode;
	    if ( GetExitCodeProcess( d->pid->hProcess, &exitCode ) ) {
		if ( exitCode != STILL_ACTIVE ) { // this should ever be true?
		    that->exitNormal = exitCode != 0xf291;
		    that->exitStat = exitCode;
		}
	    }
	    d->exitValuesCalculated = true;
	}
	d->deletePid();
	d->closeHandles();
	return false;
    } else {
        return true;
    }
}

bool Q3Process::canReadLineStdout() const
{
    if( !d->pipeStdout[0] )
	return d->bufStdout.size() != 0;

    Q3Process *that = (Q3Process*)this;
    return that->membufStdout()->scanNewline( 0 );
}

bool Q3Process::canReadLineStderr() const
{
    if( !d->pipeStderr[0] )
	return d->bufStderr.size() != 0;

    Q3Process *that = (Q3Process*)this;
    return that->membufStderr()->scanNewline( 0 );
}

void Q3Process::writeToStdin( const QByteArray& buf )
{
    d->stdinBuf.enqueue( new QByteArray(buf) );
    socketWrite( 0 );
}

void Q3Process::closeStdin( )
{
    if ( d->pipeStdin[1] != 0 ) {
	CloseHandle( d->pipeStdin[1] );
	d->pipeStdin[1] = 0;
    }
}

void Q3Process::socketRead( int fd )
{
    // fd == 1: stdout, fd == 2: stderr
    HANDLE dev;
    if ( fd == 1 ) {
	dev = d->pipeStdout[0];
    } else if ( fd == 2 ) {
	dev = d->pipeStderr[0];
    } else {
	return;
    }
#ifndef Q_OS_WINCE
    // get the number of bytes that are waiting to be read
    unsigned long i, r;
    char dummy;
    if ( !PeekNamedPipe( dev, &dummy, 1, &r, &i, 0 ) ) {
	return; // ### is it worth to dig for the reason of the error?
    }
#else
    unsigned long i = 1000;
#endif
    if ( i > 0 ) {
	Q3Membuf *buffer;
	if ( fd == 1 )
	    buffer = &d->bufStdout;
	else
	    buffer = &d->bufStderr;

	QByteArray *ba = new QByteArray( i );
	uint sz = readStddev( dev, ba->data(), i );
	if ( sz != i )
	    ba->resize( i );

	if ( sz == 0 ) {
	    delete ba;
	    return;
	}
	buffer->append( ba );
	if ( fd == 1 )
	    emit readyReadStdout();
	else
	    emit readyReadStderr();
    }
}

void Q3Process::socketWrite( int )
{
    DWORD written;
    while ( !d->stdinBuf.isEmpty() && isRunning() ) {
	if ( !WriteFile( d->pipeStdin[1],
		    d->stdinBuf.head()->data() + d->stdinBufRead,
		    qMin( 8192, int(d->stdinBuf.head()->size() - d->stdinBufRead) ),
		    &written, 0 ) ) {
	    d->lookup->start( 100 );
	    return;
	}
	d->stdinBufRead += written;
	if ( d->stdinBufRead == (DWORD)d->stdinBuf.head()->size() ) {
	    d->stdinBufRead = 0;
	    delete d->stdinBuf.dequeue();
	    if ( wroteToStdinConnected && d->stdinBuf.isEmpty() )
		emit wroteToStdin();
	}
    }
}

void Q3Process::flushStdin()
{
    socketWrite( 0 );
}

/*
  Use a timer for polling misc. stuff.
*/
void Q3Process::timeout()
{
    // Disable the timer temporary since one of the slots that are connected to
    // the readyRead...(), etc. signals might trigger recursion if
    // processEvents() is called.
    d->lookup->stop();

    // try to write pending data to stdin
    if ( !d->stdinBuf.isEmpty() )
	socketWrite( 0 );

    if ( ioRedirection ) {
	socketRead( 1 ); // try stdout
	socketRead( 2 ); // try stderr
    }

    if ( isRunning() ) {
	// enable timer again, if needed
	if ( !d->stdinBuf.isEmpty() || ioRedirection || notifyOnExit )
	    d->lookup->start( 100 );
    } else if ( notifyOnExit ) {
	emit processExited();
    }
}

/*
  read on the pipe
*/
uint Q3Process::readStddev( HANDLE dev, char *buf, uint bytes )
{
    if ( bytes > 0 ) {
	ulong r;
	if ( ReadFile( dev, buf, bytes, &r, 0 ) )
	    return r;
    }
    return 0;
}

/*
  Used by connectNotify() and disconnectNotify() to change the value of
  ioRedirection (and related behaviour)
*/
void Q3Process::setIoRedirection( bool value )
{
    ioRedirection = value;
    if ( !ioRedirection && !notifyOnExit )
	d->lookup->stop();
    if ( ioRedirection ) {
	if ( isRunning() )
	    d->lookup->start( 100 );
    }
}

/*
  Used by connectNotify() and disconnectNotify() to change the value of
  notifyOnExit (and related behaviour)
*/
void Q3Process::setNotifyOnExit( bool value )
{
    notifyOnExit = value;
    if ( !ioRedirection && !notifyOnExit )
	d->lookup->stop();
    if ( notifyOnExit ) {
	if ( isRunning() )
	    d->lookup->start( 100 );
    }
}

/*
  Used by connectNotify() and disconnectNotify() to change the value of
  wroteToStdinConnected (and related behaviour)
*/
void Q3Process::setWroteStdinConnected( bool value )
{
    wroteToStdinConnected = value;
}

Q3Process::PID Q3Process::processIdentifier()
{
    return d->pid;
}

QT_END_NAMESPACE

#endif // QT_NO_PROCESS
