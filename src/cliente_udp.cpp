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
    bool primeira_conexao = true;
    struct sockaddr_in endereco_servidor_dedicado = server_addr;

    while (true) {
        std::cout << "> ";
        std::string linha_comando;
        std::getline(std::cin, linha_comando);

        if (linha_comando.empty()) continue;

        std::stringstream ss(linha_comando);
        std::string str_comando, argumento;
        ss >> str_comando;
        std::getline(ss >> std::ws, argumento);

        if (str_comando == "put") {
            // --- INÍCIO DA NOVA LÓGICA PARA O 'PUT' ---
            if (argumento.empty()) {
                std::cout << "ERRO: Uso: put <arquivo_local> [nome_remoto_opcional]" << std::endl;
                continue;
            }

            std::stringstream args_ss(argumento);
            std::string arquivo_local, nome_remoto;

            args_ss >> arquivo_local;
            args_ss >> nome_remoto;

            if (nome_remoto.empty()) {
                nome_remoto = std::filesystem::path(arquivo_local).filename().string();
            }

            if (!std::filesystem::exists(arquivo_local)) {
                std::cerr << "ERRO: Arquivo local '" << arquivo_local << "' não encontrado." << std::endl;
                continue;
            }

            // 1. Monta e envia o comando 'put' limpo para o servidor
            std::string comando_para_servidor = "put " + nome_remoto;
            std::cout << "Enviando comando para criar '" << nome_remoto << "' no servidor..." << std::endl;
            // Garante que o comando seja enviado para o endereço correto (principal ou dedicado)
            struct sockaddr_in endereco_destino = primeira_conexao ? server_addr : endereco_servidor_dedicado;
            uint32_t ultimo_id = enviar_dados(socket_cliente, endereco_destino, id_sequencia, comando_para_servidor);

            if (!ultimo_id) {
                std::cerr << "Falha ao enviar comando 'put' para o servidor." << std::endl;
                continue;
            }
            id_sequencia = ultimo_id + 1;

            // 2. Espera o "OK" do servidor
            std::string resposta_ok;
            struct sockaddr_in remetente;
            if (receber_dados(socket_cliente, resposta_ok, remetente) && resposta_ok == "OK") {
                 if (primeira_conexao) {
                    endereco_servidor_dedicado = remetente;
                    primeira_conexao = false;
                    std::cout << "Conectado ao socket dedicado na porta: " << ntohs(remetente.sin_port) << std::endl;
                }

                // 3. Servidor está pronto, envia o conteúdo do arquivo
                std::cout << "Servidor pronto. Enviando conteúdo de '" << arquivo_local << "'..." << std::endl;
                std::ifstream stream_arquivo_local(arquivo_local, std::ios::binary);
                std::stringstream buffer_arquivo;
                buffer_arquivo << stream_arquivo_local.rdbuf();
                
                ultimo_id = enviar_dados(socket_cliente, endereco_servidor_dedicado, id_sequencia, buffer_arquivo.str());
                if (ultimo_id > 0) {
                    id_sequencia = ultimo_id + 1;
                    
                    std::string resposta_final;
                    if(receber_dados(socket_cliente, resposta_final, remetente)) {
                        std::cout << "Servidor: " << resposta_final << std::endl;
                    } else {
                        std::cerr << "ERRO: Não recebeu confirmação final do servidor." << std::endl;
                    }
                } else {
                    std::cerr << "ERRO: Falha ao enviar o conteúdo do arquivo." << std::endl;
                }
            } else {
                std::cerr << "ERRO: Servidor não confirmou o comando 'put'. Resposta: " << resposta_ok << std::endl;
            }
            // --- FIM DA NOVA LÓGICA PARA O 'PUT' ---
        } else { // Lógica para todos os outros comandos
            struct sockaddr_in endereco_destino = primeira_conexao ? server_addr : endereco_servidor_dedicado;

            uint32_t ultimo_id = enviar_dados(socket_cliente, endereco_destino, id_sequencia, linha_comando);

            if (ultimo_id > 0) {
                id_sequencia = ultimo_id + 1;

                if (str_comando == "sair") break;

                std::string resposta_servidor;
                struct sockaddr_in remetente;
                std::cout << "Aguardando resposta do servidor..." << std::endl;

                if (receber_dados(socket_cliente, resposta_servidor, remetente)) {
                    if (primeira_conexao) {
                        endereco_servidor_dedicado = remetente;
                        primeira_conexao = false;
                        std::cout << "Conectado ao socket dedicado na porta: " << ntohs(remetente.sin_port) << std::endl;
                    }

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