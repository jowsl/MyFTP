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

#include "udp_seguro.cpp" // funções de envio/recebimento seguro

constexpr int PORT = 8080; // Porta d
constexpr int TAM_BUFFER = 1024;
constexpr const char* SERVER_IP = "127.0.0.1"; //localhost padrão

using namespace std;


int main(int argc, char const *argv[]){
    
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

    while (true){
        
        cout << "Digite a mensagem: ";
        string input;
        getline(cin, input); // Lê a linha inteira, incluindo espaços

        // envio
        // parametros: socket, dados a serem enviados, tamanho dos dados, flags (0), endereço do servidor e tamanho do endereço do servidor.
        // o c_str() converte a string para um ponteiro de char, necessário para o sendto.
        ssize_t bytes_enviados = sendto(socket_cliente, input.c_str(), input.size(), 0,
                                       (const struct sockaddr *)&server_addr, sizeof(server_addr));
        
        if (bytes_enviados < 0) {
            perror("Erro nada enviado");
            continue;
        }

        if (input == "sair") {
            cout << "Cliente encerrado." << endl;
            break;
        }

        // Dados recebidos
        vector<char> buffer(TAM_BUFFER);
        ssize_t bytes_recebidos = recvfrom(socket_cliente, buffer.data(), buffer.size(), 0, NULL, NULL);

        if (bytes_recebidos < 0) {
            perror("Erro ao receber a resposta");
        }
        string resposta(buffer.data(), bytes_recebidos);
        cout << "Servidor: " << resposta << endl;
    
    }

    close(socket_cliente); // fecha o socket
    return 0;
}
