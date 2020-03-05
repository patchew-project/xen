# Xenstore Migration

## Background

The design for *Non-Cooperative Migration of Guests*[1] explains that extra
save records are required in the migrations stream to allow a guest running
PV drivers to be migrated without its co-operation. Moreover the save
records must include details of registered xenstore watches as well as
content; information that cannot currently be recovered from `xenstored`,
and hence some extension to the xenstore protocol[2] will also be required.

The *libxenlight Domain Image Format* specification[3] already defines a
record type `EMULATOR_XENSTORE_DATA` but this is not suitable for
transferring xenstore data pertaining to the domain directly as it is
specified such that keys are relative to the path
`/local/domain/$dm_domid/device-model/$domid`. Thus it is necessary to
define at least one new save record type.

## Proposal

### New Save Record

A new mandatory record type should be defined within the libxenlight Domain
Image Format:

`0x00000007: DOMAIN_XENSTORE_DATA`

The format of each of these new records should be as follows:


```
0     1     2     3     4     5     6     7 octet
+------------------------+------------------------+
| type                   | record specific data   |
+------------------------+                        |
...
+-------------------------------------------------+
```

NB: The record data does not contain a length because the libxenlight record
header specifies the length.


| Field  | Description                                      |
|--------|--------------------------------------------------|
| `type` | 0x00000000: invalid                              |
|        | 0x00000001: node data                            |
|        | 0x00000002: watch data                           |
|        | 0x00000003: transaction data                     |
|        | 0x00000004 - 0xFFFFFFFF: reserved for future use |


where data is always in the form of a tuple as follows


**node data**


`<path>|<perm-count>|<perm-as-string>|+<value|>`


`<path>` and `<value|>` should be suitable to formulate a `WRITE` operation
to the receiving xenstored and the `<perm-as-string>|+` list should be
similarly suitable to formulate a subsequent `SET_PERMS` operation.
`<perm-count>` specifies the number of entries in the `<perm-as-string>|+`
list and `<value|>` must be placed at the end because it may contain NUL
octets.


**watch data**


`<path>|<token>|`

`<path>` again is absolute and, together with `<token>`, should
be suitable to formulate an `ADD_DOMAIN_WATCHES` operation (see below).


**transaction data**


`<transid-count>|<transid>|+`

Each `<transid>` should be a uint32_t value represented as unsigned decimal
suitable for passing as a *tx_id* to the re-defined `TRANSACTION_START`
operation (see below). `<transid-count>` is the number of entries in the
`<transid>|+` list.


### Protocol Extension

Before xenstore state is migrated it is necessary to wait for any pending
reads, writes, watch registrations etc. to complete, and also to make sure
that xenstored does not start processing any new requests (so that new
requests remain pending on the shared ring for subsequent processing on the
new host). Hence the following operation is needed:

```
QUIESCE                 <domid>|

Complete processing of any request issued by the specified domain, and
do not process any further requests from the shared ring.
```

The `WATCH` operation does not allow specification of a `<domid>`; it is
assumed that the watch pertains to the domain that owns the shared ring
over which the operation is passed. Hence, for the tool-stack to be able
to register a watch on behalf of a domain a new operation is needed:

```
ADD_DOMAIN_WATCHES      <domid>|<watch>|+

Adds watches on behalf of the specified domain.

<watch> is a NUL separated tuple of <path>|<token>. The semantics of this
operation are identical to the domain issuing WATCH <path>|<token>| for
each <watch>.
```

The watch information for a domain also needs to be extracted from the
sending xenstored so the following operation is also needed:

```
GET_DOMAIN_WATCHES      <domid>|<index>   <gencnt>|<watch>|*

Gets the list of watches that are currently registered for the domain.

<watch> is a NUL separated tuple of <path>|<token>. The sub-list returned
will start at <index> items into the the overall list of watches and may
be truncated (at a <watch> boundary) such that the returned data fits
within XENSTORE_PAYLOAD_MAX.

If <index> is beyond the end of the overall list then the returned sub-
list will be empty. If the value of <gencnt> changes then it indicates
that the overall watch list has changed and thus it may be necessary
to re-issue the operation for previous values of <index>.
```

To deal with transactions that were pending when the domain is migrated
it is necessary to start transactions with the same `<trans-id>` in the
receiving xenstored but for them to result in an `EAGAIN` when the
`TRANSACTION_END` operation is peformed. Thus the `TRANSACTION_START`
operation needs to be re-defined as follows:

```
TRANSACTION_START	|			<transid>|
	<transid> is an opaque uint32_t represented as unsigned decimal.
    If tx_id is 0 for this operation then a new transaction will be started
    with a tx_id allocated by xenstored. If a non-0 tx_id is specified then
    a transaction with that tx_id will be started and automatically marked
    `conflicting'. The tx_id will always be passed back in <transid>.
    After this, the tx_id may be used in the request header field for
    other operations.
    When a transaction is started whole db is copied; reads and writes
    happen on the copy.
```

It may also be desirable to state in the protocol specification that
the `INTRODUCE` operation should not clear the `<gfn>` specified such that
a `RELEASE` operation followed by an `INTRODUCE` operation form an
idempotent pair. The current implementation of *C xentored* does this
(in the `domain_conn_reset()` function) but this could be dropped as this
behaviour is not currently specified and the page will always be zeroed
for a newly created domain.


* * *

[1] See https://xenbits.xen.org/gitweb/?p=xen.git;a=blob;f=docs/designs/non-cooperative-migration.md
[2] See https://xenbits.xen.org/gitweb/?p=xen.git;a=blob;f=docs/misc/xenstore.txt
[3] See https://xenbits.xen.org/gitweb/?p=xen.git;a=blob;f=docs/specs/libxl-migration-stream.pandoc
