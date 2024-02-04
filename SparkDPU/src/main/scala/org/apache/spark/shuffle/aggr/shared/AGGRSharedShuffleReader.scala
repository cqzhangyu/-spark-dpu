/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.apache.spark.shuffle.aggr.shared

import org.apache.spark.{InterruptibleIterator, MapOutputTracker, SparkEnv, TaskContext}
import org.apache.spark.internal.{config, Logging}
import org.apache.spark.serializer.SerializerManager
import org.apache.spark.shuffle.{BaseShuffleHandle, ShuffleReader}
import org.apache.spark.shuffle.aggr.{AGGRSharedShuffleHandle, DPDK, JavaSerializerInt}
import org.apache.spark.storage.{BlockManager, ShuffleBlockFetcherIterator}
import org.apache.spark.util.CompletionIterator
import org.apache.spark.util.collection.ExternalSorter

private[spark] class AGGRSharedShuffleReader[K, C](
    handle: AGGRSharedShuffleHandle[K, C],
    startPartition: Int,
    endPartition: Int,
    context: TaskContext,
    jSerializer: JavaSerializerInt,
    serializerManager: SerializerManager = SparkEnv.get.serializerManager,
    blockManager: BlockManager = SparkEnv.get.blockManager,
    mapOutputTracker: MapOutputTracker = SparkEnv.get.mapOutputTracker)
  extends ShuffleReader[K, C] with Logging {
  private val dep = handle.dependency
  override def read(): Iterator[Product2[K, C]] = {
    var serializerInstance = jSerializer.newInstance()
    val fetcher = new AGGRSharedShuffleFetcher(
      context,
      blockManager,
      serializerManager.wrapStream,
      mapOutputTracker.getMapSizesByExecutorId(handle.shuffleId, startPartition, endPartition))
    // val serializerInstance = dep.serializer.newInstance()
    // Create a key/value iterator for each stream
    val recordIter = fetcher.flatMap { case (blockId, wrappedStream) =>
      serializerInstance.deserializeStream(wrappedStream).asKeyValueIterator
    }
    // Update the context task metrics for each record read.
    val readMetrics = context.taskMetrics.createTempShuffleReadMetrics()
    val metricIter = CompletionIterator[(Any, Any), Iterator[(Any, Any)]](
      recordIter.map { record =>
        readMetrics.incRecordsRead(1)
        record
      },
      context.taskMetrics().mergeShuffleReadMetrics())

    // An interruptible iterator must be used here in order to support task cancellation
    val interruptibleIter = new InterruptibleIterator[(Any, Any)](context, metricIter)

    val aggregatedIter: Iterator[Product2[K, C]] = if (dep.aggregator.isDefined) {
      if (dep.mapSideCombine) {
        val combinedKeyValuesIterator = interruptibleIter.asInstanceOf[Iterator[(K, C)]]
        dep.aggregator.get.combineCombinersByKey(combinedKeyValuesIterator, context)
      } else {
        val keyValuesIterator = interruptibleIter.asInstanceOf[Iterator[(K, Nothing)]]
        dep.aggregator.get.combineValuesByKey(keyValuesIterator, context)
      }
    } else {
      require(!dep.mapSideCombine, "Map-side combine without Aggregator specified!")
      interruptibleIter.asInstanceOf[Iterator[Product2[K, C]]]
    }

    // Sort the output if there is a sort ordering defined.
    dep.keyOrdering match {
      case Some(keyOrd: Ordering[K]) =>
        // Create an ExternalSorter to sort the data.
        // val sorter = new ExternalSorter[K, C, C](
        //     context, ordering = Some(keyOrd), serializer = dep.serializer)
        val sorter = new ExternalSorter[K, C, C](
            context, ordering = Some(keyOrd), serializer = jSerializer)
        sorter.insertAll(aggregatedIter)
        context.taskMetrics().incMemoryBytesSpilled(sorter.memoryBytesSpilled)
        context.taskMetrics().incDiskBytesSpilled(sorter.diskBytesSpilled)
        context.taskMetrics().incPeakExecutionMemory(sorter.peakMemoryUsedBytes)
        CompletionIterator[Product2[K, C], Iterator[Product2[K, C]]](sorter.iterator,
          sorter.stop())
      case None =>
        aggregatedIter
    }
  }
}
