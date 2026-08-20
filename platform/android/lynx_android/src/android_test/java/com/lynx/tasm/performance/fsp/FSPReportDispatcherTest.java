// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

package com.lynx.tasm.performance.fsp;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Test;
import org.mockito.InOrder;

public class FSPReportDispatcherTest {
  private static final long NATIVE_PTR = 1;

  @Test
  public void postDispatchesSnapshotsInOrder() {
    FSPTracer tracer = mock(FSPTracer.class);
    FakeNativeDelegate nativeDelegate = new FakeNativeDelegate();
    FSPReportDispatcher dispatcher = new FSPReportDispatcher(tracer, nativeDelegate);
    MeaningfulContentSnapshot firstSnapshot = mock(MeaningfulContentSnapshot.class);
    MeaningfulContentSnapshot secondSnapshot = mock(MeaningfulContentSnapshot.class);

    assertTrue(dispatcher.post(firstSnapshot, 100));
    assertTrue(dispatcher.post(secondSnapshot, 200));
    dispatcher.dispatchOne();
    dispatcher.dispatchOne();

    InOrder inOrder = inOrder(tracer);
    inOrder.verify(tracer).processSnapshotOnReportThread(firstSnapshot, 100);
    inOrder.verify(tracer).processSnapshotOnReportThread(secondSnapshot, 200);
    assertEquals(2, nativeDelegate.postCount);
  }

  @Test
  public void failedNativePostRemovesSnapshot() {
    FSPTracer tracer = mock(FSPTracer.class);
    FakeNativeDelegate nativeDelegate = new FakeNativeDelegate();
    nativeDelegate.postResult = false;
    FSPReportDispatcher dispatcher = new FSPReportDispatcher(tracer, nativeDelegate);
    MeaningfulContentSnapshot snapshot = mock(MeaningfulContentSnapshot.class);

    assertFalse(dispatcher.post(snapshot, 100));
    dispatcher.dispatchOne();

    verify(tracer, never()).processSnapshotOnReportThread(snapshot, 100);
  }

  @Test
  public void closeRejectsNewSnapshotsButKeepsPendingSnapshots() {
    FSPTracer tracer = mock(FSPTracer.class);
    FakeNativeDelegate nativeDelegate = new FakeNativeDelegate();
    FSPReportDispatcher dispatcher = new FSPReportDispatcher(tracer, nativeDelegate);
    MeaningfulContentSnapshot pendingSnapshot = mock(MeaningfulContentSnapshot.class);
    MeaningfulContentSnapshot newSnapshot = mock(MeaningfulContentSnapshot.class);

    assertTrue(dispatcher.post(pendingSnapshot, 100));
    dispatcher.close();
    dispatcher.close();
    assertFalse(dispatcher.post(newSnapshot, 200));
    dispatcher.dispatchOne();

    verify(tracer).processSnapshotOnReportThread(pendingSnapshot, 100);
    verify(tracer, never()).processSnapshotOnReportThread(newSnapshot, 200);
    assertEquals(1, nativeDelegate.destroyCount);
  }

  private static class FakeNativeDelegate extends FSPReportDispatcher.NativeDelegate {
    boolean postResult = true;
    int postCount;
    int destroyCount;

    @Override
    public long create(FSPReportDispatcher dispatcher) {
      return NATIVE_PTR;
    }

    @Override
    public boolean post(long nativePtr) {
      postCount++;
      return postResult;
    }

    @Override
    public void destroy(long nativePtr) {
      destroyCount++;
    }
  }
}
