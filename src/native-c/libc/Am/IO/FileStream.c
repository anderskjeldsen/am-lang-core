#include <libc/core.h>
#include <Am/IO/FileStream.h>
#include <Am/IO/Stream.h>
#include <Am/IO/File.h>
#include <Am/Lang/Array.h>
#include <Am/Lang/Byte.h>
#include <Am/Lang/Long.h>
#include <libc/core_inline_functions.h>

#include <stdio.h>

typedef struct _file_holder file_holder;

struct _file_holder {
	FILE *file;
};

char * const get_file_path(aobject * const this) {
	aobject * const file = this->object_properties.class_object_properties.properties[Am_IO_FileStream_P_file].nullable_value.value.object_value;
	if (file != NULL) {
		aobject * const path = file->object_properties.class_object_properties.properties[Am_IO_File_P_filename].nullable_value.value.object_value;
		if (path != NULL) {
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
	if (this != NULL) {
		__increase_reference_count(this);
	}

	char * const path = get_file_path(this);
	FILE *f = fopen(path, "rw"); // TODO: Provide access mode
	// throw exception if not found or any other error
	if (f == NULL) {
		__throw_simple_exception("Failed to open file", "in Am_IO_FileStream__native_init_0", &__result);
		goto __exit;
    }
	file_holder *holder = calloc(1, sizeof(file_holder));
	this->object_properties.class_object_properties.object_data.value.custom_value = holder;
	holder->file = f;
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_IO_FileStream__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	file_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;
	if (holder != NULL) {
		fclose(holder->file);
		free(holder);
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

function_result Am_IO_FileStream_read_0(aobject * const this, aobject * buffer, long long offset, unsigned int length)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
	if (buffer != NULL) {
		__increase_reference_count(buffer);
	}

	file_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;

	array_holder *a_holder = (array_holder *) &buffer[1]; // buffer->object_properties.class_object_properties.object_data.value.custom_value;
	__result.return_value.value.uint_value = fread(a_holder->array_data + offset, 1, length, holder->file);

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	if (buffer != NULL) {
		__decrease_reference_count(buffer);
	}
	return __result;
};

function_result Am_IO_FileStream_write_0(aobject * const this, aobject * buffer, long long offset, unsigned int length)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
	if (buffer != NULL) {
		__increase_reference_count(buffer);
	}

	file_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;

	array_holder *a_holder = (array_holder *) &buffer[1]; 
	// buffer->object_properties.class_object_properties.object_data.value.custom_value;
	fwrite(a_holder->array_data + offset, 1, length, holder->file);

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	if (buffer != NULL) {
		__decrease_reference_count(buffer);
	}
	return __result;
};

function_result Am_IO_FileStream_seekFromStart_0(aobject * const this, long long offset)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_IO_FileStream_readByte_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}

	file_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;
	unsigned char bytes[1];
	size_t bytes_read = fread(bytes, 1, 1, holder->file);
	if (bytes_read == 0) {
		__result.return_value = (nullable_value) { .value = { .int_value = -1 }, .flags = 0 };
	} else {
		__result.return_value = (nullable_value) { .value = { .int_value = bytes[0] }, .flags = 0 };
	}

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_IO_FileStream_writeByte_0(aobject * const this, int byte)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}

	file_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;
	unsigned char bytes[1];
	bytes[0] = byte;
	fwrite(bytes, 1, 1, holder->file);

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

