/*
 * Copyright (c) 2010 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Ilya Dryomov <idryomov@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "hammer.h"
#include <libutil.h>
#include <crypto/sha2/sha2.h>

#define DEDUP_BUF (64 * 1024)

/* Sorted list of block CRCs - light version for dedup-simulate */
struct sim_dedup_entry_rb_tree;
RB_HEAD(sim_dedup_entry_rb_tree, sim_dedup_entry) sim_dedup_tree =
					RB_INITIALIZER(&sim_dedup_tree);
RB_PROTOTYPE2(sim_dedup_entry_rb_tree, sim_dedup_entry, rb_entry,
		rb_sim_dedup_entry_compare, hammer_crc_t);

struct sim_dedup_entry {
	hammer_crc_t	crc;
	u_int64_t	ref_blks; /* number of blocks referenced */
	u_int64_t	ref_size; /* size of data referenced */
	RB_ENTRY(sim_dedup_entry) rb_entry;
};

/* Sorted list of HAMMER B-Tree keys */
struct dedup_entry_rb_tree;
struct sha_dedup_entry_rb_tree;

RB_HEAD(dedup_entry_rb_tree, dedup_entry) dedup_tree =
					RB_INITIALIZER(&dedup_tree);
RB_PROTOTYPE2(dedup_entry_rb_tree, dedup_entry, rb_entry,
		rb_dedup_entry_compare, hammer_crc_t);

RB_PROTOTYPE(sha_dedup_entry_rb_tree, sha_dedup_entry, fict_entry,
		rb_sha_dedup_entry_compare);

struct dedup_entry {
	struct hammer_btree_leaf_elm leaf;
	union {
		struct {
			u_int64_t ref_blks;
			u_int64_t ref_size;
		} de;
		RB_HEAD(sha_dedup_entry_rb_tree, sha_dedup_entry) fict_root;
	} u;
	u_int8_t flags;
	RB_ENTRY(dedup_entry) rb_entry;
};

#define HAMMER_DEDUP_ENTRY_FICTITIOUS	0x0001

struct sha_dedup_entry {
	struct hammer_btree_leaf_elm	leaf;
	u_int64_t			ref_blks;
	u_int64_t			ref_size;
	u_int8_t			sha_hash[SHA256_DIGEST_LENGTH];
	RB_ENTRY(sha_dedup_entry)	fict_entry;
};

/*
 * Pass2 list - contains entries that were not dedup'ed because ioctl failed
 */
STAILQ_HEAD(, pass2_dedup_entry) pass2_dedup_queue =
				STAILQ_HEAD_INITIALIZER(pass2_dedup_queue);

struct pass2_dedup_entry {
	struct hammer_btree_leaf_elm	leaf;
	STAILQ_ENTRY(pass2_dedup_entry)	sq_entry;
};

#define DEDUP_PASS2	0x0001 /* process_btree_elm() mode */

/* PFS global ids - we deal with just one PFS at a run */
int glob_fd;
struct hammer_ioc_pseudofs_rw glob_pfs;

/*
 * Global accounting variables
 *
 * Last three don't have to be 64-bit, just to be safe..
 */
u_int64_t dedup_alloc_size = 0;
u_int64_t dedup_ref_size = 0;
u_int64_t dedup_skipped_size = 0;
u_int64_t dedup_crc_failures = 0;
u_int64_t dedup_sha_failures = 0;
u_int64_t dedup_underflows = 0;

static int rb_sim_dedup_entry_compare(struct sim_dedup_entry *sim_de1,
				struct sim_dedup_entry *sim_de2);
static int rb_dedup_entry_compare(struct dedup_entry *de1,
				struct dedup_entry *de2);
static int rb_sha_dedup_entry_compare(struct sha_dedup_entry *sha_de1,
				struct sha_dedup_entry *sha_de2);
