#include "types.h"

typedef void socket;

socket* create_socket();
void close_socket(socket* s);
void connect(socket *s, char* host, uint16_t port);
bool isconnected(socket* s);

