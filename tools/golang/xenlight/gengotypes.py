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

# List of strings that need to be written to a file
# after a struct definition.
type_extras = []

# cgo preamble for xenlight_helpers.go, created during type generation and
# written later.
cgo_helpers_preamble = []

# List of strings that need to be written to a file
# after a helper func definition.
helper_extras = []

def xenlight_golang_generate_types(path = None, types = None, comment = None):
    """
    Generate a .go file (xenlight_types.go by default)
    that contains a Go type for each type in types.
    """
    if path is None:
        path = 'xenlight_types.go'

    with open(path, 'w') as f:
        if comment is not None:
            f.write(comment)
        f.write('package xenlight\n')

        for ty in types:
            f.write(xenlight_golang_type_define(ty))
            f.write('\n')

            # Append extra types
            for extra in type_extras:
                f.write(extra)
                f.write('\n')

            del type_extras[:]

    go_fmt(path)

def xenlight_golang_type_define(ty = None):
    s = ''

    if isinstance(ty, idl.Enumeration):
        s += xenlight_golang_define_enum(ty)

    elif isinstance(ty, idl.Aggregate):
        s += xenlight_golang_define_struct(ty)

    return s

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
            s += xenlight_golang_define_struct(f.type, typename=f.name, nested=True)

        elif isinstance(f.type, idl.KeyedUnion):
            s += xenlight_golang_define_union(f.type, ty.typename)

        else:
            raise Exception('type {} not supported'.format(f.type))

    # End struct definition
    s += '}\n'

    return s

def xenlight_golang_define_union(ty = None, structname = ''):
    """
    Generate the Go translation of a KeyedUnion.

    Define an unexported interface to be used as
    the type of the union. Then, define a struct
    for each field of the union which implements
    that interface.
    """
    s = ''

    interface_name = '{}_{}_union'.format(structname, ty.keyvar.name)
    interface_name = xenlight_golang_fmt_name(interface_name, exported=False)

    s += 'type {} interface {{\n'.format(interface_name)
    s += 'is{}()\n'.format(interface_name)
    s += '}\n'

    type_extras.append(s)

    for f in ty.fields:
        if f.type is None:
            continue

        # Define struct
        name = '{}_{}_union_{}'.format(structname, ty.keyvar.name, f.name)
        s = xenlight_golang_define_struct(f.type, typename=name)
        type_extras.append(s)

        xenlight_golang_union_cgo_preamble(f.type, name=name)

        # Define function to implement 'union' interface
        name = xenlight_golang_fmt_name(name)
        s = 'func (x {}) is{}(){{}}\n'.format(name, interface_name)
        type_extras.append(s)

    # Return the field entries, but the associated types are generated later.
    fname = xenlight_golang_fmt_name(ty.keyvar.name)
    ftype = xenlight_golang_fmt_name(ty.keyvar.type.typename)
    s = '{} {}\n'.format(fname, ftype)

    fname = xenlight_golang_fmt_name('{}_union'.format(ty.keyvar.name))
    s += '{} {}\n'.format(fname, interface_name)

    return s

def xenlight_golang_union_cgo_preamble(ty = None, name = ''):
    s = ''

    s += 'typedef struct {} {{\n'.format(name)

    for f in ty.fields:
        s += '\t{} {};\n'.format(f.type.typename, f.name)

    s += '}} {};\n'.format(name)

    cgo_helpers_preamble.append(s)

