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

function_result Am_IO_Networking_Socket_createSocket_0(aobject * const this, int addressFamily, int socketType, int protocolFamily)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}

	int s = socket(addressFamily, socketType, protocolFamily); 
	if ( s < 0 )
	{
		__throw_simple_exception("Unable to create socket", "in Am_IO_Networking_Socket_createSocket_0", &__result);
		goto __exit;
	}

	this->object_properties.class_object_properties.object_data.value.int_value = s;

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_IO_Networking_Socket_connectNative_0(aobject * const this, aobject * hostName, int port, int addressFamily)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
	if (hostName != NULL) {
		__increase_reference_count(hostName);
	}

	int result = 0;
	long ipadd;
	struct sockaddr_in peer_addr;

	string_holder *host_name_holder = this->object_properties.class_object_properties.object_data.value.custom_value;

	struct hostent * hostent = gethostbyname(host_name_holder->string_value);
	if (hostent)
	{
		peer_addr.sin_addr = *(struct in_addr *) hostent->h_addr_list[0];
		peer_addr.sin_family = addressFamily; 
		peer_addr.sin_port =  htons(port); 


		int s = this->object_properties.class_object_properties.object_data.value.int_value;
		result = connect(s, (struct sockaddr *) &peer_addr, sizeof(struct sockaddr_in)); 
		if ( result != 0 )
		{
			__throw_simple_exception("Unable to connect to host", "in Am_IO_Networking_Socket_connectNative_0", &__result);
			goto __exit;
		}
	}
	else
	{
		__throw_simple_exception("Unable to resolve host", "in Am_IO_Networking_Socket_connectNative_0", &__result);
		goto __exit;
	}

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	if (hostName != NULL) {
		__increase_reference_count(hostName);
	}
	return __result;
};