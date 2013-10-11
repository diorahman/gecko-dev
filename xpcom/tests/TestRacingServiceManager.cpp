/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"

#include "nsIFactory.h"
#include "mozilla/Module.h"
#include "nsXULAppAPI.h"
#include "nsIThread.h"
#include "nsIComponentRegistrar.h"

#include "nsAutoPtr.h"
#include "nsThreadUtils.h"
#include "nsXPCOMCIDInternal.h"
#include "pratom.h"
#include "prmon.h"
#include "mozilla/Attributes.h"

#include "mozilla/ReentrantMonitor.h"
using namespace mozilla;

#ifdef DEBUG
#define TEST_ASSERTION(_test, _msg) \
    NS_ASSERTION(_test, _msg);
#else
#define TEST_ASSERTION(_test, _msg) \
  PR_BEGIN_MACRO \
    if (!(_test)) { \
      NS_DebugBreak(NS_DEBUG_ABORT, _msg, #_test, __FILE__, __LINE__); \
    } \
  PR_END_MACRO
#endif

/* f93f6bdc-88af-42d7-9d64-1b43c649a3e5 */ 
#define FACTORY_CID1                                 \
{                                                    \
  0xf93f6bdc,                                        \
  0x88af,                                            \
  0x42d7,                                            \
  { 0x9d, 0x64, 0x1b, 0x43, 0xc6, 0x49, 0xa3, 0xe5 } \
}
NS_DEFINE_CID(kFactoryCID1, FACTORY_CID1);

/* ef38ad65-6595-49f0-8048-e819f81d15e2 */
#define FACTORY_CID2                                 \
{                                                    \
  0xef38ad65,                                        \
  0x6595,                                            \
  0x49f0,                                            \
  { 0x80, 0x48, 0xe8, 0x19, 0xf8, 0x1d, 0x15, 0xe2 } \
}
NS_DEFINE_CID(kFactoryCID2, FACTORY_CID2);

#define FACTORY_CONTRACTID                           \
  "TestRacingThreadManager/factory;1"

int32_t gComponent1Count = 0;
int32_t gComponent2Count = 0;

ReentrantMonitor* gReentrantMonitor = nullptr;

bool gCreateInstanceCalled = false;
bool gMainThreadWaiting = false;

class AutoCreateAndDestroyReentrantMonitor
{
public:
  AutoCreateAndDestroyReentrantMonitor(ReentrantMonitor** aReentrantMonitorPtr)
  : mReentrantMonitorPtr(aReentrantMonitorPtr) {
    *aReentrantMonitorPtr =
      new ReentrantMonitor("TestRacingServiceManager::AutoMon");
    TEST_ASSERTION(*aReentrantMonitorPtr, "Out of memory!");
  }

  ~AutoCreateAndDestroyReentrantMonitor() {
    if (*mReentrantMonitorPtr) {
      delete *mReentrantMonitorPtr;
      *mReentrantMonitorPtr = nullptr;
    }
  }

private:
  ReentrantMonitor** mReentrantMonitorPtr;
};

class Factory MOZ_FINAL : public nsIFactory
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  Factory() : mFirstComponentCreated(false) { }

  NS_IMETHOD CreateInstance(nsISupports* aDelegate,
                            const nsIID& aIID,
                            void** aResult);

  NS_IMETHOD LockFactory(bool aLock) {
    return NS_OK;
  }

  bool mFirstComponentCreated;
};

NS_IMPL_ISUPPORTS1(Factory, nsIFactory)

class Component1 MOZ_FINAL : public nsISupports
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  Component1() {
    // This is the real test - make sure that only one instance is ever created.
    int32_t count = PR_AtomicIncrement(&gComponent1Count);
    TEST_ASSERTION(count == 1, "Too many components created!");
  }
};

NS_IMPL_ADDREF(Component1)
NS_IMPL_RELEASE(Component1)

NS_INTERFACE_MAP_BEGIN(Component1)
  NS_INTERFACE_MAP_ENTRY(Component1)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

class Component2 MOZ_FINAL : public nsISupports
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  Component2() {
    // This is the real test - make sure that only one instance is ever created.
    int32_t count = PR_AtomicIncrement(&gComponent2Count);
    TEST_ASSERTION(count == 1, "Too many components created!");
  }
};

NS_IMPL_ADDREF(Component2)
NS_IMPL_RELEASE(Component2)

