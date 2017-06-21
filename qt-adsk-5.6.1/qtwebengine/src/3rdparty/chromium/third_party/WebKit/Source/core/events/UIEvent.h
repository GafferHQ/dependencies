/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef UIEvent_h
#define UIEvent_h

#include "core/CoreExport.h"
#include "core/events/Event.h"
#include "core/events/EventDispatchMediator.h"
#include "core/events/UIEventInit.h"
#include "core/frame/DOMWindow.h"
#include "core/input/InputDevice.h"

namespace blink {

// FIXME: Get rid of this typedef.
typedef DOMWindow AbstractView;

class CORE_EXPORT UIEvent : public Event {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<UIEvent> create()
    {
        return adoptRefWillBeNoop(new UIEvent);
    }
    static PassRefPtrWillBeRawPtr<UIEvent> create(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtrWillBeRawPtr<AbstractView> view, int detail)
    {
        return adoptRefWillBeNoop(new UIEvent(type, canBubble, cancelable, view, detail));
    }
    static PassRefPtrWillBeRawPtr<UIEvent> create(const AtomicString& type, const UIEventInit& initializer)
    {
        return adoptRefWillBeNoop(new UIEvent(type, initializer));
    }
    virtual ~UIEvent();

    void initUIEvent(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtrWillBeRawPtr<AbstractView>, int detail);

    AbstractView* view() const { return m_view.get(); }
    int detail() const { return m_detail; }
    InputDevice* sourceDevice() const { return m_sourceDevice.get(); }

    virtual const AtomicString& interfaceName() const override;
    virtual bool isUIEvent() const override final;

    virtual int keyCode() const;
    virtual int charCode() const;

    virtual int which() const;

    DECLARE_VIRTUAL_TRACE();

protected:
    UIEvent();
    UIEvent(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtrWillBeRawPtr<AbstractView>, int detail, InputDevice* sourceDevice = nullptr);
    UIEvent(const AtomicString&, const UIEventInit&);

private:
    RefPtrWillBeMember<AbstractView> m_view;
    int m_detail;
    PersistentWillBeMember<InputDevice> m_sourceDevice;
};

} // namespace blink

#endif // UIEvent_h
