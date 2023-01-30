#pragma once
#include <libc/core.h>
#include <Am/IO/Networking/Socket.h>
#include <Am/Lang/Object.h>
#include <Am/IO/Networking/AddressFamily.h>

#include <sys/socket.h>
#include <netinet/in.h>

struct _socket_holder {
	int socket;
};

typedef struct _socket_holder socket_holder;