def xenlight_golang_generate_helpers(path = None, types = None, comment = None):
    """
    Generate a .go file (xenlight_helpers.go by default)
    that contains helper functions for marshaling between
    C and Go types.
    """
    if path is None:
        path = 'xenlight_helpers.go'

    with open(path, 'w') as f:
        if comment is not None:
            f.write(comment)
        f.write('package xenlight\n')
        f.write('import (\n"unsafe"\n"errors"\n"fmt"\n)\n')

        # Cgo preamble
        f.write('/*\n')
        f.write('#cgo LDFLAGS: -lxenlight\n')
        f.write('#include <stdlib.h>\n')
        f.write('#include <libxl.h>\n')
        f.write('\n')

        for s in cgo_helpers_preamble:
            f.write(s)
            f.write('\n')

        f.write('*/\nimport "C"\n')

        for ty in types:
            if not isinstance(ty, idl.Struct):
                continue

            f.write(xenlight_golang_define_from_C(ty))
            f.write('\n')

            for extra in helper_extras:
                f.write(extra)
                f.write('\n')

            del helper_extras[:]

            f.write(xenlight_golang_define_to_C(ty))
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
                s += xenlight_golang_array_from_C(f)
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
            s += xenlight_golang_union_from_C(f.type, f.name, ty.typename)

        else:
            raise Exception('type {} not supported'.format(f.type))

    if not nested:
        s += 'return nil'
        s += '}\n'

    return s

def xenlight_golang_union_from_C(ty = None, union_name = '', struct_name = ''):
    keyname   = ty.keyvar.name
    gokeyname = xenlight_golang_fmt_name(keyname)
    keytype   = ty.keyvar.type.typename
    gokeytype = xenlight_golang_fmt_name(keytype)

    interface_name = '{}_{}_union'.format(struct_name, keyname)
    interface_name = xenlight_golang_fmt_name(interface_name, exported=False)

    cgo_keyname = keyname
    if cgo_keyname in go_keywords:
        cgo_keyname = '_' + cgo_keyname

    cases = {}

    for f in ty.fields:
        val = '{}_{}'.format(keytype, f.name)
        val = xenlight_golang_fmt_name(val)

        # Add to list of cases to make for the switch
        # statement below.
        if f.type is None:
            continue

        cases[f.name] = val

        # Define fromC func for 'union' struct.
        typename   = '{}_{}_union_{}'.format(struct_name,keyname,f.name)
        gotypename = xenlight_golang_fmt_name(typename)

        # Define the function here. The cases for keyed unions are a little
        # different.
        s = 'func (x *{}) fromC(xc *C.{}) error {{\n'.format(gotypename,struct_name)
        s += 'if {}(xc.{}) != {} {{\n'.format(gokeytype,cgo_keyname,val)
        err_string = '"expected union key {}"'.format(val)
        s += 'return errors.New({})\n'.format(err_string)
        s += '}\n\n'
        s += 'tmp := (*C.{})(unsafe.Pointer(&xc.{}[0]))\n'.format(typename,union_name)

        s += xenlight_golang_union_fields_from_C(f.type)
        s += 'return nil\n'
        s += '}\n'

        helper_extras.append(s)

    s = 'x.{} = {}(xc.{})\n'.format(gokeyname,gokeytype,cgo_keyname)
    s += 'switch x.{}{{\n'.format(gokeyname)

    # Create switch statement to determine which 'union element'
    # to populate in the Go struct.
    for case_name, case_val in cases.items():
        s += 'case {}:\n'.format(case_val)

        gotype = '{}_{}_union_{}'.format(struct_name,keyname,case_name)
        gotype = xenlight_golang_fmt_name(gotype)
        goname = '{}_{}'.format(keyname,case_name)
        goname = xenlight_golang_fmt_name(goname,exported=False)

        s += 'var {} {}\n'.format(goname, gotype)
        s += 'if err := {}.fromC(xc);'.format(goname)
        s += 'err != nil {\n return err \n}\n'

        field_name = xenlight_golang_fmt_name('{}_union'.format(keyname))
        s += 'x.{} = {}\n'.format(field_name, goname)

    # End switch statement
    s += 'default:\n'
    err_string = '"invalid union key \'%v\'", x.{}'.format(gokeyname)
    s += 'return fmt.Errorf({})'.format(err_string)
    s += '}\n'

    return s

