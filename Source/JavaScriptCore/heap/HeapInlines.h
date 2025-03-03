/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

#include "GCDeferralContext.h"
#include "Heap.h"
#include "HeapCellInlines.h"
#include "IndexingHeader.h"
#include "JSCallee.h"
#include "JSCell.h"
#include "Structure.h"
#include <type_traits>
#include <wtf/Assertions.h>
#include <wtf/MainThread.h>
#include <wtf/RandomNumber.h>

namespace JSC {

inline bool Heap::shouldCollect()
{
    if (isDeferred())
        return false;
    if (!m_isSafeToCollect)
        return false;
    if (m_operationInProgress != NoOperation)
        return false;
    if (Options::gcMaxHeapSize())
        return m_bytesAllocatedThisCycle > Options::gcMaxHeapSize();
    return m_bytesAllocatedThisCycle > m_maxEdenSize;
}

inline bool Heap::isBusy()
{
    return m_operationInProgress != NoOperation;
}

inline bool Heap::isCollecting()
{
    return m_operationInProgress == FullCollection || m_operationInProgress == EdenCollection;
}

ALWAYS_INLINE Heap* Heap::heap(const HeapCell* cell)
{
    return cell->heap();
}

inline Heap* Heap::heap(const JSValue v)
{
    if (!v.isCell())
        return 0;
    return heap(v.asCell());
}

ALWAYS_INLINE bool Heap::isMarked(const void* rawCell)
{
    ASSERT(!mayBeGCThread());
    HeapCell* cell = bitwise_cast<HeapCell*>(rawCell);
    if (cell->isLargeAllocation())
        return cell->largeAllocation().isMarked();
    MarkedBlock& block = cell->markedBlock();
    return block.isMarked(
        block.vm()->heap.objectSpace().markingVersion(), cell);
}

ALWAYS_INLINE bool Heap::isMarkedConcurrently(const void* rawCell)
{
    HeapCell* cell = bitwise_cast<HeapCell*>(rawCell);
    if (cell->isLargeAllocation())
        return cell->largeAllocation().isMarked();
    MarkedBlock& block = cell->markedBlock();
    return block.isMarkedConcurrently(
        block.vm()->heap.objectSpace().markingVersion(), cell);
}

ALWAYS_INLINE bool Heap::testAndSetMarked(HeapVersion markingVersion, const void* rawCell)
{
    HeapCell* cell = bitwise_cast<HeapCell*>(rawCell);
    if (cell->isLargeAllocation())
        return cell->largeAllocation().testAndSetMarked();
    MarkedBlock& block = cell->markedBlock();
    block.aboutToMark(markingVersion);
    return block.testAndSetMarked(cell);
}

ALWAYS_INLINE size_t Heap::cellSize(const void* rawCell)
{
    return bitwise_cast<HeapCell*>(rawCell)->cellSize();
}

inline void Heap::writeBarrier(const JSCell* from, JSValue to)
{
#if ENABLE(WRITE_BARRIER_PROFILING)
    WriteBarrierCounters::countWriteBarrier();
#endif
    if (!to.isCell())
        return;
    writeBarrier(from, to.asCell());
}

inline void Heap::writeBarrier(const JSCell* from, JSCell* to)
{
#if ENABLE(WRITE_BARRIER_PROFILING)
    WriteBarrierCounters::countWriteBarrier();
#endif
    if (!from)
        return;
    if (!isWithinThreshold(from->cellState(), barrierThreshold()))
        return;
    if (LIKELY(!to || to->cellState() != CellState::NewWhite))
        return;
    writeBarrierSlowPath(from);
}

inline void Heap::writeBarrier(const JSCell* from)
{
    ASSERT_GC_OBJECT_LOOKS_VALID(const_cast<JSCell*>(from));
    if (!from)
        return;
    if (UNLIKELY(isWithinThreshold(from->cellState(), barrierThreshold())))
        writeBarrierSlowPath(from);
}

inline void Heap::writeBarrierWithoutFence(const JSCell* from)
{
    ASSERT_GC_OBJECT_LOOKS_VALID(const_cast<JSCell*>(from));
    if (!from)
        return;
    if (UNLIKELY(isWithinThreshold(from->cellState(), blackThreshold)))
        addToRememberedSet(from);
}

inline void Heap::reportExtraMemoryAllocated(size_t size)
{
    if (size > minExtraMemory) 
        reportExtraMemoryAllocatedSlowCase(size);
}

inline void Heap::reportExtraMemoryVisited(JSCell* cell, size_t size)
{
    // We don't want to double-count the extra memory that was reported in previous collections.
    if (operationInProgress() == EdenCollection && cell->cellState() == CellState::OldBlack)
        return;

    size_t* counter = &m_extraMemorySize;
    
    for (;;) {
        size_t oldSize = *counter;
        if (WTF::weakCompareAndSwap(counter, oldSize, oldSize + size))
            return;
    }
}

#if ENABLE(RESOURCE_USAGE)
inline void Heap::reportExternalMemoryVisited(JSCell* cell, size_t size)
{
    // We don't want to double-count the external memory that was reported in previous collections.
    if (operationInProgress() == EdenCollection && cell->cellState() == CellState::OldBlack)
        return;

    size_t* counter = &m_externalMemorySize;

    for (;;) {
        size_t oldSize = *counter;
        if (WTF::weakCompareAndSwap(counter, oldSize, oldSize + size))
            return;
    }
}
#endif

inline void Heap::deprecatedReportExtraMemory(size_t size)
{
    if (size > minExtraMemory) 
        deprecatedReportExtraMemorySlowCase(size);
}

template<typename Functor> inline void Heap::forEachCodeBlock(const Functor& func)
{
    forEachCodeBlockImpl(scopedLambdaRef<bool(CodeBlock*)>(func));
}

template<typename Functor> inline void Heap::forEachProtectedCell(const Functor& functor)
{
    for (auto& pair : m_protectedValues)
        functor(pair.key);
    m_handleSet.forEachStrongHandle(functor, m_protectedValues);
}

inline void* Heap::allocateWithDestructor(size_t bytes)
{
#if ENABLE(ALLOCATION_LOGGING)
    dataLogF("JSC GC allocating %lu bytes with normal destructor.\n", bytes);
#endif
    ASSERT(isValidAllocation(bytes));
    return m_objectSpace.allocateWithDestructor(bytes);
}

inline void* Heap::allocateWithoutDestructor(size_t bytes)
{
#if ENABLE(ALLOCATION_LOGGING)
    dataLogF("JSC GC allocating %lu bytes without destructor.\n", bytes);
#endif
    ASSERT(isValidAllocation(bytes));
    return m_objectSpace.allocateWithoutDestructor(bytes);
}

inline void* Heap::allocateWithDestructor(GCDeferralContext* deferralContext, size_t bytes)
{
    ASSERT(isValidAllocation(bytes));
    return m_objectSpace.allocateWithDestructor(deferralContext, bytes);
}

inline void* Heap::allocateWithoutDestructor(GCDeferralContext* deferralContext, size_t bytes)
{
    ASSERT(isValidAllocation(bytes));
    return m_objectSpace.allocateWithoutDestructor(deferralContext, bytes);
}

template<typename ClassType>
inline void* Heap::allocateObjectOfType(size_t bytes)
{
    // JSCell::classInfo() expects objects allocated with normal destructor to derive from JSDestructibleObject.
    ASSERT((!ClassType::needsDestruction || (ClassType::StructureFlags & StructureIsImmortal) || std::is_convertible<ClassType, JSDestructibleObject>::value));

    if (ClassType::needsDestruction)
        return allocateWithDestructor(bytes);
    return allocateWithoutDestructor(bytes);
}

template<typename ClassType>
inline void* Heap::allocateObjectOfType(GCDeferralContext* deferralContext, size_t bytes)
{
    ASSERT((!ClassType::needsDestruction || (ClassType::StructureFlags & StructureIsImmortal) || std::is_convertible<ClassType, JSDestructibleObject>::value));

    if (ClassType::needsDestruction)
        return allocateWithDestructor(deferralContext, bytes);
    return allocateWithoutDestructor(deferralContext, bytes);
}

template<typename ClassType>
inline MarkedSpace::Subspace& Heap::subspaceForObjectOfType()
{
    // JSCell::classInfo() expects objects allocated with normal destructor to derive from JSDestructibleObject.
    ASSERT((!ClassType::needsDestruction || (ClassType::StructureFlags & StructureIsImmortal) || std::is_convertible<ClassType, JSDestructibleObject>::value));
    
    if (ClassType::needsDestruction)
        return subspaceForObjectDestructor();
    return subspaceForObjectWithoutDestructor();
}

template<typename ClassType>
inline MarkedAllocator* Heap::allocatorForObjectOfType(size_t bytes)
{
    // JSCell::classInfo() expects objects allocated with normal destructor to derive from JSDestructibleObject.
    ASSERT((!ClassType::needsDestruction || (ClassType::StructureFlags & StructureIsImmortal) || std::is_convertible<ClassType, JSDestructibleObject>::value));

    MarkedAllocator* result;
    if (ClassType::needsDestruction)
        result = allocatorForObjectWithDestructor(bytes);
    else
        result = allocatorForObjectWithoutDestructor(bytes);
    
    ASSERT(result || !ClassType::info()->isSubClassOf(JSCallee::info()));
    return result;
}

inline void* Heap::allocateAuxiliary(JSCell* intendedOwner, size_t bytes)
{
    void* result = m_objectSpace.allocateAuxiliary(bytes);
#if ENABLE(ALLOCATION_LOGGING)
    dataLogF("JSC GC allocating %lu bytes of auxiliary for %p: %p.\n", bytes, intendedOwner, result);
#else
    UNUSED_PARAM(intendedOwner);
#endif
    return result;
}

inline void* Heap::tryAllocateAuxiliary(JSCell* intendedOwner, size_t bytes)
{
    void* result = m_objectSpace.tryAllocateAuxiliary(bytes);
#if ENABLE(ALLOCATION_LOGGING)
    dataLogF("JSC GC allocating %lu bytes of auxiliary for %p: %p.\n", bytes, intendedOwner, result);
#else
    UNUSED_PARAM(intendedOwner);
#endif
    return result;
}

inline void* Heap::tryAllocateAuxiliary(GCDeferralContext* deferralContext, JSCell* intendedOwner, size_t bytes)
{
    void* result = m_objectSpace.tryAllocateAuxiliary(deferralContext, bytes);
#if ENABLE(ALLOCATION_LOGGING)
    dataLogF("JSC GC allocating %lu bytes of auxiliary for %p: %p.\n", bytes, intendedOwner, result);
#else
    UNUSED_PARAM(intendedOwner);
#endif
    return result;
}

inline void* Heap::tryReallocateAuxiliary(JSCell* intendedOwner, void* oldBase, size_t oldSize, size_t newSize)
{
    void* newBase = tryAllocateAuxiliary(intendedOwner, newSize);
    if (!newBase)
        return nullptr;
    memcpy(newBase, oldBase, oldSize);
    return newBase;
}

inline void Heap::ascribeOwner(JSCell* intendedOwner, void* storage)
{
#if ENABLE(ALLOCATION_LOGGING)
    dataLogF("JSC GC ascribing %p as owner of storage %p.\n", intendedOwner, storage);
#else
    UNUSED_PARAM(intendedOwner);
    UNUSED_PARAM(storage);
#endif
}

#if USE(FOUNDATION)
template <typename T>
inline void Heap::releaseSoon(RetainPtr<T>&& object)
{
    m_delayedReleaseObjects.append(WTFMove(object));
}
#endif

inline void Heap::incrementDeferralDepth()
{
    RELEASE_ASSERT(m_deferralDepth < 100); // Sanity check to make sure this doesn't get ridiculous.
    m_deferralDepth++;
}

inline void Heap::decrementDeferralDepth()
{
    RELEASE_ASSERT(m_deferralDepth >= 1);
    m_deferralDepth--;
}

inline bool Heap::collectIfNecessaryOrDefer(GCDeferralContext* deferralContext)
{
    if (!shouldCollect())
        return false;

    if (deferralContext)
        deferralContext->m_shouldGC = true;
    else
        collect();
    return true;
}

inline void Heap::collectAccordingToDeferGCProbability()
{
    if (isDeferred() || !m_isSafeToCollect || m_operationInProgress != NoOperation)
        return;

    if (randomNumber() < Options::deferGCProbability()) {
        collect();
        return;
    }

    // If our coin flip told us not to GC, we still might GC,
    // but we GC according to our memory pressure markers.
    collectIfNecessaryOrDefer();
}

inline void Heap::decrementDeferralDepthAndGCIfNeeded()
{
    decrementDeferralDepth();
    if (UNLIKELY(Options::deferGCShouldCollectWithProbability()))
        collectAccordingToDeferGCProbability();
    else
        collectIfNecessaryOrDefer();
}

inline HashSet<MarkedArgumentBuffer*>& Heap::markListSet()
{
    if (!m_markListSet)
        m_markListSet = std::make_unique<HashSet<MarkedArgumentBuffer*>>();
    return *m_markListSet;
}

inline void Heap::registerWeakGCMap(void* weakGCMap, std::function<void()> pruningCallback)
{
    m_weakGCMaps.add(weakGCMap, WTFMove(pruningCallback));
}

inline void Heap::unregisterWeakGCMap(void* weakGCMap)
{
    m_weakGCMaps.remove(weakGCMap);
}

inline void Heap::didAllocateBlock(size_t capacity)
{
#if ENABLE(RESOURCE_USAGE)
    m_blockBytesAllocated += capacity;
#else
    UNUSED_PARAM(capacity);
#endif
}

inline void Heap::didFreeBlock(size_t capacity)
{
#if ENABLE(RESOURCE_USAGE)
    m_blockBytesAllocated -= capacity;
#else
    UNUSED_PARAM(capacity);
#endif
}

} // namespace JSC
