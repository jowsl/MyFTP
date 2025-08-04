#include <iostream>
#include <string>
#include <vector>       // Para usar vector como buffer
#include <cstring>      // Para memset

//Sockets com API POSIX
#include <unistd.h>     // para usar o close
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

constexpr int PORT = 8080;
constexpr const char* SERVER_IP = "127.0.0.1";  // ip padrão de Localhost
constexpr int TAM_BUFFER = 1024;

int main(int argc, char const *argv[])
{

    // criando socket
    int socket_server = socket(AF_INET, SOCK_DGRAM, 0); //sock_dgram é utilizado para UDP
    
    if (socket_server < 0) {
        perror("Erro ao tentar criar o socket!");
        return 1;
    }
    cout << "Socket criado" << endl;

    //config do endereço do servidor

    struct sockaddr_in server_addr, client_addr;
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
    cout << "Servidor: Bind na porta" << PORT << "." << endl;

    // loop com recvfrom: recebe os dados e identifica o remetente.
    while(true){

        vector<char> buffer(TAM_BUFFER);
        // socklen_t é um tipo de dado usado para armazenar o tamanho de estruturas de endereço.
        socklen_t client_len = sizeof(client_addr);
        /*
        recvfrom é usado para receber dados de um socket UDP.
        Parâmetros: socket, buffer para armazenar os dados recebidos, tamanho do buffer, 
        flags, ponteiro para a estrutura do endereço do cliente e 
        ponteiro para o tamanho da estrutura do endereço do cliente.
        metodo .data() do vector é usado para obter o ponteiro para os dados do buffer.
        */
        ssize_t bytes_recebidos = recvfrom(socket_server, buffer.data(), buffer.size(), 0, 
                                            (struct sockaddr*)&client_addr, &client_len);
        if (bytes_recebidos < 0){
            perror("Erro no recvfrom!"); 
            continue;
        }

        // Obtém o IP e a porta do cliente para exibição
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);

        //contruindo uma string com dados recebidos, buffer.data() é o ponteiro para os dados do buffer,
        // e bytes_recebidos é o tamanho dos dados recebidos.
        string mensagem(buffer.data(), bytes_recebidos);

        if (mensagem == "sair") {
            cout << "Servidor encerrado." << endl;
            break; // Encerra o loop se receber a mensagem "sair"
        }

        cout << "=================================" << endl;
        cout << "Recebidos " << bytes_recebidos << " bytes de " << client_ip << ":" << client_port << endl;
        cout << "Mensagem: \"" << mensagem << "\"" << endl;

        // Envia uma Resposta ao Cliente
        const char *response = "Mensagem recebida!";
        sendto(socket_server, response, strlen(response), 0,
               (const struct sockaddr *)&client_addr, client_len);
        
    }

    close(socket_server);
    return 0;
}