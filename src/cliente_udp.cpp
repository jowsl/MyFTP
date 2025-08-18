#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sstream>      // Para parsear o comando do usuário
#include <fstream>      // Para salvar e ler arquivos
#include <filesystem>   // Para verificar se arquivos existem

#include "udp_seguro.h" // Funções de envio/recebimento seguro

constexpr int PORT = 8080;
constexpr const char* SERVER_IP = "127.0.0.1";

int main() {
    int socket_cliente = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_cliente < 0) {
        perror("Erro ao criar o socket do cliente");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Erro ao converter o endereço IP!");
        close(socket_cliente);
        return 1;
    }

    std::cout << "Cliente MyFTP conectado. Digite um comando (login, ls, cd, get, put, sair)." << std::endl;

    uint32_t id_sequencia = 1;

    while (true) {
        std::cout << "> ";
        std::string linha_comando;
        std::getline(std::cin, linha_comando);

        if (linha_comando.empty()) continue;

        // Parseia o comando e o argumento digitado pelo usuário
        std::stringstream ss(linha_comando);
        std::string str_comando, argumento;
        ss >> str_comando;
        std::getline(ss >> std::ws, argumento);

        // Lógica especial para 'put': precisamos ler o arquivo ANTES de enviar
        if (str_comando == "put") {
            if (argumento.empty()) {
                std::cout << "Uso: put <arquivo_local>" << std::endl;
                continue;
            }
            if (!std::filesystem::exists(argumento)) {
                std::cerr << "ERRO: Arquivo local '" << argumento << "' não encontrado." << std::endl;
                continue;
            }

            // 1. Envia o comando 'put' inicial para o servidor
            std::cout << "Iniciando 'put' para o arquivo: " << argumento << std::endl;
            uint32_t ultimo_id = enviar_dados(socket_cliente, server_addr, id_sequencia, linha_comando);
            if (!ultimo_id) {
                std::cerr << "Falha ao enviar comando 'put' para o servidor." << std::endl;
                continue;
            }
            id_sequencia = ultimo_id + 1;

            // 2. Espera o "OK" do servidor
            std::string resposta_ok;
            struct sockaddr_in remetente;
            if (receber_dados(socket_cliente, resposta_ok, remetente) && resposta_ok == "OK") {
                // 3. Servidor está pronto, agora envia o conteúdo do arquivo
                std::cout << "Servidor pronto. Enviando conteúdo do arquivo..." << std::endl;
                std::ifstream arquivo_local(argumento, std::ios::binary);
                std::stringstream buffer_arquivo;
                buffer_arquivo << arquivo_local.rdbuf();
                
                ultimo_id = enviar_dados(socket_cliente, server_addr, id_sequencia, buffer_arquivo.str());
                if (ultimo_id > 0) {
                    id_sequencia = ultimo_id + 1;
                    // 4. Espera a confirmação final do servidor
                    std::string resposta_final;
                    if(receber_dados(socket_cliente, resposta_final, remetente)) {
                        std::cout << "Servidor: " << resposta_final << std::endl;
                    }
                } else {
                    std::cerr << "Falha ao enviar o conteúdo do arquivo." << std::endl;
                }
            } else {
                std::cerr << "Servidor não confirmou o recebimento do comando 'put'. Resposta: " << resposta_ok << std::endl;
            }
        } 
        else { // Lógica para todos os outros comandos (ls, get, cd, login, sair, etc)
            uint32_t ultimo_id = enviar_dados(socket_cliente, server_addr, id_sequencia, linha_comando);

            if (ultimo_id > 0) {
                id_sequencia = ultimo_id + 1;

                if (str_comando == "sair") break;

                // Espera a resposta (pode ser uma lista de arquivos, o conteúdo de um arquivo, etc.)
                std::string resposta_servidor;
                struct sockaddr_in remetente;
                std::cout << "Aguardando resposta do servidor..." << std::endl;

                if (receber_dados(socket_cliente, resposta_servidor, remetente)) {
                    // Lógica especial para 'get': salvar a resposta em um arquivo
                    if (str_comando == "get" && resposta_servidor.rfind("ERRO:", 0) != 0) {
                        std::ofstream arquivo_saida(argumento, std::ios::binary);
                        if(arquivo_saida.is_open()) {
                            arquivo_saida.write(resposta_servidor.c_str(), resposta_servidor.length());
                            arquivo_saida.close();
                            std::cout << "Arquivo '" << argumento << "' baixado com sucesso (" << resposta_servidor.length() << " bytes)." << std::endl;
                        } else {
                            std::cerr << "ERRO: Nao foi possivel criar o arquivo '" << argumento << "' localmente." << std::endl;
                        }
                    } else {
                        // Para todos os outros comandos (ls, cd, erros do get), apenas imprime a resposta
                        std::cout << "Servidor:\n" << resposta_servidor << std::endl;
                    }
                } else {
                    std::cout << "Falha ao receber a resposta." << std::endl;
                }
            } else {
                std::cout << "Falha ao enviar o comando para o servidor." << std::endl;
            }
        }
    }

    close(socket_cliente);
    return 0;
}