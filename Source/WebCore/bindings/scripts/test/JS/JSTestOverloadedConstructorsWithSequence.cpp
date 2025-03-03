/*
    This file is part of the WebKit open source project.
    This file has been generated by generate-bindings.pl. DO NOT MODIFY!

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "JSTestOverloadedConstructorsWithSequence.h"

#include "ExceptionCode.h"
#include "JSDOMBinding.h"
#include "JSDOMConstructor.h"
#include <runtime/Error.h>
#include <runtime/FunctionPrototype.h>
#include <wtf/GetPtr.h>

using namespace JSC;

namespace WebCore {

// Attributes

JSC::EncodedJSValue jsTestOverloadedConstructorsWithSequenceConstructor(JSC::ExecState*, JSC::EncodedJSValue, JSC::PropertyName);
bool setJSTestOverloadedConstructorsWithSequenceConstructor(JSC::ExecState*, JSC::EncodedJSValue, JSC::EncodedJSValue);

class JSTestOverloadedConstructorsWithSequencePrototype : public JSC::JSNonFinalObject {
public:
    typedef JSC::JSNonFinalObject Base;
    static JSTestOverloadedConstructorsWithSequencePrototype* create(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::Structure* structure)
    {
        JSTestOverloadedConstructorsWithSequencePrototype* ptr = new (NotNull, JSC::allocateCell<JSTestOverloadedConstructorsWithSequencePrototype>(vm.heap)) JSTestOverloadedConstructorsWithSequencePrototype(vm, globalObject, structure);
        ptr->finishCreation(vm);
        return ptr;
    }

    DECLARE_INFO;
    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
    }

private:
    JSTestOverloadedConstructorsWithSequencePrototype(JSC::VM& vm, JSC::JSGlobalObject*, JSC::Structure* structure)
        : JSC::JSNonFinalObject(vm, structure)
    {
    }

    void finishCreation(JSC::VM&);
};

typedef JSDOMConstructor<JSTestOverloadedConstructorsWithSequence> JSTestOverloadedConstructorsWithSequenceConstructor;

static inline EncodedJSValue constructJSTestOverloadedConstructorsWithSequence1(ExecState* state)
{
    VM& vm = state->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    UNUSED_PARAM(throwScope);
    auto* castedThis = jsCast<JSTestOverloadedConstructorsWithSequenceConstructor*>(state->callee());
    ASSERT(castedThis);
    auto sequenceOfStrings = state->argument(0).isUndefined() ? Vector<String>() : toNativeArray<String>(*state, state->uncheckedArgument(0));
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
    auto object = TestOverloadedConstructorsWithSequence::create(WTFMove(sequenceOfStrings));
    return JSValue::encode(toJSNewlyCreated(state, castedThis->globalObject(), WTFMove(object)));
}

static inline EncodedJSValue constructJSTestOverloadedConstructorsWithSequence2(ExecState* state)
{
    VM& vm = state->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    UNUSED_PARAM(throwScope);
    auto* castedThis = jsCast<JSTestOverloadedConstructorsWithSequenceConstructor*>(state->callee());
    ASSERT(castedThis);
    if (UNLIKELY(state->argumentCount() < 1))
        return throwVMError(state, throwScope, createNotEnoughArgumentsError(state));
    auto string = state->uncheckedArgument(0).toWTFString(state);
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
    auto object = TestOverloadedConstructorsWithSequence::create(WTFMove(string));
    return JSValue::encode(toJSNewlyCreated(state, castedThis->globalObject(), WTFMove(object)));
}

template<> EncodedJSValue JSC_HOST_CALL JSTestOverloadedConstructorsWithSequenceConstructor::construct(ExecState* state)
{
    VM& vm = state->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    UNUSED_PARAM(throwScope);
    size_t argsCount = std::min<size_t>(1, state->argumentCount());
    if (argsCount == 0) {
        return constructJSTestOverloadedConstructorsWithSequence1(state);
    }
    if (argsCount == 1) {
        JSValue distinguishingArg = state->uncheckedArgument(0);
        if (distinguishingArg.isUndefined())
            return constructJSTestOverloadedConstructorsWithSequence1(state);
        if (hasIteratorMethod(*state, distinguishingArg))
            return constructJSTestOverloadedConstructorsWithSequence1(state);
        return constructJSTestOverloadedConstructorsWithSequence2(state);
    }
    return throwVMTypeError(state, throwScope);
}

template<> JSValue JSTestOverloadedConstructorsWithSequenceConstructor::prototypeForStructure(JSC::VM& vm, const JSDOMGlobalObject& globalObject)
{
    UNUSED_PARAM(vm);
    return globalObject.functionPrototype();
}

template<> void JSTestOverloadedConstructorsWithSequenceConstructor::initializeProperties(VM& vm, JSDOMGlobalObject& globalObject)
{
    putDirect(vm, vm.propertyNames->prototype, JSTestOverloadedConstructorsWithSequence::prototype(vm, &globalObject), DontDelete | ReadOnly | DontEnum);
    putDirect(vm, vm.propertyNames->name, jsNontrivialString(&vm, String(ASCIILiteral("TestOverloadedConstructorsWithSequence"))), ReadOnly | DontEnum);
    putDirect(vm, vm.propertyNames->length, jsNumber(0), ReadOnly | DontEnum);
}

template<> const ClassInfo JSTestOverloadedConstructorsWithSequenceConstructor::s_info = { "TestOverloadedConstructorsWithSequence", &Base::s_info, 0, CREATE_METHOD_TABLE(JSTestOverloadedConstructorsWithSequenceConstructor) };

/* Hash table for prototype */

