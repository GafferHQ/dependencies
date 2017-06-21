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

#ifndef DESKTOP_SCREEN_QT_H
#define DESKTOP_SCREEN_QT_H

#include "ui/gfx/screen.h"

#include <QtGlobal>

namespace QtWebEngineCore {

class DesktopScreenQt : public gfx::Screen {
public:
    // Overridden from gfx::Screen:
    virtual gfx::Point GetCursorScreenPoint() Q_DECL_OVERRIDE;
    virtual gfx::NativeWindow GetWindowUnderCursor() Q_DECL_OVERRIDE;
    virtual gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) Q_DECL_OVERRIDE;
    virtual int GetNumDisplays() const Q_DECL_OVERRIDE;
    virtual std::vector<gfx::Display> GetAllDisplays() const Q_DECL_OVERRIDE;
    virtual gfx::Display GetDisplayNearestWindow(gfx::NativeView window) const Q_DECL_OVERRIDE;
    virtual gfx::Display GetDisplayNearestPoint(const gfx::Point& point) const Q_DECL_OVERRIDE;
    virtual gfx::Display GetDisplayMatching(const gfx::Rect& match_rect) const Q_DECL_OVERRIDE;
    virtual gfx::Display GetPrimaryDisplay() const Q_DECL_OVERRIDE;
    virtual void AddObserver(gfx::DisplayObserver* observer) Q_DECL_OVERRIDE;
    virtual void RemoveObserver(gfx::DisplayObserver* observer) Q_DECL_OVERRIDE;
};

} // namespace QtWebEngineCore

#endif // DESKTOP_SCREEN_QT_H
