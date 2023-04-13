#include <libc/core.h>
#include <Am/IO/Networking/SslSocketStream.h>
#include <libc/Am/IO/Networking/SslSocketStream.h>
#include <Am/IO/Stream.h>
#include <Am/Lang/Object.h>
#include <Am/IO/Networking/Socket.h>
#include <Am/Lang/Int.h>
#include <Am/Lang/Array.h>
#include <Am/Lang/Byte.h>
#include <Am/Lang/Long.h>
#include <Am/Lang/Exception.h>
#include <Am/Lang/String.h>
#include <libc/Am/IO/Networking/SslSocketStream.h>

function_result Am_IO_Networking_SslSocketStream__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	// Add reference count for this in SslSocketStream._native_init
	if (this != NULL) {
		__increase_reference_count(this);
	}

	SSL_library_init();
    SSL_load_error_strings();

	SSL_CTX *ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (ssl_ctx == NULL) {
		__throw_simple_exception("Failed to create SSL context", "in Am_IO_Networking_SslSocketStream__native_init_0", &__result);
		goto __exit;
    }

/*
	// get cert object
	aobject * const cert_object = this->object_properties.class_object_properties.properties[Am_IO_Networking_SslSocketStream_P_cert].nullable_value.value.object_value;
	// get host name string_holder
	string_holder * const cert_object_string_holder = cert_object->object_properties.class_object_properties.object_data.value.custom_value;

	if (SSL_CTX_load_verify_mem(ssl_ctx, cert_object_string_holder->string_value, cert_object_string_holder->length) != 1) {
		__throw_simple_exception("Failed to load cacert.pem", "in Am_IO_Networking_SslSocketStream__native_init_0", &__result);
		goto __fail2;
	}
	*/

	SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, NULL);

	// Load default trusted CA certificates
	if (!SSL_CTX_set_default_verify_paths(ssl_ctx)) {
		__throw_simple_exception("Failed to set default verify paths", "in Am_IO_Networking_SslSocketStream__native_init_0", &__result);
		goto __fail2;
	}


    SSL *ssl = SSL_new(ssl_ctx);
    if (ssl == NULL) {
		__throw_simple_exception("Failed to create SSL", "in Am_IO_Networking_SslSocketStream__native_init_0", &__result);
		goto __fail2;
    }

	// get socket object
	aobject * const socket = this->object_properties.class_object_properties.properties[Am_IO_Networking_SslSocketStream_P_socket].nullable_value.value.object_value;
	// get socket fd
	int s = socket->object_properties.class_object_properties.object_data.value.int_value;

    if (!SSL_set_fd(ssl, s)) {
		__throw_simple_exception("Failed to set SSL file descriptor", "in Am_IO_Networking_SslSocketStream__native_init_0", &__result);
		goto __fail3;
    }	

	// get host name
	aobject * const host_name = this->object_properties.class_object_properties.properties[Am_IO_Networking_SslSocketStream_P_hostName].nullable_value.value.object_value;
	// get host name string_holder
	string_holder * const host_name_string_holder = host_name->object_properties.class_object_properties.object_data.value.custom_value;

	if (!SSL_set_tlsext_host_name(ssl, host_name_string_holder->string_value)) {
		__throw_simple_exception("Failed to set SSL host name", "in Am_IO_Networking_SslSocketStream__native_init_0", &__result);
		goto __fail4;
	}

    if (SSL_connect(ssl) != 1) {
		__throw_simple_exception("Failed to do SSL handshake", "in Am_IO_Networking_SslSocketStream__native_init_0", &__result);
		goto __fail4;
    }

    X509 *cert = SSL_get_peer_certificate(ssl);
    if (cert == NULL) {
		__throw_simple_exception("Failed to retrieve server certificate", "in Am_IO_Networking_SslSocketStream__native_init_0", &__result);
		goto __fail4;
    }

    long verify_result = SSL_get_verify_result(ssl);
    if (verify_result != X509_V_OK) {
		__throw_simple_exception("Certificate verification failed", "in Am_IO_Networking_SslSocketStream__native_init_0", &__result);
		goto __fail5;
    }

	ssl_socket_stream_holder * holder = calloc(1, sizeof(ssl_socket_stream_holder));
	holder->ssl_ctx = ssl_ctx;
	holder->ssl = ssl;
	holder->cert = cert;
	this->object_properties.class_object_properties.object_data.value.custom_value = holder;

	goto __exit;
__fail5: ;
	X509_free(cert);
__fail4: ;
	SSL_shutdown(ssl);
__fail3: ;
	SSL_free(ssl);
__fail2: ;
	SSL_CTX_free(ssl_ctx);
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_IO_Networking_SslSocketStream__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	ssl_socket_stream_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;

	if (holder != NULL) {
		X509_free(holder->cert);
		SSL_shutdown(holder->ssl);
		SSL_free(holder->ssl);
		SSL_CTX_free(holder->ssl_ctx);
	}

__exit: ;
	return __result;
};

function_result Am_IO_Networking_SslSocketStream_read_0(aobject * const this, aobject * buffer, long long offset, int length)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
	if (buffer != NULL) {
		__increase_reference_count(buffer);
	}
	ssl_socket_stream_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;

	if (holder != NULL) {
				array_holder *array_holder = buffer->object_properties.class_object_properties.object_data.value.custom_value;

		if (length > array_holder->size) {
			__throw_simple_exception("Receive length is bigger than array", "in Am_IO_Networking_Socket_send_0", &__result);
			goto __exit;
		}

		int received = SSL_read(holder->ssl, array_holder->array_data, length);
//		printf("Received %d bytes\n", received);
		//	printf("Received data: %s\n", array_holder->array_data);
		__result.return_value.value.int_value = received;
		__result.return_value.flags = PRIMITIVE_INT;
		__returning = true;
	}

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	if (buffer != NULL) {
		__decrease_reference_count(buffer);
	}
	return __result;
};

function_result Am_IO_Networking_SslSocketStream_write_0(aobject * const this, aobject * buffer, long long offset, int length)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
	if (buffer != NULL) {
		__increase_reference_count(buffer);
	}
	ssl_socket_stream_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;

	if (holder != NULL) {
		array_holder *array_holder = buffer->object_properties.class_object_properties.object_data.value.custom_value;

		if (length > array_holder->size) {
			__throw_simple_exception("Send length is bigger than array", "in Am_IO_Networking_Socket_send_0", &__result);
			__returning = true;
			goto __exit;
		}

//		printf("Sending: %s\n", array_holder->array_data);
		int sent = SSL_write(holder->ssl, array_holder->array_data, length);
		__result.return_value.value.int_value = sent;
		__result.return_value.flags = PRIMITIVE_UINT;
	}
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	if (buffer != NULL) {
		__decrease_reference_count(buffer);
	}
	return __result;
};
