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
cluster.  Aggregation rules are seen as matches.  The syntax in this
file is as follows:

```
# comments are allowed in any place and start with a hash (#)

cluster <name>
    <forward | any_of | <carbon_ch | fnv1a_ch> [replication <count>]>
        <host[:port]> ...
    ;
match <* | <expression>>
    send to <cluster | blackhole>
    [stop]
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
match rule.  A `forward` cluster simply sends everything it receives to
all defined members (host addresses).  The `any_of` cluster is a small
variant of the `forward` cluster, but instead of sending to all defined
members, it sends each incoming metric to one of defined members.  This
is not much useful in itself, but since any of the members can receive
each metric, this means that when one of the members is unreachable, the
other members will receive all of the metrics.  This can be useful when
the cluster points to other relays.  The `any_of` router tries to send
the same metrics to the same destination.  The `carbon_ch` cluster sends
the metrics to the member that is responsible according to the
consistent hash algorithm (as used in the original carbon), or multiple
members if replication is set to more than 1.  The `fnv1a_ch` cluster is
a identical in behaviour to `carbon_ch`, but it uses a different hash
technique (FNV1a) which is faster but more importantly defined to get by
a limitation of `carbon_ch` to use both host and port from the members.
This is useful when multiple targets live on the same host just separated
by port.

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

The aggregations defined take one or more input metrics expressed by one
or more regular expresions, similar to the match rules.  Incoming
metrics are aggregated over a period of time defined by the interval in
seconds.  Since events may arrive a bit later in time, the expiration
time in seconds defines when the aggregations should be considered
final, as no new entries are allowed to be added any more.  On top of an
aggregation multiple aggregations can be computed.  They can be of the
same or different aggregation types, but should write to a unique new
metric.  Produced metrics are sent to the relay as if they were
submitted from the outside, hence match and aggregation rules apply to
those.  Care should be taken that loops are avoided.  Also, since
aggregations appear as matches without `stop` keyword, their positioning
matters in the same way ordering of match statements.


Author
------
Fabian Groffen


Acknowledgement
---------------
This program was originally developed for Booking.com.  With approval
from Booking.com, the code was generalised and published as Open Source
on github, for which the author would like to express his gratitude.