typedef int (*scan_pfs_cb_t)(hammer_btree_leaf_elm_t scan_leaf, int flags);
static void scan_pfs(char *filesystem, scan_pfs_cb_t func);
static int collect_btree_elm(hammer_btree_leaf_elm_t scan_leaf, int flags);
static int process_btree_elm(hammer_btree_leaf_elm_t scan_leaf, int flags);
static int upgrade_chksum(hammer_btree_leaf_elm_t leaf, u_int8_t *sha_hash);
static void dump_simulated_dedup(void);
static void dump_real_dedup(void);
static void dedup_usage(int code);

RB_GENERATE2(sim_dedup_entry_rb_tree, sim_dedup_entry, rb_entry,
		rb_sim_dedup_entry_compare, hammer_crc_t, crc);
RB_GENERATE2(dedup_entry_rb_tree, dedup_entry, rb_entry,
		rb_dedup_entry_compare, hammer_crc_t, leaf.data_crc);
RB_GENERATE(sha_dedup_entry_rb_tree, sha_dedup_entry, fict_entry,
		rb_sha_dedup_entry_compare);

static int
rb_sim_dedup_entry_compare(struct sim_dedup_entry *sim_de1,
			struct sim_dedup_entry *sim_de2)
{
	if (sim_de1->crc < sim_de2->crc)
		return (-1);
	if (sim_de1->crc > sim_de2->crc)
		return (1);

	return (0);
}

static int
rb_dedup_entry_compare(struct dedup_entry *de1, struct dedup_entry *de2)
{
	if (de1->leaf.data_crc < de2->leaf.data_crc)
		return (-1);
	if (de1->leaf.data_crc > de2->leaf.data_crc)
		return (1);

	return (0);
}

static int
rb_sha_dedup_entry_compare(struct sha_dedup_entry *sha_de1,
			struct sha_dedup_entry *sha_de2)
{
	unsigned long *h1 = (unsigned long *)&sha_de1->sha_hash;
	unsigned long *h2 = (unsigned long *)&sha_de2->sha_hash;
	int i;

	for (i = 0; i < SHA256_DIGEST_LENGTH / (int)sizeof(unsigned long); ++i) {
		if (h1[i] < h2[i])
			return (-1);
		if (h1[i] > h2[i])
			return (1);
	}

	return (0);
}

/*
 * dedup-simulate <filesystem>
 */
void
hammer_cmd_dedup_simulate(char **av, int ac)
{
	struct sim_dedup_entry *sim_de;

	if (ac != 1)
		dedup_usage(1);

	glob_fd = getpfs(&glob_pfs, av[0]);

	printf("Dedup-simulate ");
	scan_pfs(av[0], &collect_btree_elm);
	printf("Dedup-simulate %s succeeded\n", av[0]);

	relpfs(glob_fd, &glob_pfs);

	if (VerboseOpt >= 2)
		dump_simulated_dedup();

	/*
	 * Calculate simulated dedup ratio and get rid of the tree
	 */
	while ((sim_de = RB_ROOT(&sim_dedup_tree)) != NULL) {
		assert(sim_de->ref_blks != 0);
		dedup_ref_size += sim_de->ref_size;
		dedup_alloc_size += sim_de->ref_size / sim_de->ref_blks;

		RB_REMOVE(sim_dedup_entry_rb_tree, &sim_dedup_tree, sim_de);
		free(sim_de);
	}

	printf("Simulated dedup ratio = %.2f\n",
	    (dedup_alloc_size != 0) ?
		(double)dedup_ref_size / dedup_alloc_size : 0);
}

/*
 * dedup <filesystem>
 */
