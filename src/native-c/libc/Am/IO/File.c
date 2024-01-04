#include <libc/core.h>
#include <Am/IO/File.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/String.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
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
		Am_Collections_List_add_0_object(list, filename_str);
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



