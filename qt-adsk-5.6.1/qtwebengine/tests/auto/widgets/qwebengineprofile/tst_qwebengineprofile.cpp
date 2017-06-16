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

#include "../util.h"
#include <QtCore/qbuffer.h>
#include <QtTest/QtTest>
#include <QtWebEngineCore/qwebengineurlrequestjob.h>
#include <QtWebEngineCore/qwebengineurlschemehandler.h>
#include <QtWebEngineWidgets/qwebengineprofile.h>
#include <QtWebEngineWidgets/qwebenginesettings.h>
#include <QtWebEngineWidgets/qwebengineview.h>

class tst_QWebEngineProfile : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultProfile();
    void profileConstructors();
    void urlSchemeHandlers();
    void urlSchemeHandlerFailRequest();
    void customUserAgent();
    void httpAcceptLanguage();
};

void tst_QWebEngineProfile::defaultProfile()
{
    QWebEngineProfile *profile = QWebEngineProfile::defaultProfile();
    QVERIFY(profile);
    QVERIFY(!profile->isOffTheRecord());
    QCOMPARE(profile->storageName(), QStringLiteral("Default"));
    QCOMPARE(profile->httpCacheType(), QWebEngineProfile::DiskHttpCache);
    QCOMPARE(profile->persistentCookiesPolicy(), QWebEngineProfile::AllowPersistentCookies);
}

void tst_QWebEngineProfile::profileConstructors()
{
    QWebEngineProfile otrProfile;
    QWebEngineProfile diskProfile(QStringLiteral("Test"));

    QVERIFY(otrProfile.isOffTheRecord());
    QVERIFY(!diskProfile.isOffTheRecord());
    QCOMPARE(diskProfile.storageName(), QStringLiteral("Test"));
    QCOMPARE(otrProfile.httpCacheType(), QWebEngineProfile::MemoryHttpCache);
    QCOMPARE(diskProfile.httpCacheType(), QWebEngineProfile::DiskHttpCache);
    QCOMPARE(otrProfile.persistentCookiesPolicy(), QWebEngineProfile::NoPersistentCookies);
    QCOMPARE(diskProfile.persistentCookiesPolicy(), QWebEngineProfile::AllowPersistentCookies);

}

class RedirectingUrlSchemeHandler : public QWebEngineUrlSchemeHandler
{
public:
    void requestStarted(QWebEngineUrlRequestJob *job)
    {
        job->redirect(QUrl(QStringLiteral("data:text/plain;charset=utf-8,")
                           + job->requestUrl().fileName()));
    }
};

class ReplyingUrlSchemeHandler : public QWebEngineUrlSchemeHandler
{
    QBuffer m_buffer;
    QByteArray m_bufferData;
public:
    ReplyingUrlSchemeHandler(QObject *parent = nullptr)
        : QWebEngineUrlSchemeHandler(parent)
    {
        m_buffer.setBuffer(&m_bufferData);
    }

    void requestStarted(QWebEngineUrlRequestJob *job)
    {
        m_bufferData = job->requestUrl().toString().toUtf8();
        job->reply("text/plain;charset=utf-8", &m_buffer);
    }
};

static bool loadSync(QWebEngineView *view, const QUrl &url, int timeout = 5000)
{
    // Ripped off QTRY_VERIFY.
    QSignalSpy loadFinishedSpy(view, SIGNAL(loadFinished(bool)));
    view->load(url);
    if (loadFinishedSpy.isEmpty())
        QTest::qWait(0);
    for (int i = 0; i < timeout; i += 50) {
        if (!loadFinishedSpy.isEmpty())
            return true;
        QTest::qWait(50);
    }
    return false;
}

void tst_QWebEngineProfile::urlSchemeHandlers()
{
    RedirectingUrlSchemeHandler lettertoHandler;
    QWebEngineProfile profile(QStringLiteral("urlSchemeHandlers"));
    profile.installUrlSchemeHandler("letterto", &lettertoHandler);
    QWebEngineView view;
    view.setPage(new QWebEnginePage(&profile, &view));
    view.settings()->setAttribute(QWebEngineSettings::ErrorPageEnabled, false);
    QString emailAddress = QStringLiteral("egon@olsen-banden.dk");
    QVERIFY(loadSync(&view, QUrl(QStringLiteral("letterto:") + emailAddress)));
    QCOMPARE(toPlainTextSync(view.page()), emailAddress);

    // Install a gopher handler after the view has been fully initialized.
    ReplyingUrlSchemeHandler gopherHandler;
    profile.installUrlSchemeHandler("gopher", &gopherHandler);
    QUrl url = QUrl(QStringLiteral("gopher://olsen-banden.dk/benny"));
    QVERIFY(loadSync(&view, url));
    QCOMPARE(toPlainTextSync(view.page()), url.toString());

    // Remove the letterto scheme, and check whether it is not handled anymore.
    profile.removeUrlScheme("letterto");
    emailAddress = QStringLiteral("kjeld@olsen-banden.dk");
    QVERIFY(loadSync(&view, QUrl(QStringLiteral("letterto:") + emailAddress)));
    QVERIFY(toPlainTextSync(view.page()) != emailAddress);

    // Check if gopher is still working after removing letterto.
    url = QUrl(QStringLiteral("gopher://olsen-banden.dk/yvonne"));
    QVERIFY(loadSync(&view, url));
    QCOMPARE(toPlainTextSync(view.page()), url.toString());

    // Does removeAll work?
    profile.removeAllUrlSchemeHandlers();
    url = QUrl(QStringLiteral("gopher://olsen-banden.dk/harry"));
    QVERIFY(loadSync(&view, url));
    QVERIFY(toPlainTextSync(view.page()) != url.toString());

    // Install a handler that is owned by the view. Make sure this doesn't crash on shutdown.
    profile.installUrlSchemeHandler("aviancarrier", new ReplyingUrlSchemeHandler(&view));
    url = QUrl(QStringLiteral("aviancarrier:inspector.mortensen@politistyrke.dk"));
    QVERIFY(loadSync(&view, url));
    QCOMPARE(toPlainTextSync(view.page()), url.toString());
}