void
hammer_cmd_dedup(char **av, int ac)
{
	struct dedup_entry *de;
	struct sha_dedup_entry *sha_de;
	struct pass2_dedup_entry *pass2_de;
	char buf[8];

	if (TimeoutOpt > 0)
		alarm(TimeoutOpt);

	if (ac != 1)
		dedup_usage(1);

	STAILQ_INIT(&pass2_dedup_queue);

	glob_fd = getpfs(&glob_pfs, av[0]);

	printf("Dedup ");
	scan_pfs(av[0], &process_btree_elm);

	while ((pass2_de = STAILQ_FIRST(&pass2_dedup_queue)) != NULL) {
		if (process_btree_elm(&pass2_de->leaf, DEDUP_PASS2))
			dedup_skipped_size -= pass2_de->leaf.data_len;

		STAILQ_REMOVE_HEAD(&pass2_dedup_queue, sq_entry);
		free(pass2_de);
	}
	assert(STAILQ_EMPTY(&pass2_dedup_queue));

	printf("Dedup %s succeeded\n", av[0]);

	relpfs(glob_fd, &glob_pfs);

	if (VerboseOpt >= 2)
		dump_real_dedup();

	/*
	 * Calculate dedup ratio and get rid of the trees
	 */
	while ((de = RB_ROOT(&dedup_tree)) != NULL) {
		if (de->flags & HAMMER_DEDUP_ENTRY_FICTITIOUS) {
			while ((sha_de = RB_ROOT(&de->u.fict_root)) != NULL) {
				assert(sha_de->ref_blks != 0);
				dedup_ref_size += sha_de->ref_size;
				dedup_alloc_size += sha_de->ref_size / sha_de->ref_blks;

				RB_REMOVE(sha_dedup_entry_rb_tree,
						&de->u.fict_root, sha_de);
				free(sha_de);
			}
			assert(RB_EMPTY(&de->u.fict_root));
		} else {
			assert(de->u.de.ref_blks != 0);
			dedup_ref_size += de->u.de.ref_size;
			dedup_alloc_size += de->u.de.ref_size / de->u.de.ref_blks;
		}

		RB_REMOVE(dedup_entry_rb_tree, &dedup_tree, de);
		free(de);
	}
	assert(RB_EMPTY(&dedup_tree));

	humanize_unsigned(buf, sizeof(buf), dedup_ref_size, "B", 1024);
	printf("Dedup ratio = %.2f\n"
	       "    %8s referenced\n",
	       ((dedup_alloc_size != 0) ?
			(double)dedup_ref_size / dedup_alloc_size : 0),
	       buf
	);
	humanize_unsigned(buf, sizeof(buf), dedup_alloc_size, "B", 1024);
	printf("    %8s allocated\n", buf);
	humanize_unsigned(buf, sizeof(buf), dedup_skipped_size, "B", 1024);
	printf("    %8s skipped\n", buf);
	printf("    %8jd CRC collisions\n"
	       "    %8jd SHA collisions\n"
	       "    %8jd bigblock underflows\n",
	       (intmax_t)dedup_crc_failures,
	       (intmax_t)dedup_sha_failures,
               (intmax_t)dedup_underflows
	);
}

static int
collect_btree_elm(hammer_btree_leaf_elm_t scan_leaf, int flags __unused)
{
	struct sim_dedup_entry *sim_de;

	sim_de = RB_LOOKUP(sim_dedup_entry_rb_tree, &sim_dedup_tree, scan_leaf->data_crc);

	if (sim_de == NULL) {
		sim_de = calloc(sizeof(*sim_de), 1);
		sim_de->crc = scan_leaf->data_crc;
		RB_INSERT(sim_dedup_entry_rb_tree, &sim_dedup_tree, sim_de);
	}

	sim_de->ref_blks += 1;
	sim_de->ref_size += scan_leaf->data_len;
	return (1);
}

static __inline int
validate_dedup_pair(hammer_btree_leaf_elm_t p, hammer_btree_leaf_elm_t q)
{
	if ((p->data_offset & HAMMER_OFF_ZONE_MASK) !=
	    (q->data_offset & HAMMER_OFF_ZONE_MASK)) {
		return (1);
	}
	if (p->data_len != q->data_len) {
		return (1);
	}

	return (0);
}

#define DEDUP_TECH_FAILURE	1
#define DEDUP_CMP_FAILURE	2
#define DEDUP_INVALID_ZONE	3
#define DEDUP_UNDERFLOW		4
#define DEDUP_VERS_FAILURE	5

