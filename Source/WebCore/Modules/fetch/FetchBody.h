/*
 * Copyright (C) 2016 Canon Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Canon Inc. nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FetchBody_h
#define FetchBody_h

#if ENABLE(FETCH_API)

#include "Blob.h"
#include "FetchBodyConsumer.h"
#include "FetchLoader.h"
#include "FormData.h"
#include "JSDOMPromise.h"
#include "URLSearchParams.h"
#include <wtf/Variant.h>

namespace JSC {
class ExecState;
class JSValue;
};

namespace WebCore {

class DOMFormData;
class FetchBodyOwner;
class FetchHeaders;
class FetchResponseSource;
class ScriptExecutionContext;

class FetchBody {
public:
    void arrayBuffer(FetchBodyOwner&, Ref<DeferredPromise>&&);
    void blob(FetchBodyOwner&, Ref<DeferredPromise>&&);
    void json(FetchBodyOwner&, Ref<DeferredPromise>&&);
    void text(FetchBodyOwner&, Ref<DeferredPromise>&&);
    void formData(FetchBodyOwner&, Ref<DeferredPromise>&& promise) { promise.get().reject(0); }

#if ENABLE(READABLE_STREAM_API)
    void consumeAsStream(FetchBodyOwner&, FetchResponseSource&);
#endif

    bool isEmpty() const { return m_type == Type::None; }

    void updateContentType(FetchHeaders&);
    void setContentType(const String& contentType) { m_contentType = contentType; }
    String contentType() const { return m_contentType; }

    static FetchBody extract(ScriptExecutionContext&, JSC::ExecState&, JSC::JSValue);
    static FetchBody extractFromBody(FetchBody*);
    static FetchBody loadingBody() { return { Type::Loading }; }
    FetchBody() = default;

    void loadingFailed();
    void loadingSucceeded();

    RefPtr<FormData> bodyForInternalRequest(ScriptExecutionContext&) const;

    enum class Type { None, ArrayBuffer, ArrayBufferView, Blob, FormData, Text, URLSeachParams, Loading, Loaded, ReadableStream };
    Type type() const { return m_type; }

    FetchBodyConsumer& consumer() { return m_consumer; }

    void cleanConsumePromise() { m_consumePromise = nullptr; }

    FetchBody clone() const;

private:
    FetchBody(Ref<const Blob>&&);
    FetchBody(Ref<const ArrayBuffer>&&);
    FetchBody(Ref<const ArrayBufferView>&&);
    FetchBody(DOMFormData&, Document&);
    FetchBody(String&&);
    FetchBody(Ref<const URLSearchParams>&&);
    FetchBody(Type, const String&, const FetchBodyConsumer&);
    FetchBody(Type type) : m_type(type) { }

    void consume(FetchBodyOwner&, Ref<DeferredPromise>&&);

    void consumeArrayBuffer(Ref<DeferredPromise>&&);
    void consumeArrayBufferView(Ref<DeferredPromise>&&);
    void consumeText(Ref<DeferredPromise>&&, const String&);
    void consumeBlob(FetchBodyOwner&, Ref<DeferredPromise>&&);

    const Blob& blobBody() const { return std::experimental::get<Ref<const Blob>>(m_data).get(); }
    FormData& formDataBody() { return std::experimental::get<Ref<FormData>>(m_data).get(); }
    const FormData& formDataBody() const { return std::experimental::get<Ref<FormData>>(m_data).get(); }
    const ArrayBuffer& arrayBufferBody() const { return std::experimental::get<Ref<const ArrayBuffer>>(m_data).get(); }
    const ArrayBufferView& arrayBufferViewBody() const { return std::experimental::get<Ref<const ArrayBufferView>>(m_data).get(); }
    String& textBody() { return std::experimental::get<String>(m_data); }
    const String& textBody() const { return std::experimental::get<String>(m_data); }
    const URLSearchParams& urlSearchParamsBody() const { return std::experimental::get<Ref<const URLSearchParams>>(m_data).get(); }

    Type m_type { Type::None };

    std::experimental::variant<std::nullptr_t, Ref<const Blob>, Ref<FormData>, Ref<const ArrayBuffer>, Ref<const ArrayBufferView>, Ref<const URLSearchParams>, String> m_data;
    String m_contentType;

    FetchBodyConsumer m_consumer { FetchBodyConsumer::Type::None };
    RefPtr<DeferredPromise> m_consumePromise;
};

} // namespace WebCore

#endif // ENABLE(FETCH_API)

#endif // FetchBody_h
