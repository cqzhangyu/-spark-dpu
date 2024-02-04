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

import org.apache.spark.{SparkEnv, TaskContext}
import org.apache.spark.internal.Logging
import org.apache.spark.scheduler.MapStatus
import org.apache.spark.shuffle.ShuffleWriter
import org.apache.spark.shuffle.aggr.AGGRSharedShuffleHandle
import org.apache.spark.shuffle.aggr.DPDK
import org.apache.spark.shuffle.aggr.JavaSerializerInt
import org.apache.spark.util.collection.ExternalSorter

class AGGRSharedShuffleWriter[K, V](
    handle: AGGRSharedShuffleHandle[K, V],
    mapId: Int,
    context: TaskContext,
    jSerializer: JavaSerializerInt)
  extends ShuffleWriter[K, V] with Logging {
  private val batch_size = 8192
  private val dep = handle.dependency
  private val partitioner = dep.partitioner
  private val blockManager = SparkEnv.get.blockManager
  private val numPartitions = partitioner.numPartitions
  private var stopping = false
  private var mapStatus: MapStatus = null
  private val writeMetrics = context.taskMetrics().shuffleWriteMetrics
  private var sorter: ExternalSorter[K, V, _] = null

  override def write(records: Iterator[Product2[K, V]]): Unit = {
    if (!records.hasNext) {
      val partitionLengths = new Array[Long](numPartitions)
      mapStatus = MapStatus(blockManager.shuffleServerId, partitionLengths)
    } else {
      var startTime = System.nanoTime()
      var kv_counter = 0
      var kv_total = 0
      val kv_buff = new Array[Int](batch_size)

      // logInfo("ShuffleWrite Start")

      val int_records = records.asInstanceOf[Iterator[Product2[Int, Int]]]
      /* val int_records = if (dep.mapSideCombine) {
        sorter = new ExternalSorter[K, V, V](
          context, dep.aggregator, Some(dep.partitioner), dep.keyOrdering, jSerializer)
        sorter.insertAll(records)
        sorter.iterator.asInstanceOf[Iterator[Product2[Int, Int]]]
      } else {
        records.asInstanceOf[Iterator[Product2[Int, Int]]]
      } */

      while (int_records.hasNext) {
        val record = int_records.next()
        kv_buff(kv_counter) = record._1
        kv_counter = kv_counter + 1
        kv_buff(kv_counter) = record._2
        kv_counter = kv_counter + 1

        if (kv_counter == batch_size) {
            DPDK.ipc_write(mapId, numPartitions, kv_buff, batch_size)
            kv_total = kv_total + batch_size
            kv_counter = 0
        }
      }
      if (kv_counter != 0) {
        DPDK.ipc_write(mapId, numPartitions, kv_buff, kv_counter)
        kv_total = kv_total + kv_counter
      }
      var stopTime = System.nanoTime()
      // writeMetrics.incWriteTime(stopTime - startTime)
      writeMetrics.incBytesWritten(kv_total * 4)
      writeMetrics.incRecordsWritten(kv_total / 2)

      val partitionLengths = DPDK.ipc_lengths(mapId, numPartitions)
      mapStatus = MapStatus(blockManager.shuffleServerId, partitionLengths)
    }
  }

  override def stop(success: Boolean): Option[MapStatus] = {
    try {
      if (stopping) {
        return None
      }
      stopping = true
      if (success) {
        return Option(mapStatus)
      } else {
        return None
      }
    } finally {
      // Clean up our sorter, which may have its own intermediate files
      if (sorter != null) {
        val startTime = System.nanoTime()
        sorter.stop()
        writeMetrics.incWriteTime(System.nanoTime - startTime)
        sorter = null
      }
    }
  }
}