static __inline int
deduplicate(hammer_btree_leaf_elm_t p, hammer_btree_leaf_elm_t q)
{
	struct hammer_ioc_dedup dedup;

	bzero(&dedup, sizeof(dedup));

	/*
	 * If data_offset fields are the same there is no need to run ioctl,
	 * candidate is already dedup'ed.
	 */
	if (p->data_offset == q->data_offset) {
		return (0);
	}

	dedup.elm1 = p->base;
	dedup.elm2 = q->base;
	RunningIoctl = 1;
	if (ioctl(glob_fd, HAMMERIOC_DEDUP, &dedup) < 0) {
		if (errno == EOPNOTSUPP) {
			/* must be at least version 5 */
			return (DEDUP_VERS_FAILURE);
		}
		/* Technical failure - locking or w/e */
		return (DEDUP_TECH_FAILURE);
	}
	if (dedup.head.flags & HAMMER_IOC_DEDUP_CMP_FAILURE) {
		return (DEDUP_CMP_FAILURE);
	}
	if (dedup.head.flags & HAMMER_IOC_DEDUP_INVALID_ZONE) {
		return (DEDUP_INVALID_ZONE);
	}
	if (dedup.head.flags & HAMMER_IOC_DEDUP_UNDERFLOW) {
		return (DEDUP_UNDERFLOW);
	}
	RunningIoctl = 0;

	return (0);
}

