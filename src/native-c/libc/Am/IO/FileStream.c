#include <libc/core.h>
#include <Am/IO/FileStream.h>
#include <Am/IO/Stream.h>
#include <Am/IO/File.h>
#include <Am/Lang/Array.h>
#include <Am/Lang/Byte.h>
#include <Am/Lang/Long.h>
#include <libc/core_inline_functions.h>

#include <stdio.h>
#include <errno.h>
#include <limits.h>
#if defined(__APPLE__) || defined(__linux__)
#include <unistd.h>
#include <fcntl.h>
#endif
#include <string.h>
#if defined(PLATFORM_AMIGAOS) || defined(PLATFORM_MORPHOS_PPC)
#include <dos/dosextens.h>
#include <proto/dos.h>
#endif

#if defined(PLATFORM_AMIGAOS) || defined(PLATFORM_MORPHOS_PPC)
// Worker isolates are separate Exec processes. Avoid libnix's shared stdio
// descriptor/buffer tables: each stream owns a DOS handle directly instead.
// These aliases are confined to this translation unit.
typedef struct {
    BPTR handle;
    int append;
    int readable;
    int writable;
} am_dos_stream;
static am_dos_stream *am_dos_open(const char *path, const char *mode)
{
    LONG access = mode[0] == 'w' ? MODE_NEWFILE :
        (mode[0] == 'a' ? MODE_READWRITE : MODE_OLDFILE);
    BPTR handle = Open((CONST_STRPTR)path, access);
    if (!handle) { errno = EIO; return NULL; }
    am_dos_stream *stream = calloc(1, sizeof(*stream));
    if (!stream) { Close(handle); errno = ENOMEM; return NULL; }
    stream->handle = handle;
    stream->append = mode[0] == 'a';
    stream->readable = mode[0] == 'r' || strchr(mode, '+') != NULL;
    stream->writable = mode[0] != 'r' || strchr(mode, '+') != NULL;
    return stream;
}
static int am_dos_close(am_dos_stream *stream)
{
    LONG ok = Close(stream->handle);
    free(stream);
    if (!ok) { errno = EIO; return EOF; }
    return 0;
}
static long am_dos_tell(am_dos_stream *stream)
{
    LONG position = Seek(stream->handle, 0, OFFSET_CURRENT);
    if (position < 0) { errno = EIO; }
    return position;
}
static int am_dos_seek(am_dos_stream *stream, long offset, int origin)
{
    LONG mode = origin == SEEK_SET ? OFFSET_BEGINNING :
        (origin == SEEK_END ? OFFSET_END : OFFSET_CURRENT);
    if (Seek(stream->handle, offset, mode) < 0) { errno = EIO; return -1; }
    return 0;
}
static size_t am_dos_read(void *buffer, size_t size, size_t count, am_dos_stream *stream)
{
    if (!size || !count) { return 0; }
    if (!stream->readable || count > LONG_MAX / size) { errno = EINVAL; return 0; }
    LONG got = Read(stream->handle, buffer, (LONG)(size * count));
    if (got < 0) { errno = EIO; return 0; }
    return (size_t)got / size;
}
static size_t am_dos_write(const void *buffer, size_t size, size_t count, am_dos_stream *stream)
{
    if (!size || !count) { return 0; }
    if (!stream->writable || count > LONG_MAX / size) { errno = EINVAL; return 0; }
    if (stream->append && am_dos_seek(stream, 0, SEEK_END) != 0) { return 0; }
    LONG wrote = Write(stream->handle, (APTR)buffer, (LONG)(size * count));
    if (wrote < 0) { errno = EIO; return 0; }
    return (size_t)wrote / size;
}
static int am_dos_flush(am_dos_stream *stream)
{
    if (!stream->writable) { return 0; }
    if (!Flush(stream->handle)) { errno = EIO; return EOF; }
    return 0;
}
#define FILE am_dos_stream
#define fopen am_dos_open
#define fclose am_dos_close
#define ftell am_dos_tell
#define fseek am_dos_seek
#define fread am_dos_read
#define fwrite am_dos_write
#define fflush am_dos_flush
#endif

typedef struct _file_holder file_holder;

struct _file_holder {
	FILE *file;
};

