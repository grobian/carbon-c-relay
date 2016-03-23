# 2.0 (unreleased master branch)

### New Features

### Breaking Changes

### Enhancements

### Bugfixes

# 1.11 (23-03-2016)

### Enhancements
* **collector** UDP connections are now suffixed with `-udp` in
  destination target
* **router** `send statistics to` construct was added to direct internal
  statistics to a specific cluster

### Bugfixes
* [Issue #159](https://github.com/grobian/carbon-c-relay/issues/159)
  corrupted statistics for file clusters
* [Issue #160](https://github.com/grobian/carbon-c-relay/issues/160)
  metricsBlackholed stays zero when blackhole target is used

# 1.10 (09-03-2016)

### Breaking Changes
* **statistics** dispatch\_busy and dispatch\_idle have been replaced with
  wallTime\_us and sleepTime\_us

### Bugfixes
* [Issue #152](https://github.com/grobian/carbon-c-relay/issues/152)
  crash in aggregator\_expire for data-contained aggregations

# 1.9 (07-03-2016)

### Enhancements
* **statistics** dispatch\_busy is slightly more realistic now

### Bugfixes
* [Issue #153](https://github.com/grobian/carbon-c-relay/issues/153)
  aggregator statistics are garbage with `-m`

# 1.8 (23-02-2016)

### New Features
* **relay** new flags `-D` for daemon mode and `-p` for pidfile
  creation

### Enhancements
* **dispatcher** server stalling (to slow down too fast writers) is now
  based on a random timeout
* **server** write timeout is now large enough to deal with upstream
  relay stalling
* **relay** number of workers/dispatchers is now determined in a way
  that doesn''t need OpenMP any more

# 1.7 (29-01-2016)

### New Features
* **relay** new flag `-B` to set the listen backlog for TCP and UNIX
  connections, [issue #143](https://github.com/grobian/carbon-c-relay/issues/143)

### Enhancements
* **dispatcher** switch from select() to poll() to fix crashes when too
  many connections were made to the relay
* Misc (memory) leak fixes

# 1.6 (27-01-2016)

### Breaking Changes
* **relay** startup and shutdown messages are now better in line

### Enhancements
* **relay** fixed segfault when issuing `SIGHUP` under active load

# 1.5 (13-01-2016)

### Enhancements
* **aggregator** metrics are now written directly to dispatchers to
  avoid overload of the internal\_submission queue, which is likely to to
  happen with many aggregates
* **collector** properly report file-based servers in statistics
* **collector** re-introduce the interal destination in statistics

# 1.4 (04-01-2016)

### New Features
* **collector** when run in debug and submission mode, there is a iostat
  like output

### Enhancements
* **relay** reloading config now no longer unconditionally starts the
  aggregator
* **aggregator** misc cleanup/free fixes
* **relay** allow reloading aggregator

### Bugfixes
* [Issue #133](https://github.com/grobian/carbon-c-relay/issues/133)
  _stub_aggregator metrics seen after a reload

# 1.3 (16-12-2015)

### Enhancements
* **consistent-hash** fix jump\_fnv1a\_ch metric submission, it didn''t
  work at all

### Bugfixes
* [Issue #126](https://github.com/grobian/carbon-c-relay/issues/126)
  double free crash
* [Issue #131](https://github.com/grobian/carbon-c-relay/issues/131)
  segfault using stddev in aggregator
* [Issue #132](https://github.com/grobian/carbon-c-relay/issues/132)
  crash with glibc double free message

# 1.2 (10-12-2015)

### New Features
* **consistent-hash** new algorithm jump\_fnv1a\_ch for near perfect
  distribution of metrics
* **distributiontest** test program used to see unbalancedness of
  clusters for a given input metric see
  [graphite-project/carbon#485](https://github.com/graphite-project/carbon/issues/485)

### Enhancements
* **router** fix cluster checking with regards replication count and the
  number of servers to allow equal counts

### Bugfixes
* [Issue #126](https://github.com/grobian/carbon-c-relay/issues/126)
  prevent calling read() too often

# 1.1 (25-11-2015)

### Enhancements
* **router** fix distribution of any\_of cluster if members have failed

# 1.0 (23-11-2015)

* many improvements

# 0.45 (05-11-2015)

* Many aggregator improvements, more flexible routing support.

# 0.44 (13-08-2015)

* Feature to set hash-keys for fnv1a\_ch.

# 0.43 (27-07-2015)

* Bugfix release for segfault when using any\_of clusters.

# 0.42 (24-07-2015)

* Reduced warning level for submission mode queue pileups.  Allow
  writing to a file (cluster type).  Fix splay on aggregator not to
  affect timestamps of input.  No more dep on openssl for md5.

# 0.40 (11-05-2015)

* Hefty optimisations on aggregations.  Fix for UDP port closure.