static int
process_btree_elm(hammer_btree_leaf_elm_t scan_leaf, int flags)
{
	struct dedup_entry *de;
	struct sha_dedup_entry *sha_de, temp;
	struct pass2_dedup_entry *pass2_de;
	int error;

	de = RB_LOOKUP(dedup_entry_rb_tree, &dedup_tree, scan_leaf->data_crc);
	if (de == NULL) {
		/*
		 * Insert new entry into CRC tree
		 */
		de = calloc(sizeof(*de), 1);
		de->leaf = *scan_leaf;
		RB_INSERT(dedup_entry_rb_tree, &dedup_tree, de);
		goto upgrade_stats;
	}

	/*
	 * Found entry in CRC tree
	 */
	if (de->flags & HAMMER_DEDUP_ENTRY_FICTITIOUS) {
		/*
		 * Entry in CRC tree is fictitious, so we already had problems
		 * with this CRC. Upgrade (compute SHA) the candidate and
		 * dive into SHA subtree. If upgrade fails insert the candidate
		 * into Pass2 list (it will be processed later).
		 */
		if (upgrade_chksum(scan_leaf, temp.sha_hash))
			goto pass2_insert;

		sha_de = RB_FIND(sha_dedup_entry_rb_tree, &de->u.fict_root, &temp);
		if (sha_de == NULL) {
			/*
			 * Nothing in SHA subtree so far, so this is a new
			 * 'dataset'. Insert new entry into SHA subtree.
			 */
			sha_de = calloc(sizeof(*sha_de), 1);
			sha_de->leaf = *scan_leaf;
			memcpy(sha_de->sha_hash, temp.sha_hash, SHA256_DIGEST_LENGTH);
			RB_INSERT(sha_dedup_entry_rb_tree, &de->u.fict_root, sha_de);
			goto upgrade_stats_sha;
		}

		/*
		 * Found entry in SHA subtree, it means we have a potential
		 * dedup pair. Validate it (zones have to match and data_len
		 * field have to be the same too. If validation fails, treat
		 * it as a SHA collision (jump to sha256_failure).
		 */
		if (validate_dedup_pair(&sha_de->leaf, scan_leaf))
			goto sha256_failure;

		/*
		 * We have a valid dedup pair (SHA match, validated).
		 *
		 * In case of technical failure (dedup pair was good, but
		 * ioctl failed anyways) insert the candidate into Pass2 list
		 * (we will try to dedup it after we are done with the rest of
		 * the tree).
		 *
		 * If ioctl fails because either of blocks is in the non-dedup
		 * zone (we can dedup only in LARGE_DATA and SMALL_DATA) don't
		 * bother with the candidate and terminate early.
		 *
		 * If ioctl fails because of bigblock underflow replace the
		 * leaf node that found dedup entry represents with scan_leaf.
		 */
		error = deduplicate(&sha_de->leaf, scan_leaf);
		switch(error) {
		case 0:
			goto upgrade_stats_sha;
		case DEDUP_TECH_FAILURE:
			goto pass2_insert;
		case DEDUP_CMP_FAILURE:
			goto sha256_failure;
		case DEDUP_INVALID_ZONE:
			goto terminate_early;
		case DEDUP_UNDERFLOW:
			++dedup_underflows;
			sha_de->leaf = *scan_leaf;
			memcpy(sha_de->sha_hash, temp.sha_hash, SHA256_DIGEST_LENGTH);
			goto upgrade_stats_sha;
		case DEDUP_VERS_FAILURE:
			fprintf(stderr,
				"HAMMER filesystem must be at least "
				"version 5 to dedup\n");
			exit (1);
		default:
			fprintf(stderr, "Unknown error\n");
			goto terminate_early;
		}

		/*
		 * Ooh la la.. SHA-256 collision. Terminate early, there's
		 * nothing we can do here.
		 */
sha256_failure:
		++dedup_sha_failures;
		goto terminate_early;
	} else {
		/*
		 * Candidate CRC is good for now (we found an entry in CRC
		 * tree and it's not fictitious). This means we have a
		 * potential dedup pair.
		 */
		if (validate_dedup_pair(&de->leaf, scan_leaf))
			goto crc_failure;

		/*
		 * We have a valid dedup pair (CRC match, validated)
		 */
		error = deduplicate(&de->leaf, scan_leaf);
		switch(error) {
		case 0:
			goto upgrade_stats;
		case DEDUP_TECH_FAILURE:
			goto pass2_insert;
		case DEDUP_CMP_FAILURE:
			goto crc_failure;
		case DEDUP_INVALID_ZONE:
			goto terminate_early;
		case DEDUP_UNDERFLOW:
			++dedup_underflows;
			de->leaf = *scan_leaf;
			goto upgrade_stats;
		case DEDUP_VERS_FAILURE:
			fprintf(stderr,
				"HAMMER filesystem must be at least "
				"version 5 to dedup\n");
			exit (1);
		default:
			fprintf(stderr, "Unknown error\n");
			goto terminate_early;
		}

crc_failure:
		/*
		 * We got a CRC collision - either ioctl failed because of
		 * the comparison failure or validation of the potential
		 * dedup pair went bad. In all cases insert both blocks
		 * into SHA subtree (this requires checksum upgrade) and mark
		 * entry that corresponds to this CRC in the CRC tree
		 * fictitious, so that all futher operations with this CRC go
		 * through SHA subtree.
		 */
		++dedup_crc_failures;
		/*
		 * Insert block that was represented by now fictitious dedup
		 * entry (create a new SHA entry and preserve stats of the
		 * old CRC one). If checksum upgrade fails insert the
		 * candidate into Pass2 list and return - keep both trees
		 * unmodified.
		 */
		sha_de = calloc(sizeof(*sha_de), 1);
		sha_de->leaf = de->leaf;
		sha_de->ref_blks = de->u.de.ref_blks;
		sha_de->ref_size = de->u.de.ref_size;
		if (upgrade_chksum(&sha_de->leaf, sha_de->sha_hash)) {
			free(sha_de);
			goto pass2_insert;
		}

		RB_INIT(&de->u.fict_root);
		/*
		 * Here we can insert without prior checking because the tree
		 * is empty at this point
		 */
		RB_INSERT(sha_dedup_entry_rb_tree, &de->u.fict_root, sha_de);

		/*
		 * Mark entry in CRC tree fictitious
		 */
		de->flags |= HAMMER_DEDUP_ENTRY_FICTITIOUS;

		/*
		 * Upgrade checksum of the candidate and insert it into
		 * SHA subtree. If upgrade fails insert the candidate into
		 * Pass2 list.
		 */
		if (upgrade_chksum(scan_leaf, temp.sha_hash)) {
			goto pass2_insert;
		}
		sha_de = RB_FIND(sha_dedup_entry_rb_tree, &de->u.fict_root, &temp);
		if (sha_de != NULL)
			/* There is an entry with this SHA already, but the only
			 * RB-tree element at this point is that entry we just
			 * added. We know for sure these blocks are different
			 * (this is crc_failure branch) so treat it as SHA
			 * collision.
			 */
			goto sha256_failure;

		sha_de = calloc(sizeof(*sha_de), 1);
		sha_de->leaf = *scan_leaf;
		memcpy(sha_de->sha_hash, temp.sha_hash, SHA256_DIGEST_LENGTH);
		RB_INSERT(sha_dedup_entry_rb_tree, &de->u.fict_root, sha_de);
		goto upgrade_stats_sha;
	}

upgrade_stats:
	de->u.de.ref_blks += 1;
	de->u.de.ref_size += scan_leaf->data_len;
	return (1);

upgrade_stats_sha:
	sha_de->ref_blks += 1;
	sha_de->ref_size += scan_leaf->data_len;
	return (1);

pass2_insert:
	/*
	 * If in pass2 mode don't insert anything, fall through to
	 * terminate_early
	 */
	if ((flags & DEDUP_PASS2) == 0) {
		pass2_de = calloc(sizeof(*pass2_de), 1);
		pass2_de->leaf = *scan_leaf;
		STAILQ_INSERT_TAIL(&pass2_dedup_queue, pass2_de, sq_entry);
		dedup_skipped_size += scan_leaf->data_len;
		return (1);
	}

terminate_early:
	/*
	 * Early termination path. Fixup stats.
	 */
	dedup_alloc_size += scan_leaf->data_len;
	dedup_ref_size += scan_leaf->data_len;
	return (0);
}