char * const get_file_access_mode(aobject * const this) {
	// Get the FileAccess enum from the FileStream's access property (index 1, file is index 0)
	property access_prop = this->object_properties.class_object_properties.properties[Am_IO_FileStream_P_access];
	
	// FileAccess enum is stored directly as an int in the nullable_value
	int access_value = access_prop.nullable_value.value.int_value;
	
	// Map enum values to C file mode strings. We always use binary
	// mode ("b") because callers expect read/write to round-trip the
	// raw bytes. On POSIX systems text and binary modes are identical
	// so this changes nothing; on AmigaOS / Windows-style libcs the
	// text mode performs LF↔CRLF translation, which silently corrupts
	// binary payloads like zlib streams.
	switch (access_value) {
		case 1: return "rb";    // readOnly
		case 2: return "wb";    // writeOnly
		case 3: return "ab";    // appendOnly
		case 4: return "rb+";   // readWrite
		case 5: return "wb+";   // readWriteTruncate
		case 6: return "ab+";   // readAppend
		default: return "rb+";  // Default fallback
	}
}

char * const get_file_path(aobject * const this) {
	// Get the File object from the FileStream's file property (index 0)
	property file_prop = this->object_properties.class_object_properties.properties[Am_IO_FileStream_P_file];
	if (file_prop.nullable_value.flags == 0 && file_prop.nullable_value.value.object_value != NULL) {
		aobject * const file = file_prop.nullable_value.value.object_value;
		
		// Get the filename string from the File object's filename property (index 0)
		property filename_prop = file->object_properties.class_object_properties.properties[Am_IO_File_P_filename];
		if (filename_prop.nullable_value.flags == 0 && filename_prop.nullable_value.value.object_value != NULL) {
			aobject * const path = filename_prop.nullable_value.value.object_value;
			string_holder *holder = path->object_properties.class_object_properties.object_data.value.custom_value;
			if ( holder != NULL ) {
				return holder->string_value;
			}
		}
	}
	return NULL;
}

function_result Am_IO_FileStream__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	char * const path = get_file_path(this);
	char * const mode = get_file_access_mode(this);
	
	FILE *f = fopen(path, mode);
	// throw exception if not found or any other error
	if (f == NULL) {
		// Name the file (and mode / OS reason) — "Failed to open file" alone
		// is useless when a config or asset path is wrong on the user's box.
		char msg[1024];
		snprintf(msg, sizeof(msg), "Failed to open file '%s' (mode %s): %s",
			path != NULL ? path : "(null)", mode != NULL ? mode : "(null)", strerror(errno));
		__throw_simple_exception_copy(msg, "in Am_IO_FileStream__native_init_0", &__result);
		goto __exit;
    }
	file_holder *holder = calloc(1, sizeof(file_holder));
	this->object_properties.class_object_properties.object_data.value.custom_value = holder;
	holder->file = f;
__exit: ;
	return __result;
};

function_result Am_IO_FileStream__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	file_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;
	if (holder != NULL) {
		// Holder->file may already be NULL if close() was called
		// explicitly — see Am_IO_FileStream_close_0 below.
		if (holder->file != NULL) {
			fclose(holder->file);
			holder->file = NULL;
		}
		free(holder);
		this->object_properties.class_object_properties.object_data.value.custom_value = NULL;
	}

__exit: ;
	return __result;
};

function_result Am_IO_FileStream_close_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	// Eagerly release the OS file handle. Needed on AmigaOS where
	// a same-process write→read on one path doesn't round-trip
	// while the writer's FILE* is still alive. Idempotent — a
	// second close() (or the eventual native_release_0) is a no-op
	// on the already-NULLed file pointer.
	file_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;
	if (holder != NULL && holder->file != NULL) {
		fclose(holder->file);
		holder->file = NULL;
	}

__exit: ;
	return __result;
};

function_result Am_IO_FileStream__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
};

// Size of the OPEN file, measured through the handle (fseek/ftell) rather
// than a path stat(). This is cwd-independent — important because a
// -noixemul child (e.g. am-git spawned by another process) can have a
// libc cwd that doesn't match its DOS pr_CurrentDir, making a stat() on a
// relative path fail. readAll() relies on this to always take the fast
// bulk-read path instead of the byte-by-byte fallback.
function_result Am_IO_FileStream_fileSize_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	file_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;
	long long size = 0;
	if (holder != NULL && holder->file != NULL) {
		long cur = ftell(holder->file);
		if (fseek(holder->file, 0, SEEK_END) == 0) {
			long end = ftell(holder->file);
			if (end >= 0) {
				size = (long long) end;
			}
			fseek(holder->file, (cur >= 0) ? cur : 0, SEEK_SET);
		}
	}
	__result.return_value.value.long_value = size;