def xenlight_golang_union_fields_from_C(ty = None):
    s = ''

    for f in ty.fields:
        gotypename = xenlight_golang_fmt_name(f.type.typename)
        ctypename  = f.type.typename
        gofname    = xenlight_golang_fmt_name(f.name)
        cfname     = f.name

        is_castable = (f.type.json_parse_type == 'JSON_INTEGER' or
                       isinstance(f.type, idl.Enumeration) or
                       gotypename in go_builtin_types)

        if not is_castable:
            s += 'if err := x.{}.fromC(&tmp.{});'.format(gofname,cfname)
            s += 'err != nil {\n return err \n}\n'

        # We just did an unsafe.Pointer cast from []byte to the 'union' type
        # struct, so we need to make sure that any string fields are actually
        # converted properly.
        elif gotypename == 'string':
            s += 'x.{} = C.GoString(tmp.{})\n'.format(gofname,cfname)

        else:
            s += 'x.{} = {}(tmp.{})\n'.format(gofname,gotypename,cfname)

    return s

def xenlight_golang_array_from_C(ty = None):
    """
    Convert C array to Go slice using the method
    described here:

    https://github.com/golang/go/wiki/cgo#turning-c-arrays-into-go-slices
    """
    s = ''

    gotypename = xenlight_golang_fmt_name(ty.type.elem_type.typename)
    goname     = xenlight_golang_fmt_name(ty.name)
    ctypename  = ty.type.elem_type.typename
    cname      = ty.name
    cslice     = 'c{}'.format(goname)
    clenvar    = ty.type.lenvar.name
    golenvar   = xenlight_golang_fmt_name(clenvar,exported=False)

    s += '{} := int(xc.{})\n'.format(golenvar, clenvar)
    s += '{} := '.format(cslice)
    s +='(*[1<<28]C.{})(unsafe.Pointer(xc.{}))[:{}:{}]\n'.format(ctypename, cname,
                                                                golenvar, golenvar)
    s += 'x.{} = make([]{}, {})\n'.format(goname, gotypename, golenvar)
    s += 'for i, v := range {} {{\n'.format(cslice)

    is_enum = isinstance(ty.type.elem_type,idl.Enumeration)
    if gotypename in go_builtin_types or is_enum:
        s += 'x.{}[i] = {}(v)\n'.format(goname, gotypename)
    else:
        s += 'var e {}\n'.format(gotypename)
        s += 'if err := e.fromC(&v); err != nil {\n'
        s += 'return err }\n'
        s += 'x.{}[i] = e\n'.format(goname)

    s += '}\n'

    return s

def xenlight_golang_define_to_C(ty = None, typename = None, nested = False):
    s = ''

    gotypename = ctypename = ''

    if typename is not None:
        gotypename = xenlight_golang_fmt_name(typename)
        ctypename  = typename
    else:
        gotypename = xenlight_golang_fmt_name(ty.typename)
        ctypename  = ty.typename

    if not nested:
        s += 'func (x *{}) toC() (xc C.{},err error) {{\n'.format(gotypename,ctypename)
        s += 'C.{}(&xc)\n'.format(ty.init_fn)

    for f in ty.fields:
        if f.type.typename is not None:
            if isinstance(f.type, idl.Array):
                # TODO
                continue

            gotypename = xenlight_golang_fmt_name(f.type.typename)
            ctypename  = f.type.typename
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

            is_castable = (f.type.json_parse_type == 'JSON_INTEGER' or
                           isinstance(f.type, idl.Enumeration) or
                           gotypename in go_builtin_types)

            if is_castable:
                # Use the cgo helper for converting C strings.
                if gotypename == 'string':
                    s += 'xc.{} = C.CString(x.{})\n'.format(cname,goname)
                    continue

                s += 'xc.{} = C.{}(x.{})\n'.format(cname,ctypename,goname)

            else:
                s += 'xc.{}, err = x.{}.toC()\n'.format(cname,goname)
                s += 'if err != nil {\n'
                s += 'C.{}(&xc)\n'.format(ty.dispose_fn)
                s += 'return xc, err\n'
                s += '}\n'

        elif isinstance(f.type, idl.Struct):
            s += xenlight_golang_define_to_C(f.type, typename=f.name, nested=True)

        elif isinstance(f.type, idl.KeyedUnion):
            s += xenlight_golang_union_to_C(f.type, f.name, ty.typename, ty.dispose_fn)

        else:
            raise Exception('type {} not supported'.format(f.type))

    if not nested:
        s += 'return xc, nil'
        s += '}\n'

    return s

