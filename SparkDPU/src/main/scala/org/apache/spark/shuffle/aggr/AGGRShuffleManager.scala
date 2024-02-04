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

package org.apache.spark.shuffle.aggr

import java.io._
import java.nio._
import java.nio.channels._
import java.nio.file.Files
import java.util.concurrent.ConcurrentHashMap

import scala.collection.JavaConverters._
import scala.concurrent.{Future, Promise}
import scala.concurrent.ExecutionContext.Implicits.global
import scala.util.{Failure, Success}

import org.apache.spark.{ShuffleDependency, SparkConf, SparkEnv, TaskContext}
import org.apache.spark.internal.Logging
import org.apache.spark.shuffle.{BaseShuffleHandle, IndexShuffleBlockResolver, ShuffleHandle,
  ShuffleManager, ShuffleReader, ShuffleWriter}
import org.apache.spark.shuffle.aggr.shared.{AGGRSharedShuffleReader, AGGRSharedShuffleWriter}
import org.apache.spark.shuffle.aggr.writer.wrapper.AGGRWrapperShuffleWriter
import org.apache.spark.shuffle.sort.SortShuffleManager

private[spark] class AGGRShuffleManager(val conf: SparkConf) extends ShuffleManager with Logging {
  private[this] val numMapsForShuffle = new ConcurrentHashMap[Int, Int]()
  override val shuffleBlockResolver = new AGGRShuffleBlockResolver(conf)
  // override val shuffleBlockResolver = new IndexShuffleBlockResolver(conf)
  val jSerializer = new JavaSerializerInt(conf)
  // DPDK.ipc_init()

  override def registerShuffle[K, V, C](
      shuffleId: Int,
      numMaps: Int,
      dependency: ShuffleDependency[K, V, C]): ShuffleHandle = {
    DPDK.ipc_init(shuffleId, numMaps, dependency.partitioner.numPartitions)
    new AGGRSharedShuffleHandle(
      shuffleId, numMaps, dependency.asInstanceOf[ShuffleDependency[K, V, V]])
    // if (AGGRShuffleManager.shouldBypassAGGR(conf, dependency)) {
    //   // new AGGRBypassShuffleHandle(
    //   //   shuffleId, numMaps, dependency.asInstanceOf[ShuffleDependency[K, V, V]])
    //   new AGGRSharedShuffleHandle(
    //     shuffleId, numMaps, dependency.asInstanceOf[ShuffleDependency[K, V, V]])
    // } else {
    //   // Otherwise, buffer map outputs in a deserialized form:
    //   new AGGRBaseShuffleHandle(shuffleId, numMaps, dependency)
    // }
  }

  override def getReader[K, C](
      handle: ShuffleHandle,
      startPartition: Int,
      endPartition: Int,
      context: TaskContext): ShuffleReader[K, C] = {
    handle match {
      case sharedShuffleHandle: AGGRSharedShuffleHandle[K @unchecked, C @unchecked] =>
        new AGGRSharedShuffleReader (
          sharedShuffleHandle,
          startPartition,
          endPartition,
          context,
          jSerializer)
      case baseShuffleHandle: BaseShuffleHandle[K @unchecked, _, C @unchecked] =>
        new AGGRShuffleReader(
          baseShuffleHandle,
          startPartition,
          endPartition,
          context,
          jSerializer)
    }
    // new AGGRShuffleReader(
    //   handle.asInstanceOf[BaseShuffleHandle[K, _, C]],
    //   startPartition,
    //   endPartition,
    //   context,
    //   jSerializer)
  }

  override def getWriter[K, V](
      handle: ShuffleHandle,
      mapId: Int,
      context: TaskContext): ShuffleWriter[K, V] = {
    if (AGGRShuffleManager.get_id() == 1) {
      val blockManager = SparkEnv.get.blockManager
      val workdir = blockManager.diskBlockManager.localDirs(0).getPath()
      // DPDK.ipc_workdir(workdir)

      // 2~3 ms
      if (handle.isInstanceOf[AGGRSharedShuffleHandle[K @unchecked, V @unchecked]]) {
        val temp_handle =
          handle.asInstanceOf[AGGRSharedShuffleHandle[K @unchecked, V @unchecked]]
        // DPDK.ipc_initall(temp_handle.dependency.partitioner.numPartitions)
      }
    }
    numMapsForShuffle.putIfAbsent(
      handle.shuffleId, handle.asInstanceOf[BaseShuffleHandle[_, _, _]].numMaps)
    val env = SparkEnv.get
    handle match {
      case bypassShuffleHandle: AGGRBypassShuffleHandle[K @unchecked, V @unchecked] =>
        new AGGRBypassShuffleWriter(
          env.blockManager,
          shuffleBlockResolver,
          bypassShuffleHandle,
          mapId,
          context,
          env.conf,
          jSerializer)
      case sharedShuffleHandle: AGGRSharedShuffleHandle[K @unchecked, V @unchecked] =>
        new AGGRSharedShuffleWriter(
          sharedShuffleHandle,
          mapId,
          context,
          jSerializer)
      case baseShuffleHandle: BaseShuffleHandle[K @unchecked, V @unchecked, _] =>
        new AGGRWrapperShuffleWriter(
          shuffleBlockResolver,
          baseShuffleHandle,
          mapId,
          context,
          jSerializer)
    }
  }

  override def unregisterShuffle(shuffleId: Int): Boolean = {
    DPDK.ipc_clean()
    Option(numMapsForShuffle.remove(shuffleId)).foreach { numMaps =>
      (0 until numMaps).foreach { mapId =>
        shuffleBlockResolver.removeDataByMap(shuffleId, mapId)
      }
    }
    true
  }

  override def stop(): Unit = {
    shuffleBlockResolver.stop()
  }
}

private [spark] object AGGRShuffleManager extends Logging {
  private var writer_id = 0

  def get_id(): Int = this.synchronized {
    writer_id = writer_id + 1
    writer_id
  }
  def shouldBypassAGGR(conf: SparkConf, dep: ShuffleDependency[_, _, _]): Boolean = {
    // We cannot bypass sorting if we need to do map-side aggregation.
    if (dep.mapSideCombine) {
      false
    } else {
      true
    }
  }
}
