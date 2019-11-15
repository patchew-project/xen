#!/usr/bin/python

import os
import sys

sys.path.append('{}/tools/libxl'.format(os.environ['XEN_ROOT']))
import idl

# Go versions of some builtin types.
# Append the libxl-defined builtins after IDL parsing.
builtin_type_names = {
    idl.bool.typename: 'bool',
    idl.string.typename: 'string',
    idl.integer.typename: 'int',
    idl.uint8.typename: 'byte',
    idl.uint16.typename: 'uint16',
    idl.uint32.typename: 'uint32',
    idl.uint64.typename: 'uint64',
}

# Some go keywords that conflict with field names in libxl structs.
go_keywords = ['type', 'func']

go_builtin_types = ['bool', 'string', 'int', 'byte',
                    'uint16', 'uint32', 'uint64']

def xenlight_golang_generate_types(path = None, types = None, comment = None):
    """
    Generate a .go file (types.gen.go by default)
    that contains a Go type for each type in types.
    """
    if path is None:
        path = 'types.gen.go'

    with open(path, 'w') as f:
        if comment is not None:
            f.write(comment)
        f.write('package xenlight\n')

        for ty in types:
            (tdef, extras) = xenlight_golang_type_define(ty)

            f.write(tdef)
            f.write('\n')

            # Append extra types
            for extra in extras:
                f.write(extra)
                f.write('\n')

    go_fmt(path)

def xenlight_golang_type_define(ty = None):
    """
    Generate the Go type definition of ty.

    Return a tuple that contains a string with the
    type definition, and a (potentially empty) list
    of extra definitions that are associated with
    this type.
    """
    if isinstance(ty, idl.Enumeration):
        return (xenlight_golang_define_enum(ty), [])

    elif isinstance(ty, idl.Aggregate):
        return xenlight_golang_define_struct(ty)

def xenlight_golang_define_enum(ty = None):
    s = ''
    typename = ''

    if ty.typename is not None:
        typename = xenlight_golang_fmt_name(ty.typename)
        s += 'type {} int\n'.format(typename)

    # Start const block
    s += 'const(\n'

    for v in ty.values:
        name = xenlight_golang_fmt_name(v.name)
        s += '{} {} = {}\n'.format(name, typename, v.value)

    # End const block
    s += ')\n'

    return s

def xenlight_golang_define_struct(ty = None, typename = None, nested = False):
    s = ''
    extras = []
    name = ''

    if typename is not None:
        name = xenlight_golang_fmt_name(typename)
    else:
        name = xenlight_golang_fmt_name(ty.typename)

    # Begin struct definition
    if nested:
        s += '{} struct {{\n'.format(name)
    else:
        s += 'type {} struct {{\n'.format(name)

    # Write struct fields
    for f in ty.fields:
        if f.type.typename is not None:
            if isinstance(f.type, idl.Array):
                typename = f.type.elem_type.typename
                typename = xenlight_golang_fmt_name(typename)
                name     = xenlight_golang_fmt_name(f.name)

                s += '{} []{}\n'.format(name, typename)
            else:
                typename = f.type.typename
                typename = xenlight_golang_fmt_name(typename)
                name     = xenlight_golang_fmt_name(f.name)

                s += '{} {}\n'.format(name, typename)

        elif isinstance(f.type, idl.Struct):
            r = xenlight_golang_define_struct(f.type, typename=f.name, nested=True)

            s += r[0]
            extras.extend(r[1])

        elif isinstance(f.type, idl.KeyedUnion):
            r = xenlight_golang_define_union(f.type, ty.typename)

            s += r[0]
            extras.extend(r[1])

        else:
            raise Exception('type {} not supported'.format(f.type))

    # End struct definition
    s += '}\n'

    return (s,extras)

def xenlight_golang_define_union(ty = None, structname = ''):
    """
    Generate the Go translation of a KeyedUnion.

    Define an unexported interface to be used as
    the type of the union. Then, define a struct
    for each field of the union which implements
    that interface.
    """
    s = ''
    extras = []

    interface_name = '{}_{}_union'.format(structname, ty.keyvar.name)
    interface_name = xenlight_golang_fmt_name(interface_name, exported=False)

    s += 'type {} interface {{\n'.format(interface_name)
    s += 'is{}()\n'.format(interface_name)
    s += '}\n'

    extras.append(s)

    for f in ty.fields:
        if f.type is None:
            continue

        # Define struct
        name = '{}_{}_union_{}'.format(structname, ty.keyvar.name, f.name)
        r = xenlight_golang_define_struct(f.type, typename=name)
        extras.append(r[0])
        extras.extend(r[1])

        # Define function to implement 'union' interface
        name = xenlight_golang_fmt_name(name)
        s = 'func (x {}) is{}(){{}}\n'.format(name, interface_name)
        extras.append(s)

    fname = xenlight_golang_fmt_name(ty.keyvar.name)
    ftype = xenlight_golang_fmt_name(ty.keyvar.type.typename)
    s = '{} {}\n'.format(fname, ftype)

    fname = xenlight_golang_fmt_name('{}_union'.format(ty.keyvar.name))
    s += '{} {}\n'.format(fname, interface_name)

    return (s,extras)

