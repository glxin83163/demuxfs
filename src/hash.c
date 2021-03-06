/* 
 * Copyright (c) 2008-2010, Lucas C. Villa Real <lucasvr@gobolinux.org>
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
#include "hash.h"

void hashtable_lock(struct hash_table *hash)
{
	pthread_mutex_lock(&hash->mutex);
}

void hashtable_unlock(struct hash_table *hash)
{
	pthread_mutex_unlock(&hash->mutex);
}

struct hash_table *hashtable_new(int size)
{
	struct hash_table *table = (struct hash_table *) calloc(1, sizeof(struct hash_table));
	assert(table);
	table->size = size;
	table->items = (struct hash_item **) calloc(size, sizeof(struct hash_item *));
	assert(table->items);
	pthread_mutex_init(&table->mutex, NULL);
	return table;
}

void hashtable_destroy(struct hash_table *table, hashtable_free_function_t free_function)
{
	int i;
	pthread_mutex_destroy(&table->mutex);
	for (i=0; i<table->size; ++i) {
		struct hash_item *item = table->items[i];
		if (item) {
			if (item->free_function && item->data)
				item->free_function(item->data);
			else if (free_function && item->data)
				free_function(item->data);
			free(item);
			table->items[i] = NULL;
		}
	}
	free(table->items);
	free(table);
}

void hashtable_invalidate_contents(struct hash_table *table)
{
	int i;
	pthread_mutex_destroy(&table->mutex);
	for (i=0; i<table->size; ++i) {
		struct hash_item *item = table->items[i];
		if (item) {
			item->data = NULL;
			table->items[i] = NULL;
		}
	}
}

void *hashtable_get(struct hash_table *table, ino_t key)
{
	int index = key % table->size;
	struct hash_item *item = table->items[index];
	struct hash_item *start = item;
	do {
		if (! item)
			return NULL;
		else if (item->key == key)
			return item->data;
		else {
			index = (index+1) % table->size;
			item = table->items[index];
		}
	} while (item != start);
	return NULL;
}

bool hashtable_add(struct hash_table *table, ino_t key, void *data, hashtable_free_function_t free_function)
{
	int index = key % table->size;
	struct hash_item *item = table->items[index];
	struct hash_item *start = item;
	do {
		if (item == NULL) {
			item = (struct hash_item *) calloc(1, sizeof(struct hash_item));
			item->key = key;
			item->data = data;
			item->free_function = free_function;
			table->items[index] = item;
			return true;
		} else if (item->key == key) {
			dprintf("overwriting previous contents (key=%#llx)", key);
			item->key = key;
			item->data = data;
			item->free_function = free_function;
			return true;
		} else {
			index = (index+1) % table->size;
			item = table->items[index];
		}
	} while (item != start);
	return false;
}

void _hashtable_del_item(struct hash_item *item)
{
	if (item) {
		if (item->free_function && item->data)
			item->free_function(item->data);
		free(item);
	}
}

bool hashtable_del(struct hash_table *table, ino_t key)
{
	int index = key % table->size;
	struct hash_item *item = table->items[index];
	struct hash_item *start = item;
	do {
		if (! item) 
			return true;
		else if (item->key == key) {
			_hashtable_del_item(item);
			table->items[index] = NULL;
			return true;
		} else {
			index = (index+1) % table->size;
			item = table->items[index];
		}
	} while (item != start);
	return false;
}
