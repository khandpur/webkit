/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "JSMediaDevicesCustom.h"

#if ENABLE(MEDIA_STREAM)

#include "ArrayValue.h"
#include "Dictionary.h"
#include "ExceptionCode.h"
#include "JSMediaDevices.h"
#include "Logging.h"
#include "MediaConstraintsImpl.h"
#include "RealtimeMediaSourceCenter.h"
#include "RealtimeMediaSourceSupportedConstraints.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

using namespace JSC;

namespace WebCore {

enum class ConstraintSetType { Mandatory, Advanced };

static void initializeStringConstraintWithList(StringConstraint& constraint, void (StringConstraint::*appendValue)(const String&), const ArrayValue& list)
{
    size_t size;
    if (!list.length(size))
        return;

    for (size_t i = 0; i < size; ++i) {
        String value;
        if (list.get(i, value))
            (constraint.*appendValue)(value);
    }
}

static Optional<StringConstraint> createStringConstraint(const Dictionary& mediaTrackConstraintSet, const String& name, MediaConstraintType type, ConstraintSetType constraintSetType)
{
    auto constraint = StringConstraint(name, type);

    // Dictionary constraint value.
    Dictionary dictionaryValue;
    if (mediaTrackConstraintSet.get(name, dictionaryValue) && !dictionaryValue.isUndefinedOrNull()) {
        ArrayValue exactArrayValue;
        if (dictionaryValue.get("exact", exactArrayValue) && !exactArrayValue.isUndefinedOrNull())
            initializeStringConstraintWithList(constraint, &StringConstraint::appendExact, exactArrayValue);
        else {
            String exactStringValue;
            if (dictionaryValue.get("exact", exactStringValue))
                constraint.setExact(exactStringValue);
        }

        ArrayValue idealArrayValue;
        if (dictionaryValue.get("ideal", idealArrayValue) && !idealArrayValue.isUndefinedOrNull())
            initializeStringConstraintWithList(constraint, &StringConstraint::appendIdeal, idealArrayValue);
        else {
            String idealStringValue;
            if (!dictionaryValue.get("ideal", idealStringValue))
                constraint.setIdeal(idealStringValue);
        }

        if (constraint.isEmpty()) {
            LOG(Media, "createStringConstraint() - ignoring string constraint '%s' with dictionary value since it has no valid or supported key/value pairs.", name.utf8().data());
            return Nullopt;
        }
        
        return WTFMove(constraint);
    }

    // Array constraint value.
    ArrayValue arrayValue;
    if (mediaTrackConstraintSet.get(name, arrayValue) && !arrayValue.isUndefinedOrNull()) {
        initializeStringConstraintWithList(constraint, &StringConstraint::appendIdeal, arrayValue);

        if (constraint.isEmpty()) {
            LOG(Media, "createStringConstraint() - ignoring string constraint '%s' with array value since it is empty.", name.utf8().data());
            return Nullopt;
        }

        return WTFMove(constraint);
    }

    // Scalar constraint value.
    String value;
    if (mediaTrackConstraintSet.get(name, value)) {
        if (constraintSetType == ConstraintSetType::Mandatory)
            constraint.setIdeal(value);
        else
            constraint.setExact(value);
        
        return WTFMove(constraint);
    }

    // Invalid constraint value.
    LOG(Media, "createStringConstraint() - ignoring string constraint '%s' since it has neither a dictionary nor sequence nor scalar value.", name.utf8().data());
    return Nullopt;
}

static Optional<BooleanConstraint> createBooleanConstraint(const Dictionary& mediaTrackConstraintSet, const String& name, MediaConstraintType type, ConstraintSetType constraintSetType)
{
    auto constraint = BooleanConstraint(name, type);

    // Dictionary constraint value.
    Dictionary dictionaryValue;
    if (mediaTrackConstraintSet.get(name, dictionaryValue) && !dictionaryValue.isUndefinedOrNull()) {
        bool exactValue;
        if (dictionaryValue.get("exact", exactValue))
            constraint.setExact(exactValue);

        bool idealValue;
        if (dictionaryValue.get("ideal", idealValue))
            constraint.setIdeal(idealValue);

        if (constraint.isEmpty()) {
            LOG(Media, "createBooleanConstraint() - ignoring boolean constraint '%s' with dictionary value since it has no valid or supported key/value pairs.", name.utf8().data());
            return Nullopt;
        }

        return WTFMove(constraint);
    }

    // Scalar constraint value.
    bool value;
    if (mediaTrackConstraintSet.get(name, value)) {
        if (constraintSetType == ConstraintSetType::Mandatory)
            constraint.setIdeal(value);
        else
            constraint.setExact(value);
        
        return WTFMove(constraint);
    }

    // Invalid constraint value.
    LOG(Media, "createBooleanConstraint() - ignoring boolean constraint '%s' since it has neither a dictionary nor scalar value.", name.utf8().data());
    return Nullopt;
}

static Optional<DoubleConstraint> createDoubleConstraint(const Dictionary& mediaTrackConstraintSet, const String& name, MediaConstraintType type, ConstraintSetType constraintSetType)
{
    auto constraint = DoubleConstraint(name, type);

    // Dictionary constraint value.
    Dictionary dictionaryValue;
    if (mediaTrackConstraintSet.get(name, dictionaryValue) && !dictionaryValue.isUndefinedOrNull()) {
        double minValue;
        if (dictionaryValue.get("min", minValue))
            constraint.setMin(minValue);

        double maxValue;
        if (dictionaryValue.get("max", maxValue))
            constraint.setMax(maxValue);

        double exactValue;
        if (dictionaryValue.get("exact", exactValue))
            constraint.setExact(exactValue);

        double idealValue;
        if (dictionaryValue.get("ideal", idealValue))
            constraint.setIdeal(idealValue);

        if (constraint.isEmpty()) {
            LOG(Media, "createDoubleConstraint() - ignoring double constraint '%s' with dictionary value since it has no valid or supported key/value pairs.", name.utf8().data());
            return Nullopt;
        }

        return WTFMove(constraint);
    }

    // Scalar constraint value.
    double value;
    if (mediaTrackConstraintSet.get(name, value)) {
        if (constraintSetType == ConstraintSetType::Mandatory)
            constraint.setIdeal(value);
        else
            constraint.setExact(value);
        
        return WTFMove(constraint);
    }

    // Invalid constraint value.
    LOG(Media, "createDoubleConstraint() - ignoring double constraint '%s' since it has neither a dictionary nor scalar value.", name.utf8().data());
    return Nullopt;
}

static Optional<IntConstraint> createIntConstraint(const Dictionary& mediaTrackConstraintSet, const String& name, MediaConstraintType type, ConstraintSetType constraintSetType)
{
    auto constraint = IntConstraint(name, type);

    // Dictionary constraint value.
    Dictionary dictionaryValue;
    if (mediaTrackConstraintSet.get(name, dictionaryValue) && !dictionaryValue.isUndefinedOrNull()) {
        int minValue;
        if (dictionaryValue.get("min", minValue))
            constraint.setMin(minValue);

        int maxValue;
        if (dictionaryValue.get("max", maxValue))
            constraint.setMax(maxValue);

        int exactValue;
        if (dictionaryValue.get("exact", exactValue))
            constraint.setExact(exactValue);

        int idealValue;
        if (dictionaryValue.get("ideal", idealValue))
            constraint.setIdeal(idealValue);

        if (constraint.isEmpty()) {
            LOG(Media, "createIntConstraint() - ignoring long constraint '%s' with dictionary value since it has no valid or supported key/value pairs.", name.utf8().data());
            return Nullopt;
        }

        return WTFMove(constraint);
    }

    // Scalar constraint value.
    int value;
    if (mediaTrackConstraintSet.get(name, value)) {
        if (constraintSetType == ConstraintSetType::Mandatory)
            constraint.setIdeal(value);
        else
            constraint.setExact(value);
        
        return WTFMove(constraint);
    }

    // Invalid constraint value.
    LOG(Media, "createIntConstraint() - ignoring long constraint '%s' since it has neither a dictionary nor scalar value.", name.utf8().data());
    return Nullopt;
}

static void parseMediaTrackConstraintSetForKey(const Dictionary& mediaTrackConstraintSet, const String& name, MediaTrackConstraintSetMap& map, ConstraintSetType constraintSetType)
{
    MediaConstraintType constraintType = RealtimeMediaSourceSupportedConstraints::constraintFromName(name);
    switch (constraintType) {
    case MediaConstraintType::Width:
        map.set(constraintType, createIntConstraint(mediaTrackConstraintSet, name, constraintType, constraintSetType));
        break;
    case MediaConstraintType::Height:
        map.set(constraintType, createIntConstraint(mediaTrackConstraintSet, name, constraintType, constraintSetType));
        break;
    case MediaConstraintType::SampleRate:
        map.set(constraintType, createIntConstraint(mediaTrackConstraintSet, name, constraintType, constraintSetType));
        break;
    case MediaConstraintType::SampleSize:
        map.set(constraintType, createIntConstraint(mediaTrackConstraintSet, name, constraintType, constraintSetType));
        break;

    case MediaConstraintType::AspectRatio:
        map.set(constraintType, createDoubleConstraint(mediaTrackConstraintSet, name, constraintType, constraintSetType));
        break;
    case MediaConstraintType::FrameRate:
        map.set(constraintType, createDoubleConstraint(mediaTrackConstraintSet, name, constraintType, constraintSetType));
        break;
    case MediaConstraintType::Volume:
        map.set(constraintType, createDoubleConstraint(mediaTrackConstraintSet, name, constraintType, constraintSetType));
        break;

    case MediaConstraintType::EchoCancellation:
        map.set(constraintType, createBooleanConstraint(mediaTrackConstraintSet, name, constraintType, constraintSetType));
        break;

    case MediaConstraintType::FacingMode:
        map.set(constraintType, createStringConstraint(mediaTrackConstraintSet, name, constraintType, constraintSetType));
        break;
    case MediaConstraintType::DeviceId:
        map.set(constraintType, createStringConstraint(mediaTrackConstraintSet, name, constraintType, constraintSetType));
        break;
    case MediaConstraintType::GroupId:
        map.set(constraintType, createStringConstraint(mediaTrackConstraintSet, name, constraintType, constraintSetType));
        break;

    case MediaConstraintType::Unknown:
        LOG(Media, "parseMediaTrackConstraintSetForKey() - ignoring unsupported constraint '%s'.", name.utf8().data());
        return;
    }
}

static void parseAdvancedConstraints(const Dictionary& mediaTrackConstraints, Vector<MediaTrackConstraintSetMap>& advancedConstraints)
{
    ArrayValue sequenceOfMediaTrackConstraintSets;
    if (!mediaTrackConstraints.get("advanced", sequenceOfMediaTrackConstraintSets) || sequenceOfMediaTrackConstraintSets.isUndefinedOrNull()) {
        LOG(Media, "parseAdvancedConstraints() - value of advanced key is not a list.");
        return;
    }

    size_t numberOfConstraintSets;
    if (!sequenceOfMediaTrackConstraintSets.length(numberOfConstraintSets)) {
        LOG(Media, "parseAdvancedConstraints() - ignoring empty advanced sequence of MediaTrackConstraintSets.");
        return;
    }

    for (size_t i = 0; i < numberOfConstraintSets; ++i) {
        Dictionary mediaTrackConstraintSet;
        if (!sequenceOfMediaTrackConstraintSets.get(i, mediaTrackConstraintSet) || mediaTrackConstraintSet.isUndefinedOrNull()) {
            LOG(Media, "parseAdvancedConstraints() - ignoring constraint set with index '%zu' in advanced list.", i);
            continue;
        }

        MediaTrackConstraintSetMap map;

        Vector<String> localKeys;
        mediaTrackConstraintSet.getOwnPropertyNames(localKeys);
        for (auto& localKey : localKeys)
            parseMediaTrackConstraintSetForKey(mediaTrackConstraintSet, localKey, map, ConstraintSetType::Advanced);

        if (!map.isEmpty())
            advancedConstraints.append(WTFMove(map));
    }
}

void parseMediaConstraintsDictionary(const Dictionary& mediaTrackConstraints, MediaTrackConstraintSetMap& mandatoryConstraints, Vector<MediaTrackConstraintSetMap>& advancedConstraints)
{
    if (mediaTrackConstraints.isUndefinedOrNull())
        return;

    Vector<String> keys;
    mediaTrackConstraints.getOwnPropertyNames(keys);

    for (auto& key : keys) {
        if (key == "advanced")
            parseAdvancedConstraints(mediaTrackConstraints, advancedConstraints);
        else
            parseMediaTrackConstraintSetForKey(mediaTrackConstraints, key, mandatoryConstraints, ConstraintSetType::Mandatory);
    }
}

static void JSMediaDevicesGetUserMediaPromiseFunction(ExecState& state, Ref<DeferredPromise>&& promise)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 1)) {
        throwVMError(&state, scope, createNotEnoughArgumentsError(&state));
        return;
    }

    ExceptionCode ec = 0;
    auto constraintsDictionary = Dictionary(&state, state.uncheckedArgument(0));

    MediaTrackConstraintSetMap mandatoryAudioConstraints;
    Vector<MediaTrackConstraintSetMap> advancedAudioConstraints;
    bool areAudioConstraintsValid = false;

    Dictionary audioConstraintsDictionary;
    if (constraintsDictionary.get("audio", audioConstraintsDictionary) && !audioConstraintsDictionary.isUndefinedOrNull()) {
        parseMediaConstraintsDictionary(audioConstraintsDictionary, mandatoryAudioConstraints, advancedAudioConstraints);
        areAudioConstraintsValid = true;
    } else
        constraintsDictionary.get("audio", areAudioConstraintsValid);

    MediaTrackConstraintSetMap mandatoryVideoConstraints;
    Vector<MediaTrackConstraintSetMap> advancedVideoConstraints;
    bool areVideoConstraintsValid = false;

    Dictionary videoConstraintsDictionary;
    if (constraintsDictionary.get("video", videoConstraintsDictionary) && !videoConstraintsDictionary.isUndefinedOrNull()) {
        parseMediaConstraintsDictionary(videoConstraintsDictionary, mandatoryVideoConstraints, advancedVideoConstraints);
        areVideoConstraintsValid = true;
    } else
        constraintsDictionary.get("video", areVideoConstraintsValid);

    auto audioConstraints = MediaConstraintsImpl::create(WTFMove(mandatoryAudioConstraints), WTFMove(advancedAudioConstraints), areAudioConstraintsValid);
    auto videoConstraints = MediaConstraintsImpl::create(WTFMove(mandatoryVideoConstraints), WTFMove(advancedVideoConstraints), areVideoConstraintsValid);
    castThisValue<JSMediaDevices>(state).wrapped().getUserMedia(WTFMove(audioConstraints), WTFMove(videoConstraints), WTFMove(promise), ec);
    setDOMException(&state, ec);
}

JSValue JSMediaDevices::getUserMedia(ExecState& state)
{
    return callPromiseFunction<JSMediaDevicesGetUserMediaPromiseFunction, PromiseExecutionScope::WindowOnly>(state);
}

}

#endif