__exit: ;
	return __result;
};

function_result Am_IO_FileStream_read_0(aobject * const this, aobject * buffer, long long offset, unsigned int length)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	file_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;

	array_holder *a_holder = (array_holder *) ((char *) buffer + sizeof(aobject)); // buffer->object_properties.class_object_properties.object_data.value.custom_value;
	__result.return_value.value.uint_value = fread(a_holder->array_data + offset, 1, length, holder->file);

__exit: ;
	return __result;
};

function_result Am_IO_FileStream_write_0(aobject * const this, aobject * buffer, long long offset, unsigned int length)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	file_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;

	array_holder *a_holder = (array_holder *) ((char *) buffer + sizeof(aobject)); 
	// buffer->object_properties.class_object_properties.object_data.value.custom_value;
	fwrite(a_holder->array_data + offset, 1, length, holder->file);
	fflush(holder->file);

__exit: ;
	return __result;
};

function_result Am_IO_FileStream_seekFromStart_0(aobject * const this, long long offset)
{
	function_result __result = { .has_return_value = false };
	file_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;
	// fread's offset argument addresses its destination buffer, not the file.
	// Paged readers depend on this absolute seek; the old no-op silently read
	// the following bytes instead of the requested page. Check the cast too:
	// Amiga libc uses a 32-bit long even though the API accepts a Long.
	if (holder == NULL || holder->file == NULL) {
		__throw_simple_exception_copy("Cannot seek a closed file", "in Am_IO_FileStream_seekFromStart_0", &__result);
	} else if (offset < 0 || offset > LONG_MAX) {
		__throw_simple_exception_copy("File seek offset out of range", "in Am_IO_FileStream_seekFromStart_0", &__result);
	} else if (fseek(holder->file, (long) offset, SEEK_SET) != 0) {
		char msg[256];
		snprintf(msg, sizeof(msg), "File seek failed: %s", strerror(errno));
		__throw_simple_exception_copy(msg, "in Am_IO_FileStream_seekFromStart_0", &__result);
	}
	return __result;
};

function_result Am_IO_FileStream_readByte_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	file_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;
	unsigned char bytes[1];
	size_t bytes_read = fread(bytes, 1, 1, holder->file);
	if (bytes_read == 0) {
		__result.return_value = (nullable_value) { .value = { .int_value = -1 }, .flags = 0 };
	} else {
		__result.return_value = (nullable_value) { .value = { .int_value = bytes[0] }, .flags = 0 };
	}

__exit: ;
	return __result;
};

function_result Am_IO_FileStream_writeByte_0(aobject * const this, int byte)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	file_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;
	unsigned char bytes[1];
	bytes[0] = byte;
	fwrite(bytes, 1, 1, holder->file);
	fflush(holder->file);

__exit: ;
	return __result;
};


#if defined(PLATFORM_AMIGAOS) || defined(PLATFORM_MORPHOS_PPC)
// libnix fsync is a no-op. Ask the filesystem handler to flush both file
// data and directory changes instead; unsupported handlers must fail closed.
static int am_stream_flush_volume(const char *path)
{
    if (path == NULL) { return 0; }
    struct DevProc *device = GetDeviceProc((CONST_STRPTR)path, NULL);
    if (device == NULL) { return 0; }
    LONG ok = DoPkt(device->dvp_Port, ACTION_FLUSH, 0, 0, 0, 0, 0);
    FreeDeviceProc(device);
    return ok != DOSFALSE;
}

static int am_stream_resize(FILE *stream, const char *path, long length)
{
    // Resize the same DOS handle, with no secondary stdio descriptor.
    if (!stream->writable) { return 0; }
    long position = ftell(stream);
    if (position < 0 || fflush(stream) != 0 || fseek(stream, 0, SEEK_SET) != 0) { return 0; }
    int ok = SetFileSize(stream->handle, length, OFFSET_BEGINNING) == length;
    if (fseek(stream, position, SEEK_SET) != 0) { ok = 0; }
    return ok;
}
#endif

