/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#include "camerabinresourcepolicy.h"

//#define DEBUG_RESOURCE_POLICY
#include <QtCore/qdebug.h>
#include <QtCore/qset.h>

#ifdef HAVE_RESOURCE_POLICY
#include <policy/resource.h>
#include <policy/resources.h>
#include <policy/resource-set.h>
#endif

QT_BEGIN_NAMESPACE

CamerabinResourcePolicy::CamerabinResourcePolicy(QObject *parent) :
    QObject(parent),
    m_resourceSet(NoResources),
    m_releasingResources(false),
    m_canCapture(false)
{
#ifdef HAVE_RESOURCE_POLICY
    //loaded resource set is also kept requested for image and video capture sets
    m_resource = new ResourcePolicy::ResourceSet("camera");
    m_resource->setAlwaysReply();
    m_resource->initAndConnect();

    connect(m_resource, SIGNAL(resourcesGranted(QList<ResourcePolicy::ResourceType>)),
            SLOT(handleResourcesGranted()));
    connect(m_resource, SIGNAL(resourcesDenied()), SIGNAL(resourcesDenied()));
    connect(m_resource, SIGNAL(lostResources()), SLOT(handleResourcesLost()));
    connect(m_resource, SIGNAL(resourcesReleased()), SLOT(handleResourcesReleased()));
    connect(m_resource, SIGNAL(resourcesBecameAvailable(QList<ResourcePolicy::ResourceType>)),
            this, SLOT(resourcesAvailable()));
    connect(m_resource, SIGNAL(updateOK()), this, SLOT(updateCanCapture()));
#endif
}

CamerabinResourcePolicy::~CamerabinResourcePolicy()
{
#ifdef HAVE_RESOURCE_POLICY
    //ensure the resources are released
    if (m_resourceSet != NoResources)
        setResourceSet(NoResources);

    //don't delete the resource set until resources are released
    if (m_releasingResources) {
        m_resource->connect(m_resource, SIGNAL(resourcesReleased()),
                            SLOT(deleteLater()));
    } else {
        delete m_resource;
        m_resource = 0;
    }
#endif
}

CamerabinResourcePolicy::ResourceSet CamerabinResourcePolicy::resourceSet() const
{
    return m_resourceSet;
}

void CamerabinResourcePolicy::setResourceSet(CamerabinResourcePolicy::ResourceSet set)
{
    CamerabinResourcePolicy::ResourceSet oldSet = m_resourceSet;
    m_resourceSet = set;

#ifdef DEBUG_RESOURCE_POLICY
    qDebug() << Q_FUNC_INFO << set;
#endif

#ifdef HAVE_RESOURCE_POLICY
    QSet<ResourcePolicy::ResourceType> requestedTypes;

    switch (set) {
    case NoResources:
        break;
    case LoadedResources:
        requestedTypes << ResourcePolicy::LensCoverType //to detect lens cover is opened/closed
                       << ResourcePolicy::VideoRecorderType; //to open camera device
        break;
    case ImageCaptureResources:
        requestedTypes << ResourcePolicy::LensCoverType
                       << ResourcePolicy::VideoPlaybackType
                       << ResourcePolicy::VideoRecorderType
                       << ResourcePolicy::LedsType;
        break;
    case VideoCaptureResources:
        requestedTypes << ResourcePolicy::LensCoverType
                       << ResourcePolicy::VideoPlaybackType
                       << ResourcePolicy::VideoRecorderType
                       << ResourcePolicy::AudioPlaybackType
                       << ResourcePolicy::AudioRecorderType
                       << ResourcePolicy::LedsType;
        break;
    }

    QSet<ResourcePolicy::ResourceType> currentTypes;
    foreach (ResourcePolicy::Resource *resource, m_resource->resources())
        currentTypes << resource->type();

    foreach (ResourcePolicy::ResourceType resourceType, currentTypes - requestedTypes)
        m_resource->deleteResource(resourceType);

    foreach (ResourcePolicy::ResourceType resourceType, requestedTypes - currentTypes) {
        if (resourceType == ResourcePolicy::LensCoverType) {
            ResourcePolicy::LensCoverResource *lensCoverResource = new ResourcePolicy::LensCoverResource;
            lensCoverResource->setOptional(true);
            m_resource->addResourceObject(lensCoverResource);
        } else if (resourceType == ResourcePolicy::AudioPlaybackType) {
            ResourcePolicy::Resource *resource = new ResourcePolicy::AudioResource;
            resource->setOptional(true);
            m_resource->addResourceObject(resource);
        } else if (resourceType == ResourcePolicy::AudioRecorderType) {
            ResourcePolicy::Resource *resource = new ResourcePolicy::AudioRecorderResource;
            resource->setOptional(true);
            m_resource->addResourceObject(resource);
        } else {
            m_resource->addResource(resourceType);
        }
    }

    m_resource->update();
    if (set != NoResources) {
        m_resource->acquire();
    } else {
        if (oldSet != NoResources) {
            m_releasingResources = true;
            m_resource->release();
        }
    }
#else
    Q_UNUSED(oldSet);
    updateCanCapture();
#endif
}

bool CamerabinResourcePolicy::isResourcesGranted() const
{
#ifdef HAVE_RESOURCE_POLICY
    foreach (ResourcePolicy::Resource *resource, m_resource->resources())
        if (!resource->isOptional() && !resource->isGranted())
            return false;
#endif
    return true;
}

void CamerabinResourcePolicy::handleResourcesLost()
{
    updateCanCapture();
    emit resourcesLost();
}

void CamerabinResourcePolicy::handleResourcesGranted()
{
    updateCanCapture();
    emit resourcesGranted();
}

void CamerabinResourcePolicy::handleResourcesReleased()
{
#ifdef HAVE_RESOURCE_POLICY
#ifdef DEBUG_RESOURCE_POLICY
    qDebug() << Q_FUNC_INFO;
#endif
    m_releasingResources = false;
#endif
    updateCanCapture();
}

void CamerabinResourcePolicy::resourcesAvailable()
{
#ifdef HAVE_RESOURCE_POLICY
    if (m_resourceSet != NoResources) {
        m_resource->acquire();
    }
#endif
}

bool CamerabinResourcePolicy::canCapture() const
{
    return m_canCapture;
}

void CamerabinResourcePolicy::updateCanCapture()
{
    const bool wasAbleToRecord = m_canCapture;
    m_canCapture = (m_resourceSet == VideoCaptureResources) || (m_resourceSet == ImageCaptureResources);
#ifdef HAVE_RESOURCE_POLICY
    foreach (ResourcePolicy::Resource *resource, m_resource->resources()) {
        if (resource->type() != ResourcePolicy::LensCoverType)
            m_canCapture = m_canCapture && resource->isGranted();
    }
#endif
    if (wasAbleToRecord != m_canCapture)
        emit canCaptureChanged();
}

QT_END_NAMESPACE
