#ifndef native_libc_aclass_Am_IO_FileStream_c
#define native_libc_aclass_Am_IO_FileStream_c
#include <core.h>
#include <Am/IO/FileStream.h>
#include <Am/IO/Stream.h>
#include <Am/IO/File.h>
#include <Am/Lang/Array.h>
#include <Am/Lang/Byte.h>
#include <Am/Lang/Long.h>

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

	file_holder *holder = malloc(sizeof(file_holder));
	this->object_properties.class_object_properties.object_data.value.custom_value = holder;

	char * const path = get_file_path(this);
	holder->file = fopen(path, "rw"); // TODO: Provide access mode
	// throw exception if not found or any other error

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
	fclose(holder->file);
	free(holder);

__exit: ;
	return __result;
};

function_result Am_IO_FileStream_read_0(aobject * const this, aobject * buffer, long long offset, long long length)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}

	file_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;

	array_holder *array_holder = buffer->object_properties.class_object_properties.object_data.value.custom_value;
	fread(array_holder->array_data + offset, 1, length, holder->file);

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	if (buffer != NULL) {
		__increase_reference_count(buffer);
	}
	return __result;
};

function_result Am_IO_FileStream_write_0(aobject * const this, aobject * buffer, long long offset, long long length)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}

	file_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;

	array_holder *array_holder = buffer->object_properties.class_object_properties.object_data.value.custom_value;
	fwrite(array_holder->array_data + offset, 1, length, holder->file);

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	if (buffer != NULL) {
		__increase_reference_count(buffer);
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
	fread(bytes, 1, 1, holder->file);
	__result.return_value = (nullable_value) { .value = { .int_value = bytes[0] }, .flags = 0 };

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

#endif
