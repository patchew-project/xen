# xenlight

## About

The xenlight package provides Go bindings to Xen's libxl C library via cgo. The package is currently in an unstable "preview" state. This means the package is ready for initial use and evaluation, but is not yet fully functional. Namely, only a subset of libxl's API is implemented, and breaking changes may occur in future package versions.

Much of the package is generated using the libxl IDL. Changes to the generated code can be made by modifying `tools/golang/xenlight/gengotypes.py` in the xen.git tree.

## Getting Started

```go
import (
        "xenbits.xen.org/git-http/xen.git/tools/golang/xenlight"
)
```

The module is not yet tagged independently of xen.git, so expect to see `v0.0.0-<date>-<git hash>` as the package version. If you want to point to a Xen release, such as 4.14.0, you can run `go get xenbits.xen.org/git-http/xen.git/tools/golang/xenlight@RELEASE-4.14.0`.
