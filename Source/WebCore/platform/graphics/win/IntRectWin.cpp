
/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
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
#include "IntRect.h"

#include "MathExtras.h"
#include <d2d1.h>
#include <windows.h>

namespace WebCore {

IntRect::IntRect(const RECT& r)
    : m_location(IntPoint(r.left, r.top)), m_size(IntSize(r.right - r.left, r.bottom - r.top))
{
}

IntRect::operator RECT() const
{
    RECT rect = { x(), y(), maxX(), maxY() };
    return rect;
}

IntRect::IntRect(const D2D1_RECT_F& r)
    : m_location(IntPoint(clampToInteger(r.left), clampToInteger(r.top)))
    , m_size(IntSize(clampToInteger(r.right - r.left), clampToInteger(r.bottom - r.top)))
{
}

IntRect::IntRect(const D2D1_RECT_U& r)
    : m_location(IntPoint(r.left, r.top))
    , m_size(IntSize(r.right - r.left, r.bottom - r.top))
{
}

IntRect::operator D2D1_RECT_F() const
{
    return D2D1::RectF(x(), y(), maxX(), maxY());
}

IntRect::operator D2D1_RECT_U() const
{
    return D2D1::RectU(x(), y(), maxX(), maxY());
}

}
