carbon-c-relay
==============

Carbon-like graphite line mode relay.

This project aims to be a replacement of the original Carbon relay
[#carbon]_.

.. [#carbon] http://graphite.readthedocs.org/en/1.0/carbon-daemons.html#carbon-relay-py

The main reason to build a replacement is performance and
configurability.  Carbon is single threaded, and sending metrics to
multiple consistent-hash clusters requires chaining of relays.  This
project provides a multithreaded relay which can address multiple
targets and clusters for each and every metric based on pattern matches.

There are a couple more replacement projects out there we know of, which
are carbon-relay-ng [#carbon-relay-ng]_ and graphite-relay
[#graphite-relay]_.

.. [#carbon-relay-ng] https://github.com/rcrowley/carbon-relay-ng
.. [#graphite-relay] https://github.com/markchadwick/graphite-relay

Compared to carbon-relay-ng, this project does provide carbon's
consistent-hash routing.  graphite-relay, which does this, however
doesn't do metric-based matches to direct the traffic, which this
project does as well.

The relay is a simple program that reads its routing information from a
file.  The command line arguments allow to set the location for this
file, as well as the amount of dispatchers (worker threads) to use for
reading the data from incoming connections and passing them onto the
right destination(s).  The route file supports two main constructs:
clusters and matches.  The first define groups of hosts data metrics can
be sent to, the latter define which metrics should be sent to which
cluster.  The syntax in this file is as follows:

```
# comments are allowed in any place and start with a hash (#)

cluster <name> <forward | carbon_ch [replication <count>]> <ip[:port]> ... ;

match <* | <expression>> send to <cluster> [stop] ;
```

Multiple clusters can be defined, and need not to be referenced by a
match rule.  A `forward` cluster simply sends everything it receives to
all defined members (ip addresses).  The `carbon_ch` cluster sends it to
the member that is responsible according to the consistent hash
algorithm, or multiple memmbers if replication is set to more than 1.

Match rules are the way to direct incoming metrics to one or more
clusters.  Match rules are processed top to bottom as they are defined
in the file.  Each match rule can send data to just one cluster.  Since
match rules "fall through" unless the `stop` keyword is added to the
match rule, the same match expression can be used to target multiple
clusters.  This ability allows to replicate metrics, as well as send
certain metrics to alternative clusters with careful ordering and usage
of the `stop` keyword.


Author
------
Fabian Groffen


Acknowledgement
---------------
This program was originally developed for Booking.com.  With approval
from Booking.com, the code was generalised and published as Open Source
on github, for which the author would like to express his gratitude.
