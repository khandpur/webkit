/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Assertions.h>

namespace JSC {

enum class CellState : uint8_t {
    // The object is black for the first time during this GC.
    NewBlack = 0,
    
    // The object is black for the Nth time during this full GC cycle (N > 1). An object may get to
    // this state if it transitions from black back to grey during a concurrent GC, or because it
    // wound up in the remembered set because of a generational barrier.
    OldBlack = 1,
    
    // The object is in eden. During GC, this means that the object has not been marked yet.
    NewWhite = 2,

    // The object is grey - i.e. it will be scanned - and this is the first time in this GC that we are
    // going to scan it. If this is an eden GC, this also means that the object is in eden.
    NewGrey = 3,

    // The object is grey - i.e. it will be scanned - but it either belongs to old gen (if this is eden
    // GC) or it is grey a second time in this current GC (because a concurrent store barrier requested
    // re-greying).
    OldGrey = 4
};

static const unsigned blackThreshold = 1; // x <= blackThreshold means x is black.
static const unsigned tautologicalThreshold = 100; // x <= tautologicalThreshold is always true.

inline bool isWithinThreshold(CellState cellState, unsigned threshold)
{
    return static_cast<unsigned>(cellState) <= threshold;
}

inline bool isBlack(CellState cellState)
{
    return isWithinThreshold(cellState, blackThreshold);
}

inline CellState blacken(CellState cellState)
{
    if (cellState == CellState::NewGrey)
        return CellState::NewBlack;
    ASSERT(cellState == CellState::NewBlack || cellState == CellState::OldBlack || cellState == CellState::OldGrey);
    return CellState::OldBlack;
}

} // namespace JSC
