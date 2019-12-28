# 3.7 (unreleased master branch)

### New Features

### Bugfixes

* [Issue #398](https://github.com/grobian/carbon-c-relay/issues/398)
  metric includes all tags up to one before last

# 3.6 (26-10-2019)

### New Features

* **router** now emits the bad string if an invalid port was given
* **router** new support for dynamic clusters
* **router** allow dynamic on/off debug mode (with SIGUSR1 sigal)
  [Issue #367](https://github.com/grobian/carbon-c-relay/issues/367)
* **dispatcher** allow limits to input and metric lengths
  [Issue #354](https://github.com/grobian/carbon-c-relay/issues/354)

### Bugfixes

* [Issue #346](https://github.com/grobian/carbon-c-relay/issues/346)
  sporadic segfaults when using UDP connections
* [Issue #368](https://github.com/grobian/carbon-c-relay/issues/368)
  segfault sending or receiving metrics using ssl transport
* [Issue #369](https://github.com/grobian/carbon-c-relay/issues/369)
  quoted expressions aren't printed as such by relay
* [Issue #377](https://github.com/grobian/carbon-c-relay/issues/377)
  periodic segfault on SIGHUP (diff popen failed)
* [Issue #381](https://github.com/grobian/carbon-c-relay/issues/381)
  segfault on reload after removal of node from cluster
* [Issue #384](https://github.com/grobian/carbon-c-relay/issues/384)
  some errors in router\_readconfig invisible due to closed stderr
* [Issue #386](https://github.com/grobian/carbon-c-relay/issues/386)
  some metrics on gzip tranported connections could get lost on close


# 3.5 (22-02-2019)

### New Features

### Bugfixes

* [Issue #336](https://github.com/grobian/carbon-c-relay/issues/336)
  LZ4 compression update to use framed format.
* [Issue #351](https://github.com/grobian/carbon-c-relay/issues/351)
  ssl transport cannot be used due to syntax errors
* defining aggregator with equal expire and interval causes a crash
* [Issue #357](https://github.com/grobian/carbon-c-relay/issues/357)
  routes optimiser could get stuck in an infinite loop
* [Issue #358](https://github.com/grobian/carbon-c-relay/issues/358)
  remove `-S` option from usage


# 3.4 (18-10-2018)

### New Features

* **router** `syslog` message type to emit syslog packets was added.
  [Pull Request #329](https://github.com/grobian/carbon-c-relay/pull/329)
* **server** will now set the SNI hostname before connecting over TLS.
* **relay** a new flag `-E` was added to disable disconnecting idle
  clients.
  [Bug #330](https://github.com/grobian/carbon-c-relay/issues/330)
* **relay** support for snappy compression was added.
* **relay** SSL support can now be used next to compression
  [Bug #323](https://github.com/grobian/carbon-c-relay/issues/323)
* **relay** add option to set CA certs using `-C`, in addition
  certificate verification failures are now treated as fatal
  [Bug #314](https://github.com/grobian/carbon-c-relay/issues/314)

### Bugfixes

* [Issue #327](https://github.com/grobian/carbon-c-relay/issues/327)
  gzip compression leaking half closed sockets
* [Issue #341](https://github.com/grobian/carbon-c-relay/issues/341)
  after SIGHUP with changed config relay no longer accepts connections
* passing a directory as config file silently succeeds with empty config
  as result


# 3.3 (23-03-2018)

### New Features

* **dispatcher** tags support was added, it is only activated when the
  `;` character is *not* in the list of allowed characters (`-c` flag).
  Note that by default this character is not allowed, hence tags support
  enabled.
  [Issue #311](https://github.com/grobian/carbon-c-relay/issues/311)
* **dispatcher** support for oniguruma, pcre2 and pcre regex libraries
  was added.  These libraries perform all better than the libc
  implementation, bringing down cpu time of the relay considerably.

### Bugfixes

* Fix conffile.tab.h dependency for parallel make.


# 3.2 (21-10-2017)

### Notes

* The deprecated `-S` and `-m` options were removed.  Use the
  `statistics` configuration file construct instead.
* The `-i` option was removed.  Use tue `listen` configuration file
  construct instead.

### New Features

* **relay** the flag `-tt` can now be used to test the configuration and
  exit immediately.  In this mode, any standard output is suppressed as
  well.
  [Issue #247](https://github.com/grobian/carbon-c-relay/issues/247)
* **relay** the `listen` construct was added to precisely control listen
  ports and protocols.
* **server** unresolvable hosts are now treated as failed servers, this
  means the relay will only warn about unresolvable hosts, but not error
  out.  If a host becomes unresolvable this server is transitioned to
  failed state until it is resolvable again.
  [Issue #293](https://github.com/grobian/carbon-c-relay/issues/293)
* **relay** compression and encryption support was added to listeners
  and servers.
  [Issue #212](https://github.com/grobian/carbon-c-relay/issues/212)
  [Issue #75](https://github.com/grobian/carbon-c-relay/issues/75)

### Bugfixes

* [Issue #265](https://github.com/grobian/carbon-c-relay/issues/265)
  exit code is sucess on error when daemonised
* [Issue #266](https://github.com/grobian/carbon-c-relay/issues/266)
  incorrect hash when using aggregate function
* [Issue #267](https://github.com/grobian/carbon-c-relay/issues/267)
  segmentation fault when trying to use a file cluster
* [PR #273](https://github.com/grobian/carbon-c-relay/pull/273)
  regex matching slow on glibc due to lock contention
* [Issue #287](https://github.com/grobian/carbon-c-relay/issues/287)
  unexpected warning about rejected socket bufsize
* [PR #282](https://github.com/grobian/carbon-c-relay/pull/282)
  route using should not change the metric key sent
* [PR #274](https://github.com/grobian/carbon-c-relay/pull/274)
  aggregator expiry deadlocks under pressure
* [buckytools Issue #17](https://github.com/jjneely/buckytools/issues/17)
  consistent-hash implementation returns a different distribution with
  the binary search approach


# 3.1 (29-04-2017)

### New Features

* **router** a new `route using` clause was added to `match` rules to
  set the string to be used as key for consistent-hash based clusters.
  [Pull Request #235](https://github.com/grobian/carbon-c-relay/pull/235)
* **consistent-hash** node traversal is now done through a binary search
  instead of forward pointer chasing.

### Bugfixes

* [Issue #261](https://github.com/grobian/carbon-c-relay/issues/261)
  unable to use numeric instance name
* [Issue #262](https://github.com/grobian/carbon-c-relay/issues/262)
  DNS round-robin on any\_of cluster doesn't rotate and causes lots of
  hung connections
* [Issue #263](https://github.com/grobian/carbon-c-relay/issues/263)
  parser fails on `%` in regular expressions, and exits with a crash
* [Issue #264](https://github.com/grobian/carbon-c-relay/issues/264)
  carbon\_ch hash algorithm change between 2.6 and 3.0


# 3.0 (2017-04-07)

### Notes

* Parsing of the configuration file has changed implementation in this
  version.  Should you find problems with configurations that worked
  fine before, please report the problem.
* The use of `send statistics to` is now deprecated, use the new
  `statistics send to` instead.
* The use of statistics controlling flags `-S` sending interval and `-m`
  summary mode, are now deprecated because they can be set in the
  configuration.

### New Features

* **collector** statistics now can be controlled via the configuration
  file using the new `statistics` clause.  This overrides some command
  line flags, and adds some new posibilities.
* **router** the rewrite rule got a new replacement mode: `\.`  This
  mode replaces all dots with underscores in the matched group.  A
  combination with upper/lowercasing is possible, e.g. `\_.`.
* **collector** one can now set the internal statistics prefix
  [Issue #25](https://github.com/grobian/carbon-c-relay/issues/25)
  [Issue #113](https://github.com/grobian/carbon-c-relay/issues/113)
  [Issue #249](https://github.com/grobian/carbon-c-relay/issues/249).

### Bugfixes

* [Issue #249](https://github.com/grobian/carbon-c-relay/issues/249)
  `send statistics` changes metric prefix from dots into underscores


# 2.6 (2017-01-26)

### New Features

* **router** `useall` is now also allowed for `forward` and `failover`
             clusters
* **server** hostnames are now re-resolved at every connect
             [Issue #115](https://github.com/grobian/carbon-c-relay/issues/115)
* **server** when connecting to a hostname, now /all/ resolved addresses
             are tried in order before failing
* **relay** A new `-O` flag allows to set the minimum rules threshold
            before trying to optimise the rules.

### Bugfixes

* [Issue #246](https://github.com/grobian/carbon-c-relay/issues/246)
  using `stop` in a `match` rule does not stop metrics from being
  processed afterwards
* [Issue #206](https://github.com/grobian/carbon-c-relay/issues/206)
  some metrics are randomly prefixed with garbage characters


# 2.5 (2017-01-09)

### Bugfixes

* [Issue #239](https://github.com/grobian/carbon-c-relay/issues/239)
  segfault when date format is incorrect
* [Issue #242](https://github.com/grobian/carbon-c-relay/issues/242)
  dispatcher/aggregations broken (relay seems too slow)


# 2.4 (2017-01-03)

### New Features

* **router** `match` rules now support a `validate` clause to do data
             filtering, [Issue #228](https://github.com/grobian/carbon-c-relay/issues/228),
			 [Issue #142](https://github.com/grobian/carbon-c-relay/issues/142),
			 [Issue #121](https://github.com/grobian/carbon-c-relay/issues/121),
			 [Pull #127](https://github.com/grobian/carbon-c-relay/issues/127),
			 [Pull #87](https://github.com/grobian/carbon-c-relay/issues/87).

### Bugfixes

* **server** connection errors are no longer endlessly repeated
* [Issue #240](https://github.com/grobian/carbon-c-relay/issues/240)
  'include' directive doesn't care about rewrites.
* [Issue #241](https://github.com/grobian/carbon-c-relay/issues/241)
  XXX characters being prepended to metrics when sent via UDP
* [Issue #204](https://github.com/grobian/carbon-c-relay/issues/204)
  relay is sending data randomly while kill -HUP happens


# 2.3 (2016-11-07)

### Bugfixes

* [Issue #213](https://github.com/grobian/carbon-c-relay/issues/213)
  Change to aggregates to not cause HUP to reload when more than 10
  aggregates are defined.
* [Issue #214](https://github.com/grobian/carbon-c-relay/issues/214)
  `-U` option doesn't set UDP receive buffer size.
* [Issue #218](https://github.com/grobian/carbon-c-relay/issues/218)
  zeros inserted after some metrics.
* [Issue #219](https://github.com/grobian/carbon-c-relay/issues/219)
  should fail if port is unavailable
* [Issue #224](https://github.com/grobian/carbon-c-relay/issues/224)
  segfault during SIGHUP


# 2.2 (2016-09-11)

### New Features

* **relay** socket receive and send buffer sizes can be adjusted
            the `-U` option was introduced to allow setting the socket
            buffer size in bytes, [Issue #207](https://github.com/grobian/carbon-c-relay/issues/207).

### Breaking Changes

### Enhancements

* **relay** the listen backlog default got increased from 3 to 32
* **server** TCP\_NODELAY is now set to improve small writes

### Bugfixes

* [Issue #188](https://github.com/grobian/carbon-c-relay/issues/188)
  SIGHUP leads to SIGSEGV when config didn't change, or a SIGHUP is
  received while a previous HUP is being processed
* [Issue #193](https://github.com/grobian/carbon-c-relay/issues/193)
  race condition in aggregator leads to crash
* [Issue #195](https://github.com/grobian/carbon-c-relay/issues/195)
  assertion fails when reloading config
* [Issue #200](https://github.com/grobian/carbon-c-relay/issues/200)
  use after free during shutdown in aggregator
* [Issue #203](https://github.com/grobian/carbon-c-relay/issues/203)
  change default connection listen backlog
* [Issue #208](https://github.com/grobian/carbon-c-relay/issues/208)
  TCP\_NODELAY should be off for connections relaying data
* [Issue #199](https://github.com/grobian/carbon-c-relay/issues/199)
  Various race conditions reported by TSAN


# 2.1 (16-06-2016)

### Enhancements

* **router** the optimiser now tries harder to form groups of
  consecutive rules that have a matching common pattern

### Bugfixes

* [Issue #180](https://github.com/grobian/carbon-c-relay/issues/180)
  include directive possibly overrides previous included components
* [Issue #184](https://github.com/grobian/carbon-c-relay/issues/184)
  router optimise doesn't work correctly with regex groups

# 2.0 (30-05-2016)

### New Features
* **router** `include` directive was added to add content of another
             file, see also [Issue #165](https://github.com/grobian/carbon-c-relay/issues/165).  The include can also use glob patterns, see [Pull #174](https://github.com/grobian/carbon-c-relay/pull/174)
* **server** the number of stalls performed on writes can now be
             controlled (and also disabled) using the `-L` flag.
             [Issue #172](https://github.com/grobian/carbon-c-relay/issues/172)

### Breaking Changes

### Enhancements
* **server** incomplete writes are now retried a couple of times before
             they are considered fatal.  This should reduce the amount
             of messages in the logs about them, and be more like the
             consumer expects, e.g. less sudden disconnects for the
             client.
* **router** reloading the config now prints the difference between the
             old and the new config in `diff -u` format.
* **router** reloading the config now maintains the queues for the
             servers, such that unavailable servers don't get metrics
             dropped.

### Bugfixes
* [Issue #154](https://github.com/grobian/carbon-c-relay/issues/159)
  when a store becomes a bottleneck it shouldn't indefinitely stall
* [Issue #164](https://github.com/grobian/carbon-c-relay/issues/164)
  config reload should re-use unmodified servers

# 1.11 (23-03-2016)

### New Features
* **router** `send statistics to` construct was added to direct internal
  statistics to a specific cluster

### Enhancements
* **collector** UDP connections are now suffixed with `-udp` in
  destination target

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
