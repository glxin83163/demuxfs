#ifndef __fsutils_h
#define __fsutils_h

#define FS_DEFAULT_TMPDIR               "/tmp"

#define FS_ES_FIFO_NAME                 "ES"
#define FS_PES_FIFO_NAME                "PES"
#define FS_AIT_NAME                     "AIT"
#define FS_PAT_NAME                     "PAT"
#define FS_PMT_NAME                     "PMT"
#define FS_NIT_NAME                     "NIT"
#define FS_H_EIT_NAME                   "H-EIT"
#define FS_M_EIT_NAME                   "M-EIT"
#define FS_L_EIT_NAME                   "L-EIT"
#define FS_SDT_NAME                     "SDT"
#define FS_SDTT_NAME                    "SDTT"
#define FS_TOT_NAME                     "TOT"
#define FS_DII_NAME                     "DII"
#define FS_DSI_NAME                     "DSI"
#define FS_DDB_NAME                     "DDB"
#define FS_DSMCC_NAME                   "DSM-CC"

#define FS_PROGRAMS_NAME                "Programs"
#define FS_CURRENT_NAME                 "Current"
#define FS_PRIMARY_NAME                 "Primary"
#define FS_SECONDARY_NAME               "Secondary"
#define FS_BROKEN_SYMLINK_NAME          "BrokenSymlink"
#define FS_UNNAMED_APPLICATION_NAME     "UnnamedApplication"

#define FS_VIDEO_SNAPSHOT_NAME          "snapshot.gif"
#define FS_STREAMS_NAME                 "Streams"
#define FS_AUDIO_STREAMS_NAME           "AudioStreams"
#define FS_VIDEO_STREAMS_NAME           "VideoStreams"
#define FS_ONE_SEG_AUDIO_STREAMS_NAME   "AudioStreams-OneSeg"
#define FS_ONE_SEG_VIDEO_STREAMS_NAME   "VideoStreams-OneSeg"
#define FS_CLOSED_CAPTION_STREAMS_NAME  "ClosedCaptionStreams"
#define FS_SUPERIMPOSED_STREAMS_NAME    "SuperImposedStreams"
#define FS_OBJECT_CAROUSEL_STREAMS_NAME "ObjectCarouselStreams"
#define FS_DATA_CAROUSEL_STREAMS_NAME   "DataCarouselStreams"
#define FS_EVENT_MESSAGE_STREAMS_NAME   "EventMessageStreams"
#define FS_MPE_STREAMS_NAME             "MPEStreams"
#define FS_RESERVED_STREAMS_NAME        "ReservedStreams"

#define FS_TABLE_HEADER_DIRNAME                     "tableHeader"
#define FS_DSMCC_DOWNLOAD_DATA_HEADER_DIRNAME       "dsmccDownloadDataHeader"
#define FS_DSMCC_MESSAGE_HEADER_DIRNAME             "dsmccMessageHeader"
#define FS_DSMCC_COMPATIBILITY_DESCRIPTOR_DIRNAME   "dsmccCompatibilityDescriptor"
#define FS_DSMCC_GROUP_INFO_INDICATION_DIRNAME      "dsmccGroupInfoIndication"
#define FS_DSMCC_MESSAGE_SELECTOR_DIRNAME           "dsmccMessageSelector"
#define FS_BIOP_SERVICE_GATEWAY_INFORMATION_DIRNAME "biopServiceGatewayInformation"
#define FS_BIOP_MESSAGE_DIRNAME                     "biopMessage"
#define FS_BIOP_PROFILE_BODY_DIRNAME                "biopProfileBody"
#define FS_BIOP_OBJECT_LOCATION_DIRNAME             "biopObjectLocation"
#define FS_BIOP_CONNBINDER_DIRNAME                  "biopConnBinder"
#define FS_BIOP_MODULE_INFO_DIRNAME                 "biopModuleInfo"

