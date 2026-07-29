#include <libc/core.h>
#include <Am/IO/File.h>
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

#ifdef __amigaos__
#include <proto/dos.h>
#include <dos/dosextens.h>
#endif

function_result Am_IO_File__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
};

function_result Am_IO_File__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	
	// Check if this is a temporary file and delete it if so
	bool isTemporary = this->object_properties.class_object_properties.properties[Am_IO_File_P_temporary].nullable_value.value.bool_value;
	if (isTemporary) {
		// Delete the temporary file
		Am_IO_File_delete_0(this);
	}
	
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
		// Skip the synthetic self/parent entries — POSIX readdir
		// surfaces them but virtually no caller of a high-level
		// `listFiles()` wants them (recursive walks blow up on `.`,
		// counts are off by 2, etc). Matches Java / Python / Node /
		// Rust / Go stdlibs, which all filter at this same layer.
		if (dir->d_name[0] == '.' &&
		    (dir->d_name[1] == '\0' ||
		     (dir->d_name[1] == '.' && dir->d_name[2] == '\0'))) {
			continue;
		}
		aobject *filename_str = __create_string(dir->d_name, &Am_Lang_String);
		Am_Collections_List_ta_Am_Lang_String_f_add_0(list, filename_str);
		__decrease_reference_count(filename_str);
	}
	closedir(d);

__exit: ;
	return __result;
};

// List only the sub-directory names of `folderFilename` — the type bit
// is read during the single listing walk, so no per-entry stat/Examine.
function_result Am_IO_File_listDirsNative_0(aobject * const this, aobject * folderFilename, aobject * list)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	aobject *filename = this->object_properties.class_object_properties.properties[Am_IO_File_P_filename].nullable_value.value.object_value;
	string_holder *filename_string_holder = (string_holder *) (filename + 1);
	const char *dir_path = filename_string_holder->string_value;

#ifdef __amigaos__
	// dos.library walk: Lock + Examine + ExNext gives every entry's
	// fib_DirEntryType in one pass (each ExNext is one packet — the
	// stat() fallback would be Lock+Examine+UnLock per entry through
	// libnix). FIB must be longword-aligned → AllocDosObject.
	BPTR lock = Lock((CONST_STRPTR) dir_path, ACCESS_READ);
	if (lock == (BPTR) NULL) {
		__throw_simple_exception("Failed to open directory", "in Am_IO_File_listDirsNative_0", &__result);
		goto __exit;
	}
	struct FileInfoBlock *fib = (struct FileInfoBlock *) AllocDosObject(DOS_FIB, NULL);
	if (fib == NULL) {
		UnLock(lock);
		__throw_simple_exception("Out of memory listing directory", "in Am_IO_File_listDirsNative_0", &__result);
		goto __exit;
	}
	if (Examine(lock, fib)) {
		while (ExNext(lock, fib)) {
			// >0 = directory-kind. Exclude ST_SOFTLINK (3): it may
			// resolve to a file, and following links during a scan
			// invites cycles.
			LONG t = fib->fib_DirEntryType;
			if (t > 0 && t != ST_SOFTLINK) {
				aobject *name_str = __create_string((const char *) fib->fib_FileName, &Am_Lang_String);
				Am_Collections_List_ta_Am_Lang_String_f_add_0(list, name_str);
				__decrease_reference_count(name_str);
			}
		}
	}
	FreeDosObject(DOS_FIB, fib);
	UnLock(lock);
#else
	DIR *d = opendir(dir_path);
	if (!d) {
		__throw_simple_exception("Failed to open directory", "in Am_IO_File_listDirsNative_0", &__result);
		goto __exit;
	}
	struct dirent *dir;
	while ((dir = readdir(d)) != NULL) {
		if (dir->d_name[0] == '.' &&
		    (dir->d_name[1] == '\0' ||
		     (dir->d_name[1] == '.' && dir->d_name[2] == '\0'))) {
			continue;
		}
		bool is_dir = false;
		bool need_stat = true;
#ifdef DT_DIR
		if (dir->d_type != DT_UNKNOWN) {
			is_dir = (dir->d_type == DT_DIR);
			need_stat = false;
		}
#endif
		if (need_stat) {
			// Listing didn't report the type (no d_type, or the
			// filesystem returns DT_UNKNOWN) — one stat for this entry.
			char full[PATH_MAX];
			int n = snprintf(full, sizeof(full), "%s/%s", dir_path, dir->d_name);
			struct stat s;
			is_dir = (n > 0 && n < (int) sizeof(full)
				&& stat(full, &s) == 0 && S_ISDIR(s.st_mode));
		}
		if (is_dir) {
			aobject *name_str = __create_string(dir->d_name, &Am_Lang_String);
			Am_Collections_List_ta_Am_Lang_String_f_add_0(list, name_str);
			__decrease_reference_count(name_str);
		}
	}
	closedir(d);
