/*
 * Copyright (C) 2009, 2013 Apple Inc. All rights reserved.
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

#pragma once

#include "ArrayBuffer.h"
#include "TypedArrayType.h"
#include <algorithm>
#include <limits.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace JSC {

class JSArrayBufferView;
class JSGlobalObject;
class ExecState;

class ArrayBufferView : public RefCounted<ArrayBufferView> {
public:
    virtual TypedArrayType getType() const = 0;

    bool isNeutered() const
    {
        return !m_buffer || m_buffer->isNeutered();
    }
    
    PassRefPtr<ArrayBuffer> buffer() const
    {
        if (isNeutered())
            return 0;
        return m_buffer;
    }

    void* baseAddress() const
    {
        if (isNeutered())
            return 0;
        return m_baseAddress;
    }

    unsigned byteOffset() const
    {
        if (isNeutered())
            return 0;
        return m_byteOffset;
    }

    virtual unsigned byteLength() const = 0;

    JS_EXPORT_PRIVATE void setNeuterable(bool flag);
    bool isNeuterable() const { return m_isNeuterable; }

    JS_EXPORT_PRIVATE virtual ~ArrayBufferView();

    // Helper to verify byte offset is size aligned.
    static bool verifyByteOffsetAlignment(unsigned byteOffset, size_t size)
    {
        return !(byteOffset & (size - 1));
    }

    // Helper to verify that a given sub-range of an ArrayBuffer is
    // within range.
    static bool verifySubRangeLength(PassRefPtr<ArrayBuffer> buffer, unsigned byteOffset, unsigned numElements, size_t size)
    {
        unsigned byteLength = buffer->byteLength();
        if (byteOffset > byteLength)
            return false;
        unsigned remainingElements = (byteLength - byteOffset) / size;
        if (numElements > remainingElements)
            return false;
        return true;
    }
    
    virtual JSArrayBufferView* wrap(ExecState*, JSGlobalObject*) = 0;
    
protected:
    JS_EXPORT_PRIVATE ArrayBufferView(PassRefPtr<ArrayBuffer>, unsigned byteOffset);

    inline bool setImpl(ArrayBufferView*, unsigned byteOffset);

    inline bool setRangeImpl(const char* data, size_t dataByteLength, unsigned byteOffset);

    inline bool zeroRangeImpl(unsigned byteOffset, size_t rangeByteLength);

    static inline void calculateOffsetAndLength(
        int start, int end, unsigned arraySize,
        unsigned* offset, unsigned* length);

    // Input offset is in number of elements from this array's view;
    // output offset is in number of bytes from the underlying buffer's view.
    template <typename T>
    static void clampOffsetAndNumElements(
        PassRefPtr<ArrayBuffer> buffer,
        unsigned arrayByteOffset,
        unsigned *offset,
        unsigned *numElements)
    {
        unsigned maxOffset = (UINT_MAX - arrayByteOffset) / sizeof(T);
        if (*offset > maxOffset) {
            *offset = buffer->byteLength();
            *numElements = 0;
            return;
        }
        *offset = arrayByteOffset + *offset * sizeof(T);
        *offset = std::min(buffer->byteLength(), *offset);
        unsigned remainingElements = (buffer->byteLength() - *offset) / sizeof(T);
        *numElements = std::min(remainingElements, *numElements);
    }

    // This is the address of the ArrayBuffer's storage, plus the byte offset.
    void* m_baseAddress;

    unsigned m_byteOffset : 31;
    bool m_isNeuterable : 1;

private:
    friend class ArrayBuffer;
    RefPtr<ArrayBuffer> m_buffer;
};

bool ArrayBufferView::setImpl(ArrayBufferView* array, unsigned byteOffset)
{
    if (byteOffset > byteLength()
        || byteOffset + array->byteLength() > byteLength()
        || byteOffset + array->byteLength() < byteOffset) {
        // Out of range offset or overflow
        return false;
    }
    
    char* base = static_cast<char*>(baseAddress());
    memmove(base + byteOffset, array->baseAddress(), array->byteLength());
    return true;
}

bool ArrayBufferView::setRangeImpl(const char* data, size_t dataByteLength, unsigned byteOffset)
{
    if (byteOffset > byteLength()
        || byteOffset + dataByteLength > byteLength()
        || byteOffset + dataByteLength < byteOffset) {
        // Out of range offset or overflow
        return false;
    }
    
    char* base = static_cast<char*>(baseAddress());
    memmove(base + byteOffset, data, dataByteLength);
    return true;
}

bool ArrayBufferView::zeroRangeImpl(unsigned byteOffset, size_t rangeByteLength)
{
    if (byteOffset > byteLength()
        || byteOffset + rangeByteLength > byteLength()
        || byteOffset + rangeByteLength < byteOffset) {
        // Out of range offset or overflow
        return false;
    }
    
    char* base = static_cast<char*>(baseAddress());
    memset(base + byteOffset, 0, rangeByteLength);
    return true;
}

void ArrayBufferView::calculateOffsetAndLength(
    int start, int end, unsigned arraySize, unsigned* offset, unsigned* length)
{
    if (start < 0)
        start += arraySize;
    if (start < 0)
        start = 0;
    if (end < 0)
        end += arraySize;
    if (end < 0)
        end = 0;
    if (static_cast<unsigned>(end) > arraySize)
        end = arraySize;
    if (end < start)
        end = start;
    *offset = static_cast<unsigned>(start);
    *length = static_cast<unsigned>(end - start);
}

} // namespace JSC

using JSC::ArrayBufferView;
