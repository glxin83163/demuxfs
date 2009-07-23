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
#include "tables/psi.h"
#include "dsm-cc/dsmcc.h"
#include "dsm-cc/ddb.h"
#include "dsm-cc/dii.h"

static void ddb_free(struct ddb_table *ddb)
{
	struct dsmcc_download_data_header *data_header;
	
	if (! ddb)
		return;

	data_header = &ddb->dsmcc_download_data_header;
	dsmcc_free_download_data_header(data_header);
//	if (ddb->block_data_bytes)
//		free(ddb->block_data_bytes);
	free(ddb->dentry);
	free(ddb);
}

static struct dentry * ddb_find_dii(struct ddb_table *ddb, uint16_t *block_size, struct demuxfs_data *priv)
{
	struct dentry *dii = fsutils_get_dentry(priv->root, "/DII");
	if (! dii) {
		dprintf("no /dii found");
		return NULL;
	}

	char dii_path[PATH_MAX];
	struct dentry *module, *target, *file;
	list_for_each_entry(module, &dii->children, list) {
		sprintf(dii_path, "%s/%s/block_size", module->name, FS_CURRENT_NAME);
		dprintf("looking for %s", dii_path);
		file = fsutils_get_dentry(module, dii_path);
		if (file)
			*block_size = strtol(file->contents, NULL, 16);

		sprintf(dii_path, "%s/%s/module_%02d", module->name, FS_CURRENT_NAME, ddb->module_id+1);
		target = fsutils_get_dentry(module, dii_path);
		if (! target)
			continue;

		file = fsutils_get_child(target, "module_id");
		uint16_t module_id = strtol(file->contents, NULL, 16);

		file = fsutils_get_child(target, "module_version");
		uint8_t module_version = strtol(file->contents, NULL, 16);

		if (module_id == ddb->module_id && module_version == ddb->module_version)
			return target;
	}
	return NULL;
}

static char * ddb_get_filename(struct dentry *dii_module_dir, struct demuxfs_data *priv)
{
	char *path_to_file;
	struct dentry *entry;
	
	/* Try to find the name descriptor */
	entry = fsutils_get_child(dii_module_dir, "NAME");
	if (entry) {
		struct dentry *name = fsutils_get_child(entry, "text_char");
		if (name) {
			asprintf(&path_to_file, "%s/%s", priv->options.tmpdir, name->contents);
			return path_to_file;
		}
	}
	asprintf(&path_to_file, "%s/file.bin", priv->options.tmpdir);
	return path_to_file;
}

static void ddb_update_file_contents(const char *filename, uint16_t block_size,
		struct ddb_table *ddb, struct demuxfs_data *priv)
{
	FILE *fp = fopen(filename, "a");
	if (! fp) {
		perror(filename);
		return;
	}
	fseek(fp, ddb->block_number * block_size, SEEK_SET);
	fwrite(ddb->block_data_bytes, 1, ddb->_block_data_size, fp);
	fclose(fp);
}

static bool ddb_block_number_already_parsed(struct ddb_table *current_ddb, 
	uint16_t module_id, uint16_t block_number)
{
	struct dentry *current_dentry, *module_dentry, *block_dentry;
	char dname[64], fname[64];

	if (! current_ddb)
		return false;

	current_dentry = fsutils_get_current(current_ddb->dentry);
	if (current_dentry) {
		sprintf(dname, "module_%02d", module_id);
		sprintf(fname, "block_%02d.bin", block_number);
		module_dentry = fsutils_get_child(current_dentry, dname);
		if (module_dentry) {
			block_dentry = fsutils_get_child(module_dentry, fname);
			return block_dentry ? true : false;
		}
	}

	return false;
}

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
	if (! ddb->current_next_indicator) {
		dprintf("ddb doesn't have current_next_indicator bit set, skipping it");
		free(ddb->dentry);
		free(ddb);
		return 0;
	}

	/** DSM-CC Download Data Header */
	struct dsmcc_download_data_header *data_header = &ddb->dsmcc_download_data_header;
	int j = dsmcc_parse_download_data_header(data_header, payload, 8);
	
	if (data_header->_dsmcc_type != 0x03 ||
		data_header->_message_id != 0x1003) {
		ddb_free(ddb);
		return 0;
	}

	if (data_header->message_length < 5) {
		// XXX: expose header in the fs?
		if (data_header->message_length)
			dprintf("skipping message with len=%d", data_header->message_length);
		ddb_free(ddb);
		return 0;
	}

	/** DDB bits */
	ddb->module_id = CONVERT_TO_16(payload[j], payload[j+1]);
	ddb->module_version = payload[j+2];
	ddb->reserved = payload[j+3];
	ddb->block_number = CONVERT_TO_16(payload[j+4], payload[j+5]);
	if (ddb_block_number_already_parsed(current_ddb, ddb->module_id, ddb->block_number)) {
		ddb_free(ddb);
		return 0;
	}

	ddb->_block_data_size = data_header->message_length - data_header->adaptation_length - 6;
	if (! ddb->_block_data_size) {
		ddb_free(ddb);
		return 0;
	}

	/* Find the corresponding DII entry for this packet */
//	uint16_t block_size = 0;
//	struct dentry *dii = ddb_find_dii(ddb, &block_size, priv);
//	if (! dii) {
//		ddb_free(ddb);
//		return 0;
//	}

//	dprintf("*** DDB parser: pid=%d, table_id=%#x, ddb->version_number=%#x, ddb->block_number=%d, module_id=%d, current=%p ***", 
//			header->pid, ddb->table_id, ddb->version_number, ddb->block_number, ddb->module_id, current_ddb);

	/* Create filesystem entries for this table */
	struct dentry *version_dentry = NULL;
	if (current_ddb)
		version_dentry = fsutils_get_current(current_ddb->dentry);
	else
		ddb_create_directory(header, ddb, &version_dentry, priv);

	uint16_t this_block_size = payload_len - (j+6) - 4;
	uint16_t this_block_start = j+6;

	/* Create individual block file */
	struct dentry *module_dir = CREATE_DIRECTORY(version_dentry, "module_%02d", ddb->module_id);
	struct dentry *block_dentry = (struct dentry *) calloc(1, sizeof(struct dentry));
	block_dentry->size = this_block_size;
	block_dentry->mode = S_IFREG | 0444;
	block_dentry->obj_type = OBJ_TYPE_FILE;
	block_dentry->contents = malloc(this_block_size);
	memcpy(block_dentry->contents, &payload[this_block_start], this_block_size);
	asprintf(&block_dentry->name, "block_%02d.bin", ddb->block_number);
	CREATE_COMMON(module_dir, block_dentry);
	xattr_add(block_dentry, XATTR_FORMAT, XATTR_FORMAT_BIN, strlen(XATTR_FORMAT_BIN), false);
	
	if (current_ddb)
		ddb_free(ddb);
	else
		hashtable_add(priv->psi_tables, ddb->dentry->inode, ddb);

	return 0;
}
