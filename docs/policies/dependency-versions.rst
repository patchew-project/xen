.. SPDX-License-Identifier: CC-BY-4.0

Build and runtime dependencies
==============================

Xen depends on other programs and libraries to build and to run.
Chosing a minimum version of these tools to support requires a careful
balance: Supporting older versions of these tools or libraries means
that Xen can compile on a wider variety of systems; but means that Xen
cannot take advantage of features available in newer versions.
Conversely, requiring newer versions means that Xen can take advantage
of newer features, but cannot work on as wide a variety of systems.

Specific dependencies and versions for a given Xen release will be
listed in the toplevel README, and/or specified by the ``configure``
system.  This document lays out the principles by which those versions
should be chosen.

The general principle is this:

    Xen should build on currently-supported versions of major distros
    when released.

"Currently-supported" means whatever that distro's version of "full
support".  For instance, at the time of writing, CentOS 7 and 8 are
listed as being given "Full Updates", but CentOS 6 is listed as
"Maintenance updates"; under this criterium, we would try to ensure
that Xen could build on CentOS 7 and 8, but not on CentOS 6.

Exceptions for specific distros or tools may be made when appropriate.

One exception to this is compiler versions for the hypervisor.
Support for new instructions, and in particular support for new safety
features, may require a newer compiler than many distros support.
These will be specified in the README.

Distros we consider when deciding minimum versions
--------------------------------------------------

We currently aim to support Xen building and running on the following distributions:
Debian_,
Ubuntu_,
OpenSUSE_,
Arch Linux,
SLES_,
Yocto_,
CentOS_,
and RHEL_.

.. _Debian: https://www.debian.org/releases/
.. _Ubuntu: https://wiki.ubuntu.com/Releases
.. _OpenSUSE: https://en.opensuse.org/Lifetime
.. _SLES: https://www.suse.com/lifecycle/
.. _Yocto: https://wiki.yoctoproject.org/wiki/Releases
.. _CentOS: https://wiki.centos.org/About/Product
.. _RHEL: https://access.redhat.com/support/policy/updates/errata

Specific distro versions supported in this release
--------------------------------------------------

======== ==================
Distro   Supported releases
======== ==================
Debian   10 (Buster)
Ubuntu   20.10 (Groovy Gorilla), 20.04 (Focal Fossa), 18.04 (Bionic Beaver), 16.04 (Xenial Xerus)
OpenSUSE Leap 15.2
SLES     SLES 11, 12, 15
Yocto    3.1 (Dunfell)
CentOS   8
RHEL     8
======== ==================

.. note::

   We also support Arch Linux, but as it's a rolling distribution, the
   concept of "security supported releases" doesn't really apply.