class FailingUrlSchemeHandler : public QWebEngineUrlSchemeHandler
{
public:
    void requestStarted(QWebEngineUrlRequestJob *job) override
    {
        job->fail(QWebEngineUrlRequestJob::RequestFailed);
    }
};

void tst_QWebEngineProfile::urlSchemeHandlerFailRequest()
{
    FailingUrlSchemeHandler handler;
    QWebEngineProfile profile;
    profile.installUrlSchemeHandler("foo", &handler);
    QWebEngineView view;
    QSignalSpy loadFinishedSpy(&view, SIGNAL(loadFinished(bool)));
    view.setPage(new QWebEnginePage(&profile, &view));
    view.settings()->setAttribute(QWebEngineSettings::ErrorPageEnabled, false);
    view.load(QUrl(QStringLiteral("foo://bar")));
    QVERIFY(loadFinishedSpy.wait());
    QCOMPARE(toPlainTextSync(view.page()), QString());
}

void tst_QWebEngineProfile::customUserAgent()
{
    QString defaultUserAgent = QWebEngineProfile::defaultProfile()->httpUserAgent();
    QWebEnginePage page;
    QSignalSpy loadFinishedSpy(&page, SIGNAL(loadFinished(bool)));
    page.setHtml(QStringLiteral("<html><body>Hello world!</body></html>"));
    QTRY_COMPARE(loadFinishedSpy.count(), 1);

    // First test the user-agent is default
    QCOMPARE(evaluateJavaScriptSync(&page, QStringLiteral("navigator.userAgent")).toString(), defaultUserAgent);

    const QString testUserAgent = QStringLiteral("tst_QWebEngineProfile 1.0");
    QWebEngineProfile testProfile;
    testProfile.setHttpUserAgent(testUserAgent);

    // Test a new profile with custom user-agent works
    QWebEnginePage page2(&testProfile);
    QSignalSpy loadFinishedSpy2(&page2, SIGNAL(loadFinished(bool)));
    page2.setHtml(QStringLiteral("<html><body>Hello again!</body></html>"));
    QTRY_COMPARE(loadFinishedSpy2.count(), 1);
    QCOMPARE(evaluateJavaScriptSync(&page2, QStringLiteral("navigator.userAgent")).toString(), testUserAgent);
    QCOMPARE(evaluateJavaScriptSync(&page, QStringLiteral("navigator.userAgent")).toString(), defaultUserAgent);

    // Test an existing page and profile with custom user-agent works
    QWebEngineProfile::defaultProfile()->setHttpUserAgent(testUserAgent);
    QCOMPARE(evaluateJavaScriptSync(&page, QStringLiteral("navigator.userAgent")).toString(), testUserAgent);
}

void tst_QWebEngineProfile::httpAcceptLanguage()
{
    QWebEnginePage page;
    QSignalSpy loadFinishedSpy(&page, SIGNAL(loadFinished(bool)));
    page.setHtml(QStringLiteral("<html><body>Hello world!</body></html>"));
    QTRY_COMPARE(loadFinishedSpy.count(), 1);

    QStringList defaultLanguages = evaluateJavaScriptSync(&page, QStringLiteral("navigator.languages")).toStringList();

    const QString testLang = QStringLiteral("xx-YY");
    QWebEngineProfile testProfile;
    testProfile.setHttpAcceptLanguage(testLang);

    // Test a completely new profile
    QWebEnginePage page2(&testProfile);
    QSignalSpy loadFinishedSpy2(&page2, SIGNAL(loadFinished(bool)));
    page2.setHtml(QStringLiteral("<html><body>Hello again!</body></html>"));
    QTRY_COMPARE(loadFinishedSpy2.count(), 1);
    QCOMPARE(evaluateJavaScriptSync(&page2, QStringLiteral("navigator.languages")).toStringList(), QStringList(testLang));
    // Test the old one wasn't affected
    QCOMPARE(evaluateJavaScriptSync(&page, QStringLiteral("navigator.languages")).toStringList(), defaultLanguages);

    // Test changing an existing page and profile
    QWebEngineProfile::defaultProfile()->setHttpAcceptLanguage(testLang);
    QCOMPARE(evaluateJavaScriptSync(&page, QStringLiteral("navigator.languages")).toStringList(), QStringList(testLang));
}

QTEST_MAIN(tst_QWebEngineProfile)
#include "tst_qwebengineprofile.moc"
