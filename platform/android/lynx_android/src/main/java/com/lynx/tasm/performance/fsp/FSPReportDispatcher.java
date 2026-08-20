// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

package com.lynx.tasm.performance.fsp;

import androidx.annotation.Keep;
import com.lynx.tasm.base.CalledByNative;
import java.lang.ref.WeakReference;
import java.util.concurrent.ConcurrentLinkedQueue;

/**
 * Posts FSP snapshots to the report thread without creating a global JNI reference per snapshot.
 */
@Keep
final class FSPReportDispatcher {
  abstract static class NativeDelegate {
    abstract long create(FSPReportDispatcher dispatcher);

    abstract boolean post(long nativePtr);

    abstract void destroy(long nativePtr);
  }

  private static final class PendingSnapshot {
    final MeaningfulContentSnapshot rawSnapshot;
    final long currentTimestampUs;

    PendingSnapshot(MeaningfulContentSnapshot rawSnapshot, long currentTimestampUs) {
      this.rawSnapshot = rawSnapshot;
      this.currentTimestampUs = currentTimestampUs;
    }
  }

  private static final NativeDelegate NATIVE_DELEGATE = new NativeDelegate() {
    @Override
    public long create(FSPReportDispatcher dispatcher) {
      return nativeCreate(dispatcher);
    }

    @Override
    public boolean post(long nativePtr) {
      return nativePost(nativePtr);
    }

    @Override
    public void destroy(long nativePtr) {
      nativeDestroy(nativePtr);
    }
  };

  private final WeakReference<FSPTracer> mTracerRef;
  private final ConcurrentLinkedQueue<PendingSnapshot> mPendingSnapshots =
      new ConcurrentLinkedQueue<>();
  private final NativeDelegate mNativeDelegate;
  private long mNativePtr;

  FSPReportDispatcher(FSPTracer tracer) {
    this(tracer, NATIVE_DELEGATE);
  }

  FSPReportDispatcher(FSPTracer tracer, NativeDelegate nativeDelegate) {
    mTracerRef = new WeakReference<>(tracer);
    mNativeDelegate = nativeDelegate;
    mNativePtr = mNativeDelegate.create(this);
  }

  synchronized boolean post(MeaningfulContentSnapshot rawSnapshot, long currentTimestampUs) {
    if (rawSnapshot == null || mNativePtr == 0) {
      return false;
    }
    PendingSnapshot pendingSnapshot = new PendingSnapshot(rawSnapshot, currentTimestampUs);
    mPendingSnapshots.offer(pendingSnapshot);
    if (mNativeDelegate.post(mNativePtr)) {
      return true;
    }
    mPendingSnapshots.remove(pendingSnapshot);
    return false;
  }

  synchronized void close() {
    if (mNativePtr == 0) {
      return;
    }
    long nativePtr = mNativePtr;
    mNativePtr = 0;
    mNativeDelegate.destroy(nativePtr);
  }

  @CalledByNative
  void dispatchOne() {
    PendingSnapshot pendingSnapshot = mPendingSnapshots.poll();
    FSPTracer tracer = mTracerRef.get();
    if (pendingSnapshot == null || tracer == null) {
      return;
    }
    tracer.processSnapshotOnReportThread(
        pendingSnapshot.rawSnapshot, pendingSnapshot.currentTimestampUs);
  }

  private static native long nativeCreate(FSPReportDispatcher dispatcher);

  private static native boolean nativePost(long nativePtr);

  private static native void nativeDestroy(long nativePtr);
}
