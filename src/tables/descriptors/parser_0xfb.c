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
#include "fsutils.h"
#include "byteops.h"
#include "xattr.h"
#include "ts.h"
#include "tables/psi.h"
#include "tables/pat.h"
#include "descriptors.h"

struct formatted_descriptor {
	uint16_t service_id;
};

/* PARTIAL_RECEPTION_DESCRIPTOR parser */
int descriptor_0xfb_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	uint8_t i;
	struct dentry *dentry, *subdir;

	if (! descriptor_is_parseable(parent, payload[0], 2, len))
		return -ENODATA;

	dentry = CREATE_DIRECTORY(parent, "Partial_Reception_Descriptor");
	for (i=2; i<len; i+=2) {
		struct formatted_descriptor f;
		f.service_id = CONVERT_TO_16(payload[i], payload[i+1]);

		subdir = CREATE_DIRECTORY(dentry, "Service_%02d", ((i-2)/2)+1);
		CREATE_FILE_NUMBER(subdir, &f, service_id);

		if (! pat_announces_service(f.service_id, priv))
			TS_WARNING("service_id %#x not declared by the PAT", f.service_id);
	}
    return 0;
}

