#ifndef __LINUX_NODEMASK_H
#define __LINUX_NODEMASK_H

/*
 * Nodemasks provide a bitmap suitable for representing the
 * set of Node's in a system, one bit position per Node number.
 *
 * See detailed comments in the file linux/bitmap.h describing the
 * data type on which these nodemasks are based.
 *
 * The available nodemask operations are:
 *
 * void nodemask_set(node, mask)	turn on bit 'node' in mask
 * void __nodemask_set(node, mask)	turn on bit 'node' in mask (unlocked)
 * void nodemask_clear(node, mask)	turn off bit 'node' in mask
 * void __nodemask_clear(node, mask)	turn off bit 'node' in mask (unlocked)
 * bool nodemask_test(node, mask)	true iff bit 'node' set in mask
 * bool nodemask_test_and_set(node, mask) test and set bit 'node' in mask
 *
 * void nodemask_and(dst, src1, src2)	dst = src1 & src2  [intersection]
 * void nodemask_or(dst, src1, src2)	dst = src1 | src2  [union]
 * void nodemask_xor(dst, src1, src2)	dst = src1 ^ src2
 * void nodemask_andnot(dst, src1, src2)dst = src1 & ~src2
 * void nodemask_complement(dst, src)	dst = ~src
 *
 * bool nodemask_equal(mask1, mask2)	Does mask1 == mask2?
 * bool nodemask_intersects(mask1, mask2) Do mask1 and mask2 intersect?
 * bool nodemask_subset(mask1, mask2)	Is mask1 a subset of mask2?
 * bool nodemask_empty(mask)		Is mask empty (no bits sets)?
 * bool nodemask_full(mask)		Is mask full (all bits sets)?
 * unsigned int nodemask_weight(mask)	Hamming weight - number of set bits
 *
 * node nodemask_first(mask)		Number lowest set bit, or MAX_NUMNODES
 * node nodemask_next(node, mask) 	Next node past 'node', or MAX_NUMNODES
 * node nodemask_last(mask)		Number highest set bit, or MAX_NUMNODES
 * node nodemask_cycle(node, mask) 	Next node cycling from 'node', or
 *					MAX_NUMNODES
 *
 * nodemask_t NODEMASK_OF(node)		Initializer - bit 'node' set
 * NODEMASK_ALL				Initializer - all bits set
 * NODEMASK_NONE			Initializer - no bits set
 * unsigned long *nodemask_bits(mask)	Array of unsigned long's in mask
 *
 * for_each_node_mask(node, mask)	for-loop node over mask
 *
 * unsigned int num_online_nodes()	Number of online Nodes
 *
 * bool node_online(node)		Is this node online?
 *
 * node_set_online(node)		set bit 'node' in node_online_map
 * node_set_offline(node)		clear bit 'node' in node_online_map
 *
 * for_each_online_node(node)		for-loop node over node_online_map
 */

#include <xen/kernel.h>
#include <xen/bitmap.h>
#include <xen/numa.h>

typedef struct { DECLARE_BITMAP(bits, MAX_NUMNODES); } nodemask_t;

/*
 * printf arguments for a nodemask.  Shorthand for using '%*pb[l]' when
 * printing a nodemask.
 */
#define NODEMASK_PR(src) MAX_NUMNODES, nodemask_bits(src)

#define nodemask_bits(src) ((src)->bits)

#define NODEMASK_LAST_WORD BITMAP_LAST_WORD_MASK(MAX_NUMNODES)

#define NODEMASK_NONE                                                   \
((nodemask_t) {{                                                        \
        [0 ... BITS_TO_LONGS(MAX_NUMNODES) - 1] = 0                     \
}})

#if MAX_NUMNODES <= BITS_PER_LONG

#define NODEMASK_ALL      ((nodemask_t) {{ NODEMASK_LAST_WORD }})
#define NODEMASK_OF(node) ((nodemask_t) {{ 1UL << (node) }})

#else /* MAX_NUMNODES > BITS_PER_LONG */

#define NODEMASK_ALL                                                    \
((nodemask_t) {{                                                        \
        [0 ... BITS_TO_LONGS(MAX_NUMNODES) - 2] = ~0UL,                 \
        [BITS_TO_LONGS(MAX_NUMNODES) - 1] = NODEMASK_LAST_WORD          \
}})

#define NODEMASK_OF(node)                                               \
({                                                                      \
    nodemask_t m = NODES_NONE;                                          \
    m.bits[(node) / BITS_PER_LONG] = 1UL << ((node) % BITS_PER_LONG);   \
    m;                                                                  \
})

#endif /* MAX_NUMNODES */

static inline void nodemask_set(unsigned int node, nodemask_t *dst)
{
    set_bit(node, dst->bits);
}

static inline void __nodemask_set(unsigned int node, nodemask_t *dst)
{
    __set_bit(node, dst->bits);
}

