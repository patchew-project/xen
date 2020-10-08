# Domain Context (v1)

## Background

The design for *Non-Cooperative Migration of Guests*[1] explains that extra
information is required in the migration stream to allow a guest running PV
drivers to be migrated without its co-operation. This information includes
hypervisor state such as the set of event channels in operation and the
content of the grant table.

There already exists in Xen a mechanism to save and restore *HVM context*[2].
This specification describes a new framework that will replace that
mechanism and be suitable for transferring additional *PV state* records
conveying the information mentioned above. There is also on-going work to
implement *live update* of Xen where hypervisor state must be transferred in
memory from one incarnation to the next. The framework described is designed
to also be suitable for this purpose.

## Image format

The format will read from or written to the hypervisor in a single virtually
contiguous buffer segmented into **context records** specified in the following
sections.

Fields within the records will always be aligned to their size. Padding and
reserved fields will always be set to zero when the context buffer is read
from the hypervisor and will be verified when written.
The endianness of numerical values will the native endianness of the
hypervisor. In the case of migration, that endianness is specified in the
*libxenctrl (libxc) Domain Image Format*[3].

### Record format

All records have the following format:

```
    0       1       2       3       4       5       6       7    octet
+-------+-------+-------+-------+-------+-------+-------+-------+
| type                          | instance                      |
+-------------------------------+-------------------------------+
| length                                                        |
+---------------------------------------------------------------+
| body
...
|       | padding (0 to 7 octets)                               |
+-------+-------------------------------------------------------+
```

\pagebreak
The fields are defined as follows:


| Field      | Description                                      |
|------------|--------------------------------------------------|
| `type`     | A code which determines the layout and semantics |
|            | of `body`                                        |
|            |                                                  |
| `instance` | The instance of the record                       |
|            |                                                  |
| `length`   | The length (in octets) of `body`                 |
|            |                                                  |
| `body`     | Zero or more octets of record data               |
|            |                                                  |
| `padding`  | Zero to seven octets of zero-filled padding to   |
|            | bring the total record length up to the next     |
|            | 64-bit boundary                                  |

The `instance` field is present to distinguish multiple occurences of
a record. E.g. state that is per-vcpu may need to be described in multiple
records.

The first record in the image is always a **START** record. The version of
the image format can be inferred from the `type` of this record.

## Image content

The following records are defined for the v1 image format. This set may be
extended in newer versions of the hypervisor. It is not expected that an image
saved on a newer version of Xen will need to be restored on an older version.
Therefore an image containing unrecognized record types should be rejected.

### START

```
    0       1       2       3       4       5       6       7    octet
+-------+-------+-------+-------+-------+-------+-------+-------+
| type == 1                     | instance == 0                 |
+-------------------------------+-------------------------------+
| length == 8                                                   |
+-------------------------------+-------------------------------+
| xen_major                     | xen_minor                     |
+-------------------------------+-------------------------------+
```

A type 1 **START** record implies a v1 image. If a new image format version
is needed in future then this can be indicated by a new type value for this
(first) record in the image.

\pagebreak
The record body contains the following fields:

| Field       | Description                                     |
|-------------|-------------------------------------------------|
| `xen_major` | The major version of Xen that created this      |
|             | image                                           |
|             |                                                 |
| `xen_minor` | The minor version of Xen that created this      |
|             | image                                           |

The version of Xen that created the image can be useful to the version that
is restoring the image to determine whether certain records are expected to
be present in the image. For example, a version of Xen prior to X.Y may not
generate a FOO record but Xen X.Y+ can infer its content. But Xen X.Y+1
**must** generate the FOO record as, from that version onward, its content
can no longer be safely inferred.

### END

```
    0       1       2       3       4       5       6       7    octet
+-------+-------+-------+-------+-------+-------+-------+-------+
| type == 0                     | instance == 0                 |
+-------------------------------+-------------------------------+
| length == 0                                                   |
+---------------------------------------------------------------+
```

A record of this type terminates the image. No further data from the buffer
should be consumed.

* * *

[1] See https://xenbits.xen.org/gitweb/?p=xen.git;a=blob;f=docs/designs/non-cooperative-migration.md

[2] See https://xenbits.xen.org/gitweb/?p=xen.git;a=blob;f=xen/include/public/hvm/save.h

[3] See https://xenbits.xen.org/gitweb/?p=xen.git;a=blob;f=docs/specs/libxc-migration-stream.pandoc
