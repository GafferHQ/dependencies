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

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromium_gpu_helper.h"

// Including gpu/command_buffer headers before content/gpu headers makes sure that
// guards are defined to prevent duplicate definition errors with forward declared
// GL typedefs cascading through content header includes.
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"

#include "content/common/gpu/gpu_channel_manager.h"
#include "content/gpu/gpu_child_thread.h"

#ifdef Q_OS_QNX
#include "content/common/gpu/stream_texture_qnx.h"
#endif

static void addSyncPointCallbackDelegate(gpu::SyncPointManager *syncPointManager, uint32 sync_point, const base::Closure& callback)
{
    syncPointManager->AddSyncPointCallback(sync_point, callback);
}

QMap<uint32, gfx::TransferableFence> transferFences()
{
    QMap<uint32, gfx::TransferableFence> ret;
    content::GpuChannelManager *gpuChannelManager = content::GpuChildThread::instance()->ChannelManager();
    content::GpuChannelManager::SyncPointGLFences::iterator it = gpuChannelManager->sync_point_gl_fences_.begin();
    content::GpuChannelManager::SyncPointGLFences::iterator end = gpuChannelManager->sync_point_gl_fences_.end();
    for (; it != end; ++it) {
        ret[it->first] = it->second->Transfer();
        delete it->second;
    }
    gpuChannelManager->sync_point_gl_fences_.clear();
    return ret;
}

base::MessageLoop *gpu_message_loop()
{
    return content::GpuChildThread::instance()->message_loop();
}

gpu::SyncPointManager *sync_point_manager()
{
    content::GpuChannelManager *gpuChannelManager = content::GpuChildThread::instance()->ChannelManager();
    return gpuChannelManager->sync_point_manager();
}

void AddSyncPointCallbackOnGpuThread(base::MessageLoop *gpuMessageLoop, gpu::SyncPointManager *syncPointManager, uint32 sync_point, const base::Closure& callback)
{
    // We need to set our callback from the GPU thread, where the SyncPointManager lives.
    gpuMessageLoop->PostTask(FROM_HERE, base::Bind(&addSyncPointCallbackDelegate, make_scoped_refptr(syncPointManager), sync_point, callback));
}

gpu::gles2::MailboxManager *mailbox_manager()
{
    content::GpuChannelManager *gpuChannelManager = content::GpuChildThread::instance()->ChannelManager();
    return gpuChannelManager->mailbox_manager();
}

gpu::gles2::Texture* ConsumeTexture(gpu::gles2::MailboxManager *mailboxManager, unsigned target, const gpu::Mailbox& mailbox)
{
    Q_UNUSED(target);
    return mailboxManager->ConsumeTexture(mailbox);
}

unsigned int service_id(gpu::gles2::Texture *tex)
{
    return tex->service_id();
}

#ifdef Q_OS_QNX
EGLStreamData eglstream_connect_consumer(gpu::gles2::Texture *tex)
{
    EGLStreamData egl_stream;
    content::StreamTexture* image = static_cast<content::StreamTexture *>(tex->GetLevelImage(GL_TEXTURE_EXTERNAL_OES, 0));
    if (image) {
        image->ConnectConsumerIfNeeded(&egl_stream.egl_display, &egl_stream.egl_str_handle);
    }
    return egl_stream;
}
#endif
