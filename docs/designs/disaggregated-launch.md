# Introduction

Born out of improving support for Dynamic Root of Trust for Measurement (DRTM),
the DomB project is focused on restructuring the system launch of Xen. It
provides a security architecture that builds on the principles of Least
Privilege and Strong Isolation, achieving this through the disaggregation of
system functions. DomB enables this with the introduction of a boot domain that
works in conjunction with the hypervisor to provide the ability to launch
multiple domains as part of host boot while maintaining a least privilege
implementation.

While the DomB project inception was and continues to be driven by a focus on
security through disaggregation, there are multiple use cases with a
non-security focus that would benefit from or are dependent upon the ability to
launch multiple domains at host boot. This has been proven with the need that
drove the implementation of the dom0less capability in the ARM branch of Xen. As
such the design for DomB has been developed to allow for a flexible solution
that is reusable across multiple use cases. In doing so this should ensure that
DomB is capable, widely exercised, comprehensively tested, and well understood
by the Xen community.

When looking across those that have expressed interest or discussed a need for
launching multiple domains at host boot, a majority are able to be binned in one
of the four following use cases. 

  * Deprivileging Dom0: disaggregating and/or eliminating one or more of Dom0’s roles

  * System partitioning: dividing a systems resources and/or functions performed to a set of domains

  * Low-latency boot and reboot: fast launch of the initial and possible final set of domains

  * Isolated multiplexing of singular system resources: enabling dedicated domains to manage and multiplex a system resource for which only one exist but all domains must believe have exclusive access

It is with this understanding as presented that the DomB project used as the
basis for the development of its multiple domain boot capability for Xen. Within
the remainder of this document is a detailed explanation of the multiple domain
boot, the objectives it strives to achieve, the structure behind the approach,
the sequence events in a boot, a contrasting with ARM’s dom0less, and closing
with some exemplar implementations.

## Terminology

To help ensure clarity in reading this document, the following is the definition
of terminology used within this document.

* __Host Boot__: the system startup of Xen using the configuration provided by the bootloader
* __Classic Boot__: the existing host boot that ends with the launch of a single domain (Dom0)
* __Multiple Domain Boot__: a host boot that ends with the launch of one or more domains
* __Boot Domain__: a domain with limited privileges launched by the hypervisor during a Multiple Domain Boot that is responsible for assisting with higher level operations of the domain setup process
* __Initial Domain__: a domain other than the boot domain that is started as part of a multiple domain boot
* __Crash Domain__: a fallback domain that the hypervisor may launch in the event of a detectable error during the multiple domain boot process
* __Basic Configuration__: the minimal information Xen requires to instantiate a domain instance
* __Extended Configuration__: any configuration options for a domain beyond its Basic Configuration


# Approach

At the outset of design of DomB a core set of objectives were established. These
objectives were viewed as the bar that should be strived towards inorder to
minimize the impact for existing Xen environments.

## Objectives

* In general strive to maintain compatibility with existing Xen behavior
 
* A default build of Xen should be capable of booting both style of launch: Dom0 or DomB
    - Preferred that it be managed via two KCONFIG options to govern inclusion of support for each style
* The selection between classic boot and multiple domain boot should be automatic
    - Preferred that it not require a kernel command line parameter for selection
* It should not require modification to boot loaders
* It should provide a user friendly interface for its configuration and management
* It must provide a fallback to console access in the event of misconfiguration
* It should be able to boot an x86 Xen environment without the need for a Dom0 domain

## A top level look at DomB

Before delving into DomB a good basis to start with is an understanding of the
process to create a domain. A way to view this process starts with the core
configuration which is the information the hypervisor requires to make the call
to `domain_create` followed by the extended configuration used by the toolstack
to provide a domain with any additional configuration information. Until the
extended configuration is completed, a domain has access to no resources except
its allocated vcpus and memory. The exception to this is Dom0 which the
hypervisor explicitly grants control and access to all system resources except
for those that only the hypervisor should have control over. This exception for
Dom0 is driven by the system structure with a monolithic dom0 domain predating
introduction of support for disaggregation into Xen, and the corresponding
default assignment of multiple roles within the Xen system to Dom0.

