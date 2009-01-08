/* 
 * Copyright (c) 2008, Lucas C. Villa Real <lucasvr@gobolinux.org>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. Neither the name of GoboLinux nor the names of its contributors may
 * be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "demuxfs.h"
#include "byteops.h"
#include "fsutils.h"
#include "xattr.h"
#include "hash.h"
#include "fifo.h"
#include "ts.h"
#include "descriptors.h"
#include "stream_type.h"
#include "component_tag.h"
#include "tables/psi.h"
#include "tables/ddb.h"
#include "tables/pes.h"
#include "tables/pat.h"

static void ddb_check_header(struct ddb_table *ddb)
{
}

static void ddb_create_directory(const struct ts_header *header, struct ddb_table *ddb, 
		struct dentry **version_dentry, struct demuxfs_data *priv)
{
	/* Create a directory named "DDB" at the root filesystem if it doesn't exist yet */
	struct dentry *ddb_dir = CREATE_DIRECTORY(priv->root, FS_DDB_NAME);

	/* Create a directory named "<ddb_pid>" and populate it with files */
	asprintf(&ddb->dentry->name, "%#04x", header->pid);
	ddb->dentry->mode = S_IFDIR | 0555;
	CREATE_COMMON(ddb_dir, ddb->dentry);
	
	/* Create the versioned dir and update the Current symlink */
	*version_dentry = fsutils_create_version_dir(ddb->dentry, ddb->version_number);

	psi_populate((void **) &ddb, *version_dentry);
	//ddb_populate(ddb, *version_dentry, priv);
}

int ddb_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv)
{
	struct ddb_table *current_ddb = NULL;
	struct ddb_table *ddb = (struct ddb_table *) calloc(1, sizeof(struct ddb_table));
	assert(ddb);
	
	ddb->dentry = (struct dentry *) calloc(1, sizeof(struct dentry));
	assert(ddb->dentry);

	/* Copy data up to the first loop entry */
	int ret = psi_parse((struct psi_common_header *) ddb, payload, payload_len);
	if (ret < 0) {
		free(ddb->dentry);
		free(ddb);
		return 0;
	}
	ddb_check_header(ddb);
	
	/* Set hash key and check if there's already one version of this table in the hash */
	ddb->dentry->inode = TS_PACKET_HASH_KEY(header, ddb);
	current_ddb = hashtable_get(priv->psi_tables, ddb->dentry->inode);
	
	/* Check whether we should keep processing this packet or not */
	if (! ddb->current_next_indicator || (current_ddb && current_ddb->version_number == ddb->version_number)) {
		free(ddb->dentry);
		free(ddb);
		return 0;
	}

	dprintf("*** DDB parser: pid=%#x, table_id=%#x, current_ddb=%p, ddb->version_number=%#x, len=%d ***", 
			header->pid, ddb->table_id, current_ddb, ddb->version_number, payload_len);

	/* Parse DDB specific bits */
	struct dentry *version_dentry = NULL;
	ddb_create_directory(header, ddb, &version_dentry, priv);

	if (current_ddb) {
		hashtable_del(priv->psi_tables, current_ddb->dentry->inode);
		fsutils_migrate_children(current_ddb->dentry, ddb->dentry);
		fsutils_dispose_tree(current_ddb->dentry);
		free(current_ddb);
	}
	hashtable_add(priv->psi_tables, ddb->dentry->inode, ddb);

	return 0;
}