#include <libc/core.h>
#include <Am/IO/File.h>
#include <Am/IO/TempFile.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Long.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <libc/core_inline_functions.h>

function_result Am_IO_File__native_init_0(aobject * const this)
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

function_result Am_IO_File__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
};

function_result Am_IO_File__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
};

function_result Am_IO_File_getCurrentDirectory_0()
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
		aobject * path = __create_string(cwd, &Am_Lang_String);
		__result.return_value.value.object_value = path;
		__returning = true;
    } else {
		__result.return_value.value.object_value = NULL;
		__returning = true;
    }

__exit: ;
	return __result;
};

function_result Am_IO_File_listNative_0(aobject * const this, aobject * folderFilename, aobject * list)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
	if (folderFilename != NULL) {
		__increase_reference_count(folderFilename);
	}
	if (list != NULL) {
		__increase_reference_count(list);
	}

	aobject *filename = this->object_properties.class_object_properties.properties[Am_IO_File_P_filename].nullable_value.value.object_value;
	string_holder *filename_string_holder = (string_holder *) (filename + 1);

	DIR *d;
	struct dirent *dir;
	d = opendir(filename_string_holder->string_value);
	if (!d) {
		__throw_simple_exception("Failed to open directory", "in Am_IO_File_listNative_0", &__result);
		goto __exit;
	}
	while ((dir = readdir(d)) != NULL) {
		aobject *filename_str = __create_string(dir->d_name, &Am_Lang_String);
		Am_Collections_List_ta_Am_Lang_String_f_add_0(list, filename_str);
		__decrease_reference_count(filename_str);

//		printf("%s\n", dir->d_name);
	}
	closedir(d);

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	if (folderFilename != NULL) {
		__decrease_reference_count(folderFilename);
	}
	if (list != NULL) {
		__decrease_reference_count(list);
	}
	return __result;
};

function_result Am_IO_File_isDirectory_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}

	aobject *filename = this->object_properties.class_object_properties.properties[Am_IO_File_P_filename].nullable_value.value.object_value;
	string_holder *filename_string_holder = (string_holder *) (filename + 1);
	
	struct stat s;

	if (stat(filename_string_holder->string_value, &s) == 0) {
		__result.return_value.value.bool_value = S_ISDIR(s.st_mode);		
    } else {
		__throw_simple_exception("Failed to check if file is directory", "in Am_IO_File_isDirectory_0", &__result);
		goto __exit;
    }
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

// File metadata methods
function_result Am_IO_File_exists_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}

	aobject *filename = this->object_properties.class_object_properties.properties[Am_IO_File_P_filename].nullable_value.value.object_value;
	string_holder *filename_string_holder = (string_holder *) (filename + 1);
	
	struct stat s;
	__result.return_value.value.bool_value = (stat(filename_string_holder->string_value, &s) == 0);

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_IO_File_getSize_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}

	aobject *filename = this->object_properties.class_object_properties.properties[Am_IO_File_P_filename].nullable_value.value.object_value;
	string_holder *filename_string_holder = (string_holder *) (filename + 1);
	
	struct stat s;
	if (stat(filename_string_holder->string_value, &s) == 0) {
		__result.return_value.value.long_value = (long long)s.st_size;
	} else {
		__result.return_value.value.long_value = -1LL;
	}

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_IO_File_getLastModified_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}

	aobject *filename = this->object_properties.class_object_properties.properties[Am_IO_File_P_filename].nullable_value.value.object_value;
	string_holder *filename_string_holder = (string_holder *) (filename + 1);
	
	struct stat s;
	if (stat(filename_string_holder->string_value, &s) == 0) {
		// Convert time_t to milliseconds since epoch
		__result.return_value.value.long_value = (long long)s.st_mtime * 1000LL;
	} else {
		__result.return_value.value.long_value = 0LL;
	}

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_IO_File_canRead_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}

	aobject *filename = this->object_properties.class_object_properties.properties[Am_IO_File_P_filename].nullable_value.value.object_value;
	string_holder *filename_string_holder = (string_holder *) (filename + 1);
	
	__result.return_value.value.bool_value = (access(filename_string_holder->string_value, R_OK) == 0);

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_IO_File_canWrite_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}

	aobject *filename = this->object_properties.class_object_properties.properties[Am_IO_File_P_filename].nullable_value.value.object_value;
	string_holder *filename_string_holder = (string_holder *) (filename + 1);
	
	__result.return_value.value.bool_value = (access(filename_string_holder->string_value, W_OK) == 0);

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

