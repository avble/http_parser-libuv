# Dependencies
libuv1 and http-parser

``` shell
# install in ubuntu
sudo apt install libhttp-parser-dev libuv1-dev
```

# Compilation
## by make 
``` shell
$ make
```

## by cmake 
``` shell
$ mkdir build 
$ cd build && cmake ..
$ make
```

## run
```
$ ./http_srv 0.0.0.0 12345
```

# benchmark
It is impressive on `11th Gen Intel(R) Core(TM) i5-1135G7 @ 2.40GHz` and `WSL-Ubuntu 22.04` 
167,000 request per seconde

```
Concurrency Level:      50
Time taken for tests:   5.918 seconds
Complete requests:      1000000
Failed requests:        0
Keep-Alive requests:    1000000
Total transferred:      73000000 bytes
HTML transferred:       12000000 bytes
Requests per second:    168984.26 [#/sec] (mean)
Time per request:       0.296 [ms] (mean)
Time per request:       0.006 [ms] (mean, across all concurrent requests)
Transfer rate:          12046.73 [Kbytes/sec] received
Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0    0   0.0      0       1
Processing:     0    0   0.1      0       4
Waiting:        0    0   0.1      0       4
Total:          0    0   0.1      0       4
```


