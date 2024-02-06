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

import java.io._
import java.nio.ByteBuffer
import java.util.concurrent.LinkedBlockingQueue
import javax.annotation.concurrent.GuardedBy

import scala.collection.mutable
import scala.collection.mutable.{ArrayBuffer, HashMap, HashSet, Queue}

import org.apache.spark.{SparkEnv, SparkException, TaskContext}
import org.apache.spark.internal.Logging
import org.apache.spark.network.buffer.{ManagedBuffer, NioManagedBuffer}
import org.apache.spark.network.shuffle.{
    BlockFetchingListener, ShuffleClient}
import org.apache.spark.scheduler.MapStatus
import org.apache.spark.shuffle._
import org.apache.spark.shuffle.aggr.{AGGRSharedShuffleHandle, AGGRShuffleFetcher, DPDK}
import org.apache.spark.storage._
import org.apache.spark.util.Utils
import org.apache.spark.util.io.ChunkedByteBufferOutputStream

class AGGRSharedShuffleFetcher(
    context: TaskContext,
    blockManager: BlockManager,
    streamWrapper: (BlockId, InputStream) => InputStream,
    blocksByAddress: Seq[(BlockManagerId, Seq[(BlockId, Long)])])
  extends Iterator[(BlockId, InputStream)] with Logging {
  private[this] var numLocalBlocksToFetch = 1
  private[this] var numLocalBlocksProcessed = 0
  private[this] val fetcherKey = AGGRShuffleFetcher.getNumber()
  private[this] val localBlocks = new ArrayBuffer[BlockId]()
  private[this] var localResult = new LinkedBlockingQueue[AGGRShuffleFetcher.FetchResult]
  private[this] val shuffleMetrics = context.taskMetrics().createTempShuffleReadMetrics()

  @volatile private[this] var currentResult: AGGRShuffleFetcher.SuccessFetchResult = null
  @GuardedBy("this")
  private[this] var isZombie = false
  @GuardedBy("this")
  private[this] val shuffleBlockIdsSet = mutable.HashSet[ShuffleBlockId]()
  private[this] var tmpShuffleBlockId : ShuffleBlockId = null

  initialize()

  private[aggr] def releaseCurrentResultBuffer(): Unit = {
    // Release the current buffer if necessary
    if (currentResult != null && currentResult.buf != null) {
      currentResult.buf.release()
    }
    currentResult = null
  }

  def cleanup() {
    synchronized {
      isZombie = true
    }
    releaseCurrentResultBuffer()

    val iter = localResult.iterator()
    while (iter.hasNext) {
      val result = iter.next()
      result match {
        case AGGRShuffleFetcher.SuccessFetchResult(_, address, _, buf, _) =>
          if (buf != null) {
            buf.release()
          }
        case _ =>
      }
    }

    // shuffleBlockIdsSet.foreach { shuffleBlockId =>
    //   DPDK.ipc_cleanshm(shuffleBlockId.mapId, shuffleBlockId.reduceId)
    // }
  }
  private[this] def splitLocalRemoteBlocks() = {
    for ((address, blockInfos) <- blocksByAddress) {
      // NOTE: Change here if you want to handle only local or remote blocks
      logWarning("DELETE THIS!!!!!!!")
      if (false && address.executorId == blockManager.blockManagerId.executorId) {
        localBlocks ++= blockInfos.filter(_._2 != 0).map(_._1)
        numLocalBlocksToFetch += localBlocks.size
      } else {
        val len = blockInfos.size
        if (len > 0) {
          var ss = new Array[Int](len)
          var ms = new Array[Int](len)
          var rs = new Array[Int](len)
          for (loop <- 0 to len - 1) {
            val shuffleBlockId = blockInfos(loop)._1.asInstanceOf[ShuffleBlockId]
            ss(loop) = shuffleBlockId.shuffleId
            ms(loop) = shuffleBlockId.mapId
            rs(loop) = shuffleBlockId.reduceId
            if (tmpShuffleBlockId == null) {
              tmpShuffleBlockId = shuffleBlockId
            }
          }
          DPDK.ipc_fetch(fetcherKey, address.host, ss, ms, rs)
        }
      }
    }
  }

  private[this] def fetchLocalBlocks() {
    val iter = localBlocks.iterator
    while (iter.hasNext) {
      val blockId = iter.next()
      val shuffleBlockId = blockId.asInstanceOf[ShuffleBlockId]
      shuffleBlockIdsSet += shuffleBlockId
      try {
        var sum_size : Long = 0
        var read_end = false
        while (!read_end) {
          val byte_arr = DPDK.ipc_read(shuffleBlockId.mapId, shuffleBlockId.reduceId)
          if (byte_arr == null) {
            read_end = true
          }
          else {
            val buf = new NioManagedBuffer(ByteBuffer.wrap(byte_arr))
            buf.retain()
            val buf_size = buf.size()
            sum_size += buf_size
            localResult.put(new AGGRShuffleFetcher.SuccessFetchResult(
                blockId, blockManager.blockManagerId, buf.size, buf, false))
          }
        }
        localResult.put(new AGGRShuffleFetcher.SuccessFetchResult(
            blockId, blockManager.blockManagerId, 0, null, false))
        shuffleMetrics.incLocalBlocksFetched(1)
        shuffleMetrics.incLocalBytesRead(sum_size)
      } catch {
        case e: Exception =>
          logError(s"Error occurred while fetching local blocks", e)
          return
      }
    }
  }

  def wait_remaining() {
    try {
      var sum_size : Long = 0
      var wait_end = false
      while (!wait_end) {
        val byte_arr = DPDK.ipc_wait(fetcherKey)
        if (byte_arr == null) {
          // wait finishes
          wait_end = true
        }
        else {
          val buf = new NioManagedBuffer(ByteBuffer.wrap(byte_arr))
          buf.retain()
          val buf_size = buf.size()
          sum_size += buf_size
          // wait has not finished yet
          localResult.put(new AGGRShuffleFetcher.SuccessFetchResult(
            tmpShuffleBlockId, blockManager.blockManagerId, buf.size, buf, false))
        }
      }
      localResult.put(new AGGRShuffleFetcher.SuccessFetchResult(
        tmpShuffleBlockId, blockManager.blockManagerId, 0, null, false))

      shuffleMetrics.incLocalBlocksFetched(1)
      shuffleMetrics.incLocalBytesRead(sum_size)
      // logInfo("put SuccessFetchResult for key " + fetcherKey)
    } catch {
      case e: Exception =>
        logError(s"Error occurred while fetching local blocks", e)
        return
    }
  }

  def initialize() {
    context.addTaskCompletionListener(_ => cleanup())
    splitLocalRemoteBlocks()
    // Fetch the local blocks while we are fetching remote blocks
    fetchLocalBlocks()
    // logInfo("wait for fetcherKey " + fetcherKey)
    wait_remaining()
  }

  override def hasNext: Boolean = numLocalBlocksProcessed < numLocalBlocksToFetch
  override def next(): (BlockId, InputStream) = {
    if (!hasNext) {
      throw new NoSuchElementException
    }
    var result: AGGRShuffleFetcher.FetchResult = null
    var input: InputStream = null

    while (result == null) {
      val startFetchWait = System.nanoTime()
      result = localResult.take()
      val stopFetchWait = System.nanoTime()
      shuffleMetrics.incFetchWaitTime(stopFetchWait - startFetchWait)

      result match {
        case r @ AGGRShuffleFetcher.SuccessFetchResult(
            blockId, address, size, buf, isNetworkReqDone) =>
          // size = 1 if it is the end of a local block or a remote fetch
          // size = 0 if it is not the end of a local block or a remote fetch
          if (buf == null) {
            numLocalBlocksProcessed += 1
            val in = new NioManagedBuffer(ByteBuffer.allocate(0)).createInputStream()
            input = streamWrapper(blockId, in)
          }
          else {
            numLocalBlocksProcessed += 0
            val in = buf.createInputStream()
            input = streamWrapper(blockId, in)
          }
        case AGGRShuffleFetcher.FailureFetchResult(blockId, address, e) =>
          throwFetchFailedException(blockId, address, e)
      }
    }
    currentResult = result.asInstanceOf[AGGRShuffleFetcher.SuccessFetchResult]
    (currentResult.blockId, new BufferReleasingInputStream(input))
  }

  private def throwFetchFailedException(blockId: BlockId, address: BlockManagerId, e: Throwable) = {
    blockId match {
      case ShuffleBlockId(shufId, mapId, reduceId) =>
        throw new FetchFailedException(address, shufId.toInt, mapId.toInt, reduceId, e)
      case _ =>
        throw new SparkException(
          "Failed to get block " + blockId + ", which is not a shuffle block", e)
    }
  }
}

private[aggr] class BufferReleasingInputStream(
    private val delegate: InputStream)
  extends InputStream {
  private[this] var closed = false
  override def read(): Int = delegate.read()
  override def close(): Unit = {
    if (!closed) {
      delegate.close()
      closed = true
    }
  }
  override def available(): Int = delegate.available()
  override def mark(readlimit: Int): Unit = delegate.mark(readlimit)
  override def skip(n: Long): Long = delegate.skip(n)
  override def markSupported(): Boolean = delegate.markSupported()
  override def read(b: Array[Byte]): Int = delegate.read(b)
  override def read(b: Array[Byte], off: Int, len: Int): Int = delegate.read(b, off, len)
  override def reset(): Unit = delegate.reset()
}
