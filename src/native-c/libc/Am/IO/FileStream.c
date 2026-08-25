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
#include <string.h>

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

	array_holder *a_holder = (array_holder *) &buffer[1]; // buffer->object_properties.class_object_properties.object_data.value.custom_value;
	__result.return_value.value.uint_value = fread(a_holder->array_data + offset, 1, length, holder->file);

__exit: ;
	return __result;
};

function_result Am_IO_FileStream_write_0(aobject * const this, aobject * buffer, long long offset, unsigned int length)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	file_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;

	array_holder *a_holder = (array_holder *) &buffer[1]; 
	// buffer->object_properties.class_object_properties.object_data.value.custom_value;
	fwrite(a_holder->array_data + offset, 1, length, holder->file);
	fflush(holder->file);

__exit: ;
	return __result;
};

function_result Am_IO_FileStream_seekFromStart_0(aobject * const this, long long offset)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
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