The DomB approach’s primary focus is on how to assign the roles traditionally
granted to Dom0 to one or more domains at host boot. While the statement is
simple to make, the implications are not trivial by any means. This also
explains why the DomB approach is orthogonal to the existing dom0less
capability. The dom0less capability focuses on enabling the launch of multiple
domains in parallel with Dom0 at host boot. A corollary for dom0less is that for
systems that don’t require Dom0 after all guest domains have started, they are
able to do the host boot without a Dom0. Though it should be noted that it may
be possible to start  Dom0 at a later point. Whereas with DomB, its approach of
separating Dom0’s roles requires the ability to launch multiple domains at host
boot. The direct consequences from this approach are profound and provide a
myriad of possible configurations.

To enable the DomB approach a new alternative path for host boot within the
hypervisor must be introduced. This alternative path effectively branches just
before Dom0 construction and begins an alternate means of system construction.
The determination if this alternate path should be taken is through the
inspection of the Multiboot2 (MB2) chain. If the bootloader has loaded a
specific configuration, as described later, it will enable Xen to detect that a
DomB configuration has been provided. Once a DomB configuration is detected,
this alternate path can be thought of as occurring in four phases, configuration
parsing, domain creation, domain launch preparation, and boot finalization.

The configuration parsing phase begins with Xen parsing the MB2 chain, to
understand the content of the multiboot modules provided. It will then load any
microcode or XSM policy it discovers. With the domain creation phase, for each
domain configuration Xen finds, it parses the configuration to construct the
necessary domain definition to instantiate an instance of the domain and leave
it in a paused state. When all domain configurations have been instantiated as
domains, if one of them  is flagged as the Boot Domain, that domain will be
unpaused starting the domain launch preparation phase. If there is no Boot
Domain defined, then the domain launch preparation phase will be skipped and Xen
will trigger the boot finalization phase.

The domain launch preparation phase is an optional check point for the execution
of a workload specific domain, the Boot Domain. While the Boot Domain is the
first domain to run and has some degree of control over the system, similar to
Dom0, it is extremely restricted in both system resource access and hypervisor
operations. Its purpose is to:

* Access the configuration provided by the bootloader
* Finalize the configuration of the domains
* Conduct any setup and launch related operations
* Do an ordered unpause of domains that require an ordered start

When the Boot Domain has completed, it will notify the hypervisor that it is
done triggering the boot finalization phase.

The hypervisor handles the boot finalization phase which is equivalent to the
clean up phase. As such the steps taken by the hypervisor, not necessarily in
implementation order, are as follows,

* Free the MB2 module chain
* If the Boot Domain exits, reclaim Boot Domain resources
* Unpause any domains still in a paused state
* Boot Domain uses reserved domid thus can never be respawned

While the focus thus far has been on how the DomB capability will work, it is
worth mentioning what it does not do or limit from occurring. It does not stop
or inhibit the assigning of the control domain role which gives the domain the
ability to create, start, stop, restart, and destroy domains. In particular it
is still possible to construct a domain with all the privileged roles, i.e. a
Dom0, with or without the domain id being zero. In fact what limitations are
imposed now become fully configurable without the risk of circumvention by an
all privileged domain.

## Structuring of DomB

The structure of DomB is built around the existing capabilities of the MB2
protocol. This approach was driven by the objective not to require modifications
to the boot loader. As a result the DomB capability does not rely on nor
preclude any specific BIOS boot protocol, i.e legacy BIOS boot or UEFI boot. The
only requirement is that the boot loader supports MB2 protocol.

### Multiboot2

The MB2 protocol has no concept of a manifest to tell the initial kernel what is
contained in the chain, leaving it to the kernel to impose a loading convention,
use magic number identification, or both. Unfortunately for passing multiple
kernels, ramdisks, and domain configuration along with the existing modules
already passed, there is no sane convention that could be imposed and magic
number identification is nearly impossible when considering the objective not to
impose unnecessary complication to the hypervisor.

As it was just alluded to previously, a manifest describing the contents in the
MB2 chain and how they relate within a Xen context is needed. To address this
need the Launch Control Module (LCM) was designed to provide such a manifest.
The LCM was designed to have a specific set of properties,

* minimize the complexity of the parsing logic required by the hypervisor
* allow for expanding and optional configuration fragments without breaking backwards compatibility

To enable automatic detection of a DomB configuration, the LCM must be the first
MB2 module in the MB2 module chain. The LCM has a magic number in its first four
bytes to enable the hypervisor to detect its presence. The contents of the LCM
are a series of Tag, Length, Value (TLV) entries that allows,

* for the hypervisor to parse only those entries it understands,
* for packing custom information for a custom boot domain,
* the ability to use a new LCM with an older hypervisor,
* and the ability to use an older LCM with a new hypervisor.