NS_INTERFACE_MAP_BEGIN(Component2)
  NS_INTERFACE_MAP_ENTRY(Component2)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
Factory::CreateInstance(nsISupports* aDelegate,
                        const nsIID& aIID,
                        void** aResult)
{
  // Make sure that the second thread beat the main thread to the getService
  // call.
  TEST_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  {
    ReentrantMonitorAutoEnter mon(*gReentrantMonitor);

    gCreateInstanceCalled = true;
    mon.Notify();

    mon.Wait(PR_MillisecondsToInterval(3000));
  }

  NS_ENSURE_FALSE(aDelegate, NS_ERROR_NO_AGGREGATION);
  NS_ENSURE_ARG_POINTER(aResult);

  nsCOMPtr<nsISupports> instance;

  if (!mFirstComponentCreated) {
    instance = new Component1();
  }
  else {
    instance = new Component2();
  }
  NS_ENSURE_TRUE(instance, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = instance->QueryInterface(aIID, aResult);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

class Runnable : public nsRunnable
{
public:
  NS_DECL_NSIRUNNABLE

  Runnable() : mFirstRunnableDone(false) { }

  bool mFirstRunnableDone;
};

NS_IMETHODIMP
Runnable::Run()
{
  {
    ReentrantMonitorAutoEnter mon(*gReentrantMonitor);

    while (!gMainThreadWaiting) {
      mon.Wait();
    }
  }

  nsresult rv;
  nsCOMPtr<nsISupports> component;

  if (!mFirstRunnableDone) {
    component = do_GetService(kFactoryCID1, &rv);
  }
  else {
    component = do_GetService(FACTORY_CONTRACTID, &rv);
  }
  TEST_ASSERTION(NS_SUCCEEDED(rv), "GetService failed!");

  return NS_OK;
}

static Factory* gFactory;

static already_AddRefed<nsIFactory>
CreateFactory(const mozilla::Module& module, const mozilla::Module::CIDEntry& entry)
{
    if (!gFactory) {
        gFactory = new Factory();
        NS_ADDREF(gFactory);
    }
    nsCOMPtr<nsIFactory> ret = gFactory;
    return ret.forget();
}

static const mozilla::Module::CIDEntry kLocalCIDs[] = {
    { &kFactoryCID1, false, CreateFactory, nullptr },
    { &kFactoryCID2, false, CreateFactory, nullptr },
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kLocalContracts[] = {
    { FACTORY_CONTRACTID, &kFactoryCID2 },
    { nullptr }
};

static const mozilla::Module kLocalModule = {
    mozilla::Module::kVersion,
    kLocalCIDs,
    kLocalContracts
};

int main(int argc, char** argv)
{
  nsresult rv;
  XRE_AddStaticComponent(&kLocalModule);

  ScopedXPCOM xpcom("RacingServiceManager");
  NS_ENSURE_FALSE(xpcom.failed(), 1);

  AutoCreateAndDestroyReentrantMonitor mon(&gReentrantMonitor);

  nsRefPtr<Runnable> runnable = new Runnable();
  NS_ENSURE_TRUE(runnable, 1);

  // Run the classID test
  nsCOMPtr<nsIThread> newThread;
  rv = NS_NewThread(getter_AddRefs(newThread), runnable);
  NS_ENSURE_SUCCESS(rv, 1);

  {
    ReentrantMonitorAutoEnter mon(*gReentrantMonitor);

    gMainThreadWaiting = true;
    mon.Notify();

    while (!gCreateInstanceCalled) {
      mon.Wait();
    }
  }

  nsCOMPtr<nsISupports> component(do_GetService(kFactoryCID1, &rv));
  NS_ENSURE_SUCCESS(rv, 1);

  // Reset for the contractID test
  gMainThreadWaiting = gCreateInstanceCalled = false;
  gFactory->mFirstComponentCreated = runnable->mFirstRunnableDone = true;
  component = nullptr;

  rv = newThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, 1);

  {
    ReentrantMonitorAutoEnter mon(*gReentrantMonitor);

    gMainThreadWaiting = true;
    mon.Notify();

    while (!gCreateInstanceCalled) {
      mon.Wait();
    }
  }

  component = do_GetService(FACTORY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, 1);

  NS_RELEASE(gFactory);

  return 0;
}
