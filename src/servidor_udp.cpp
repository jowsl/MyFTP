#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sstream>      // usar stringstream
#include <filesystem>   // arquivos e diretórios

#include "udp_seguro.h" 
// Para compilar com o filesystem, talvez precise adicionar -lstdc++fs
// g++ servidor_udp.cpp udp_seguro.cpp -o servidor -lstdc++fs

constexpr int PORT = 8080;
const char* SERVER_IP = "127.0.0.1"; // localhost

// FUNÇÕES DOS COMANDOS

std::string comando_ls(const std::filesystem::path& diretorio_atual) {
    std::stringstream ss_resposta;
    try {
        ss_resposta << "Conteudo de " << diretorio_atual.string() << ":\n";
        for (const auto& entry : std::filesystem::directory_iterator(diretorio_atual)) {
            ss_resposta << (entry.is_directory() ? "dir| " : "file| ")
                        << entry.path().filename().string() << "\n";
        }
    } catch (const std::filesystem::filesystem_error& e) {
        return "Erro ao listar diretorio: " + std::string(e.what());
    }
    return ss_resposta.str();
}

void comando_cd(std::filesystem::path& diretorio_atual, const std::string& argumento, std::string& resposta) {
    if (argumento.empty()) {
        resposta = "Erro: 'cd' requer um argumento (nome da pasta).";
        return;
    }
    try {
        std::filesystem::path novo_caminho = diretorio_atual / argumento;
        if (std::filesystem::exists(novo_caminho) && std::filesystem::is_directory(novo_caminho)) {
            diretorio_atual = std::filesystem::canonical(novo_caminho);
            resposta = "Diretorio alterado para: " + diretorio_atual.string();
        } else {
            resposta = "Erro: Diretorio '" + argumento + "' nao existe.";
        }
    } catch (const std::filesystem::filesystem_error& e) {
        resposta = "Erro ao acessar diretorio: " + std::string(e.what());
    }
}


int main() {
    // config do socket UDP
    int socket_server = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_server < 0) {
        perror("Erro ao tentar criar o socket!");
        return 1;
    }
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) < 0) {
        std::cout << "IP inválido." << std::endl;
        close(socket_server); // fecha caso erro
        return 1;
    }
    // se for usar em outros computadores na mesma rede usar
    //server_addr.sin_addr.s_addr = INADDR_ANY; // ao invés do if acima, recebe qualquer IP na rede local
    
    server_addr.sin_port = htons(PORT);
    if (bind(socket_server, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erro no binding!");
        close(socket_server);
        return 1;
    }
    std::cout << "Servidor pronto para receber comandos." << std::endl;

    // onde estamos no sistema de arquivos
    std::filesystem::path diretorio_atual = std::filesystem::current_path();
    bool cliente_logado = false;
    uint32_t id_sequencia_resposta = 1;

    while (true) {
        std::string mensagem_recebida;
        struct sockaddr_in endereco_cliente;

        std::cout << "\nServidor aguardando transferencia..." << std::endl;

        if (receber_dados(socket_server, mensagem_recebida, endereco_cliente)) {
            std::cout << "Comando completo recebido: \"" << mensagem_recebida << "\"" << std::endl;

            
            std::stringstream ss(mensagem_recebida); // Usar stringstream para facilitar a separação do comando e argumentos
            std::string str_comando; // comando principal
            std::string argumento; // argumento do comando, se houver
            ss >> str_comando; // Pega a primeira palavra como comando
            std::getline(ss >> std::ws, argumento); // Pega todo o resto da string como argumento

            std::string resposta_str; // resposta que será enviada ao cliente

            // resolvendo o comando
            if (str_comando == "login") {
                cliente_logado = true;
                resposta_str = "Login realizado com sucesso!";
            }
            else if (!cliente_logado && str_comando != "sair") {
                resposta_str = "Erro: Voce precisa fazer login primeiro.";
            }
            else if (str_comando == "ls") {
                resposta_str = comando_ls(diretorio_atual);
            }
            else if (str_comando == "cd") {
                comando_cd(diretorio_atual, argumento, resposta_str);
            }
            else if (str_comando == "cd..") {
                diretorio_atual = diretorio_atual.parent_path();
                resposta_str = "Diretorio alterado para: " + diretorio_atual.string();
            }
            else if (str_comando == "put") {
                resposta_str = "Comando 'put'  " + argumento + ". faltando implementar";
            }
            else if (str_comando == "get") {
                resposta_str = "Comando 'get'  " + argumento + ". faltando implementar";
            }
            else if (str_comando == "mkdir") {
                resposta_str = "Comando 'mkdir'  " + argumento + ". faltando implementar";
            }
            else if (str_comando == "rmdir") {
                resposta_str = "Comando 'rmdir'  " + argumento + ". faltando implementar";
            }
            else if (str_comando == "sair") {
                resposta_str = "Cliente desconectando. Ate mais!";
                //tem que implementar o logout do cliente e verificar quantos clientes estão conectados
                cliente_logado = false;
            }
            else {
                resposta_str = "Erro: Comando '" + str_comando + "' desconhecido.";
            }


            // resposta
            std::cout << "Enviando resposta: \"" << resposta_str << "\"" << std::endl;
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