/*
 * This file is part of the internal font implementation.  It should not be included by anyone other than
 * FontMac.cpp, FontWin.cpp and Font.cpp.
 *
 * Copyright (C) 2006-2008, 2016 Apple Inc.
 * Copyright (C) 2008 Brent Fulgham
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

#include "config.h"
#include "FontPlatformData.h"

#include "HWndDC.h"
#include "SharedBuffer.h"
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

using std::min;

namespace WebCore {

FontPlatformData::FontPlatformData(GDIObject<HFONT> font, float size, bool bold, bool oblique, bool useGDI)
    : m_font(SharedGDIObject<HFONT>::create(WTFMove(font)))
    , m_size(size)
    , m_syntheticBold(bold)
    , m_syntheticOblique(oblique)
    , m_useGDI(useGDI)
{
    HWndDC hdc(0);
    SaveDC(hdc);
    
    ::SelectObject(hdc, m_font->get());
    UINT bufferSize = GetOutlineTextMetrics(hdc, 0, NULL);

    ASSERT_WITH_MESSAGE(bufferSize, "Bitmap fonts not supported with CoreGraphics.");

    if (bufferSize) {
        OUTLINETEXTMETRICW* metrics = (OUTLINETEXTMETRICW*)malloc(bufferSize);

        GetOutlineTextMetricsW(hdc, bufferSize, metrics);
        WCHAR* faceName = (WCHAR*)((uintptr_t)metrics + (uintptr_t)metrics->otmpFaceName);

        platformDataInit(m_font->get(), size, hdc, faceName);

        free(metrics);
    }

    RestoreDC(hdc, -1);
}

RefPtr<SharedBuffer> FontPlatformData::openTypeTable(uint32_t table) const
{
    HWndDC hdc(0);
    HGDIOBJ oldFont = SelectObject(hdc, hfont());

    DWORD size = GetFontData(hdc, table, 0, 0, 0);
    RefPtr<SharedBuffer> buffer;
    if (size != GDI_ERROR) {
        buffer = SharedBuffer::create(size);
        DWORD result = GetFontData(hdc, table, 0, (PVOID)buffer->data(), size);
        ASSERT(result == size);
    }

    SelectObject(hdc, oldFont);
    return buffer.release();
}

#ifndef NDEBUG
String FontPlatformData::description() const
{
    return String();
}
#endif

}
