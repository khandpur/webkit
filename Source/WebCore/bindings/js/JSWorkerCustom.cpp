/*
 * Copyright (C) 2008-2009, 2016 Apple Inc. All Rights Reserved.
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#include "JSWorker.h"

#include "Document.h"
#include "JSDOMBinding.h"
#include "JSDOMGlobalObject.h"
#include "JSDOMWindowCustom.h"
#include "JSMessagePortCustom.h"
#include "Worker.h"
#include <runtime/Error.h>

using namespace JSC;

namespace WebCore {

JSC::JSValue JSWorker::postMessage(JSC::ExecState& state)
{
    return handlePostMessage(state, &wrapped());
}

EncodedJSValue JSC_HOST_CALL constructJSWorker(ExecState& exec)
{
    VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    DOMConstructorObject* jsConstructor = jsCast<DOMConstructorObject*>(exec.callee());

    if (!exec.argumentCount())
        return throwVMError(&exec, scope, createNotEnoughArgumentsError(&exec));

    String scriptURL = exec.uncheckedArgument(0).toWTFString(&exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    // See section 4.8.2 step 14 of WebWorkers for why this is the lexicalGlobalObject.
    DOMWindow& window = asJSDOMWindow(exec.lexicalGlobalObject())->wrapped();

    ExceptionCode ec = 0;
    ASSERT(window.document());
    RefPtr<Worker> worker = Worker::create(*window.document(), scriptURL, ec);
    if (ec) {
        setDOMException(&exec, ec);
        return JSValue::encode(JSValue());
    }

    return JSValue::encode(toJSNewlyCreated(&exec, jsConstructor->globalObject(), WTFMove(worker)));
}

} // namespace WebCore
