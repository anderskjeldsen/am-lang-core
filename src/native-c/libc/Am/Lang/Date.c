#include <libc/core.h>
#include <Am/Lang/Date.h>
#include <libc/Am/Lang/Date.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/UInt.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Bool.h>
#include <Am/Lang/ULong.h>

function_result Am_Lang_Date__native_init_0(aobject * const this)
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

function_result Am_Lang_Date__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
};

function_result Am_Lang_Date_getMillis_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}

	struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long long milliseconds = tv.tv_sec * 1000LL + tv.tv_usec / 1000;
	__result.return_value.value.ulong_value = milliseconds;
//	__result.return_value.flags = PRIMITIVE_ULONG;
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

