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

import org.apache.spark.ShuffleDependency
import org.apache.spark.shuffle.BaseShuffleHandle
import org.apache.spark.shuffle.sort.SerializedShuffleHandle

class AGGRSerializedShuffleHandle[K, V](
    shuffleId: Int,
    numMaps: Int,
    dependency: ShuffleDependency[K, V, V])
    extends SerializedShuffleHandle[K, V](shuffleId, numMaps, dependency)

class AGGRBaseShuffleHandle[K, V, C](
    shuffleId: Int,
    numMaps: Int,
    dependency: ShuffleDependency[K, V, C])
    extends BaseShuffleHandle[K, V, C](shuffleId, numMaps, dependency)

class AGGRBypassShuffleHandle[K, V](
  shuffleId: Int,
  numMaps: Int,
  dependency: ShuffleDependency[K, V, V])
  extends BaseShuffleHandle(shuffleId, numMaps, dependency) {
}

class AGGRSharedShuffleHandle[K, V](
  shuffleId: Int,
  numMaps: Int,
  dependency: ShuffleDependency[K, V, V])
  extends BaseShuffleHandle(shuffleId, numMaps, dependency) {
}