char *fsutils_path_walk(struct dentry *dentry, char *buf, size_t size);
char *fsutils_realpath(struct dentry *dentry, char *buf, size_t size, struct demuxfs_data *priv);
void fsutils_dump_tree(struct dentry *dentry);
struct dentry *fsutils_get_child(struct dentry *dentry, const char *name);
struct dentry *fsutils_get_dentry(struct dentry *root, const char *path);
struct dentry *fsutils_find_by_inode(struct dentry *root, ino_t inode);
struct dentry *fsutils_get_current(struct dentry *parent);
struct dentry *fsutils_create_dentry(const char *path, mode_t mode);
struct dentry *fsutils_create_version_dir(struct dentry *parent, int version);
void fsutils_dispose_tree(struct dentry *dentry);
void fsutils_dispose_node(struct dentry *dentry);
void fsutils_migrate_children(struct dentry *source, struct dentry *target);

/* Macros to ease the creation of files and directories */
#define INITIALIZE_DENTRY_UNLINKED(_dentry) \
	INIT_LIST_HEAD(&(_dentry)->children); \
	INIT_LIST_HEAD(&(_dentry)->xattrs); \
	pthread_mutex_init(&(_dentry)->mutex, NULL); \

#define CREATE_COMMON(_parent,_dentry) \
	INITIALIZE_DENTRY_UNLINKED(_dentry); \
	if ((_dentry)->obj_type != OBJ_TYPE_FIFO) \
		_parent->size += (_dentry)->size; \
	(_dentry)->parent = _parent; \
	list_add_tail(&(_dentry)->list, &((_parent)->children));

#define UPDATE_COMMON(_dentry,_new_contents,_new_size) \
 	pthread_mutex_lock(&_dentry->mutex); \
 	if (_dentry->size != _new_size) { \
 		free(_dentry->contents); \
		_dentry->contents = malloc(_new_size); \
		memcpy(_dentry->contents, _new_contents, _new_size); \
		_dentry->parent->size -= _dentry->size; \
		_dentry->parent->size += _new_size; \
 		_dentry->size = _new_size; \
 	} else \
 		memcpy(_dentry->contents, _new_contents, _dentry->size); \
 	pthread_mutex_unlock(&_dentry->mutex);

#define UPDATE_NAME(_dentry,_name) \
	free(_dentry->name); \
	_dentry->name = strdup(_name)

#define UPDATE_PARENT(_dentry,_parent) \
	if (_dentry->parent != _parent) { \
		list_del(&(_dentry)->list); \
 		if ((_dentry)->obj_type != OBJ_TYPE_FIFO) \
 			_parent->size += (_dentry)->size; \
 		(_dentry)->parent = _parent; \
		list_add_tail(&(_dentry)->list, &_parent->children); \
	}

