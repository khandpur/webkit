/*
 * Copyright (C) 2011, 2015 Google Inc. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebCoreTestSupport.h"

#include "Frame.h"
#include "InternalSettings.h"
#include "Internals.h"
#include "JSDocument.h"
#include "JSInternals.h"
#include "LogInitialization.h"
#include "MockGamepadProvider.h"
#include "Page.h"
#include "WheelEventTestTrigger.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSValueRef.h>
#include <interpreter/CallFrame.h>
#include <runtime/IdentifierInlines.h>

using namespace JSC;
using namespace WebCore;

namespace WebCoreTestSupport {

void injectInternalsObject(JSContextRef context)
{
    ExecState* exec = toJS(context);
    JSLockHolder lock(exec);
    JSDOMGlobalObject* globalObject = jsCast<JSDOMGlobalObject*>(exec->lexicalGlobalObject());
    ScriptExecutionContext* scriptContext = globalObject->scriptExecutionContext();
    if (is<Document>(*scriptContext))
        globalObject->putDirect(exec->vm(), Identifier::fromString(exec, Internals::internalsId), toJS(exec, globalObject, Internals::create(downcast<Document>(*scriptContext))));
}

void resetInternalsObject(JSContextRef context)
{
    ExecState* exec = toJS(context);
    JSLockHolder lock(exec);
    JSDOMGlobalObject* globalObject = jsCast<JSDOMGlobalObject*>(exec->lexicalGlobalObject());
    ScriptExecutionContext* scriptContext = globalObject->scriptExecutionContext();
    Page* page = downcast<Document>(scriptContext)->frame()->page();
    Internals::resetToConsistentState(*page);
    InternalSettings::from(page)->resetToConsistentState();
}

void monitorWheelEvents(WebCore::Frame& frame)
{
    Page* page = frame.page();
    if (!page)
        return;

    page->ensureTestTrigger();
}

void setTestCallbackAndStartNotificationTimer(WebCore::Frame& frame, JSContextRef context, JSObjectRef jsCallbackFunction)
{
    Page* page = frame.page();
    if (!page || !page->expectsWheelEventTriggers())
        return;

    JSValueProtect(context, jsCallbackFunction);
    
    page->ensureTestTrigger().setTestCallbackAndStartNotificationTimer([=](void) {
        JSObjectCallAsFunction(context, jsCallbackFunction, nullptr, 0, nullptr, nullptr);
        JSValueUnprotect(context, jsCallbackFunction);
    });
}

void clearWheelEventTestTrigger(WebCore::Frame& frame)
{
    Page* page = frame.page();
    if (!page)
        return;
    
    page->clearTrigger();
}

void setLogChannelToAccumulate(const String& name)
{
#if !LOG_DISABLED
    WebCore::setLogChannelToAccumulate(name);
#else
    UNUSED_PARAM(name);
#endif
}

void initializeLogChannelsIfNecessary()
{
#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
    WebCore::initializeLogChannelsIfNecessary();
#endif
}

void setAllowsAnySSLCertificate(bool allowAnySSLCertificate)
{
    InternalSettings::setAllowsAnySSLCertificate(allowAnySSLCertificate);
}

void installMockGamepadProvider()
{
#if ENABLE(GAMEPAD)
    GamepadProvider::setSharedProvider(MockGamepadProvider::singleton());
#endif
}

void connectMockGamepad(unsigned gamepadIndex)
{
#if ENABLE(GAMEPAD)
    MockGamepadProvider::singleton().connectMockGamepad(gamepadIndex);
#else
    UNUSED_PARAM(gamepadIndex);
#endif
}

void disconnectMockGamepad(unsigned gamepadIndex)
{
#if ENABLE(GAMEPAD)
    MockGamepadProvider::singleton().disconnectMockGamepad(gamepadIndex);
#else
    UNUSED_PARAM(gamepadIndex);
#endif
}

void setMockGamepadDetails(unsigned gamepadIndex, const WTF::String& gamepadID, unsigned axisCount, unsigned buttonCount)
{
#if ENABLE(GAMEPAD)
    MockGamepadProvider::singleton().setMockGamepadDetails(gamepadIndex, gamepadID, axisCount, buttonCount);
#else
    UNUSED_PARAM(gamepadIndex);
    UNUSED_PARAM(gamepadID);
    UNUSED_PARAM(axisCount);
    UNUSED_PARAM(buttonCount);
#endif
}

void setMockGamepadAxisValue(unsigned gamepadIndex, unsigned axisIndex, double axisValue)
{
#if ENABLE(GAMEPAD)
    MockGamepadProvider::singleton().setMockGamepadAxisValue(gamepadIndex, axisIndex, axisValue);
#else
    UNUSED_PARAM(gamepadIndex);
    UNUSED_PARAM(axisIndex);
    UNUSED_PARAM(axisValue);
#endif
}

void setMockGamepadButtonValue(unsigned gamepadIndex, unsigned buttonIndex, double buttonValue)
{
#if ENABLE(GAMEPAD)
    MockGamepadProvider::singleton().setMockGamepadButtonValue(gamepadIndex, buttonIndex, buttonValue);
#else
    UNUSED_PARAM(gamepadIndex);
    UNUSED_PARAM(buttonIndex);
    UNUSED_PARAM(buttonValue);
#endif
}

}
