/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Hello.h"
#include "mozilla/dom/HelloBinding.h"

#include <cstdlib> // For rand() and srand()
#include <ctime>

namespace mozilla {
namespace dom {


// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(Hello)
NS_IMPL_CYCLE_COLLECTING_ADDREF(Hello)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Hello)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Hello)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Hello::Hello(nsISupports* aParent) : mParent(aParent)
{
    // Add |MOZ_COUNT_CTOR(Hello);| for a non-refcounted object.
}

Hello::Hello(nsISupports* aParent, const nsAString& aStr) : mParent(aParent), hStr(aStr)
{
    // Add |MOZ_COUNT_CTOR(Hello);| for a non-refcounted object.
}

Hello::~Hello()
{
    // Add |MOZ_COUNT_DTOR(Hello);| for a non-refcounted object.
}

JSObject*
Hello::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HelloBinding::Wrap(aCx, this, aGivenProto);
}

void
Hello::GetHelloStr(nsString& aRetVal) const
{
  aRetVal = hStr;
}

void
Hello::SetHelloStr(const nsAString& arg)
{
  hStr.Assign(arg);
}

void
Hello::GetReadonlyStr(nsString& aRetVal) const
{
  aRetVal = nsString(NS_LITERAL_STRING("Readonly String!"));
}

void
Hello::SayHello(nsString& aRetVal)
{
  aRetVal = nsString(NS_LITERAL_STRING("Hello!"));
}

int32_t
Hello::RandNum()
{
  static bool init = false;
  if (!init) {
    std::srand(std::time(0));
    init = true;
  }
  return std::rand() % 100;
}

int32_t
Hello::Add(int32_t a, int32_t b) {
  return a + b;
}

int32_t
Hello::Sub(int32_t a, int32_t b) {
  return a - b;
}

int32_t
Hello::Mul(int32_t a, int32_t b) {
  return a * b;
}

double
Hello::Div(int32_t a, int32_t b, ErrorResult& aRv) {
  if (!b) {
    aRv.Throw(NS_ERROR_ILLEGAL_VALUE);
  }
  return (double) a/b;
}

already_AddRefed<Hello>
Hello::Constructor(const GlobalObject& global, ErrorResult& aRv)
{
  RefPtr<Hello> obj = new Hello(global.GetAsSupports());
  return obj.forget();
}

already_AddRefed<Hello>
Hello::Constructor(const GlobalObject& global, const nsAString& str, ErrorResult& aRv)
{
  RefPtr<Hello> obj = new Hello(global.GetAsSupports(), str);
  return obj.forget();
}

} // namespace dom
} // namespace mozilla