static int
upgrade_chksum(hammer_btree_leaf_elm_t leaf, u_int8_t *sha_hash)
{
	struct hammer_ioc_data data;
	char *buf = malloc(DEDUP_BUF);
	SHA256_CTX ctx;
	int error;

	bzero(&data, sizeof(data));
	data.elm = leaf->base;
	data.ubuf = buf;
	data.size = DEDUP_BUF;

	error = 0;
	if (ioctl(glob_fd, HAMMERIOC_GET_DATA, &data) < 0) {
		fprintf(stderr, "Get-data failed: %s\n", strerror(errno));
		error = 1;
		goto done;
	}

	if (data.leaf.data_len != leaf->data_len) {
		error = 1;
		goto done;
	}

	if (data.leaf.base.btype == HAMMER_BTREE_TYPE_RECORD &&
	    data.leaf.base.rec_type == HAMMER_RECTYPE_DATA) {
		SHA256_Init(&ctx);
		SHA256_Update(&ctx, (void *)buf, data.leaf.data_len);
		SHA256_Final(sha_hash, &ctx);
	}

done:
	free(buf);
	return (error);
}

static void
scan_pfs(char *filesystem, scan_pfs_cb_t func)
{
	struct hammer_ioc_mirror_rw mirror;
	hammer_ioc_mrecord_any_t mrec;
	struct hammer_btree_leaf_elm elm;
	char *buf = malloc(DEDUP_BUF);
	int offset, bytes;

	bzero(&mirror, sizeof(mirror));
	hammer_key_beg_init(&mirror.key_beg);
	hammer_key_end_init(&mirror.key_end);

	mirror.tid_beg = glob_pfs.ondisk->sync_beg_tid;
	mirror.tid_end = glob_pfs.ondisk->sync_end_tid;
	mirror.head.flags |= HAMMER_IOC_MIRROR_NODATA; /* we want only keys */
	mirror.ubuf = buf;
	mirror.size = DEDUP_BUF;
	mirror.pfs_id = glob_pfs.pfs_id;
	mirror.shared_uuid = glob_pfs.ondisk->shared_uuid;

	printf("%s: objspace %016jx:%04x %016jx:%04x pfs_id %d\n",
		filesystem,
		(uintmax_t)mirror.key_beg.obj_id,
		mirror.key_beg.localization,
		(uintmax_t)mirror.key_end.obj_id,
		mirror.key_end.localization,
		glob_pfs.pfs_id);
	fflush(stdout);

	do {
		mirror.count = 0;
		mirror.pfs_id = glob_pfs.pfs_id;
		mirror.shared_uuid = glob_pfs.ondisk->shared_uuid;
		if (ioctl(glob_fd, HAMMERIOC_MIRROR_READ, &mirror) < 0) {
			fprintf(stderr, "Mirror-read %s failed: %s\n",
				filesystem, strerror(errno));
			exit(1);
		}
		if (mirror.head.flags & HAMMER_IOC_HEAD_ERROR) {
			fprintf(stderr, "Mirror-read %s fatal error %d\n",
				filesystem, mirror.head.error);
			exit(1);
		}
		if (mirror.count) {
			offset = 0;
			while (offset < mirror.count) {
				mrec = (void *)((char *)buf + offset);
				bytes = HAMMER_HEAD_DOALIGN(mrec->head.rec_size);
				if (offset + bytes > mirror.count) {
					fprintf(stderr, "Misaligned record\n");
					exit(1);
				}
				assert((mrec->head.type &
				    HAMMER_MRECF_TYPE_MASK) == HAMMER_MREC_TYPE_REC);

				elm = mrec->rec.leaf;
				if (elm.base.btype == HAMMER_BTREE_TYPE_RECORD &&
				    elm.base.rec_type == HAMMER_RECTYPE_DATA) {
					func(&elm, 0);
				}
				offset += bytes;
			}
		}
		mirror.key_beg = mirror.key_cur;
	} while (mirror.count != 0);

	free(buf);
}

