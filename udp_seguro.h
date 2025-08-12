#pragma once // faz o mesmo que #ifndef, #define e #endif 
#include <netinet/in.h>
#include "protocolo.h"

bool envio_seguro(int, const struct sockaddr_in&, const Pacote&);
bool recebimento_seguro(int, Pacote&, struct sockaddr_in&);
