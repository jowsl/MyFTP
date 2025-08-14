#include <iostream>
#include <string>
#include <cstring> // menset() e strlen()
#include <unistd.h> // close()
#include <pthread.h> // manipulação dos threads
// bibliotecas que vamos usar para sockets com WSL (API POSIX)
#include <sys/socket.h> // pardão para sockets no linux
#include <netinet/in.h>  // essa contem as strucks de adress -- struct sockaddr_in
#include <arpa/inet.h>  // para usar a inet_pton que converte a string SERVER_IP para formato de rede.
#include <cerrno> // para usar errno
#include <vector> // para usar vector como buffer

#include "udp_seguro.h" // funções de envio/recebimento seguro

constexpr int PORT = 8080; // Porta d
constexpr int TAM_BUFFER = 1024;
constexpr const char* SERVER_IP = "127.0.0.1"; //localhost padrão

using namespace std;


int main(){
    
    int socket_cliente = socket(AF_INET, SOCK_DGRAM, 0); // socket

    // configuração do endereço do servidor
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Erro ao converter o endereço IP!");
        close(socket_cliente);
        return 1;
    }

    cout << "Cliente UDP iniciado. sair para finalizar" << endl;

    uint32_t id_sequencia = 1; //contador de sequência

    while (true) {
        cout << "> ";
        string comando;
        getline(cin, comando);

        if (comando.empty()) continue;

        if (comando == "...") {
            std::string texto_longo;
            for (int i = 0; i < 150; ++i) {
                texto_longo += "0123456789";
            }
            comando = texto_longo; // substitui o comando por um texto longo
        }

        // função para enviar dados que vai chamr o envio seguro
        uint32_t ultimo_id = enviar_dados(socket_cliente, server_addr, id_sequencia, comando);
        //construção do pacote
        if (ultimo_id > 0) {
        id_sequencia = ultimo_id + 1; //atualiza id para o próximo envio

        if (comando == "sair") break;

        //função para receber dados que vai chamar o recebimento seguro
        string resposta_servidor;
        struct sockaddr_in remetente;
        cout << "Aguardando resposta do servidor..." << endl;
        
        if (receber_dados(socket_cliente, resposta_servidor, remetente)) {
            cout << "Servidor: " << resposta_servidor << endl;
        } else {
            cout << "Falha ao receber a resposta." << endl;
        }

    } else {
        cout << "Falha ao enviar o comando para o servidor." << endl;
    }
}

    close(socket_cliente); // fecha o socket
    return 0;
}