static void
dump_simulated_dedup(void)
{
	struct sim_dedup_entry *sim_de;

	printf("=== Dumping simulated dedup entries:\n");
	RB_FOREACH(sim_de, sim_dedup_entry_rb_tree, &sim_dedup_tree) {
		printf("\tcrc=%08x cnt=%ju size=%ju\n",
			sim_de->crc,
			(intmax_t)sim_de->ref_blks,
			(intmax_t)sim_de->ref_size);
	}
	printf("end of dump ===\n");
}

static void
dump_real_dedup(void)
{
	struct dedup_entry *de;
	struct sha_dedup_entry *sha_de;
	int i;

	printf("=== Dumping dedup entries:\n");
	RB_FOREACH(de, dedup_entry_rb_tree, &dedup_tree) {
		if (de->flags & HAMMER_DEDUP_ENTRY_FICTITIOUS) {
			printf("\tcrc=%08x fictitious\n", de->leaf.data_crc);

			RB_FOREACH(sha_de, sha_dedup_entry_rb_tree, &de->u.fict_root) {
				printf("\t\tcrc=%08x cnt=%ju size=%ju\n\t"
				       "\t\tsha=",
				       sha_de->leaf.data_crc,
				       (intmax_t)sha_de->ref_blks,
				       (intmax_t)sha_de->ref_size);
				for (i = 0; i < SHA256_DIGEST_LENGTH; ++i)
					printf("%02x", sha_de->sha_hash[i]);
				printf("\n");
			}
		} else {
			printf("\tcrc=%08x cnt=%ju size=%ju\n",
			       de->leaf.data_crc,
			       (intmax_t)de->u.de.ref_blks,
			       (intmax_t)de->u.de.ref_size);
		}
	}
	printf("end of dump ===\n");
}

static void
dedup_usage(int code)
{
	fprintf(stderr,
		"hammer dedup-simulate <filesystem>\n"
		"hammer dedup <filesystem>\n"
	);
	exit(code);
}
