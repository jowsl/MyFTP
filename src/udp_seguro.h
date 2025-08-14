#pragma once // faz o mesmo que #ifndef, #define e #endif 
#include <netinet/in.h>
#include "protocolo.h"
#include <string>


//camada de confiabilidade, envia pacotes unicos
bool envio_seguro(int, const struct sockaddr_in&, const Pacote&);
bool recebimento_seguro(int, Pacote&, struct sockaddr_in&);

// funções de envio e recebimento, fatia os dados em pacotes se passarem de 1kb
uint32_t enviar_dados(int, const struct sockaddr_in&, uint32_t, const std::string&);
bool receber_dados(int, std::string&, struct sockaddr_in&);