static const HashTableValue JSTestOverloadedConstructorsWithSequencePrototypeTableValues[] =
{
    { "constructor", DontEnum, NoIntrinsic, { (intptr_t)static_cast<PropertySlot::GetValueFunc>(jsTestOverloadedConstructorsWithSequenceConstructor), (intptr_t) static_cast<PutPropertySlot::PutValueFunc>(setJSTestOverloadedConstructorsWithSequenceConstructor) } },
};

const ClassInfo JSTestOverloadedConstructorsWithSequencePrototype::s_info = { "TestOverloadedConstructorsWithSequencePrototype", &Base::s_info, 0, CREATE_METHOD_TABLE(JSTestOverloadedConstructorsWithSequencePrototype) };

void JSTestOverloadedConstructorsWithSequencePrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    reifyStaticProperties(vm, JSTestOverloadedConstructorsWithSequencePrototypeTableValues, *this);
}

const ClassInfo JSTestOverloadedConstructorsWithSequence::s_info = { "TestOverloadedConstructorsWithSequence", &Base::s_info, 0, CREATE_METHOD_TABLE(JSTestOverloadedConstructorsWithSequence) };

JSTestOverloadedConstructorsWithSequence::JSTestOverloadedConstructorsWithSequence(Structure* structure, JSDOMGlobalObject& globalObject, Ref<TestOverloadedConstructorsWithSequence>&& impl)
    : JSDOMWrapper<TestOverloadedConstructorsWithSequence>(structure, globalObject, WTFMove(impl))
{
}

JSObject* JSTestOverloadedConstructorsWithSequence::createPrototype(VM& vm, JSGlobalObject* globalObject)
{
    return JSTestOverloadedConstructorsWithSequencePrototype::create(vm, globalObject, JSTestOverloadedConstructorsWithSequencePrototype::createStructure(vm, globalObject, globalObject->objectPrototype()));
}

JSObject* JSTestOverloadedConstructorsWithSequence::prototype(VM& vm, JSGlobalObject* globalObject)
{
    return getDOMPrototype<JSTestOverloadedConstructorsWithSequence>(vm, globalObject);
}

void JSTestOverloadedConstructorsWithSequence::destroy(JSC::JSCell* cell)
{
    JSTestOverloadedConstructorsWithSequence* thisObject = static_cast<JSTestOverloadedConstructorsWithSequence*>(cell);
    thisObject->JSTestOverloadedConstructorsWithSequence::~JSTestOverloadedConstructorsWithSequence();
}

EncodedJSValue jsTestOverloadedConstructorsWithSequenceConstructor(ExecState* state, EncodedJSValue thisValue, PropertyName)
{
    VM& vm = state->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    JSTestOverloadedConstructorsWithSequencePrototype* domObject = jsDynamicCast<JSTestOverloadedConstructorsWithSequencePrototype*>(JSValue::decode(thisValue));
    if (UNLIKELY(!domObject))
        return throwVMTypeError(state, throwScope);
    return JSValue::encode(JSTestOverloadedConstructorsWithSequence::getConstructor(state->vm(), domObject->globalObject()));
}

