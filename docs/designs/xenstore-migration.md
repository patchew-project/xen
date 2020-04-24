# Xenstore Migration

## Background

The design for *Non-Cooperative Migration of Guests*[1] explains that extra
save records are required in the migrations stream to allow a guest running PV
drivers to be migrated without its co-operation. Moreover the save records must
include details of registered xenstore watches as well ascontent; information
that cannot currently be recovered from `xenstored`, and hence some extension
to the xenstored implementations will also be required.

As a similar set of data is needed for transferring xenstore data from one
instance to another when live updating xenstored this document proposes an
image format for a 'migration stream' suitable for both purposes.

## Proposal

The image format consists of a _header_ followed by 1 or more _records_. Each
record consists of a type and length field, followed by any data mandated by
the record type. At minimum there will be a single record of type `END`
(defined below).

### Header

The header identifies the stream as a `xenstore` stream, including the version
of the specification that it complies with.

All fields in this header must be in _big-endian_ byte order, regardless of
the setting of the endianness bit.


```
    0       1       2       3       4       5       6       7    octet
+-------+-------+-------+-------+-------+-------+-------+-------+
| ident                                                         |
+-------------------------------+-------------------------------|
| version                       | flags                         |
+-------------------------------+-------------------------------+
```


| Field     | Description                                       |
|-----------|---------------------------------------------------|
| `ident`   | 0x78656e73746f7265 ('xenstore' in ASCII)          |
|           |                                                   |
| `version` | 0x00000001 (the version of the specification)     |
|           |                                                   |
| `flags`   | 0 (LSB): Endianness: 0 = little, 1 = big          |
|           |                                                   |
|           | 1-31: Reserved (must be zero)                     |

### Records

Records immediately follow the header and have the following format:


```
    0       1       2       3       4       5       6       7    octet
+-------+-------+-------+-------+-------+-------+-------+-------+
| type                          | len                           |
+-------------------------------+-------------------------------+
| body
...
|       | padding (0 to 7 octets)                               |
+-------+-------------------------------------------------------+
```

NOTE: padding octets here and in all subsequent format specifications must be
      zero, unless stated otherwise.


| Field  | Description                                          |
|--------|------------------------------------------------------|
| `type` | 0x00000000: END                                      |
|        | 0x00000001: GLOBAL_DATA                              |
|        | 0x00000002: CONNECTION_DATA                          |
|        | 0x00000003: WATCH_DATA                               |
|        | 0x00000004: TRANSACTION_DATA                         |
|        | 0x00000005: NODE_DATA                                |
|        | 0x00000006 - 0xFFFFFFFF: reserved for future use     |
|        |                                                      |
| `len`  | The length (in octets) of `body`                     |
|        |                                                      |
| `body` | The type-specific record data                        |

The various formats of the type-specific data are described in the following
sections:

\pagebreak

### END

The end record marks the end of the image, and is the final record
in the stream.

```
    0       1       2       3       4       5       6       7    octet
+-------+-------+-------+-------+-------+-------+-------+-------+
```


The end record contains no fields; its body length is 0.

\pagebreak

### GLOBAL_DATA

This record is only relevant for live update. It contains details of global
xenstored state that needs to be restored.

```
    0       1       2       3    octet
+-------+-------+-------+-------+
| rw-socket-fd                  |
+-------------------------------+
| ro-socket-fd                  |
+-------------------------------+
```


| Field          | Description                                  |
|----------------|----------------------------------------------|
| `rw-socket-fd` | The file descriptor of the socket accepting  |
|                | read-write connections                       |
|                |                                              |
| `ro-socket-fd` | The file descriptor of the socket accepting  |
|                | read-only connections                        |

xenstored will resume in the original process context. Hence `rw-socket-fd` and
`ro-socket-fd` simply specify the file descriptors of the sockets.


\pagebreak

### CONNECTION_DATA

