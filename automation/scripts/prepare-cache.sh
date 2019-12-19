#!/bin/bash

set -ex

cachedir="${CI_PROJECT_DIR:=`pwd`}/ci_cache"
mkdir -p "$cachedir"

declare -A r
r[extras/mini-os]=MINIOS_UPSTREAM_URL
r[tools/qemu-xen-dir]=QEMU_UPSTREAM_URL
r[tools/qemu-xen-traditional-dir]=QEMU_TRADITIONAL_URL
r[tools/firmware/ovmf-dir]=OVMF_UPSTREAM_URL
r[tools/firmware/seabios-dir]=SEABIOS_UPSTREAM_URL

bundle_loc() {
    echo "$cachedir/${1//\//_}.git.bundle"
}
for d in ${!r[@]}; do
    if [ -e $(bundle_loc $d) ]; then
        export ${r[$d]}=$(bundle_loc $d)
    fi
done

if ! make subtree-force-update-all; then
    # There's maybe an issue with one of the git bundle, just clear the cache
    # and allow it to be rebuilt by a different jobs.
    # Make will reclone missing clones from original URLs instead of from the
    # bundle.
    for d in ${!r[@]}; do
        rm -f "$(bundle_loc $d)"
    done
    exit
fi


tmpdir=$(mktemp -d "$CI_PROJECT_DIR/ci-tmp.XXX")
for d in ${!r[@]}; do
    bundle=$(bundle_loc $d)
    if [ -e $bundle ]; then
        # We didn't download anything new
        continue
    fi
    # We create a mirror to be able to create a bundle that is a mirror of
    # upstream. Otherwise, the bundle may not have refs that the build system
    # will want, i.e. refs/heads/master would be missing from the bundle.
    url=$(git --git-dir=$d/.git config remote.origin.url)
    repo_mirrored="$tmpdir/${d//\//_}"
    git clone --bare --mirror --reference "$d" "$url" "$repo_mirrored"
    git --git-dir="$repo_mirrored" bundle create $bundle --all
    rm -rf "$repo_mirrored"
done
rmdir "$tmpdir"