#define CREATE_FILE_BIN(parent,header,member,_size) \
	({ \
	 	struct dentry *_dentry = fsutils_get_child(parent, #member); \
	 	if (_dentry) { \
	 		UPDATE_COMMON(_dentry, (header)->member, _size); \
		} else { \
	 		_dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
	 		_dentry->contents = malloc(_size); \
	 		memcpy(_dentry->contents, (header)->member, _size); \
			_dentry->name = strdup((char *) #member); \
			_dentry->size = _size; \
			_dentry->mode = S_IFREG | 0444; \
	 		_dentry->obj_type = OBJ_TYPE_FILE; \
			CREATE_COMMON((parent),_dentry); \
			xattr_add(_dentry, XATTR_FORMAT, XATTR_FORMAT_BIN, strlen(XATTR_FORMAT_BIN), false); \
	 	} \
	 	_dentry; \
	})

#define CREATE_FILE_NUMBER(_parent,header,member) \
	({ \
	 	uint64_t member64 = (uint64_t) (header)->member; \
	 	struct dentry *_dentry = fsutils_get_child((_parent), #member); \
	 	if (_dentry) { \
	 		pthread_mutex_lock(&_dentry->mutex); \
	 		free(_dentry->contents); \
			asprintf(&_dentry->contents, "%#04llx", member64); \
			_dentry->parent->size -= _dentry->size; \
			_dentry->size = strlen(_dentry->contents); \
			_dentry->parent->size += _dentry->size; \
	 		pthread_mutex_unlock(&_dentry->mutex); \
	 	} else { \
			_dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
			asprintf(&_dentry->contents, "%#04llx", member64); \
			_dentry->name = strdup(#member); \
			_dentry->size = strlen(_dentry->contents); \
			_dentry->mode = S_IFREG | 0444; \
	 		_dentry->obj_type = OBJ_TYPE_FILE; \
			CREATE_COMMON((_parent),_dentry); \
			xattr_add(_dentry, XATTR_FORMAT, XATTR_FORMAT_NUMBER, strlen(XATTR_FORMAT_NUMBER), false); \
	 	} \
	 	_dentry; \
	})

#define CREATE_FILE_STRING(_parent,header,member,fmt) \
	({ \
	 	struct dentry *_dentry = fsutils_get_child(_parent, #member); \
	 	if (_dentry) { \
	 		UPDATE_COMMON(_dentry, (header)->member, strlen((header)->member)); \
	 	} else { \
			_dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
			_dentry->contents = strdup((header)->member); \
			_dentry->name = strdup(#member); \
			_dentry->size = strlen(_dentry->contents); \
			_dentry->mode = S_IFREG | 0444; \
	 		_dentry->obj_type = OBJ_TYPE_FILE; \
			CREATE_COMMON((_parent),_dentry); \
			xattr_add(_dentry, XATTR_FORMAT, fmt, strlen(fmt), false); \
	 	} \
	 	_dentry; \
	})

#define CREATE_SIMPLE_FILE(_parent,_name,_size,_inode) \
	({ \
	    struct dentry *_dentry = fsutils_find_by_inode(_parent, _inode); \
	 	if (! _dentry) _dentry = fsutils_get_child(_parent, _name); \
	 	if (! _dentry || _dentry->inode != _inode) { \
	 		_dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
	 		_dentry->contents = _size ? malloc(_size) : NULL; \
			_dentry->name = strdup(_name); \
			_dentry->size = _size; \
	 		_dentry->inode = _inode; \
			_dentry->mode = S_IFREG | 0444; \
	 		_dentry->obj_type = OBJ_TYPE_FILE; \
			CREATE_COMMON((_parent),_dentry); \
			xattr_add(_dentry, XATTR_FORMAT, XATTR_FORMAT_BIN, strlen(XATTR_FORMAT_BIN), false); \
	 	} else { \
	 		UPDATE_NAME(_dentry,_name); \
	 		UPDATE_PARENT(_dentry,_parent); \
	 	} \
	 	_dentry; \
	})

#define CREATE_SYMLINK(parent,sname,target) \
	({ \
	 	struct dentry *_dentry = fsutils_get_child(parent, sname); \
	 	if (! _dentry) { \
			struct dentry *_dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
			_dentry->contents = strdup(target); \
			_dentry->name = strdup(sname); \
	 		_dentry->obj_type = OBJ_TYPE_SYMLINK; \
			_dentry->mode = S_IFLNK | 0777; \
			CREATE_COMMON((parent),_dentry); \
	 	} \
	 	_dentry; \
	})

#define CREATE_SNAPSHOT_FILE(parent,fname,es_dentry,priv) \
	({ \
	 	struct dentry *_dentry = fsutils_get_child(parent, fname); \
	 	if (! _dentry) { \
	 		char _path2es[PATH_MAX]; \
	 		struct snapshot_priv *_priv = (struct snapshot_priv *) calloc(1, sizeof(struct snapshot_priv)); \
			_dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
			_dentry->name = strdup(fname); \
			_dentry->mode = S_IFREG | 0444; \
	 		_dentry->size = 0xffffff; \
	 		_dentry->obj_type = OBJ_TYPE_SNAPSHOT; \
	 		_priv->path = strdup(fsutils_realpath(_dentry, _path2es, sizeof(_path2es), priv)); \
	 		_priv->borrowed_es_dentry = es_dentry; \
	 		_dentry->priv = _priv ; \
			CREATE_COMMON((parent),_dentry); \
	 	} \
	 	_dentry; \
	})

#define CREATE_FIFO(parent,ftype,fname,priv) \
	({ \
	 	char _fifo_size[128], _fifo_path[PATH_MAX]; \
	 	struct dentry *_dentry = fsutils_get_child(parent, fname); \
	 	struct fifo *_fifo; \
	 	if (! _dentry) { \
	 		_dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
	 		_dentry->size = fifo_get_default_size(); \
	 		_dentry->name = strdup(fname); \
	 		_dentry->mode = fifo_get_type() | 0777; \
	 		_dentry->obj_type = ftype; \
	 		if (ftype == OBJ_TYPE_VIDEO_FIFO || ftype == OBJ_TYPE_AUDIO_FIFO) { \
	 			struct av_fifo_priv *_priv = (struct av_fifo_priv *) calloc(1, sizeof(struct av_fifo_priv)); \
	 			_priv->fifo = _fifo = (struct fifo *) fifo_init(); \
	 			_dentry->priv = _priv; \
	 		} else { \
	 			struct fifo_priv *_priv = (struct fifo_priv *) calloc(1, sizeof(struct fifo_priv)); \
	 			_priv->fifo = _fifo = (struct fifo *) fifo_init(); \
	 			_dentry->priv = _priv; \
	 		} \
	 		CREATE_COMMON((parent),_dentry); \
	 		fifo_set_path(_fifo, fsutils_realpath(_dentry, _fifo_path, sizeof(_fifo_path), priv)); \
	 	} \
	 	_dentry; \
	})

#define CREATE_DIRECTORY(_parent,_dname...) \
	({ \
	 	char _dbuf[PATH_MAX]; \
	 	struct dentry *_dentry; \
	 	snprintf(_dbuf, sizeof(_dbuf), _dname); \
	 	_dentry = fsutils_get_child(_parent, _dbuf); \
	 	if (! _dentry) { \
			_dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
			_dentry->name = strdup(_dbuf); \
			_dentry->mode = S_IFDIR | 0555; \
	 		_dentry->obj_type = OBJ_TYPE_DIR; \
			CREATE_COMMON((_parent),_dentry); \
	 	} else if (_dentry->parent != _parent) { \
	 		/* Update parent */ \
	 		list_del(&_dentry->list); \
	 		if ((_dentry)->obj_type != OBJ_TYPE_FIFO) \
	 			_parent->size += (_dentry)->size; \
	 		(_dentry)->parent = _parent; \
	 		list_add_tail(&(_dentry)->list, &((_parent)->children)); \
	 	} \
	 	_dentry; \
	})

#define CREATE_SIMPLE_DIRECTORY(_parent,_dname,_inode) \
	({ \
	    struct dentry *_dentry = fsutils_find_by_inode(_parent, _inode); \
	 	if (! _dentry) _dentry = fsutils_get_child(_parent, _dname); \
	 	if (! _dentry || _dentry->inode != _inode) { \
			_dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
			_dentry->name = strdup(_dname); \
			_dentry->mode = S_IFDIR | 0555; \
	 		_dentry->obj_type = OBJ_TYPE_DIR; \
	 		_dentry->inode = _inode; \
			CREATE_COMMON((_parent),_dentry); \
	 	} else if (_dentry->parent != _parent) { \
	 		/* Update parent */ \
	 		list_del(&_dentry->list); \
	 		if ((_dentry)->obj_type != OBJ_TYPE_FIFO) \
	 			_parent->size += (_dentry)->size; \
	 		(_dentry)->parent = _parent; \
	 		list_add_tail(&(_dentry)->list, &((_parent)->children)); \
	 	} \
	 	_dentry; \
	})

#endif /* __fsutils_h */
