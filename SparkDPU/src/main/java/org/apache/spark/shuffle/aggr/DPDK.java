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

package org.apache.spark.shuffle.aggr;

public class DPDK {
  public static native int ipc_init();
  public static native int ipc_initall(int shuffle_id, int num_mapper, int num_reducer);
  public static native int ipc_clean();

  public static native int ipc_write(
    int map_id, int num_partitions, int[] kv, int num);
  public static native long[] ipc_lengths(int map_id, int num_partitions);

  public static native byte[] ipc_read(int map_id, int reduce_id);
  public static native int ipc_fetch(
    int key, String host, int[] shuffle_id, int []map_id, int []reduce_id);
  public static native byte[] ipc_wait(int key);

  static {
    System.loadLibrary("spark_dpu");
    // System.load("/home/zcq/target/libspark_dpu.so");
  }
}
