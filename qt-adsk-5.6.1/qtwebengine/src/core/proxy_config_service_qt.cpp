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
// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "proxy_config_service_qt.h"

#include "base/bind.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

net::ProxyServer ProxyConfigServiceQt::fromQNetworkProxy(const QNetworkProxy &qtProxy)
{
    net::ProxyServer::Scheme proxyScheme = net::ProxyServer::SCHEME_INVALID;
    switch (qtProxy.type()) {
    case QNetworkProxy::Socks5Proxy:
        proxyScheme = net::ProxyServer::SCHEME_SOCKS5;
        break;
    case QNetworkProxy::HttpProxy:
    case QNetworkProxy::HttpCachingProxy:
    case QNetworkProxy::FtpCachingProxy:
        proxyScheme = net::ProxyServer::SCHEME_HTTP;
        break;
    case QNetworkProxy::NoProxy:
    default:
        proxyScheme = net::ProxyServer::SCHEME_DIRECT;
            break;
    }
    return net::ProxyServer(proxyScheme, net::HostPortPair(qtProxy.hostName().toStdString(), qtProxy.port()));
}

//================ Based on ChromeProxyConfigService =======================

ProxyConfigServiceQt::ProxyConfigServiceQt(net::ProxyConfigService *baseService)
    : m_baseService(baseService),
      m_registeredObserver(false)
{
}

ProxyConfigServiceQt::~ProxyConfigServiceQt()
{
    if (m_registeredObserver && m_baseService.get())
        m_baseService->RemoveObserver(this);
}

void ProxyConfigServiceQt::AddObserver(net::ProxyConfigService::Observer *observer)
{
    RegisterObserver();
    m_observers.AddObserver(observer);
}

void ProxyConfigServiceQt::RemoveObserver(net::ProxyConfigService::Observer *observer)
{
    m_observers.RemoveObserver(observer);
}

net::ProxyConfigService::ConfigAvailability ProxyConfigServiceQt::GetLatestProxyConfig(net::ProxyConfig *config)
{
    RegisterObserver();

    // Ask the base service if available.
    net::ProxyConfig systemConfig;
    ConfigAvailability systemAvailability = net::ProxyConfigService::CONFIG_UNSET;
    if (m_baseService.get())
        systemAvailability = m_baseService->GetLatestProxyConfig(&systemConfig);

    const QNetworkProxy &qtProxy = QNetworkProxy::applicationProxy();
    if (qtProxy == m_qtApplicationProxy && !m_qtProxyConfig.proxy_rules().empty()) {
        *config = m_qtProxyConfig;
        return CONFIG_VALID;
    }
    m_qtApplicationProxy = qtProxy;
    m_qtProxyConfig = net::ProxyConfig();
    if (qtProxy.type() == QNetworkProxy::NoProxy) {
        *config = systemConfig;
        return systemAvailability;
    }

    net::ProxyConfig::ProxyRules qtRules;
    net::ProxyServer server = fromQNetworkProxy(qtProxy);
    switch (qtProxy.type()) {
    case QNetworkProxy::HttpProxy:
    case QNetworkProxy::Socks5Proxy:
        qtRules.type = net::ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY;
        qtRules.single_proxies.SetSingleProxyServer(server);
        break;
    case QNetworkProxy::HttpCachingProxy:
        qtRules.type = net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME;
        qtRules.proxies_for_http.SetSingleProxyServer(server);
        break;
    case QNetworkProxy::FtpCachingProxy:
        qtRules.type = net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME;
        qtRules.proxies_for_ftp.SetSingleProxyServer(server);
        break;
    default:
        qtRules.type = net::ProxyConfig::ProxyRules::TYPE_NO_RULES;
    }

    m_qtProxyConfig.proxy_rules() = qtRules;
    *config = m_qtProxyConfig;
    return CONFIG_VALID;
}

void ProxyConfigServiceQt::OnLazyPoll()
{
    if (m_qtApplicationProxy != QNetworkProxy::applicationProxy()) {
        net::ProxyConfig unusedConfig;
        OnProxyConfigChanged(unusedConfig, CONFIG_VALID);
    }
    if (m_baseService.get())
        m_baseService->OnLazyPoll();
}


void ProxyConfigServiceQt::OnProxyConfigChanged(const net::ProxyConfig &config, ConfigAvailability availability)
{
    Q_UNUSED(config);
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

    if (m_qtApplicationProxy != QNetworkProxy::applicationProxy()
            || m_qtApplicationProxy.type() == QNetworkProxy::NoProxy) {
        net::ProxyConfig actual_config;
        availability = GetLatestProxyConfig(&actual_config);
        if (availability == CONFIG_PENDING)
            return;
        FOR_EACH_OBSERVER(net::ProxyConfigService::Observer, m_observers,
                          OnProxyConfigChanged(actual_config, availability));
    }
}

void ProxyConfigServiceQt::RegisterObserver()
{
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (!m_registeredObserver && m_baseService.get()) {
        m_baseService->AddObserver(this);
        m_registeredObserver = true;
    }
}