For live update the image format will contain a `CONNECTION_DATA` record for
each connection to xenstore. For migration it will only contain a record for
the domain being migrated.


```
    0       1       2       3       4       5       6       7    octet
+-------+-------+-------+-------+-------+-------+-------+-------+
| conn-id                       | pad                           |
+---------------+-----------------------------------------------+
| conn-type     | conn-spec
...
+-------------------------------+-------------------------------+
| data-len                      | data
+-------------------------------+
...
```


| Field       | Description                                     |
|-------------|-------------------------------------------------|
| `conn-id`   | A non-zero number used to identify this         |
|             | connection in subsequent connection-specific    |
|             | records                                         |
|             |                                                 |
| `conn-type` | 0x0000: shared ring                             |
|             | 0x0001: socket                                  |
|             |                                                 |
| `conn-spec` | See below                                       |
|             |                                                 |
| `data-len`  | The length (in octets) of any pending data not  |
|             | yet written to the connection                   |
|             |                                                 |
| `data`      | Pending data (may be empty)                     |

The format of `conn-spec` is dependent upon `conn-type`.

\pagebreak

For `shared ring` connections it is as follows:


```
    0       1       2       3       4       5       6       7    octet
                +-------+-------+-------+-------+-------+-------+
                | domid         | tdomid        | flags         |
+---------------+---------------+---------------+---------------+
| revtchn                       | levtchn                       |
+-------------------------------+-------------------------------+
| mfn                                                           |
+---------------------------------------------------------------+
```


| Field      | Description                                      |
|------------|--------------------------------------------------|
| `domid`    | The domain-id that owns the shared page          |
|            |                                                  |
| `tdomid`   | The domain-id that `domid` acts on behalf of if  |
|            | it has been subject to an SET_TARGET             |
|            | operation [2] or DOMID_INVALID otherwise         |
|            |                                                  |
| `flags`    | A bit-wise OR of:                                |
|            | 0x0001: INTRODUCE has been issued                |
|            | 0x0002: RELEASE has been issued                  |
|            |                                                  |
| `revtchn`  | The port number of the interdomain channel used  |
|            | by `domid` to communicate with xenstored         |
|            |                                                  |
| `levtchn`  | For a live update this will be the port number   |
|            | of the interdomain channel used by xenstored     |
|            | itself otherwise, for migration, it will be -1   |
|            |                                                  |
| `mfn`      | The MFN of the shared page for a live update or  |
|            | INVALID_MFN otherwise                            |

Since the ABI guarantees that entry 1 in `domid`'s grant table will always
contain the GFN of the shared page, so for a live update `mfn` can be used to
give confidence that `domid` has not been re-cycled during the update.


For `socket` connections it is as follows:


```
    0       1       2       3       4       5       6       7    octet
                +-------+-------+-------+-------+-------+-------+
                | flags         | socket-fd                     |
                +---------------+-------------------------------+
```


| Field       | Description                                     |
|-------------|-------------------------------------------------|
| `flags`     | A bit-wise OR of:                               |
|             | 0001: read-only                                 |
|             |                                                 |
| `socket-fd` | The file descriptor of the connected socket     |

This type of connection is only relevant for live update, where the xenstored
resumes in the original process context. Hence `socket-fd` simply specify
the file descriptor of the socket connection.

\pagebreak

### WATCH_DATA

The image format will contain a `WATCH_DATA` record for each watch registered
by a connection for which there is `CONNECTION_DATA` record previously present.


```
    0       1       2       3    octet
+-------+-------+-------+-------+
| conn-id                       |
+---------------+---------------+
| wpath-len     | token-len     |
+---------------+---------------+
| wpath
...
| token
...
```


| Field       | Description                                     |
|-------------|-------------------------------------------------|
| `conn-id`   | The connection that issued the `WATCH`          |
|             | operation [2]                                   |
|             |                                                 |
| `wpath-len` | The length (in octets) of `wpath` including the |
|             | NUL terminator                                  |
|             |                                                 |
| `token-len` | The length (in octets) of `token` including the |
|             | NUL terminator                                  |
|             |                                                 |
| `wpath`     | The watch path, as specified in the `WATCH`     |
|             | operation                                       |
|             |                                                 |
| `token`     | The watch identifier token, as specified in the |
|             | `WATCH` operation                               |

