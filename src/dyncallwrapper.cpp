/*
Copyright 2016 Gábor Mező (gabor.mezo@outlook.com)

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "dyncallwrapper.h"
#include "dcarg.h"
#include "dccall.h"
#include "deps.h"
#include "helpers.h"
#include "int64.h"
#include "getv8value.h"
#include <dyncall.h>
#include "defs.h"

using namespace std;
using namespace v8;
using namespace node;
using namespace fastcall;

namespace {

DCCallVM* vm = nullptr;
static v8::Local<v8::Value> workerArgs[2] = { Nan::Null(), Nan::Null() };
uv_loop_t* defLoop = nullptr;

template <typename T>
struct CallAsyncWorker {
    typedef T (*TCallFunc)(DCCallVM*, DCpointer);
    typedef v8::Local<v8::Value>(*TConvertFunc)(T);

    CallAsyncWorker(
        Nan::Global<v8::Function>&& callback,
        DCCallVM* vm,
        DCpointer funcPtr,
        TCallFunc callFunc,
        TConvertFunc convertFunc)
        : callback(std::move(callback))
        , vm(vm)
        , funcPtr(funcPtr)
        , callFunc(callFunc)
        , convertFunc(convertFunc)
    {
        work.data = this;
    }

    ~CallAsyncWorker()
    {
        dcFree(vm);
    }

    void Start()
    {
        int r = uv_queue_work(defLoop, &work, Call, Finished);
        assert(!r);
    }

private:
    Nan::Global<v8::Function> callback;
    DCCallVM* vm;
    DCpointer funcPtr;
    TCallFunc callFunc;
    TConvertFunc convertFunc;
    T result;
    uv_work_t work;

    static void Call(uv_work_t* req)
    {
        auto self = (CallAsyncWorker<T>*)req->data;
        self->Execute();
    }

    static void Finished(uv_work_t* req, int status)
    {
        Nan::HandleScope scope;

        auto self = (CallAsyncWorker<T>*)req->data;
        self->HandleOKCallback();
        delete self;
    }

    void Execute()
    {
        result = callFunc(vm, funcPtr);
    }

    void HandleOKCallback()
    {
        workerArgs[1] = convertFunc(result);
        auto cb = Nan::New(callback);
        Nan::Call(cb, cb->CreationContext()->Global(), 2, workerArgs);
    }
};

template <typename T>
inline CallAsyncWorker<T>* MakeCallAsyncWorker(
    Nan::Global<v8::Function>&& callback,
    DCCallVM* vm,
    DCpointer funcPtr,
    typename CallAsyncWorker<T>::TCallFunc callFunc,
    typename CallAsyncWorker<T>::TConvertFunc convertFunc)
{
    return new CallAsyncWorker<T>(
        std::move(callback),
        vm,
        funcPtr,
        callFunc,
        convertFunc);
}

template <typename T>
inline void CallAsync(
    const Nan::FunctionCallbackInfo<v8::Value>& info,
    typename CallAsyncWorker<T>::TCallFunc callFunc,
    typename CallAsyncWorker<T>::TConvertFunc convertFunc)
{
    auto worker = MakeCallAsyncWorker<T>(
        Nan::Global<v8::Function>(info[2].As<v8::Function>()),
        Unwrap<DCCallVM>(info[0]),
        UnwrapPointer(info[1]),
        callFunc,
        convertFunc);

    worker->Start();
}

NAN_METHOD(newCallVM)
{
    unsigned size = info[0]->Uint32Value();
    auto ptr = dcNewCallVM(size);
    info.GetReturnValue().Set(WrapPointer(ptr));
}

NAN_METHOD(free)
{
    dcFree(Unwrap<DCCallVM>(info[0]));
}

NAN_METHOD(setVM)
{
    vm = Unwrap<DCCallVM>(info[0]);
}

NAN_METHOD(setVMAndReset)
{
    vm = Unwrap<DCCallVM>(info[0]);
    dcReset(vm);
}

NAN_METHOD(mode)
{
    if (vm) {
        dcMode(vm, info[0]->Int32Value());
    }
}

NAN_METHOD(reset)
{
    if (vm) {
        dcReset(vm);
    }
}

NAN_METHOD(argBool)
{
    dcArgBoolean(vm, info[0]->BooleanValue());
}

NAN_METHOD(argChar)
{
    dcArgChar(vm, info[0]->Int32Value());
}

NAN_METHOD(argShort)
{
    dcArgShort(vm, info[0]->Int32Value());
}

NAN_METHOD(argInt)
{
    dcArgInt(vm, info[0]->Int32Value());
}

NAN_METHOD(argLong)
{
    dcArgLong(vm, GetInt64(info[0]));
}

NAN_METHOD(argLongLong)
{
    dcArgLongLong(vm, GetInt64(info[0]));
}

NAN_METHOD(argFloat)
{
    dcArgFloat(vm, info[0]->NumberValue());
}

NAN_METHOD(argDouble)
{
    dcArgDouble(vm, info[0]->NumberValue());
}

NAN_METHOD(argPointer)
{
    try {
        void* ptr = GetPointer(info[0]);
        dcArgPointer(vm, ptr);
    }
    catch (exception& ex) {
        Nan::ThrowTypeError(ex.what());
    }
}

NAN_METHOD(argInt8)
{
    dcArgInt8(vm, GetInt8(info[0]));
}

NAN_METHOD(argUInt8)
{
    dcArgUInt8(vm, GetUInt8(info[0]));
}

NAN_METHOD(argInt16)
{
    dcArgInt16(vm, GetInt16(info[0]));
}

NAN_METHOD(argUInt16)
{
    dcArgUInt16(vm, GetUInt8(info[0]));
}

NAN_METHOD(argInt32)
{
    dcArgInt32(vm, GetInt32(info[0]));
}

NAN_METHOD(argUInt32)
{
    dcArgUInt32(vm, GetUInt32(info[0]));
}

NAN_METHOD(argInt64)
{
    dcArgInt64(vm, GetInt64(info[0]));
}

NAN_METHOD(argUInt64)
{
    dcArgUInt64(vm, GetUint64(info[0]));
}

NAN_METHOD(argByte)
{
    dcArgByte(vm, GetByte(info[0]));
}

NAN_METHOD(argUChar)
{
    dcArgUChar(vm, GetUChar(info[0]));
}

NAN_METHOD(argUShort)
{
    dcArgUShort(vm, GetUShort(info[0]));
}

NAN_METHOD(argUInt)
{
    dcArgUInt(vm, GetUInt(info[0]));
}

NAN_METHOD(argULong)
{
    dcArgULong(vm, GetULong(info[0]));
}

NAN_METHOD(argULongLong)
{
    dcArgULongLong(vm, GetULongLong(info[0]));
}

NAN_METHOD(argSizeT)
{
    dcArgSizeT(vm, GetUint64(info[0]));
}

NAN_METHOD(callVoid)
{
    dcCallVoid(vm, UnwrapPointer(info[0]));
}

NAN_METHOD(callVoidAsync)
{
    CallAsync<int>(
        info,
        [](DCCallVM* vm, DCpointer funcPtr) {
            dcCallVoid(vm, funcPtr);
            return 0;
        },
        [](int) {
            return Local<Value>(Nan::Undefined());
        });
}

NAN_METHOD(callBool)
{
    auto result = dcCallBoolean(vm, UnwrapPointer(info[0]));
    info.GetReturnValue().Set(Nan::New(result));
}

NAN_METHOD(callBoolAsync)
{
    CallAsync<bool>(
        info,
        [](DCCallVM* vm, DCpointer funcPtr) {
            return dcCallBoolean(vm, funcPtr);
        },
        [](bool value) {
            return Local<Value>(Nan::New(value));
        });
}

NAN_METHOD(callChar)
{
    auto result = dcCallChar(vm, UnwrapPointer(info[0]));
    info.GetReturnValue().Set(Nan::New(result));
}

NAN_METHOD(callCharAsync)
{
    CallAsync<char>(
        info,
        dcCallChar,
        [](char value) {
            return Local<Value>(Nan::New(value));
        });
}

NAN_METHOD(callShort)
{
    auto result = dcCallShort(vm, UnwrapPointer(info[0]));
    info.GetReturnValue().Set(Nan::New(result));
}

NAN_METHOD(callShortAsync)
{
    CallAsync<short>(
        info,
        dcCallShort,
        [](short value) {
            return Local<Value>(Nan::New(value));
        });
}

NAN_METHOD(callInt)
{
    auto result = dcCallInt(vm, UnwrapPointer(info[0]));
    info.GetReturnValue().Set(Nan::New(result));
}

NAN_METHOD(callIntAsync)
{
    CallAsync<int>(
        info,
        dcCallInt,
        [](int value) {
            return Local<Value>(Nan::New(value));
        });
}

NAN_METHOD(callLong)
{
    auto result = dcCallLong(vm, UnwrapPointer(info[0]));
    info.GetReturnValue().Set(MakeInt64(result));
}

NAN_METHOD(callLongAsync)
{
    CallAsync<long>(
        info,
        dcCallLong,
        [](long value) {
            return MakeInt64(value);
        });
}

NAN_METHOD(callLongLong)
{
    auto result = dcCallLongLong(vm, UnwrapPointer(info[0]));
    info.GetReturnValue().Set(MakeInt64(result));
}

NAN_METHOD(callLongLongAsync)
{
    CallAsync<long long>(
        info,
        dcCallLongLong,
        [](long long value) {
            return MakeInt64(value);
        });
}

NAN_METHOD(callFloat)
{
    auto result = dcCallFloat(vm, UnwrapPointer(info[0]));
    info.GetReturnValue().Set(Nan::New(result));
}

NAN_METHOD(callFloatAsync)
{
    CallAsync<float>(
        info,
        dcCallFloat,
        [](float value) {
            return Local<Value>(Nan::New(value));
        });
}

NAN_METHOD(callDouble)
{
    auto result = dcCallDouble(vm, UnwrapPointer(info[0]));
    info.GetReturnValue().Set(Nan::New(result));
}

NAN_METHOD(callDoubleAsync)
{
    CallAsync<double>(
        info,
        dcCallDouble,
        [](double value) {
            return Local<Value>(Nan::New(value));
        });
}

NAN_METHOD(callPointer)
{
    auto result = dcCallPointer(vm, UnwrapPointer(info[0]));
    info.GetReturnValue().Set(WrapPointer(result));
}

NAN_METHOD(callPointerAsync)
{
    CallAsync<void*>(
        info,
        dcCallPointer,
        [](void* value) {
            return Local<Value>(WrapPointer(value));
        });
}

NAN_METHOD(callInt8)
{
    auto result = dcCallInt8(vm, UnwrapPointer(info[0]));
    info.GetReturnValue().Set(Nan::New(result));
}

NAN_METHOD(callInt8Async)
{
    CallAsync<char>(
        info,
        dcCallInt8,
        [](char value) {
            return Local<Value>(Nan::New(value));
        });
}

NAN_METHOD(callInt16)
{
    auto result = dcCallInt16(vm, UnwrapPointer(info[0]));
    info.GetReturnValue().Set(Nan::New(result));
}

NAN_METHOD(callInt16Async)
{
    CallAsync<short>(
        info,
        dcCallInt16,
        [](short value) {
            return Local<Value>(Nan::New(value));
        });
}

NAN_METHOD(callInt32)
{
    auto result = dcCallInt32(vm, UnwrapPointer(info[0]));
    info.GetReturnValue().Set(Nan::New(result));
}

NAN_METHOD(callInt32Async)
{
    CallAsync<int>(
        info,
        dcCallInt32,
        [](int value) {
            return Local<Value>(Nan::New(value));
        });
}

NAN_METHOD(callInt64)
{
    auto result = dcCallInt64(vm, UnwrapPointer(info[0]));
    info.GetReturnValue().Set(MakeInt64(result));
}

NAN_METHOD(callInt64Async)
{
    CallAsync<long long>(
        info,
        dcCallInt64,
        [](long long value) {
            return Local<Value>(MakeInt64(value));
        });
}

NAN_METHOD(callUInt8)
{
    auto result = dcCallUInt8(vm, UnwrapPointer(info[0]));
    info.GetReturnValue().Set(Nan::New(result));
}

NAN_METHOD(callUInt8Async)
{
    CallAsync<uint8_t>(
        info,
        dcCallUInt8,
        [](uint8_t value) {
            return Local<Value>(Nan::New(value));
        });
}

NAN_METHOD(callUInt16)
{
    auto result = dcCallUInt16(vm, UnwrapPointer(info[0]));
    info.GetReturnValue().Set(Nan::New(result));
}

NAN_METHOD(callUInt16Async)
{
    CallAsync<uint16_t>(
        info,
        dcCallUInt16,
        [](uint16_t value) {
            return Local<Value>(Nan::New(value));
        });
}

NAN_METHOD(callUInt32)
{
    auto result = dcCallUInt32(vm, UnwrapPointer(info[0]));
    info.GetReturnValue().Set(Nan::New(result));
}

NAN_METHOD(callUInt32Async)
{
    CallAsync<uint32_t>(
        info,
        dcCallUInt32,
        [](uint32_t value) {
            return Local<Value>(Nan::New(value));
        });
}

NAN_METHOD(callUInt64)
{
    auto result = dcCallUInt64(vm, UnwrapPointer(info[0]));
    info.GetReturnValue().Set(MakeUint64(result));
}

NAN_METHOD(callUInt64Async)
{
    CallAsync<uint64_t>(
        info,
        dcCallUInt64,
        [](uint64_t value) {
            return MakeUint64(value);
        });
}

NAN_METHOD(callByte)
{
    auto result = dcCallByte(vm, UnwrapPointer(info[0]));
    info.GetReturnValue().Set(Nan::New(result));
}

NAN_METHOD(callByteAsync)
{
    CallAsync<uint8_t>(
        info,
        dcCallByte,
        [](uint8_t value) {
            return Local<Value>(Nan::New(value));
        });
}

NAN_METHOD(callUChar)
{
    auto result = dcCallUChar(vm, UnwrapPointer(info[0]));
    info.GetReturnValue().Set(Nan::New(result));
}

NAN_METHOD(callUCharAsync)
{
    CallAsync<unsigned char>(
        info,
        dcCallUChar,
        [](unsigned char value) {
            return Local<Value>(Nan::New(value));
        });
}

NAN_METHOD(callUShort)
{
    auto result = dcCallUShort(vm, UnwrapPointer(info[0]));
    info.GetReturnValue().Set(Nan::New(result));
}

NAN_METHOD(callUShortAsync)
{
    CallAsync<unsigned short>(
        info,
        dcCallUShort,
        [](unsigned short value) {
            return Local<Value>(Nan::New(value));
        });
}

NAN_METHOD(callUInt)
{
    auto result = dcCallUInt(vm, UnwrapPointer(info[0]));
    info.GetReturnValue().Set(Nan::New(result));
}

NAN_METHOD(callUIntAsync)
{
    CallAsync<unsigned int>(
        info,
        dcCallUInt,
        [](unsigned int value) {
            return Local<Value>(Nan::New(value));
        });
}

NAN_METHOD(callULong)
{
    auto result = dcCallULong(vm, UnwrapPointer(info[0]));
    info.GetReturnValue().Set(MakeUint64(result));
}

NAN_METHOD(callULongAsync)
{
    CallAsync<unsigned long>(
        info,
        dcCallULong,
        [](unsigned long value) {
            return MakeUint64(value);
        });
}

NAN_METHOD(callULongLong)
{
    auto result = dcCallULongLong(vm, UnwrapPointer(info[0]));
    info.GetReturnValue().Set(MakeUint64(result));
}

NAN_METHOD(callULongLongAsync)
{
    CallAsync<unsigned long long>(
        info,
        dcCallULongLong,
        [](unsigned long long value) {
            return MakeUint64(value);
        });
}

NAN_METHOD(callSizeT)
{
    auto result = dcCallSizeT(vm, UnwrapPointer(info[0]));
    info.GetReturnValue().Set(MakeUint64(result));
}

NAN_METHOD(callSizeTAsync)
{
    CallAsync<size_t>(
        info,
        dcCallSizeT,
        [](size_t value) {
            return MakeUint64(value);
        });
}
}

NAN_MODULE_INIT(fastcall::InitDyncallWrapper)
{
    defLoop = uv_default_loop();

    auto dyncall = Nan::New<Object>();
    Nan::Set(target, Nan::New<String>("dyncall").ToLocalChecked(), dyncall);
    Nan::Set(dyncall, Nan::New<String>("newCallVM").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(newCallVM)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("free").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(free)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("mode").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(mode)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("reset").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(reset)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("setVM").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(setVM)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("setVMAndReset").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(setVMAndReset)).ToLocalChecked());

    Nan::Set(dyncall, Nan::New<String>("argBool").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(argBool)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("argChar").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(argChar)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("argShort").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(argShort)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("argInt").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(argInt)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("argLong").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(argLong)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("argLongLong").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(argLongLong)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("argFloat").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(argFloat)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("argDouble").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(argDouble)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("argPointer").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(argPointer)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("argInt8").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(argInt8)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("argUInt8").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(argUInt8)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("argInt16").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(argInt16)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("argUInt16").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(argUInt16)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("argInt32").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(argInt32)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("argUInt32").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(argUInt32)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("argInt64").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(argInt64)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("argUInt64").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(argUInt64)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("argByte").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(argByte)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("argUChar").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(argUChar)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("argUShort").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(argUShort)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("argUInt").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(argUInt)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("argULong").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(argULong)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("argULongLong").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(argULongLong)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("argSizeT").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(argSizeT)).ToLocalChecked());

    Nan::Set(dyncall, Nan::New<String>("callVoid").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callVoid)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callBool").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callBool)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callChar").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callChar)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callShort").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callShort)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callInt").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callInt)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callLong").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callLong)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callLongLong").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callLongLong)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callFloat").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callFloat)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callDouble").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callDouble)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callPointer").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callPointer)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callInt8").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callInt8)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callInt16").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callInt16)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callInt32").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callInt32)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callInt64").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callInt64)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callUInt8").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callUInt8)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callUInt16").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callUInt16)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callUInt32").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callUInt32)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callUInt64").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callUInt64)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callByte").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callByte)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callUChar").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callUChar)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callUShort").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callUShort)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callUInt").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callUInt)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callULong").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callULong)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callULongLong").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callULongLong)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callSizeT").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callSizeT)).ToLocalChecked());

    Nan::Set(dyncall, Nan::New<String>("callVoidAsync").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callVoidAsync)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callBoolAsync").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callBoolAsync)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callCharAsync").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callCharAsync)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callShortAsync").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callShortAsync)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callIntAsync").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callIntAsync)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callLongAsync").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callLongAsync)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callLongLongAsync").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callLongLongAsync)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callFloatAsync").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callFloatAsync)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callDoubleAsync").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callDoubleAsync)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callPointerAsync").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callPointerAsync)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callInt8Async").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callInt8Async)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callInt16Async").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callInt16Async)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callInt32Async").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callInt32Async)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callInt64Async").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callInt64Async)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callUInt8Async").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callUInt8Async)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callUInt16Async").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callUInt16Async)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callUInt32Async").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callUInt32Async)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callUInt64Async").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callUInt64Async)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callByteAsync").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callByteAsync)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callUCharAsync").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callUCharAsync)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callUShortAsync").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callUShortAsync)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callUIntAsync").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callUIntAsync)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callULongAsync").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callULongAsync)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callULongLongAsync").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callULongLongAsync)).ToLocalChecked());
    Nan::Set(dyncall, Nan::New<String>("callSizeTAsync").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(callSizeTAsync)).ToLocalChecked());
}
