/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class org_apache_spark_shuffle_aggr_DPDK */

#ifndef _Included_org_apache_spark_shuffle_aggr_DPDK
#define _Included_org_apache_spark_shuffle_aggr_DPDK
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     org_apache_spark_shuffle_aggr_DPDK
 * Method:    ipc_init
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1init
  (JNIEnv *, jclass);

/*
 * Class:     org_apache_spark_shuffle_aggr_DPDK
 * Method:    ipc_initall
 * Signature: (III)I
 */
JNIEXPORT jint JNICALL Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1initall
  (JNIEnv *, jclass, jint, jint, jint);

/*
 * Class:     org_apache_spark_shuffle_aggr_DPDK
 * Method:    ipc_clean
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1clean
  (JNIEnv *, jclass);

/*
 * Class:     org_apache_spark_shuffle_aggr_DPDK
 * Method:    ipc_write
 * Signature: (II[II)I
 */
JNIEXPORT jint JNICALL Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1write
  (JNIEnv *, jclass, jint, jint, jintArray, jint);

/*
 * Class:     org_apache_spark_shuffle_aggr_DPDK
 * Method:    ipc_lengths
 * Signature: (II)[J
 */
JNIEXPORT jlongArray JNICALL Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1lengths
  (JNIEnv *, jclass, jint, jint);

/*
 * Class:     org_apache_spark_shuffle_aggr_DPDK
 * Method:    ipc_read
 * Signature: (II)[B
 */
JNIEXPORT jbyteArray JNICALL Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1read
  (JNIEnv *, jclass, jint, jint);

/*
 * Class:     org_apache_spark_shuffle_aggr_DPDK
 * Method:    ipc_fetch
 * Signature: (ILjava/lang/String;[I[I[I)I
 */
JNIEXPORT jint JNICALL Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1fetch
  (JNIEnv *, jclass, jint, jstring, jintArray, jintArray, jintArray);

/*
 * Class:     org_apache_spark_shuffle_aggr_DPDK
 * Method:    ipc_wait
 * Signature: (I)[B
 */
JNIEXPORT jbyteArray JNICALL Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1wait
  (JNIEnv *, jclass, jint);

#ifdef __cplusplus
}
#endif
#endif
