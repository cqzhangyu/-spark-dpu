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
import java.nio.ByteBuffer
import java.util.concurrent.LinkedBlockingQueue
import javax.annotation.concurrent.GuardedBy

import scala.collection.mutable
import scala.collection.mutable.{ArrayBuffer, HashMap, HashSet, Queue}

import org.apache.spark.{SparkException, TaskContext}
import org.apache.spark.internal.Logging
import org.apache.spark.network.buffer.{FileSegmentManagedBuffer, ManagedBuffer}
import org.apache.spark.network.shuffle.{BlockFetchingListener, ShuffleClient, TempFileManager}
import org.apache.spark.shuffle._
import org.apache.spark.storage._
import org.apache.spark.util.Utils
import org.apache.spark.util.io.ChunkedByteBufferOutputStream

private[spark]
final class AGGRShuffleFetcher(
    context: TaskContext,
    blockManager: BlockManager,
    streamWrapper: (BlockId, InputStream) => InputStream,
    blocksByAddress: Seq[(BlockManagerId, Seq[(BlockId, Long)])],
    detectCorrupt: Boolean)
  extends Iterator[(BlockId, InputStream)] with TempFileManager with Logging {
  import AGGRShuffleFetcher._
  private[this] var numLocalBlocksToFetch = 0
  private[this] var numLocalBlocksProcessed = 0
  private[this] val fetcherKey = AGGRShuffleFetcher.getNumber()
  private[this] val localBlocks = new ArrayBuffer[BlockId]()
  private[this] var localResult = new LinkedBlockingQueue[FetchResult]
  private[this] val shuffleMetrics = context.taskMetrics().createTempShuffleReadMetrics()

  /**
   * Current [[FetchResult]] being processed. We track this so we can release the current buffer
   * in case of a runtime exception when processing the current buffer.
   */
  @volatile private[this] var currentResult: SuccessFetchResult = null
  /**
   * Whether the iterator is still active. If isZombie is true, the callback interface will no
   * longer place fetched blocks into [[results]].
   */
  @GuardedBy("this")
  private[this] var isZombie = false

  /**
   * A set to store the files used for shuffling remote huge blocks. Files in this set will be
   * deleted when cleanup. This is a layer of defensiveness against disk file leaks.
   */
  @GuardedBy("this")
  private[this] val shuffleFilesSet = mutable.HashSet[File]()

  initialize()

  // Decrements the buffer reference count.
  // The currentResult is set to null to prevent releasing the buffer again on cleanup()
  private[aggr] def releaseCurrentResultBuffer(): Unit = {
    // Release the current buffer if necessary
    if (currentResult != null) {
      currentResult.buf.release()
    }
    currentResult = null
  }

  override def createTempFile(): File = {
    blockManager.diskBlockManager.createTempLocalBlock()._2
  }

  override def registerTempFileToClean(file: File): Boolean = synchronized {
    if (isZombie) {
      false
    } else {
      shuffleFilesSet += file
      true
    }
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
        case SuccessFetchResult(_, address, _, buf, _) =>
          buf.release()
        case _ =>
      }
    }

    shuffleFilesSet.foreach { file =>
      if (!file.delete()) {
        logWarning("Failed to cleanup shuffle fetch temp file " + file.getAbsolutePath())
      }
    }
  }

  // def getDataFile(shuffleId: Int, mapId: Int): File = {
  //   // blockManager.diskBlockManager.getFile(ShuffleDataBlockId(shuffleId, mapId, 0))
  //   blockManager.diskBlockManager.getFile(
  //       ShuffleDataBlockId(shuffleId, mapId, 0).name, mapId)
  // }

  // private def getIndexFile(shuffleId: Int, mapId: Int): File = {
  //   // blockManager.diskBlockManager.getFile(ShuffleIndexBlockId(shuffleId, mapId, 0))
  //   blockManager.diskBlockManager.getFile(
  //       ShuffleIndexBlockId(shuffleId, mapId, 0).name, mapId)
  // }

  // private[this] def copyFile(Input: String, OutPut: String): Unit = {
  //   val fis = new FileInputStream(Input)
  //   val fos = new FileOutputStream(OutPut)
  //   val buf = new Array[Byte](1024)
  //   var len = 0
  //   while ({len = fis.read(buf);len} != -1) {
  //     fos.write(buf, 0, len)
  //   }
  //   fos.close()
  //   fis.close()
  // }

  private[this] def splitLocalRemoteBlocks() = {
    for ((address, blockInfos) <- blocksByAddress) {
      if (address.executorId == blockManager.blockManagerId.executorId) {
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
      try {
        val buf = blockManager.getBlockData(blockId)
        shuffleMetrics.incLocalBlocksFetched(1)
        shuffleMetrics.incLocalBytesRead(buf.size)
        buf.retain()
        localResult.put(new SuccessFetchResult(blockId, blockManager.blockManagerId, 0, buf, false))
        // // test
        // val shuffleBlockId = blockId.asInstanceOf[ShuffleBlockId]
        // val sFile = getDataFile(shuffleBlockId.shuffleId, shuffleBlockId.mapId)
        // val iFile = getIndexFile(shuffleBlockId.shuffleId, shuffleBlockId.mapId)
        // logInfo("Copy " + sFile.getAbsolutePath() + " to " +
        //   "/home/yongchao/tmp/" + sFile.getName())
        // logInfo("Copy " + iFile.getAbsolutePath() + " to " +
        //   "/home/yongchao/tmp/" + iFile.getName())
        // copyFile(sFile.getAbsolutePath, "/home/yongchao/tmp/" + sFile.getName())
        // copyFile(iFile.getAbsolutePath, "/home/yongchao/tmp/" + iFile.getName())
        // // test
      } catch {
        case e: Exception =>
          logError(s"Error occurred while fetching local blocks", e)
          // localResult.put(new FailureFetchResult(blockId, blockManager.blockManagerId, e))
          return
      }
    }
  }

  def wait_remaining() {
    DPDK.ipc_wait(fetcherKey)
  }

  def initialize() {
    // Add a task completion callback (called in both success case and failure case) to cleanup.
    // context.addTaskCompletionListener(_ => cleanup())

    splitLocalRemoteBlocks()
    // Fetch the local blocks while we are fetching remote blocks
    fetchLocalBlocks()
  }

  override def hasNext: Boolean = numLocalBlocksProcessed < numLocalBlocksToFetch
  override def next(): (BlockId, InputStream) = {
    if (!hasNext) {
      throw new NoSuchElementException
    }
    numLocalBlocksProcessed += 1
    var result: FetchResult = null
    var input: InputStream = null

    // Take the next fetched result and try to decompress it to detect data corruption,
    // then fetch it one more time if it's corrupt, throw FailureFetchResult if the second fetch
    // is also corrupt, so the previous stage could be retried.
    // For local shuffle block, throw FailureFetchResult for the first IOException.
    while (result == null) {
      // val startFetchWait = System.currentTimeMillis()
      val startFetchWait = System.nanoTime()
      result = localResult.take()
      val stopFetchWait = System.nanoTime()
      shuffleMetrics.incFetchWaitTime(stopFetchWait - startFetchWait)

      result match {
        case r @ SuccessFetchResult(blockId, address, size, buf, isNetworkReqDone) =>
          val in = buf.createInputStream()
          input = streamWrapper(blockId, in)
          if (detectCorrupt && !input.eq(in)) {
            val originalInput = input
            val out = new ChunkedByteBufferOutputStream(
              size.toInt + 10, ByteBuffer.allocate)
            try {
              Utils.copyStream(input, out)
              out.close()
              input = out.toChunkedByteBuffer.toInputStream(dispose = true)
            } catch {
              case e: IOException =>
                buf.release()
            } finally {
              // TODO: release the buf here to free memory earlier
              originalInput.close()
              in.close()
            }
          }
        case FailureFetchResult(blockId, address, e) =>
          throwFetchFailedException(blockId, address, e)
      }
    }
    currentResult = result.asInstanceOf[SuccessFetchResult]
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

private[aggr]
object AGGRShuffleFetcher {
  private var lastNumber: Int = 0
  def getNumber(): Int = this.synchronized {
    lastNumber += 1
    lastNumber
  }

  case class FetchRequest(address: BlockManagerId, blocks: Seq[(BlockId, Long)]) {
    val size = blocks.map(_._2).sum
  }

  private[aggr] sealed trait FetchResult {
    val blockId: BlockId
    val address: BlockManagerId
  }

  private[aggr] case class SuccessFetchResult(
      blockId: BlockId,
      address: BlockManagerId,
      size: Long,
      buf: ManagedBuffer,
      isNetworkReqDone: Boolean) extends FetchResult {
    // require(buf != null)
    require(size >= 0)
  }

  private[aggr] case class FailureFetchResult(
      blockId: BlockId,
      address: BlockManagerId,
      e: Throwable)
    extends FetchResult
}
