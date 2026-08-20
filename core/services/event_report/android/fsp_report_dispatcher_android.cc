// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <jni.h>

#include <memory>

#include "core/base/android/android_jni.h"
#include "core/services/event_report/event_tracker_platform_impl.h"
#include "platform/android/lynx_android/src/main/jni/gen/FSPReportDispatcher_jni.h"
#include "platform/android/lynx_android/src/main/jni/gen/FSPReportDispatcher_register_jni.h"

namespace {

class FSPReportDispatcherAndroid {
 public:
  FSPReportDispatcherAndroid(JNIEnv* env, jobject dispatcher)
      : dispatcher_(env, dispatcher) {}

  void DispatchOne() const {
    if (dispatcher_.IsNull()) {
      return;
    }
    JNIEnv* env = lynx::base::android::AttachCurrentThread();
    Java_FSPReportDispatcher_dispatchOne(env, dispatcher_.Get());
  }

 private:
  lynx::base::android::ScopedGlobalJavaRef<jobject> dispatcher_;
};

using FSPReportDispatcherHandle = std::shared_ptr<FSPReportDispatcherAndroid>;

}  // namespace

jlong Create(JNIEnv* env, jclass, jobject dispatcher) {
  if (dispatcher == nullptr) {
    return 0;
  }
  auto native_dispatcher =
      std::make_shared<FSPReportDispatcherAndroid>(env, dispatcher);
  return reinterpret_cast<jlong>(
      new FSPReportDispatcherHandle(std::move(native_dispatcher)));
}

jboolean Post(JNIEnv*, jclass, jlong native_ptr) {
  if (native_ptr == 0) {
    return JNI_FALSE;
  }
  auto task_runner =
      lynx::tasm::report::EventTrackerPlatformImpl::GetReportTaskRunner();
  if (!task_runner) {
    return JNI_FALSE;
  }
  auto dispatcher = *reinterpret_cast<FSPReportDispatcherHandle*>(native_ptr);
  task_runner->PostTask(
      [dispatcher = std::move(dispatcher)]() { dispatcher->DispatchOne(); });
  return JNI_TRUE;
}

void Destroy(JNIEnv*, jclass, jlong native_ptr) {
  delete reinterpret_cast<FSPReportDispatcherHandle*>(native_ptr);
}

namespace lynx {
namespace jni {

bool RegisterJNIForFSPReportDispatcher(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace jni
}  // namespace lynx
