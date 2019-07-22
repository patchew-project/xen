Development Wishlist
====================

Remove xenstored's dependencies on unstable interfaces
------------------------------------------------------

Various xenstored implementations use libxc for two purposes.  It would be a
substantial advantage to move xenstored onto entirely stable interfaces, which
disconnects it from the internal of the libxc.

1. Foreign mapping of the store ring
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This is obsolete since :xen-cs:`6a2de353a9` (2012) which allocated grant
entries instead, to allow xenstored to function as a stub-domain without dom0
permissions.  :xen-cs:`38eeb3864d` dropped foreign mapping for cxenstored.
However, there are no OCaml bindings for libxengnttab.

Work Items:

* Minimal ``tools/ocaml/libs/xg/`` binding for ``tools/libs/gnttab/``.
* Replicate :xen-cs:`38eeb3864d` for oxenstored as well.


2. Figuring out which domain(s) have gone away
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Currently, the handling of domains is asymmetric.

* When a domain is created, the toolstack explicitly sends an
  ``XS_INTRODUCE(domid, store mfn, store evtchn)`` message to xenstored, to
  cause xenstored to connect to the guest ring, and fire the
  ``@introduceDomain`` watch.
* When a domain is destroyed, Xen fires ``VIRQ_DOM_EXC`` which is bound by
  xenstored, rather than the toolstack.  xenstored updates its idea of the
  status of domains, and fires the ``@releaseDomain`` watch.

Xenstored uses ``xc_domain_getinfo()``, to work out which domain(s) have gone
away, and only cares about the shutdown status.

Furthermore, ``@releaseDomain`` (like ``VIRQ_DOM_EXC``) is a single-bit
message, which requires all listeners to evaluate whether the message applies
to them or not.  This results in a flurry of ``xc_domain_getinfo()`` calls
from multiple entities in the system, which all serialise on the domctl lock
in Xen.

Work Items:

* Figure out how shutdown status can be expressed in a stable way from Xen.
* Figure out if ``VIRQ_DOM_EXC`` and ``@releaseDomain`` can be extended to
  carry at least a domid, to make domain shutdown scale better.
* Figure out if ``VIRQ_DOM_EXC`` would better be bound by the toolstack,
  rather than xenstored.