\pagebreak

### TRANSACTION_DATA

The image format will contain a `TRANSACTION_DATA` record for each transaction
that is pending on a connection for which there is `CONNECTION_DATA` record
previously present.


```
    0       1       2       3    octet
+-------+-------+-------+-------+
| conn-id                       |
+-------------------------------+
| tx-id                         |
+-------------------------------+
```


| Field          | Description                                  |
|----------------|----------------------------------------------|
| `conn-id`      | The connection that issued the               |
|                | `TRANSACTION_START` operation [2]            |
|                |                                              |
| `tx-id`        | The transaction id passed back to the domain |
|                | by the `TRANSACTION_START` operation         |

\pagebreak

### NODE_DATA

For live update the image format will contain a `NODE_DATA` record for each
node in xenstore. For migration it will only contain a record for the nodes
relating to the domain being migrated. The `NODE_DATA` may be related to
a _committed_ node (globally visible in xenstored) or a _pending_ node (created
or modified by a transaction for which there is also a `TRANSACTION_DATA`
record previously present).


```
    0       1       2       3    octet
+-------+-------+-------+-------+
| conn-id                       |
+-------------------------------+
| tx-id                         |
+---------------+---------------+
| access        | perm-count    |
+---------------+---------------+
| perm1                         |
+-------------------------------+
...
+-------------------------------+
| permN                         |
+---------------+---------------+
| path-len      | value-len     |
+---------------+---------------+
| path
...
| value
...
```


| Field        | Description                                    |
|--------------|------------------------------------------------|
| `conn-id`    | If this value is non-zero then this record     |
|              | related to a pending transaction               |
|              |                                                |
| `tx-id`      | This value should be ignored if `conn-id` is   |
|              | zero. Otherwise it specifies the id of the     |
|              | pending transaction                            |
|              |                                                |
| `access`     | This value should be ignored if this record    |
|              | does not relate to a pending transaction,      |
|              | otherwise it specifies the accesses made to    |
|              | the node and hence is a bitwise OR of:         |
|              |                                                |
|              | 0x0001: read                                   |
|              | 0x0002: written                                |
|              |                                                |
|              | The value will be zero for a deleted node      |
|              |                                                |
| `perm-count` | The number (N) of node permission specifiers   |
|              | (which will be 0 for a node deleted in a       |
|              | pending transaction)                           |
|              |                                                |
| `perm1..N`   | A list of zero or more node permission         |
|              | specifiers (see below)                         |
|              |                                                |
| `path-len`   | The length (in octets) of `path` including the |
|              | NUL terminator                                 |
|              |                                                |
| `value-len`  | The length (in octets) of `value` (which will  |
|              | be zero for a deleted node)                    |
|              |                                                |
| `path`       | The absolute path of the node                  |
|              |                                                |
| `value`      | The node value (which may be empty or contain  |
|              | NUL octets)                                    |


A node permission specifier has the following format:


```
    0       1       2       3    octet
+-------+-------+-------+-------+
| perm  | pad   | domid         |
+-------+-------+---------------+
```

| Field   | Description                                         |
|---------|-----------------------------------------------------|
| `perm`  | One of the ASCII values `w`, `r`, `b` or `n` as     |
|         | specified for the `SET_PERMS` operation [2]         |
|         |                                                     |
| `domid` | The domain-id to which the permission relates       |


* * *

[1] See https://xenbits.xen.org/gitweb/?p=xen.git;a=blob;f=docs/designs/non-cooperative-migration.md

[2] See https://xenbits.xen.org/gitweb/?p=xen.git;a=blob;f=docs/misc/xenstore.txt