def xenlight_golang_generate_helpers(path = None, types = None, comment = None):
    """
    Generate a .go file (helpers.gen.go by default)
    that contains helper functions for marshaling between
    C and Go types.
    """
    if path is None:
        path = 'helpers.gen.go'

    with open(path, 'w') as f:
        if comment is not None:
            f.write(comment)
        f.write('package xenlight\n')

        # Cgo preamble
        f.write('/*\n')
        f.write('#cgo LDFLAGS: -lxenlight\n')
        f.write('#include <stdlib.h>\n')
        f.write('#include <libxl.h>\n')
        f.write('\n')

        f.write('*/\nimport "C"\n')

        for ty in types:
            if not isinstance(ty, idl.Struct):
                continue

            f.write(xenlight_golang_define_from_C(ty))
            f.write('\n')

    go_fmt(path)

def xenlight_golang_define_from_C(ty = None, typename = None, nested = False):
    s = ''

    gotypename = ctypename = ''

    if typename is not None:
        gotypename = xenlight_golang_fmt_name(typename)
        ctypename  = typename
    else:
        gotypename = xenlight_golang_fmt_name(ty.typename)
        ctypename  = ty.typename

    if not nested:
        s += 'func (x *{}) fromC(xc *C.{}) error {{\n'.format(gotypename,ctypename)

    for f in ty.fields:
        if f.type.typename is not None:
            if isinstance(f.type, idl.Array):
                # TODO
                continue

            gotypename = xenlight_golang_fmt_name(f.type.typename)
            gofname    = xenlight_golang_fmt_name(f.name)
            cfname     = f.name

            # In cgo, C names that conflict with Go keywords can be
            # accessed by prepending an underscore to the name.
            if cfname in go_keywords:
                cfname = '_' + cfname

            # If this is nested, we need the outer name too.
            if nested and typename is not None:
                goname = xenlight_golang_fmt_name(typename)
                goname = '{}.{}'.format(goname, gofname)
                cname  = '{}.{}'.format(typename, cfname)

            else:
                goname = gofname
                cname  = cfname

            # Types that satisfy this condition can be easily casted or
            # converted to a Go builtin type.
            is_castable = (f.type.json_parse_type == 'JSON_INTEGER' or
                           isinstance(f.type, idl.Enumeration) or
                           gotypename in go_builtin_types)

            if is_castable:
                # Use the cgo helper for converting C strings.
                if gotypename == 'string':
                    s += 'x.{} = C.GoString(xc.{})\n'.format(goname, cname)
                    continue

                s += 'x.{} = {}(xc.{})\n'.format(goname, gotypename, cname)

            else:
                # If the type is not castable, we need to call its fromC
                # function.
                varname = '{}_{}'.format(f.type.typename,f.name)
                varname = xenlight_golang_fmt_name(varname, exported=False)

                s += 'var {} {}\n'.format(varname, gotypename)
                s += 'if err := {}.fromC(&xc.{});'.format(varname, cname)
                s += 'err != nil {\n return err\n}\n'
                s += 'x.{} = {}\n'.format(goname, varname)

        elif isinstance(f.type, idl.Struct):
            s += xenlight_golang_define_from_C(f.type, typename=f.name, nested=True)

        elif isinstance(f.type, idl.KeyedUnion):
            pass

        else:
            raise Exception('type {} not supported'.format(f.type))

    if not nested:
        s += 'return nil'
        s += '}\n'

    return s

def xenlight_golang_fmt_name(name, exported = True):
    """
    Take a given type name and return an
    appropriate Go type name.
    """
    if name in builtin_type_names.keys():
        return builtin_type_names[name]

    # Name is not a builtin, format it for Go.
    words = name.split('_')

    # Remove 'libxl' prefix
    if words[0].lower() == 'libxl':
        words.remove(words[0])

    if exported:
        return ''.join(x.title() for x in words)

    return words[0] + ''.join(x.title() for x in words[1:])

def go_fmt(path):
    """ Call go fmt on the given path. """
    os.system('go fmt {}'.format(path))

if __name__ == '__main__':
    idlname = sys.argv[1]

    (builtins, types) = idl.parse(idlname)

    for b in builtins:
        name = b.typename
        builtin_type_names[name] = xenlight_golang_fmt_name(name)

    header_comment="""// DO NOT EDIT.
    //
    // This file is generated by:
    // {}
    //
    """.format(' '.join(sys.argv))

    xenlight_golang_generate_types(types=types,
                                   comment=header_comment)
    xenlight_golang_generate_helpers(types=types,
                                     comment=header_comment)
