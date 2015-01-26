carbon-c-relay
==============

Carbon-like graphite line mode relay.

This project aims to be a replacement of the original [Carbon relay](http://graphite.readthedocs.org/en/1.0/carbon-daemons.html#carbon-relay-py)

The main reason to build a replacement is performance and
configurability.  Carbon is single threaded, and sending metrics to
multiple consistent-hash clusters requires chaining of relays.  This
project provides a multithreaded relay which can address multiple
targets and clusters for each and every metric based on pattern matches.

There are a couple more replacement projects out there we know of, which
are [carbon-relay-ng](https://github.com/rcrowley/carbon-relay-ng) and [graphite-relay](https://github.com/markchadwick/graphite-relay
)

Compared to carbon-relay-ng, this project does provide carbon's
consistent-hash routing.  graphite-relay, which does this, however
doesn't do metric-based matches to direct the traffic, which this
project does as well.  To date, carbon-c-relay can do aggregations,
failover targets and more.

The relay is a simple program that reads its routing information from a
file.  The command line arguments allow to set the location for this
file, as well as the amount of dispatchers (worker threads) to use for
reading the data from incoming connections and passing them onto the
right destination(s).  The route file supports two main constructs:
clusters and matches.  The first define groups of hosts data metrics can
be sent to, the latter define which metrics should be sent to which
cluster.  Aggregation rules are seen as matches.

For every metric received by the relay, cleansing is performed.  The
following changes are performed before any match, aggregate or rewrite
rule sees the metric:

  - double dot elimination (necessary for correctly functioning
    consistent hash routing)
  - trailing/leading dot elimination
  - whitespace normalisation (this mostly affects output of the relay
    to other targets: metric, value and timestamp will be separated by
    a single space only, ever)
  - irregular char replacement with underscores (\_), currently
    irregular is defined as not being in [0-9a-zA-Z-_:#].

The route file syntax is as follows:

```
# comments are allowed in any place and start with a hash (#)

cluster <name>
    <forward | any_of [useall] | failover | <carbon_ch | fnv1a_ch> [replication <count>]>
        <host[:port] [proto <udp | tcp>]> ...
    ;
match <* | <expression>>
    send to <cluster | blackhole>
    [stop]
    ;
rewrite <expression>
    into <replacement>
    ;
aggregate
        <expression> ...
    every <interval> seconds
    expire after <expiration> seconds
    compute <sum | count | max | min | average> write to
        <metric>
    [compute ...]
    ;
```

Multiple clusters can be defined, and need not to be referenced by a
match rule.   All clusters point to one or more hosts.  `host` may be an
IPv4 or IPv6 address, or a hostname.  Since host is followed by an
optional `:` and port, for IPv6 addresses not to be interpreted wrongly,
either a port must be given, or the IPv6 address surrounded by brackets,
e.g. `[::1]`.  An optional `proto udp` or `proto tcp` may be added to
specify the use of UDP or TCP to connect to the remote server.  When
omitted this defaults to a TCP connection.

A `forward` cluster simply sends everything it receives to all defined
members (host addresses).  The `any_of` cluster is a small
variant of the `forward` cluster, but instead of sending to all defined
members, it sends each incoming metric to one of defined members.  This
is not much useful in itself, but since any of the members can receive
each metric, this means that when one of the members is unreachable, the
other members will receive all of the metrics.  This can be useful when
the cluster points to other relays.  The `any_of` router tries to send
the same metrics consistently to the same destination.  The `failover`
cluster is like the `any_of` cluster, but sticks to the order in which
servers are defined.  This is to implement a pure failover scenario
between servers.  The `carbon_ch` cluster sends the metrics to the
member that is responsible according to the consistent hash algorithm
(as used in the original carbon), or multiple members if replication is
set to more than 1.  The `fnv1a_ch` cluster is a identical in behaviour
to `carbon_ch`, but it uses a different hash technique (FNV1a) which is
faster but more importantly defined to get by a limitation of
`carbon_ch` to use both host and port from the members.  This is useful
when multiple targets live on the same host just separated by port.  The
instance that original carbon uses to get around this can be set by
appending it after the port, separated by an equals sign, e.g.
`127.0.0.1:2006=a` for instance `a`.

DNS hostnames are resolved to a single address, according to the preference
rules in [RFC 3484](https://www.ietf.org/rfc/rfc3484.txt).  The `any_of`
cluster has an explicit `usedns` flag that enables a hostname to resolve to
multiple addresses.  Each address returned becomes a cluster destination.

Match rules are the way to direct incoming metrics to one or more
clusters.  Match rules are processed top to bottom as they are defined
in the file.  Each match rule can send data to just one cluster.  Since
match rules "fall through" unless the `stop` keyword is added to the
match rule, the same match expression can be used to target multiple
clusters.  This ability allows to replicate metrics, as well as send
certain metrics to alternative clusters with careful ordering and usage
of the `stop` keyword.  The special cluster `blackhole` discards any
metrics sent to it.  This can be useful for weeding out unwanted metrics
in certain cases.  Because throwing metrics away is pointless if other
matches would accept the same data, a match with destination the
blackhole cluster, has an implicit `stop`.

Rewrite rules take a regular input to match incoming metrics, and
transform them into the desired new metric name.  In the replacement,
backreferences are allowed to match capture groups defined in the input
regular expression.  A match of `server\.(x|y|z)\.` allows to use e.g.
`role.\1.` in the substitution.  A few caveats apply to the current
implementation of rewrite rules.  First, their location in the config
file determines when the rewrite is performed.  The rewrite is done
in-place, as such a match rule before the rewrite would match the
original name, a match rule after the rewrite no longer matches the
original name.  Care should be taken with the ordering, as multiple
rewrite rules in succession can take place, e.g. `a` gets replaced by
`b` and `b` gets replaced by `c` in a succeeding rewrite rule.  The
second caveat with the current implementation, is that the rewritten
metric names are not cleansed, like newly incoming metrics are.  Thus,
double dots and potential dangerous characters can appear if the
replacement string is crafted to produce them.  It is the responsibility
of the writer to make sure the metrics are clean.  If this is an issue
for routing, one can consider to have a rewrite-only instance that
forwards all metrics to another instance that will do the routing.
Obviously the second instance will cleanse the metrics as they come in.

The aggregations defined take one or more input metrics expressed by one
or more regular expresions, similar to the match rules.  Incoming
metrics are aggregated over a period of time defined by the interval in
seconds.  Since events may arrive a bit later in time, the expiration
time in seconds defines when the aggregations should be considered
final, as no new entries are allowed to be added any more.  On top of an
aggregation multiple aggregations can be computed.  They can be of the
same or different aggregation types, but should write to a unique new
metric.  The metric names can include back references like in rewrite
expressions, allowing for powerful single aggregation rules that yield
in many aggregations.  Produced metrics are sent to the relay as if they
were submitted from the outside, hence match and aggregation rules apply
to those.  Care should be taken that loops are avoided.  Also, since
aggregations appear as matches without `stop` keyword, their positioning
matters in the same way ordering of match statements.


Examples
--------
Carbon-c-relay evolved over time, growing features on demand as the tool
proved to be stable and fitting the job well.  Below follow some
annotated examples of constructs that can be used with the relay.

Clusters are defined as much as are necessary.  They receive data from
match rules, then their type defines which members of the cluster get
the metric data.  The simplest cluster form is a `forward` cluster:

    cluster send-through
        forward
            10.1.0.1
        ;

Any metric send the `send-through` cluster would simply be forwarded to
the server at IPv4 address `10.1.0.1`.  If we define multiple servers,
all of those servers would get the same metric, thus:

    cluster send-through
        forward
            10.1.0.1
            10.2.0.1
        ;

The above results in a duplication of metrics send to both machines.
This can be useful, but most of the time it is not.  The `any_of`
cluster type is like `forward`, but it sends each incoming metric to any
of the members.  The same example with such cluster would be:

    cluster send-to-any-one
        any_of 10.1.0.1:2010 10.1.0.1:2011;

This would implement a fail-over scenario, where two servers are used,
the load between them is spreaded, but should any of them fail, all
metrics are send to the remaining one.  This typically works well for
upstream relays, or for balancing carbon-cache processes running on the
same machine.  Should any member become unavailable, for instance due to
rolling restart, the other members receive the traffic.  This is
different from the two consistent hash cluster types:

    cluster graphite
        carbon_ch
            127.0.0.1:2006=a
            127.0.0.1:2007=b
            127.0.0.1:2008=c
        ;

If a member in this example fails, all metrics that would go to that
members are kept in the queue, waiting for the member to return.  This
is useful for carbon-cache machines where it is desirable that the same
metric ends up on the same server always.  The `carbon_ch` cluster type
is compatible with carbon-relay consistent hash, and can be used for
existing clusters populated by carbon-relay.  For new clusters, however,
it is better to use the `fnv1a_ch` cluster type, for it is faster, and
allows to balance over the same address but different ports without an
instance number, unlike `carbon_ch`.

Because we can use multiple clusters, we can also replicate without the
use of the `forward` cluster type, in a more intelligent way:

    cluster dc-old
        carbon_ch replication 2
            10.1.0.1
            10.1.0.2
            10.1.0.3
        ;
    cluster dc-new1
        fnv1a_ch replication 2
            10.2.0.1
            10.2.0.2
            10.2.0.3
        ;
    cluster dc-new2
        fnv1a_ch replication 2
            10.3.0.1
            10.3.0.2
            10.3.0.3
        ;

    match *
        send to dc-old
        ;
    match *
        send to dc-new1
        ;
    match *
        send to dc-new2
        stop
        ;

In this example all incoming metrics are first sent to `dc-old`, then
`dc-new1` and finally to `dc-new2`.  Note that the cluster type of
`dc-old` is different.  Each incoming metric will be send to 2 members
of all three clusters, thus replicating to in total 6 destinations.  For
each cluster the destination members are computed independently.
Failure of clusters or members does not affect the others, since all
have individual queues.  The `stop` rule in `dc-new2` match rule is not
strictly necessary in this example, because there are no more following
match rules.  However, if the match would target a specific subset, e.g.
`^sys\.`, and more clusters would be defined, this could be necessary,
as for instance in the following abbreviated example:

    cluster dc1-sys ... ;
    cluster dc2-sys ... ;

    cluster dc1-misc ... ;
    cluster dc2-misc ... ;

    match ^sys\. send to dc1-sys;
    match ^sys\. send to dc2-sys stop;

    match * send to dc1-misc;
    match * send to dc2-misc stop;

As can be seen, without the `stop` in dc2-sys' match rule, all metrics
starting with `sys.` would also be send to dc1-misc and dc2-misc.  It
can be that this is desired, of course, but in this example there is a
dedicated cluster for the `sys` metrics.

Suppose there would be some unwanted metric that unfortunately is
generated, let's assume some bad/old software.  We don't want to store
this metric.  The `blackhole` cluster is suitable for that, when it is
harder to actually whitelist all wanted metrics.  Consider the
following:

    match some_legacy$
        send to blackhole
        stop;

This would throw away all metrics that end with `some_legacy`, that
would otherwise be hard to filter out.  Since the order matters, it
can be used in a construct like this:

    cluster old ... ;
    cluster new ... ;

    match * send to old;

    match unwanted send to blackhole stop;

    match * send to new;

In this example the old cluster would receive the metric that's unwanted
for the new cluster.  So, the order in which the rules occur does
matter for the execution.

The relay is capable of rewriting incoming metrics on the fly.  This
process is done based on regular expressions with capture groups that
allow to substitute parts in a replacement string.  Rewrite rules allow
to cleanup metrics from applications, or provide a migration path.  In
it's simplest form a rewrite rule looks like this:

    rewrite ^server\.(.+)\.(.+)\.([a-zA-Z]+)([0-9]+)
        into server.\1.\2.\3.\3\4
        ;

In this example a metric like `server.dc.role.name123` would be
transformed into `server.dc.role.name.name123`.
For rewrite rules hold the same as for matches, that their order
matters.  Hence to build on top of the old/new cluster example done
earlier, the following would store the original metric name in the old
cluster, and the new metric name in the new cluster:

    match * send to old;

    rewrite ... ;

    match * send to new;

Note that after the rewrite, the original metric name is no longer
available, as the rewrite happens in-place.

Aggregations are probably the most complex part of carbon-c-relay.
Compared to carbon-aggregator, the capabilities of carbon-c-relay are
limited.  Yet for certain scenarios, the aggregate functionalities can
be very useful.  Aggregations are performed on a static set of
aggregates, defined in the configuration.  That is, at the time of this
writing, it is not possible to have dynamically generated aggregates,
e.g. for each hostname encountered.  A typical aggregation looks like:

    aggregate
            ^sys\.dc1\.somehost-[0-9]+\.somecluster\.mysql\.replication_delay
            ^sys\.dc2\.somehost-[0-9]+\.somecluster\.mysql\.replication_delay
        every 10 seconds
        expire after 35 seconds
        compute sum write to
            mysql.somecluster.total_replication_delay
        compute average write to
            mysql.somecluster.average_replication_delay
        compute max write to
            mysql.somecluster.max_replication_delay
        compute count write to
            mysql.somecluster.replication_delay_metric_count
        ;

In this example, four aggregations are produced from the incoming
matching metrics.  In this example we could have written the two matches
as one, but for demonstration purposes we did not.  Obviously they can
refer to different metrics, if that makes sense.  The `every 10 seconds`
clause specifies in what interval the aggregator can expect new metrics
to arrive.  This interval is used to produce the aggregations, thus each
10 seconds 4 new metrics are generated from the data received sofar.
Because data may be in transit for some reason, or generation stalled,
the `expire after` clause specifies how long the data should be kept
before considering a data bucket (which is aggregated) to be complete.
In the example, 35 was used, which means after 35 + 10 seconds the first
four aggregate metrics are produced.  It also means that metrics can
arrive 35 seconds late, and still be taken into account.
The `compute` clauses demonstrate a single aggregation rule can produce
multiple aggregates, as often is the case.  Internally, this comes for
free, since all possible aggregates are always calculated, whether or
not they are used.  The produced new metrics are resubmitted to the
relay, hence matches defined before in the configuration can match
output of the aggregator.  It is important to avoid loops, that can be
generated this way.  In general, splitting aggregations to their own
carbon-c-relay instance, such that it is easy to forward the produced
metrics to another relay instance is a good practice.

The previous example could also be written as follows to be more
dynamic:

    aggregate
            ^sys\.dc[0-9].(somehost-[0-9]+)\.([^.]+)\.mysql\.replication_delay
        every 10 seconds
        expire after 35 seconds
        compute sum write to
            mysql.host.\1.replication_delay
        compute sum write to
            mysql.host.all.replication_delay
        compute sum write to
            mysql.cluster.\2.replication_delay
        compute sum write to
            mysql.cluster.all.replication_delay
        ;

Here a single match, results in four aggregations, each of a different
scope.  In this example aggregation based on hostname and cluster are
being made, as well as the more general `all` targets, which in this
example have both identical values.  Note that with this single
aggregation rule, both per-cluster, per-host and total aggregations are
produced.  Obviously, the input metrics define which hosts and clusters
are produced.


Author
------
Fabian Groffen


Acknowledgement
---------------
This program was originally developed for Booking.com.  With approval
from Booking.com, the code was generalised and published as Open Source
on github, for which the author would like to express his gratitude.
