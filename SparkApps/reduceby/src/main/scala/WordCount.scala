/* WordCount.scala */
import java.util.Random
import org.apache.spark.{SparkConf, SparkContext, Partitioner}

/*
object WordCount {
    def main(args: Array[String]) {
        val num_worker         = args(0).toInt
        val mapper_per_worker  = args(1).toInt
        val reducer_per_worker = args(2).toInt
        val path               = args(3)
        val num_mapper         = num_worker * mapper_per_worker
        val num_reducer        = num_worker * reducer_per_worker

        val cf = new SparkConf().setAppName("Word Count")
        val sc = new SparkContext(cf)

        // val myFile = "hdfs://worker21p:9000/home/yongchao/dataset/test/test"
        val myData = sc.textFile(path, num_mapper).map(word => (word.toInt, 1))
        // println(myData.count)

        val counts = myData.reduceByKey(_ + _, num_reducer)
        // val counts = myData.groupByKey(num_reducer).map(t => (t._1, t._2.sum))
        counts.collect()
        println("DONE!")
        sc.stop()
  }
}
*/

object WordCount {
    def main(args: Array[String]) {
        val num_worker         = args(0).toInt
        val mapper_per_worker  = args(1).toInt
        val reducer_per_worker = args(2).toInt
        val kv_per_mapper      = args(3).toInt
        val total_key          = args(4).toInt
        val num_mapper         = num_worker * mapper_per_worker
        val num_reducer        = num_worker * reducer_per_worker

        val cf = new SparkConf().setAppName("Word Count")
        val sc = new SparkContext(cf)

        val myData = sc.parallelize(0 until num_mapper, num_mapper).flatMap { p =>
            var arr = new Array[(Int, Int)](kv_per_mapper)
            for (loop <- 0 until kv_per_mapper) {
                arr(loop) = (loop % total_key, 1)
            }
            arr
        }.persist
        println(myData.count)

        val counts = myData.reduceByKey(_ + _, num_reducer)
        // val counts = myData.groupByKey(new AGGRPartitioner(num_reducer)).map(t => (t._1, t._2.sum))
        counts.collect()
        println("DONE")

        // println(myData.count)
        myData.unpersist(blocking = true)

        while(true) {
            
        }
        sc.stop()
     }
}

class AGGRPartitioner(partitions: Int) extends Partitioner {
    require(partitions >= 0, s"Number of partitions ($partitions) cannot be negative.")
    def numPartitions: Int = partitions

    def getPartition(key: Any): Int = {
        key.asInstanceOf[Int] % numPartitions
    }

    override def equals(other: Any): Boolean = other match {
        case a: AGGRPartitioner =>
            a.numPartitions == numPartitions
        case _ =>
            false
    }

    override def hashCode: Int = numPartitions
}