// File operations methods
function_result Am_IO_File_delete_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}

	aobject *filename = this->object_properties.class_object_properties.properties[Am_IO_File_P_filename].nullable_value.value.object_value;
	string_holder *filename_string_holder = (string_holder *) (filename + 1);
	
	struct stat s;
	int result;
	
	if (stat(filename_string_holder->string_value, &s) == 0) {
		if (S_ISDIR(s.st_mode)) {
			result = rmdir(filename_string_holder->string_value);
		} else {
			result = unlink(filename_string_holder->string_value);
		}
		__result.return_value.value.bool_value = (result == 0);
	} else {
		__result.return_value.value.bool_value = false;
	}

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_IO_File_createDirectory_0(aobject * path)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (path != NULL) {
		__increase_reference_count(path);
	}

	string_holder *path_string_holder = (string_holder *) (path + 1);
	
#ifdef _WIN32
	int result = _mkdir(path_string_holder->string_value);
#else
	int result = mkdir(path_string_holder->string_value, 0755);
#endif
	
	__result.return_value.value.bool_value = (result == 0);

__exit: ;
	if (path != NULL) {
		__decrease_reference_count(path);
	}
	return __result;
};

function_result Am_IO_File_copy_0(aobject * const this, aobject * destination)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
	if (destination != NULL) {
		__increase_reference_count(destination);
	}

	aobject *source_filename = this->object_properties.class_object_properties.properties[Am_IO_File_P_filename].nullable_value.value.object_value;
	string_holder *source_string_holder = (string_holder *) (source_filename + 1);
	string_holder *dest_string_holder = (string_holder *) (destination + 1);
	
	FILE *src = fopen(source_string_holder->string_value, "rb");
	if (!src) {
		__result.return_value.value.bool_value = false;
		goto __exit;
	}
	
	FILE *dst = fopen(dest_string_holder->string_value, "wb");
	if (!dst) {
		fclose(src);
		__result.return_value.value.bool_value = false;
		goto __exit;
	}
	
	char buffer[8192];
	size_t bytes;
	bool success = true;
	
	while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
		if (fwrite(buffer, 1, bytes, dst) != bytes) {
			success = false;
			break;
		}
	}
	
	fclose(src);
	fclose(dst);
	
	__result.return_value.value.bool_value = success;

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	if (destination != NULL) {
		__decrease_reference_count(destination);
	}
	return __result;
};

function_result Am_IO_File_move_0(aobject * const this, aobject * destination)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
	if (destination != NULL) {
		__increase_reference_count(destination);
	}

	aobject *source_filename = this->object_properties.class_object_properties.properties[Am_IO_File_P_filename].nullable_value.value.object_value;
	string_holder *source_string_holder = (string_holder *) (source_filename + 1);
	string_holder *dest_string_holder = (string_holder *) (destination + 1);
	
	int result = rename(source_string_holder->string_value, dest_string_holder->string_value);
	__result.return_value.value.bool_value = (result == 0);

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	if (destination != NULL) {
		__decrease_reference_count(destination);
	}
	return __result;
};

function_result Am_IO_File_createTempFileInternal_0(aobject * prefix, aobject * suffix)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (prefix != NULL) {
		__increase_reference_count(prefix);
	}
	if (suffix != NULL) {
		__increase_reference_count(suffix);
	}

	string_holder *prefix_string_holder = (string_holder *) (prefix + 1);
	string_holder *suffix_string_holder = (string_holder *) (suffix + 1);
	
	// Create temporary filename
	char temp_template[256];
	snprintf(temp_template, sizeof(temp_template), "/tmp/%s_XXXXXX%s", 
		prefix_string_holder->string_value, suffix_string_holder->string_value);
	
	// Use mkstemp for safe temporary file creation
	char temp_filename[256];
	strcpy(temp_filename, temp_template);
	
	int fd = mkstemp(temp_filename);
	if (fd == -1) {
		__result.return_value.value.object_value = NULL;
	} else {
		close(fd); // Close the file descriptor, we just needed the name
		
		// Create File object with the temporary filename
		aobject *temp_name_str = __create_string(temp_filename, &Am_Lang_String);
		aobject *file_obj = __allocate_object(&Am_IO_File);
		
		if (file_obj && temp_name_str) {
			// Initialize the File object
			nullable_value file_nv = { .flags = 0, .value = { .object_value = file_obj } };
			Am_IO_File___init_instance(file_nv);
			
			// Call the File constructor with the filename
			Am_IO_File_f_File_0(file_obj, temp_name_str);
			
			__result.return_value.value.object_value = file_obj;
		} else {
			__result.return_value.value.object_value = NULL;
		}
		
		if (temp_name_str) {
			__decrease_reference_count(temp_name_str);
		}
	}

__exit: ;
	if (prefix != NULL) {
		__decrease_reference_count(prefix);
	}
	if (suffix != NULL) {
		__decrease_reference_count(suffix);
	}
	return __result;
};



