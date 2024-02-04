SPARK_VERSION = spark-2.3.0
all:
	mvn -D skipTests clean package -P $(SPARK_VERSION) 

.PHONY: clean
clean:
	rm -r target