// These capabilities deliberately report unsupported rather than pretending a
// stdio flush provides durable storage. Other platforms can supply overrides.
function_result Am_IO_FileStream_setLength_0(aobject * const this, long long length)
{
    function_result result = { .has_return_value = false };
    file_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;
#if defined(__APPLE__) || defined(__linux__)
    if (holder == NULL || holder->file == NULL || length < 0 || length > LONG_MAX) {
        __throw_simple_exception_copy("Invalid file resize", "FileStream.setLength", &result);
    } else if (fflush(holder->file) != 0 || ftruncate(fileno(holder->file), (off_t)length) != 0) {
        __throw_simple_exception_copy("File resize failed", "FileStream.setLength", &result);
    }
#elif defined(PLATFORM_AMIGAOS) || defined(PLATFORM_MORPHOS_PPC)
    if (holder == NULL || holder->file == NULL || length < 0 || length > LONG_MAX || get_file_path(this) == NULL) {
        __throw_simple_exception_copy("Invalid file resize", "FileStream.setLength", &result);
    } else if (!am_stream_resize(holder->file, get_file_path(this), (long)length)) {
        __throw_simple_exception_copy("AmigaDOS file resize failed", "FileStream.setLength", &result);
    }
#else
    __throw_simple_exception_copy("File resize is unsupported on this platform", "FileStream.setLength", &result);
#endif
    return result;
}
function_result Am_IO_FileStream_syncToDisk_0(aobject * const this)
{
    function_result result = { .has_return_value = true };
    result.return_value.value.bool_value = false;
    file_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;
#if defined(__APPLE__) || defined(__linux__)
    if (holder == NULL || holder->file == NULL || fflush(holder->file) != 0) {
        __throw_simple_exception_copy("Cannot flush file", "FileStream.syncToDisk", &result);
        return result;
    }
    int rc;
#if defined(__APPLE__) && defined(F_FULLFSYNC)
    rc = fcntl(fileno(holder->file), F_FULLFSYNC);
#else
    rc = fsync(fileno(holder->file));
#endif
    if (rc != 0) {
        __throw_simple_exception_copy("File storage sync failed", "FileStream.syncToDisk", &result);
    } else { result.return_value.value.bool_value = true; }
#elif defined(PLATFORM_AMIGAOS) || defined(PLATFORM_MORPHOS_PPC)
    if (holder == NULL || holder->file == NULL || fflush(holder->file) != 0 || !am_stream_flush_volume(get_file_path(this))) {
        __throw_simple_exception_copy("Amiga filesystem flush failed or unsupported", "FileStream.syncToDisk", &result);
    } else { result.return_value.value.bool_value = true; }
#endif
    return result;
}
function_result Am_IO_FileStream_syncParentDirectory_0(aobject * const this)
{
    function_result result = { .has_return_value = true };
    result.return_value.value.bool_value = false;
#if defined(__APPLE__) || defined(__linux__)
    const char *path = get_file_path(this);
    if (path == NULL) {
        __throw_simple_exception_copy("Missing file path", "FileStream.syncParentDirectory", &result);
        return result;
    }
    char *parent = strdup(path);
    if (parent == NULL) {
        __throw_simple_exception_copy("Out of memory", "FileStream.syncParentDirectory", &result);
        return result;
    }
    char *slash = strrchr(parent, '/');
    if (slash == parent) { slash[1] = '\0'; }
    else if (slash != NULL) { *slash = '\0'; }
    else { free(parent); parent = strdup("."); }
    if (parent == NULL) {
        __throw_simple_exception_copy("Out of memory", "FileStream.syncParentDirectory", &result);
        return result;
    }
    int fd = open(parent, O_RDONLY | O_DIRECTORY);
    free(parent);
    int rc = (fd < 0) ? -1 : fsync(fd);
    if (fd >= 0) { close(fd); }
    if (rc != 0) {
        __throw_simple_exception_copy("Parent directory sync failed", "FileStream.syncParentDirectory", &result);
    } else { result.return_value.value.bool_value = true; }
#elif defined(PLATFORM_AMIGAOS) || defined(PLATFORM_MORPHOS_PPC)
    if (!am_stream_flush_volume(get_file_path(this))) {
        __throw_simple_exception_copy("Amiga filesystem directory flush failed or unsupported", "FileStream.syncParentDirectory", &result);
    } else { result.return_value.value.bool_value = true; }
#endif
    return result;
}
