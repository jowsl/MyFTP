#include <iostream>
#include <string>
#include <vector>       // Para usar vector como buffer
#include <cstring>      // Para memset

#include <unistd.h>     // para usar o close
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "udp_seguro.h" // Funções de envio/recebimento seguro

using namespace std;

constexpr int PORT = 8080;
constexpr const char* SERVER_IP = "127.0.0.1";  // ip padrão de Localhost
constexpr int TAM_BUFFER = 1024;

int main()
{

    // criando socket
    int socket_server = socket(AF_INET, SOCK_DGRAM, 0); //sock_dgram é utilizado para UDP
    
    if (socket_server < 0) {
        perror("Erro ao tentar criar o socket!");
        return 1;
    }
    cout << "Socket criado" << endl;

    //config do endereço do servidor

    struct sockaddr_in server_addr;
    // inicia a struck
    memset(&server_addr, 0, sizeof(server_addr));
    // parâmetros do memset: ponteiro pra estrutura, preenchido com 0, tamanho da estrutura


    server_addr.sin_family =  AF_INET;
    //convertendo SERVET_IP, no casso estamos usando o localhost
    //inet_pton converte o endereço IP de string para formato de rede.
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) < 0) {
        cout << "IP inválido.";
        close(socket_server); // fecha caso erro
        return 1;
    }
    // se for usar em outros computadores na mesma rede usar 
    // server_addr.sin_addr.s_addr = INADDR_ANY;  ao invés do if acima
    server_addr.sin_port = htons(PORT);

    // bind é usado para 
    // associar o socket a um endereço e porta específicos.
    // parâmetros: socket, endereço do servidor, tamanho da estrutura do endereço.
    if (bind(socket_server,(const struct sockaddr *) &server_addr, sizeof(server_addr)) < 0){
        perror("Erro no binding!"); 
        close(socket_server); 
        return 1;
    }
    cout << "Servidor: Bind na porta " << PORT << "." << endl;

    uint32_t id_sequencia_resposta = 1; // contador de sequência para as respostas
    while (true) {
        std::string mensagem_recebida;
        struct sockaddr_in endereco_cliente;

        std::cout << "\nServidor aguardando transferencia..." << std::endl;

        // 1. USE A FUNÇÃO DE ALTO NÍVEL para receber a mensagem completa do cliente
        if (receber_dados(socket_server, mensagem_recebida, endereco_cliente)) { 
            
            std::cout << "=================================" << std::endl;
            std::cout << "Transferencia recebida com sucesso!" << std::endl;
            std::cout << "Mensagem: \"" << mensagem_recebida << "\"" << std::endl;

            if (mensagem_recebida == "sair") {
                std::cout << "Comando 'sair' recebido. Encerrando." << std::endl;
                break;
            }

            // 2. ENVIE A RESPOSTA COMPLETA (que pode ser grande ou pequena)
            std::string resposta_str = "Mensagem '" + mensagem_recebida + "' recebida com sucesso!";
            
            std::cout << "Enviando resposta para o cliente..." << std::endl;
            uint32_t ultimo_id = enviar_dados(socket_server, endereco_cliente, id_sequencia_resposta, resposta_str);
            if (ultimo_id > 0) {
                id_sequencia_resposta = ultimo_id + 1;
            } else {
                std::cerr << "Falha ao enviar resposta completa para o cliente." << std::endl;
            }
        }
    }

    close(socket_server);
    return 0;
}