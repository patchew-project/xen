#!/usr/bin/env python

import re,sys

try:
    type(str.maketrans)
except AttributeError:
    # For python2
    import string as str

pats = [
 [ r"__InClUdE__(.*)", r"#include\1\n#pragma pack(4)" ],
 [ r"__IfDeF__ (XEN_HAVE.*)", r"#ifdef \1" ],
 [ r"__ElSe__", r"#else" ],
 [ r"__EnDif__", r"#endif" ],
 [ r"__DeFiNe__", r"#define" ],
 [ r"__UnDeF__", r"#undef" ],
 [ r"\"xen-compat.h\"", r"<public/xen-compat.h>" ],
 [ r"(struct|union|enum)\s+(xen_?)?(\w)", r"\1 compat_\3" ],
 [ r"@KeeP@", r"" ],
 [ r"_t([^\w]|$)", r"_compat_t\1" ],
 [ r"(8|16|32|64)_compat_t([^\w]|$)", r"\1_t\2" ],
 [ r"(^|[^\w])xen_?(\w*)_compat_t([^\w]|$$)", r"\1compat_\2_t\3" ],
 [ r"(^|[^\w])XEN_?", r"\1COMPAT_" ],
 [ r"(^|[^\w])Xen_?", r"\1Compat_" ],
 [ r"(^|[^\w])long([^\w]|$$)", r"\1int\2" ]
];

output_filename = sys.argv[1]

# tr '[:lower:]-/.' '[:upper:]___'
header_id = '_' + \
    output_filename.upper().translate(str.maketrans('-/.','___'))

header = """#ifndef {0}
#define {0}
#include <xen/compat.h>""".format(header_id)

print(header)

if not re.match("compat/arch-.*.h$", output_filename):
    x = output_filename.replace("compat/","public/")
    print('#include <%s>' % x)

def print_if_nonempty(s):
    if len(s):
        print(s)

print_if_nonempty(sys.argv[2])

for line in sys.stdin.readlines():
    for pat in pats:
        line = re.sub(pat[0], pat[1], line.rstrip())
    print_if_nonempty(line)

print_if_nonempty(sys.argv[3])

print("#endif /* %s */" % header_id)