static inline void nodemask_clear(unsigned int node, nodemask_t *dst)
{
    clear_bit(node, dst->bits);
}

static inline void __nodemask_clear(unsigned int node, nodemask_t *dst)
{
    __clear_bit(node, dst->bits);
}

static inline bool nodemask_test(unsigned int node, const nodemask_t *dst)
{
    return test_bit(node, dst->bits);
}

static inline bool nodemask_test_and_set(unsigned int node, nodemask_t *dst)
{
    return test_and_set_bit(node, dst->bits);
}

static inline void nodemask_and(nodemask_t *dst, const nodemask_t *src1,
                                const nodemask_t *src2)
{
    bitmap_and(dst->bits, src1->bits, src2->bits, MAX_NUMNODES);
}

static inline void nodemask_or(nodemask_t *dst, const nodemask_t *src1,
                               const nodemask_t *src2)
{
    bitmap_or(dst->bits, src1->bits, src2->bits, MAX_NUMNODES);
}

static inline void nodemask_xor(nodemask_t *dst, const nodemask_t *src1,
                                const nodemask_t *src2)
{
    bitmap_xor(dst->bits, src1->bits, src2->bits, MAX_NUMNODES);
}

static inline void nodemask_andnot(nodemask_t *dst, const nodemask_t *src1,
                                   const nodemask_t *src2)
{
    bitmap_andnot(dst->bits, src1->bits, src2->bits, MAX_NUMNODES);
}

static inline void nodemask_complement(nodemask_t *dst, const nodemask_t *src)
{
    bitmap_complement(dst->bits, src->bits, MAX_NUMNODES);
}

static inline bool nodemask_equal(const nodemask_t *src1,
                                  const nodemask_t *src2)
{
    return bitmap_equal(src1->bits, src2->bits, MAX_NUMNODES);
}

static inline bool nodemask_intersects(const nodemask_t *src1,
                                       const nodemask_t *src2)
{
    return bitmap_intersects(src1->bits, src2->bits, MAX_NUMNODES);
}

static inline bool nodemask_subset(const nodemask_t *src1,
                                   const nodemask_t *src2)
{
    return bitmap_subset(src1->bits, src2->bits, MAX_NUMNODES);
}

static inline bool nodemask_empty(const nodemask_t *src)
{
    return bitmap_empty(src->bits, MAX_NUMNODES);
}

static inline bool nodemask_full(const nodemask_t *src)
{
    return bitmap_full(src->bits, MAX_NUMNODES);
}

static inline unsigned int nodemask_weight(const nodemask_t *src)
{
    return bitmap_weight(src->bits, MAX_NUMNODES);
}

/* FIXME: better would be to fix all architectures to never return
          > MAX_NUMNODES, then the silly min_ts could be dropped. */

static inline unsigned int nodemask_first(const nodemask_t *src)
{
    return min_t(unsigned int, MAX_NUMNODES,
                 find_first_bit(src->bits, MAX_NUMNODES));
}

static inline unsigned int nodemask_next(unsigned int n, const nodemask_t *src)
{
    return min_t(unsigned int, MAX_NUMNODES,
                 find_next_bit(src->bits, MAX_NUMNODES, n + 1));
}

static inline unsigned int nodemask_last(const nodemask_t *src)
{
    unsigned int node, pnode = MAX_NUMNODES;

    for ( node = nodemask_first(src);
          node < MAX_NUMNODES; node = nodemask_next(node, src) )
        pnode = node;

    return pnode;
}

static inline unsigned int nodemask_cycle(unsigned int n, const nodemask_t *src)
{
    unsigned int nxt = nodemask_next(n, src);

    if ( nxt == MAX_NUMNODES )
        nxt = nodemask_first(src);

    return nxt;
}

#if MAX_NUMNODES > 1
#define for_each_node_mask(node, mask)			\
	for ((node) = nodemask_first(mask);		\
		(node) < MAX_NUMNODES;			\
		(node) = nodemask_next(node, mask))
#else /* MAX_NUMNODES == 1 */
#define for_each_node_mask(node, mask)			\
	if ( !nodemask_empty(mask) )			\
		for ((node) = 0; (node) < 1; (node)++)
#endif /* MAX_NUMNODES */

/*
 * The following particular system nodemasks and operations
 * on them manage online nodes.
 */

extern nodemask_t node_online_map;

#if MAX_NUMNODES > 1
#define num_online_nodes()	nodemask_weight(&node_online_map)
#define node_online(node)	nodemask_test(node, &node_online_map)
#else
#define num_online_nodes()	1U
#define node_online(node)	((node) == 0)
#endif

#define node_set_online(node)	   set_bit(node, node_online_map.bits)
#define node_set_offline(node)	   clear_bit(node, node_online_map.bits)

#define for_each_online_node(node) for_each_node_mask(node, &node_online_map)

#endif /* __LINUX_NODEMASK_H */
