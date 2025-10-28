#include <libc/core.h>
#include <Am/IO/TempFile.h>
#include <libc/Am/IO/TempFile.h>
#include <Am/Lang/ClassRef.h>
#include <Am/Lang/Object.h>
#include <Am/IO/File.h>
#include <Am/Lang/String.h>
#include <libc/core_inline_functions.h>

function_result Am_IO_TempFile__native_init_0(aobject * const this)
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
}

function_result Am_IO_TempFile__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	
	// Get the file property and delete the temporary file
	aobject *file = this->object_properties.class_object_properties.properties[Am_IO_TempFile_P_file].nullable_value.value.object_value;
	if (file != NULL) {
		// Delete the file
		function_result res2 = Am_IO_File_delete_0(file);
		if (res2.exception != NULL) {
			aobject * const sti = __create_string_constant("... in class Am.IO.TempFile.c line 33", &Am_Lang_String);
			__pass_exception(&__result, res2.exception, sti);
			__returning = true;
			if ( sti != NULL) {
				__decrease_reference_count(sti);
			}
			goto __exit;
		}
	}
	
__exit: ;
	return __result;
}

function_result Am_IO_TempFile__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

