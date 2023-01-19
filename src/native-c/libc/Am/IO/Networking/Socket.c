#include <libc/core.h>
#include <Am/IO/Networking/Socket.h>
#include <libc/Am/IO/Networking/Socket.h>
#include <Am/Lang/Object.h>
#include <Am/IO/Networking/AddressFamily.h>

function_result Am_IO_Networking_Socket__native_init_0(aobject * const this)
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

function_result Am_IO_Networking_Socket__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
};