#endif

__exit: ;
	return __result;
};

function_result Am_IO_File_isDirectory_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

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
	return __result;
};

// File metadata methods
function_result Am_IO_File_exists_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	aobject *filename = this->object_properties.class_object_properties.properties[Am_IO_File_P_filename].nullable_value.value.object_value;
	string_holder *filename_string_holder = (string_holder *) (filename + 1);
	
	struct stat s;
	__result.return_value.value.bool_value = (stat(filename_string_holder->string_value, &s) == 0);

__exit: ;
	return __result;
};

function_result Am_IO_File_getSize_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	aobject *filename = this->object_properties.class_object_properties.properties[Am_IO_File_P_filename].nullable_value.value.object_value;
	string_holder *filename_string_holder = (string_holder *) (filename + 1);
	
	struct stat s;
	if (stat(filename_string_holder->string_value, &s) == 0) {
		__result.return_value.value.long_value = (long long)s.st_size;
	} else {
		__result.return_value.value.long_value = -1LL;
	}

__exit: ;
	return __result;
};

function_result Am_IO_File_getLastModified_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

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
	return __result;
};

function_result Am_IO_File_canRead_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	aobject *filename = this->object_properties.class_object_properties.properties[Am_IO_File_P_filename].nullable_value.value.object_value;
	string_holder *filename_string_holder = (string_holder *) (filename + 1);
	
	__result.return_value.value.bool_value = (access(filename_string_holder->string_value, R_OK) == 0);

__exit: ;
	return __result;
};

function_result Am_IO_File_canWrite_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	aobject *filename = this->object_properties.class_object_properties.properties[Am_IO_File_P_filename].nullable_value.value.object_value;
	string_holder *filename_string_holder = (string_holder *) (filename + 1);
	
	__result.return_value.value.bool_value = (access(filename_string_holder->string_value, W_OK) == 0);

__exit: ;
	return __result;
};

// File operations methods
function_result Am_IO_File_delete_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

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
	return __result;
};

function_result Am_IO_File_createDirectory_0(aobject * path)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	string_holder *path_string_holder = (string_holder *) (path + 1);
	
#ifdef _WIN32
	int result = _mkdir(path_string_holder->string_value);
#else
	int result = mkdir(path_string_holder->string_value, 0755);
#endif
	
	__result.return_value.value.bool_value = (result == 0);

__exit: ;
	return __result;
};

function_result Am_IO_File_copy_0(aobject * const this, aobject * destination)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

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
	return __result;
};

function_result Am_IO_File_move_0(aobject * const this, aobject * destination)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	aobject *source_filename = this->object_properties.class_object_properties.properties[Am_IO_File_P_filename].nullable_value.value.object_value;
	string_holder *source_string_holder = (string_holder *) (source_filename + 1);
	string_holder *dest_string_holder = (string_holder *) (destination + 1);
	
	int result = rename(source_string_holder->string_value, dest_string_holder->string_value);
	__result.return_value.value.bool_value = (result == 0);

__exit: ;
	return __result;
};

function_result Am_IO_File_createTempFileInternal_0(aobject * directory, aobject * prefix, aobject * suffix)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	aobject *dir_filename = directory->object_properties.class_object_properties.properties[Am_IO_File_P_filename].nullable_value.value.object_value;
	string_holder *dir_string_holder = (string_holder *) (dir_filename + 1);
	string_holder *prefix_string_holder = (string_holder *) (prefix + 1);
	string_holder *suffix_string_holder = (string_holder *) (suffix + 1);
	
	// Create temporary filename in the specified directory
	char temp_template[512];
	snprintf(temp_template, sizeof(temp_template), "%s/%s_XXXXXX", 
		dir_string_holder->string_value, prefix_string_holder->string_value);
	
	// Use mkstemp for safe temporary file creation
	char temp_filename[512];
	strcpy(temp_filename, temp_template);
	
	int fd = mkstemp(temp_filename);
	if (fd == -1) {
		__result.return_value.value.object_value = NULL;
	} else {
		close(fd); // Close the file descriptor, the file now exists on disk
		
		// Now add the suffix by renaming the file
		char final_filename[512];
		snprintf(final_filename, sizeof(final_filename), "%s%s", temp_filename, suffix_string_holder->string_value);
		
		if (rename(temp_filename, final_filename) != 0) {
			// If rename fails, clean up and return null
			unlink(temp_filename);
			__result.return_value.value.object_value = NULL;
		} else {
			// Return the final filename as a String
			aobject *temp_name_str = __create_string(final_filename, &Am_Lang_String);
			__result.return_value.value.object_value = temp_name_str;
		}
	}

__exit: ;
	return __result;
};