bool setJSTestOverloadedConstructorsWithSequenceConstructor(ExecState* state, EncodedJSValue thisValue, EncodedJSValue encodedValue)
{
    VM& vm = state->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    JSValue value = JSValue::decode(encodedValue);
    JSTestOverloadedConstructorsWithSequencePrototype* domObject = jsDynamicCast<JSTestOverloadedConstructorsWithSequencePrototype*>(JSValue::decode(thisValue));
    if (UNLIKELY(!domObject)) {
        throwVMTypeError(state, throwScope);
        return false;
    }
    // Shadowing a built-in constructor
    return domObject->putDirect(state->vm(), state->propertyNames().constructor, value);
}

JSValue JSTestOverloadedConstructorsWithSequence::getConstructor(VM& vm, const JSGlobalObject* globalObject)
{
    return getDOMConstructor<JSTestOverloadedConstructorsWithSequenceConstructor>(vm, *jsCast<const JSDOMGlobalObject*>(globalObject));
}

bool JSTestOverloadedConstructorsWithSequenceOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, SlotVisitor& visitor)
{
    UNUSED_PARAM(handle);
    UNUSED_PARAM(visitor);
    return false;
}

void JSTestOverloadedConstructorsWithSequenceOwner::finalize(JSC::Handle<JSC::Unknown> handle, void* context)
{
    auto* jsTestOverloadedConstructorsWithSequence = jsCast<JSTestOverloadedConstructorsWithSequence*>(handle.slot()->asCell());
    auto& world = *static_cast<DOMWrapperWorld*>(context);
    uncacheWrapper(world, &jsTestOverloadedConstructorsWithSequence->wrapped(), jsTestOverloadedConstructorsWithSequence);
}

#if ENABLE(BINDING_INTEGRITY)
#if PLATFORM(WIN)
#pragma warning(disable: 4483)
extern "C" { extern void (*const __identifier("??_7TestOverloadedConstructorsWithSequence@WebCore@@6B@")[])(); }
#else
extern "C" { extern void* _ZTVN7WebCore38TestOverloadedConstructorsWithSequenceE[]; }
#endif
#endif

JSC::JSValue toJSNewlyCreated(JSC::ExecState*, JSDOMGlobalObject* globalObject, Ref<TestOverloadedConstructorsWithSequence>&& impl)
{

#if ENABLE(BINDING_INTEGRITY)
    void* actualVTablePointer = *(reinterpret_cast<void**>(impl.ptr()));
#if PLATFORM(WIN)
    void* expectedVTablePointer = reinterpret_cast<void*>(__identifier("??_7TestOverloadedConstructorsWithSequence@WebCore@@6B@"));
#else
    void* expectedVTablePointer = &_ZTVN7WebCore38TestOverloadedConstructorsWithSequenceE[2];
#if COMPILER(CLANG)
    // If this fails TestOverloadedConstructorsWithSequence does not have a vtable, so you need to add the
    // ImplementationLacksVTable attribute to the interface definition
    static_assert(__is_polymorphic(TestOverloadedConstructorsWithSequence), "TestOverloadedConstructorsWithSequence is not polymorphic");
#endif
#endif
    // If you hit this assertion you either have a use after free bug, or
    // TestOverloadedConstructorsWithSequence has subclasses. If TestOverloadedConstructorsWithSequence has subclasses that get passed
    // to toJS() we currently require TestOverloadedConstructorsWithSequence you to opt out of binding hardening
    // by adding the SkipVTableValidation attribute to the interface IDL definition
    RELEASE_ASSERT(actualVTablePointer == expectedVTablePointer);
#endif
    return createWrapper<TestOverloadedConstructorsWithSequence>(globalObject, WTFMove(impl));
}

JSC::JSValue toJS(JSC::ExecState* state, JSDOMGlobalObject* globalObject, TestOverloadedConstructorsWithSequence& impl)
{
    return wrap(state, globalObject, impl);
}

TestOverloadedConstructorsWithSequence* JSTestOverloadedConstructorsWithSequence::toWrapped(JSC::JSValue value)
{
    if (auto* wrapper = jsDynamicCast<JSTestOverloadedConstructorsWithSequence*>(value))
        return &wrapper->wrapped();
    return nullptr;
}

}