In the initial implementation the provided LCM entries include an entry that
describes what each MB2 module is in the chain, one or more checksum entries for
providing checksums of the MB2 modules, and one or more domain configuration
description entries. The module description is a simple dynamic array of index
and type while the checksum provides an algorithm description and digest. The
more intricate entry is the domain configuration description. It consists of the
static fields detailing the type of configuration it contains and an indexing to
the MB2 modules for its kernel and ramdisk, allowing multiple domain
configurations to use the same MB2 module for its kernel and/or ramdisk.
Depending on which type of configuration has been selected results in one of two
static Basic Configurations that is for use by the hypervisor to build the
domain. The last information that can be carried in a domain configuration is an
optional Extended Configuration which is a free form data buffer. 

### LCM Utility

An objective of this project was not to require modification to bootloaders. To
maintain that requirement a utility, lcm-tool, will be provided for generating
the LCM. The utility will be capable of consuming a user-friendly description,
for the initial implementation this will be JSON. When processing a
configuration, the utility will be able to generate checksums of  each MB2
module so that a Boot Domain might consume those checksums to verify the
integrity and correctness of the MB2 module chain.

### Xen hypervisor

It was prior discussed at a higher level of the new host boot flow that will be
introduced. Within the new flow is the configuration parsing and domain creation
phase which will be expanded upon here. The hypervisor will iterate through the
entries in the LCM processing each entry type that it understands. The MB2
module description entry is used to identify if any MB2 modules contain
microcode or an XSM policy. As it processes domain configuration entries, it
will construct the domain using the MB2 modules and the Basic Configuration
information. Once it has completed iterating through all the entries in the LCM,
if a constructed domain has the Boot Domain attribute it will then be unpaused.
Otherwise the hypervisor will start the boot finalization phase.

### Boot Domain

Traditionally domain creation was controlled by the user within the Dom0
environment whereby custom toolstacks could be implemented to impose
requirements on the process. The Boot Domain is a means to enable the user to
continue to maintain that control over domain creation but within a limited
privilege environment. The Boot Domain will have access to the LCM and the MB2
module chain along with access to a subset of the hypercall operations.When the
Boot Domain is finished it will notify the hypervisor through a hypercall op.

### Crash Domain

With the existing Dom0 host boot path, when a failure occurs there are several
assumptions that can safely be made to get the user to a console for
troubleshooting. With the DomB host boot path those assumptions can no longer be
made, thus a means is needed to get the user to a console in the case of a
recoverable failure. To handle this situation DomB introduces the concept of a
crash domain. The crash domain is configured by a domain configuration entry in
the LCM and is the only domain that will not be unpaused at boot finalization.

## A Detailed Flow of a DomB Boot

Provided here is an ordered flow of a DomB Boot with a highlight logic decision
points. Not all branch points are recorded, specifically for the variety of
error conditions that may occur.

0. Hypervisor Startup:

1. Inspect first MB2 module
    a. Is the module an LCM
        i. YES: proceed with the DomB host boot path
        ii. NO: proceed with a Dom0 host boot path

2. Iterate through the LCM entries looking for the MB2 module description entry
    a. Check if any of the MB2 modules are microcode or policy and if so, load

3. Iterate through the LCM entries processing all domain description entries
    a. Use the details from the Basic Configuration to call `domain_create`
    b. Record if a domain is flagged as the Boot Domain
    c. Record if a domain is flagged as the Crash Domain

4. Was a Boot Domain created
    a. YES:
        i. Attach console to Boot Domain
        ii. Unpause Boot Domain
        iii. Goto Boot Domain (step 5)
    b. NO: Goto Boot Finalization (step 9)

5. Boot Domain:

6. Boot Domain comes online and may do any of the following actions
    a. Process the LCM
    b. Validate the MB2 chain
    c. Make additional configuration settings for staged domains
    d. Unpause any precursor domains
    e. Set any runtime configurations

7. Boot Domain does any necessary cleanup

8. Boot Domain may make hypercall op call to signal it is finished
    i. Hypervisor reclaims all Boot Domain resources
    ii. Hypervisor records that the Boot Domain ran
    ii. Goto Boot Finalization (step 9)

9. Boot Finalization

10. If a configured domain was flagged to have the console, the hypervisor (re)assigns it

11. If no Boot Domain was launched, the hypervisor iterates through domains unpausing any domain not flagged as the crash domain
