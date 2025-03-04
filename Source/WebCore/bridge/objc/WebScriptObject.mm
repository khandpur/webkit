/*
 * Copyright (C) 2004, 2006, 2007, 2008, 2013 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebScriptObjectPrivate.h"

#import "BridgeJSC.h"
#import "Frame.h"
#import "JSDOMWindow.h"
#import "JSDOMWindowCustom.h"
#import "JSHTMLElement.h"
#import "JSMainThreadExecState.h"
#import "JSPluginElementFunctions.h"
#import "ObjCRuntimeObject.h"
#import "WebCoreObjCExtras.h"
#import "objc_instance.h"
#import "runtime_object.h"
#import "runtime_root.h"
#import <JavaScriptCore/APICast.h>
#import <JavaScriptCore/JSContextInternal.h>
#import <JavaScriptCore/JSValueInternal.h>
#import <interpreter/CallFrame.h>
#import <runtime/CatchScope.h>
#import <runtime/Completion.h>
#import <runtime/InitializeThreading.h>
#import <runtime/JSGlobalObject.h>
#import <runtime/JSLock.h>
#import <wtf/HashMap.h>
#import <wtf/Lock.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/Threading.h>
#import <wtf/text/WTFString.h>

using namespace JSC::Bindings;
using namespace WebCore;

using JSC::CallData;
using JSC::CallType;
using JSC::ExecState;
using JSC::Identifier;
using JSC::JSLockHolder;
using JSC::JSObject;
using JSC::MarkedArgumentBuffer;
using JSC::PutPropertySlot;
using JSC::jsCast;
using JSC::jsUndefined;
using JSC::makeSource;

namespace WebCore {

static StaticLock spinLock;
static CreateWrapperFunction createDOMWrapperFunction;
static DisconnectWindowWrapperFunction disconnectWindowWrapperFunction;

static HashMap<JSObject*, NSObject *>& wrapperCache()
{
    static NeverDestroyed<HashMap<JSObject*, NSObject *>> map;
    return map;
}

NSObject *getJSWrapper(JSObject* impl)
{
    ASSERT(isMainThread());
    LockHolder holder(&spinLock);

    NSObject* wrapper = wrapperCache().get(impl);
    return wrapper ? [[wrapper retain] autorelease] : nil;
}

void addJSWrapper(NSObject *wrapper, JSObject* impl)
{
    ASSERT(isMainThread());
    LockHolder holder(&spinLock);

    wrapperCache().set(impl, wrapper);
}

void removeJSWrapper(JSObject* impl)
{
    LockHolder holder(&spinLock);

    wrapperCache().remove(impl);
}

static void removeJSWrapperIfRetainCountOne(NSObject* wrapper, JSObject* impl)
{
    LockHolder holder(&spinLock);

    if ([wrapper retainCount] == 1)
        wrapperCache().remove(impl);
}

id createJSWrapper(JSC::JSObject* object, PassRefPtr<JSC::Bindings::RootObject> origin, PassRefPtr<JSC::Bindings::RootObject> root)
{
    if (id wrapper = getJSWrapper(object))
        return wrapper;
    return [[[WebScriptObject alloc] _initWithJSObject:object originRootObject:origin rootObject:root] autorelease];
}

static void addExceptionToConsole(ExecState* exec, JSC::Exception* exception)
{
    JSDOMWindow* window = asJSDOMWindow(exec->vmEntryGlobalObject());
    if (!window || !exception)
        return;
    reportException(exec, exception);
}

static void addExceptionToConsole(ExecState* exec)
{
    JSC::VM& vm = exec->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);
    JSC::Exception* exception = scope.exception();
    scope.clearException();
    addExceptionToConsole(exec, exception);
}

void initializeDOMWrapperHooks(CreateWrapperFunction createFunction, DisconnectWindowWrapperFunction disconnectFunction)
{
    ASSERT(createFunction);
    ASSERT(disconnectFunction);
    ASSERT(!createDOMWrapperFunction);
    ASSERT(!disconnectWindowWrapperFunction);
    createDOMWrapperFunction = createFunction;
    disconnectWindowWrapperFunction = disconnectFunction;
}

void disconnectWindowWrapper(WebScriptObject *windowWrapper)
{
    ASSERT(windowWrapper);
    ASSERT(disconnectWindowWrapperFunction);
    disconnectWindowWrapperFunction(windowWrapper);
}

} // namespace WebCore

@implementation WebScriptObjectPrivate

@end

@implementation WebScriptObject

+ (void)initialize
{
#if !USE(WEB_THREAD)
    JSC::initializeThreading();
    WTF::initializeMainThreadToProcessMainThread();
#endif
}

+ (id)scriptObjectForJSObject:(JSObjectRef)jsObject originRootObject:(RootObject*)originRootObject rootObject:(RootObject*)rootObject
{
    ASSERT(jsObject);
    auto& wrapped = *toJS(jsObject);

    if (WebCore::createDOMWrapperFunction) {
        if (auto wrapper = WebCore::createDOMWrapperFunction(wrapped)) {
            if (![wrapper _hasImp]) // new wrapper, not from cache
                [wrapper _setImp:&wrapped originRootObject:originRootObject rootObject:rootObject];
            return wrapper;
        }
    }

    return WebCore::createJSWrapper(&wrapped, originRootObject, rootObject);
}

- (void)_setImp:(JSObject*)imp originRootObject:(PassRefPtr<RootObject>)originRootObject rootObject:(PassRefPtr<RootObject>)rootObject
{
    // This function should only be called once, as a (possibly lazy) initializer.
    ASSERT(!_private->imp);
    ASSERT(!_private->rootObject);
    ASSERT(!_private->originRootObject);
    ASSERT(imp);

    _private->imp = imp;
    _private->rootObject = rootObject.leakRef();
    _private->originRootObject = originRootObject.leakRef();

    WebCore::addJSWrapper(self, imp);

    if (_private->rootObject)
        _private->rootObject->gcProtect(imp);
}

- (void)_setOriginRootObject:(PassRefPtr<RootObject>)originRootObject andRootObject:(PassRefPtr<RootObject>)rootObject
{
    ASSERT(_private->imp);

    if (rootObject)
        rootObject->gcProtect(_private->imp);

    if (_private->rootObject && _private->rootObject->isValid())
        _private->rootObject->gcUnprotect(_private->imp);

    if (_private->rootObject)
        _private->rootObject->deref();

    if (_private->originRootObject)
        _private->originRootObject->deref();

    _private->rootObject = rootObject.leakRef();
    _private->originRootObject = originRootObject.leakRef();
}

- (id)_initWithJSObject:(JSC::JSObject*)imp originRootObject:(PassRefPtr<JSC::Bindings::RootObject>)originRootObject rootObject:(PassRefPtr<JSC::Bindings::RootObject>)rootObject
{
    ASSERT(imp);

    self = [super init];
    _private = [[WebScriptObjectPrivate alloc] init];
    [self _setImp:imp originRootObject:originRootObject rootObject:rootObject];
    
    return self;
}

- (JSObject*)_imp
{
    // Associate the WebScriptObject with the JS wrapper for the ObjC DOM wrapper.
    // This is done on lazily, on demand.
    if (!_private->imp && _private->isCreatedByDOMWrapper)
        [self _initializeScriptDOMNodeImp];
    return [self _rootObject] ? _private->imp : 0;
}

- (BOOL)_hasImp
{
    return _private->imp != nil;
}

// Node that DOMNode overrides this method. So you should almost always
// use this method call instead of _private->rootObject directly.
- (RootObject*)_rootObject
{
    return _private->rootObject && _private->rootObject->isValid() ? _private->rootObject : 0;
}

- (RootObject *)_originRootObject
{
    return _private->originRootObject && _private->originRootObject->isValid() ? _private->originRootObject : 0;
}

- (BOOL)_isSafeScript
{
    RootObject *root = [self _rootObject];
    if (!root)
        return false;

    if (!_private->originRootObject)
        return true;

    if (!_private->originRootObject->isValid())
        return false;

    // It's not actually correct to call shouldAllowAccessToFrame in this way because
    // JSDOMWindowBase* isn't the right object to represent the currently executing
    // JavaScript. Instead, we should use ExecState, like we do elsewhere.
    JSDOMWindowBase* target = jsCast<JSDOMWindowBase*>(root->globalObject());
    return BindingSecurity::shouldAllowAccessToDOMWindow(_private->originRootObject->globalObject()->globalExec(), target->wrapped());
}

- (JSGlobalContextRef)_globalContextRef
{
    if (![self _isSafeScript])
        return nil;
    return toGlobalRef([self _rootObject]->globalObject()->globalExec());
}

- (oneway void)release
{
    // If we're releasing the last reference to this object, remove if from the map.
    if (_private->imp)
        WebCore::removeJSWrapperIfRetainCountOne(self, _private->imp);

    [super release];
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([WebScriptObject class], self))
        return;

    if (_private->rootObject && _private->rootObject->isValid())
        _private->rootObject->gcUnprotect(_private->imp);

    if (_private->rootObject)
        _private->rootObject->deref();

    if (_private->originRootObject)
        _private->originRootObject->deref();

    [_private release];

    [super dealloc];
}

+ (BOOL)throwException:(NSString *)exceptionMessage
{
    ObjcInstance::setGlobalException(exceptionMessage);
    return YES;
}

static void getListFromNSArray(ExecState *exec, NSArray *array, RootObject* rootObject, MarkedArgumentBuffer& aList)
{
    int i, numObjects = array ? [array count] : 0;
    
    for (i = 0; i < numObjects; i++) {
        id anObject = [array objectAtIndex:i];
        aList.append(convertObjcValueToValue(exec, &anObject, ObjcObjectType, rootObject));
    }
}

- (id)callWebScriptMethod:(NSString *)name withArguments:(NSArray *)args
{
    if (![self _isSafeScript])
        return nil;

    // Look up the function object.
    auto globalObject = [self _rootObject]->globalObject();
    auto& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);
    ExecState* exec = globalObject->globalExec();
    UNUSED_PARAM(scope);

    JSC::JSValue function = [self _imp]->get(exec, Identifier::fromString(exec, String(name)));
    CallData callData;
    CallType callType = getCallData(function, callData);
    if (callType == CallType::None)
        return nil;

    MarkedArgumentBuffer argList;
    getListFromNSArray(exec, args, [self _rootObject], argList);

    if (![self _isSafeScript])
        return nil;

    NakedPtr<JSC::Exception> exception;
    JSC::JSValue result = JSMainThreadExecState::profiledCall(exec, JSC::ProfilingReason::Other, function, callType, callData, [self _imp], argList, exception);

    if (exception) {
        addExceptionToConsole(exec, exception);
        result = jsUndefined();
    }

    // Convert and return the result of the function call.
    id resultObj = [WebScriptObject _convertValueToObjcValue:result originRootObject:[self _originRootObject] rootObject:[self _rootObject]];

    return resultObj;
}

- (id)evaluateWebScript:(NSString *)script
{
    if (![self _isSafeScript])
        return nil;
    
    auto globalObject = [self _rootObject]->globalObject();
    auto& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);
    ExecState* exec = globalObject->globalExec();
    UNUSED_PARAM(scope);
    
    JSC::JSValue returnValue = JSMainThreadExecState::profiledEvaluate(exec, JSC::ProfilingReason::Other, makeSource(String(script)), JSC::JSValue());

    id resultObj = [WebScriptObject _convertValueToObjcValue:returnValue originRootObject:[self _originRootObject] rootObject:[self _rootObject]];
    
    return resultObj;
}

- (void)setValue:(id)value forKey:(NSString *)key
{
    if (![self _isSafeScript])
        return;

    auto globalObject = [self _rootObject]->globalObject();
    auto& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);
    ExecState* exec = globalObject->globalExec();

    JSObject* object = JSC::jsDynamicCast<JSObject*>([self _imp]);
    PutPropertySlot slot(object);
    object->methodTable()->put(object, exec, Identifier::fromString(exec, String(key)), convertObjcValueToValue(exec, &value, ObjcObjectType, [self _rootObject]), slot);

    if (UNLIKELY(scope.exception())) {
        addExceptionToConsole(exec);
        scope.clearException();
    }
}

- (id)valueForKey:(NSString *)key
{
    if (![self _isSafeScript])
        return nil;

    id resultObj;
    {
        auto globalObject = [self _rootObject]->globalObject();
        auto& vm = globalObject->vm();

        // Need to scope this lock to ensure that we release the lock before calling
        // [super valueForKey:key] which might throw an exception and bypass the JSLock destructor,
        // leaving the lock permanently held
        JSLockHolder lock(vm);

        auto scope = DECLARE_CATCH_SCOPE(vm);
        ExecState* exec = globalObject->globalExec();

        JSC::JSValue result = [self _imp]->get(exec, Identifier::fromString(exec, String(key)));
        
        if (UNLIKELY(scope.exception())) {
            addExceptionToConsole(exec);
            result = jsUndefined();
            scope.clearException();
        }

        resultObj = [WebScriptObject _convertValueToObjcValue:result originRootObject:[self _originRootObject] rootObject:[self _rootObject]];
    }
    
    if ([resultObj isKindOfClass:[WebUndefined class]])
        resultObj = [super valueForKey:key];    // defaults to throwing an exception

    return resultObj;
}

- (void)removeWebScriptKey:(NSString *)key
{
    if (![self _isSafeScript])
        return;

    auto globalObject = [self _rootObject]->globalObject();
    auto& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);
    ExecState* exec = globalObject->globalExec();

    [self _imp]->methodTable()->deleteProperty([self _imp], exec, Identifier::fromString(exec, String(key)));

    if (UNLIKELY(scope.exception())) {
        addExceptionToConsole(exec);
        scope.clearException();
    }
}

- (BOOL)hasWebScriptKey:(NSString *)key
{
    if (![self _isSafeScript])
        return NO;

    auto globalObject = [self _rootObject]->globalObject();
    auto& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);
    ExecState* exec = globalObject->globalExec();

    BOOL result = [self _imp]->hasProperty(exec, Identifier::fromString(exec, String(key)));

    if (UNLIKELY(scope.exception())) {
        addExceptionToConsole(exec);
        scope.clearException();
    }

    return result;
}

- (NSString *)stringRepresentation
{
    if (![self _isSafeScript]) {
        // This is a workaround for a gcc 3.3 internal compiler error.
        return @"Undefined";
    }

    ExecState* exec = [self _rootObject]->globalObject()->globalExec();
    JSLockHolder lock(exec);

    id result = convertValueToObjcValue(exec, [self _imp], ObjcObjectType).objectValue;

    NSString *description = [result description];

    return description;
}

- (id)webScriptValueAtIndex:(unsigned)index
{
    if (![self _isSafeScript])
        return nil;

    auto globalObject = [self _rootObject]->globalObject();
    auto& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);
    ExecState* exec = globalObject->globalExec();

    JSC::JSValue result = [self _imp]->get(exec, index);

    if (UNLIKELY(scope.exception())) {
        addExceptionToConsole(exec);
        result = jsUndefined();
        scope.clearException();
    }

    id resultObj = [WebScriptObject _convertValueToObjcValue:result originRootObject:[self _originRootObject] rootObject:[self _rootObject]];

    return resultObj;
}

- (void)setWebScriptValueAtIndex:(unsigned)index value:(id)value
{
    if (![self _isSafeScript])
        return;

    auto globalObject = [self _rootObject]->globalObject();
    auto& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);
    ExecState* exec = globalObject->globalExec();

    [self _imp]->methodTable()->putByIndex([self _imp], exec, index, convertObjcValueToValue(exec, &value, ObjcObjectType, [self _rootObject]), false);

    if (UNLIKELY(scope.exception())) {
        addExceptionToConsole(exec);
        scope.clearException();
    }
}

- (void)setException:(NSString *)description
{
    if (![self _rootObject])
        return;
    ObjcInstance::setGlobalException(description, [self _rootObject]->globalObject());
}

- (JSObjectRef)JSObject
{
    if (![self _isSafeScript])
        return 0;
    ExecState* exec = [self _rootObject]->globalObject()->globalExec();

    JSLockHolder lock(exec);
    return toRef([self _imp]);
}

+ (id)_convertValueToObjcValue:(JSC::JSValue)value originRootObject:(RootObject*)originRootObject rootObject:(RootObject*)rootObject
{
    if (value.isObject()) {
        JSObject* object = asObject(value);
        JSLockHolder lock(rootObject->globalObject()->vm());

        if (object->inherits(JSHTMLElement::info())) {
            // Plugin elements cache the instance internally.
            if (ObjcInstance* instance = static_cast<ObjcInstance*>(pluginInstance(jsCast<JSHTMLElement*>(object)->wrapped())))
                return instance->getObject();
        } else if (object->inherits(ObjCRuntimeObject::info())) {
            ObjCRuntimeObject* runtimeObject = static_cast<ObjCRuntimeObject*>(object);
            ObjcInstance* instance = runtimeObject->getInternalObjCInstance();
            if (instance)
                return instance->getObject();
            return nil;
        }

        return [WebScriptObject scriptObjectForJSObject:toRef(object) originRootObject:originRootObject rootObject:rootObject];
    }

    if (value.isString())
        return asString(value)->value(rootObject->globalObject()->globalExec());

    if (value.isNumber())
        return [NSNumber numberWithDouble:value.asNumber()];

    if (value.isBoolean())
        return [NSNumber numberWithBool:value.asBoolean()];

    if (value.isUndefined())
        return [WebUndefined undefined];

    // jsNull is not returned as NSNull because existing applications do not expect
    // that return value. Return as nil for compatibility. <rdar://problem/4651318> <rdar://problem/4701626>
    // Other types (e.g., UnspecifiedType) also return as nil.
    return nil;
}


#if JSC_OBJC_API_ENABLED
- (JSValue *)JSValue
{
    if (![self _isSafeScript])
        return 0;

    return [JSValue valueWithJSValueRef:[self JSObject]
                    inContext:[JSContext contextWithJSGlobalContextRef:[self _globalContextRef]]];
}
#endif

@end

@interface WebScriptObject (WebKitCocoaBindings)

- (id)objectAtIndex:(unsigned)index;

@end

@implementation WebScriptObject (WebKitCocoaBindings)

#if 0

// FIXME: We'd like to add this, but we can't do that until this issue is resolved:
// http://bugs.webkit.org/show_bug.cgi?id=13129: presence of 'count' method on
// WebScriptObject breaks Democracy player.

- (unsigned)count
{
    id length = [self valueForKey:@"length"];
    if (![length respondsToSelector:@selector(intValue)])
        return 0;
    return [length intValue];
}

#endif

- (id)objectAtIndex:(unsigned)index
{
    return [self webScriptValueAtIndex:index];
}

@end

@implementation WebUndefined

+ (id)allocWithZone:(NSZone *)unusedZone
{
    UNUSED_PARAM(unusedZone);

    static WebUndefined *sharedUndefined = 0;
    if (!sharedUndefined)
        sharedUndefined = [super allocWithZone:NULL];
    return sharedUndefined;
}

- (NSString *)description
{
    return @"undefined";
}

- (id)initWithCoder:(NSCoder *)unusedCoder
{
    UNUSED_PARAM(unusedCoder);

    return self;
}

- (void)encodeWithCoder:(NSCoder *)unusedCoder
{
    UNUSED_PARAM(unusedCoder);
}

- (id)copyWithZone:(NSZone *)unusedZone
{
    UNUSED_PARAM(unusedZone);

    return self;
}

- (id)retain
{
    return self;
}

- (oneway void)release
{
}

- (NSUInteger)retainCount
{
    return NSUIntegerMax;
}

- (id)autorelease
{
    return self;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-missing-super-calls"
- (void)dealloc
{
    return;
}
#pragma clang diagnostic pop

+ (WebUndefined *)undefined
{
    return [WebUndefined allocWithZone:NULL];
}

@end
