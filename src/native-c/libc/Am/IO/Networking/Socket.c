#include <libc/core.h>
#include <Am/IO/Networking/Socket.h>
#include <libc/Am/IO/Networking/Socket.h>
#include <Am/Lang/Object.h>
#include <Am/IO/Networking/AddressFamily.h>

#include <unistd.h>
#include <string.h>

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

	printf("create socket %d, %d, %d\n", addressFamily, socketType, protocolFamily);
	int s = socket(addressFamily, socketType, protocolFamily); 
	printf("newsocket %d\n", s);
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

	string_holder *host_name_holder = hostName->object_properties.class_object_properties.object_data.value.custom_value;

	printf("host name: %s\n", host_name_holder->string_value);
	struct hostent * hostent = gethostbyname(host_name_holder->string_value);
	if (hostent)
	{
		printf("host: %d\n", *(int *)hostent->h_addr_list[0]);
		peer_addr.sin_addr = *(struct in_addr *) hostent->h_addr_list[0];
		peer_addr.sin_family = addressFamily; 
		peer_addr.sin_port =  htons(port); 

		int s = this->object_properties.class_object_properties.object_data.value.int_value;				
		printf("socket %d\n", s);
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
		__decrease_reference_count(hostName);
	}
	return __result;
};

function_result Am_IO_Networking_Socket_send_0(aobject * const this, aobject * bytes, const int length)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
	if (bytes != NULL) {
		__increase_reference_count(bytes);
	}

	int s = this->object_properties.class_object_properties.object_data.value.int_value;				
	if ( s < 0 )
	{
		__throw_simple_exception("Socket not created", "in Am_IO_Networking_Socket_send_0", &__result);
		__returning = true;
		goto __exit;
	}

	array_holder *a_holder = (array_holder *) &bytes[1]; // bytes->object_properties.class_object_properties.object_data.value.custom_value;

	if (length > a_holder->size) {
		__throw_simple_exception("Send length is bigger than array", "in Am_IO_Networking_Socket_send_0", &__result);
		__returning = true;
		goto __exit;
	}

//	printf("Sending: %s\n", array_holder->array_data);
	int sent = send(s, a_holder->array_data, length, 0);
	__result.return_value.value.int_value = sent;
	__result.return_value.flags = PRIMITIVE_UINT;
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	if (bytes != NULL) {
		__decrease_reference_count(bytes);
	}
	return __result;
};

function_result Am_IO_Networking_Socket_receive_0(aobject * const this, aobject * bytes, const int length)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
	if (bytes != NULL) {
		__increase_reference_count(bytes);
	}

	int s = this->object_properties.class_object_properties.object_data.value.int_value;				
	if ( s < 0 )
	{
		__throw_simple_exception("Socket not created", "in Am_IO_Networking_Socket_send_0", &__result);
		goto __exit;
	}

	array_holder *a_holder = (array_holder *) &bytes[1]; // bytes->object_properties.class_object_properties.object_data.value.custom_value;

	if (length > a_holder->size) {
		__throw_simple_exception("Receive length is bigger than array", "in Am_IO_Networking_Socket_send_0", &__result);
		goto __exit;
	}

	int received = recv(s, a_holder->array_data, length, 0);
//	printf("Received %d bytes\n", received);
//	printf("Received data: %s\n", array_holder->array_data);
	__result.return_value.value.int_value = received;
	__result.return_value.flags = PRIMITIVE_INT;
	__returning = true;

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	if (bytes != NULL) {
		__decrease_reference_count(bytes);
	}
	return __result;
};

function_result Am_IO_Networking_Socket_close_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	// Add reference count for this in Socket.close
	if (this != NULL) {
		__increase_reference_count(this);
	}

	int s = this->object_properties.class_object_properties.object_data.value.int_value;				
	if ( s < 0 )
	{
		__throw_simple_exception("Socket not created", "in Am_IO_Networking_Socket_send_0", &__result);
		goto __exit;
	}

	close(s);
	this->object_properties.class_object_properties.object_data.value.int_value = -1;

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};