def xenlight_golang_union_to_C(ty = None, union_name = '',
                               struct_name = '', dispose_fn = ''):
    keyname   = ty.keyvar.name
    gokeyname = xenlight_golang_fmt_name(keyname)
    keytype   = ty.keyvar.type.typename
    gokeytype = xenlight_golang_fmt_name(keytype)

    interface_name = '{}_{}_union'.format(struct_name, keyname)
    interface_name = xenlight_golang_fmt_name(interface_name, exported=False)

    cgo_keyname = keyname
    if cgo_keyname in go_keywords:
        cgo_keyname = '_' + cgo_keyname


    s = 'xc.{} = C.{}(x.{})\n'.format(cgo_keyname,keytype,gokeyname)
    s += 'switch x.{}{{\n'.format(gokeyname)

    # Create switch statement to determine how to populate the C union.
    for f in ty.fields:
        key_val = '{}_{}'.format(keytype, f.name)
        key_val = xenlight_golang_fmt_name(key_val)
        if f.type is None:
            continue

        s += 'case {}:\n'.format(key_val)
        cgotype = '{}_{}_union_{}'.format(struct_name,keyname,f.name)
        gotype  = xenlight_golang_fmt_name(cgotype)
        goname  = '{}_{}'.format(keyname,f.name)
        goname  = xenlight_golang_fmt_name(goname,exported=False)

        field_name = xenlight_golang_fmt_name('{}_union'.format(keyname))
        s += 'tmp, ok := x.{}.({})\n'.format(field_name,gotype)
        s += 'if !ok {\n'
        s += 'C.{}(&xc)\n'.format(dispose_fn)
        s += 'return xc,errors.New("wrong type for union key {}")\n'.format(keyname)
        s += '}\n'

        s += 'var {} C.{}\n'.format(f.name,cgotype)
        for uf in f.type.fields:
            gotypename = xenlight_golang_fmt_name(uf.type.typename)
            ctypename  = uf.type.typename
            gofname    = xenlight_golang_fmt_name(uf.name)

            is_castable = (uf.type.json_parse_type == 'JSON_INTEGER' or
                           isinstance(uf.type, idl.Enumeration) or
                           gotypename in go_builtin_types)

            if not is_castable:
                s += '{}.{}, err = tmp.{}.toC()\n'.format(f.name,uf.name,gofname)
                s += 'if err != nil {\n'
                s += 'C.{}(&xc)\n'.format(dispose_fn)
                s += 'return xc,err \n}\n'

            elif gotypename == 'string':
                s += '{}.{} = C.CString(tmp.{})\n'.format(f.name,uf.name,gofname)

            else:
                s += '{}.{} = C.{}(tmp.{})\n'.format(f.name,uf.name,ctypename,gofname)

        # The union is still represented as Go []byte.
        s += '{}Bytes := C.GoBytes(unsafe.Pointer(&{}),C.sizeof_{})\n'.format(f.name,
                                                                              f.name,
                                                                              cgotype)
        s += 'copy(xc.{}[:],{}Bytes)\n'.format(union_name,f.name)

    # End switch statement
    s += 'default:\n'
    err_string = '"invalid union key \'%v\'", x.{}'.format(gokeyname)
    s += 'return xc, fmt.Errorf({})'.format(err_string)
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
