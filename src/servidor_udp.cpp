#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sstream>      // usar stringstream
#include <filesystem>   // arquivos e diretórios
#include <fstream> // manipulação de arquivos
#include <map> // vou usar para salvar o banco de dados do cliente

#include "udp_seguro.h" 
// Para compilar com o filesystem, talvez precise adicionar -lstdc++fs
// g++ servidor_udp.cpp udp_seguro.cpp -o servidor -lstdc++fs

constexpr int PORT = 8080;
const char* SERVER_IP = "127.0.0.1"; // localhost

// struck para dados dos clientes
struct ClienteData {
    bool logado = false;
    std::string user;
    std::filesystem::path diretorio_raiz;      // Diretório raiz do usuário (ex: ./server_files/user1)
    std::filesystem::path diretorio_atual;   // Diretório atual do usuário (pode ser subpasta)
};

//carrega os dados do arquivo para um map/dicionario
std::map<std::string, std::string> InicializaBD(const std::string& nome_arquivo) {
    std::map<std::string, std::string> banco_de_dados;
    std::ifstream arquivo(nome_arquivo); // Abre o arquivo para leitura

    std::string linha;
    // Lê o arquivo linha por linha
    while (std::getline(arquivo, linha)) {
        // Ignora linhas vazias ou que são comentários
        if (linha.empty() || linha[0] == '#') {
            continue;
        }

        std::stringstream ss(linha);
        std::string user, senha;

        // Extrai o usuário e a senha da linha
        if (ss >> user >> senha) {
            banco_de_dados[user] = senha;
            std::cout << "Usuario '" << user << "' carregado." << std::endl;
        }
    }

    arquivo.close();
    return banco_de_dados;
}

// FUNÇÕES DOS COMANDOS

void comando_login(const std::string& argumento, ClienteData& cliente, const std::map<std::string, std::string>& user_db, std::string& resposta) {
    if (cliente.logado) {
        resposta = "ERRO: Voce ja esta logado como '" + cliente.user + "'.";
        return;
    }

    std::stringstream ss(argumento);
    std::string user, pass;
    ss >> user >> pass;

    if (user.empty() || pass.empty()) {
        resposta = "ERRO: Uso: login <usuario> <senha>";
        return;
    }

    auto it = user_db.find(user);
    if (it != user_db.end() && it->second == pass) {
        cliente.logado = true;
        cliente.user = user;
        
        // Define e cria o diretório raiz do usuário se não existir
        cliente.diretorio_raiz = std::filesystem::current_path() / "server_files" / user;
        cliente.diretorio_atual = cliente.diretorio_raiz;
        
        try {
            if (!std::filesystem::exists(cliente.diretorio_raiz)) {
                std::filesystem::create_directories(cliente.diretorio_raiz);
            }
        } catch (const std::exception& e) {
            resposta = "ERRO: Nao foi possivel criar o diretorio do usuario no servidor.";
            cliente.logado = false; // Desfaz o login se falhar
            return;
        }

        resposta = "Login realizado com sucesso! Bem-vindo, " + user + ".";
    } else {
        resposta = "ERRO: Usuario ou senha invalidos.";
    }
}

std::string comando_ls(const std::filesystem::path& diretorio_atual) {
    std::stringstream ss_resposta;
    try {
        ss_resposta << "Conteudo de " << diretorio_atual.string() << ":\n";
        for (const auto& entry : std::filesystem::directory_iterator(diretorio_atual)) {
            ss_resposta << (entry.is_directory() ? "dir| " : "file| ")
                        << entry.path().filename().string() << "\n";
        }
    } catch (const std::filesystem::filesystem_error& e) {
        return "ERRO: ao listar diretorio: " + std::string(e.what());
    }
    return ss_resposta.str();
}

void comando_cd(ClienteData& cliente, const std::string& argumento, std::string& resposta){
    if (argumento.empty()) { 
        
     }
    try {
        // Normaliza o caminho para evitar ambiguidades
        std::filesystem::path novo_caminho = std::filesystem::canonical(cliente.diretorio_atual / argumento);
        
        // CHECAGEM DE SEGURANÇA: Impede que o usuário saia do seu diretório raiz (ex: cd ../../)
        std::string root_str = cliente.diretorio_raiz.string();
        std::string new_path_str = novo_caminho.string();

        if (new_path_str.rfind(root_str, 0) == 0) { // Verifica se o novo caminho começa com o caminho raiz
            cliente.diretorio_atual = novo_caminho;
            resposta = "Diretorio alterado para: " + cliente.diretorio_atual.string();
        } else {
            resposta = "ERRO: Acesso negado. Nao e permitido sair do seu diretorio raiz.";
        }
    } catch (const std::exception& e) { 

     }
}

std::string comando_get(const std::filesystem::path& diretorio_atual, const std::string& nome_arquivo) {
    if (nome_arquivo.empty()) {
        return "ERRO: faltou nome de arquivo.";
    }

    std::filesystem::path caminho_completo = diretorio_atual / nome_arquivo;

    if (!std::filesystem::exists(caminho_completo)) {
        return "ERRO: Arquivo '" + nome_arquivo + "' nao encontrado no servidor.";
    }

    //Abre o arquivo mas em binario
    std::ifstream arquivo(caminho_completo, std::ios::binary);
    if (!arquivo.is_open()) {
        return "ERRO: Nao foi possivel abrir o arquivo no servidor.";
    }

    //lê o arquivo inteiro para uma string
    std::stringstream buffer;
    buffer << arquivo.rdbuf();
    
    std::cout << "Enviando arquivo '" << nome_arquivo << "' com " << buffer.str().length() << " bytes." << std::endl;
    
    return buffer.str();
}

void comando_mkdir(const std::string& argumento, const ClienteData& cliente, std::string& resposta) {
    if (argumento.empty()) {
        resposta = "ERRO: 'mkdir' requer o nome da pasta a ser criada.";
        return;
    }

    try {
        std::filesystem::path novo_diretorio = cliente.diretorio_atual / argumento;
        if (std::filesystem::create_directory(novo_diretorio)) {
            resposta = "Diretorio '" + argumento + "' criado com sucesso.";
        } else {
            resposta = "ERRO: Diretorio '" + argumento + "' ja existe ou nao pode ser criado.";
        }
    } catch(const std::exception& e) {
        resposta = "ERRO: Nome de diretorio invalido.";
    }
}

int main() {
    // config do socket UDP
    int socket_server = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_server < 0) {
        perror("ERRO: ao tentar criar o socket!");
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
        perror("ERRO: no binding!");
        close(socket_server);
        return 1;
    }

    auto user_db = InicializaBD("user.txt");
    std::filesystem::create_directory("server_files");
    std::cout << "Servidor pronto para receber comandos." << std::endl;

    // onde estamos no sistema de arquivos
    std::filesystem::path diretorio_atual = std::filesystem::current_path();
    ClienteData cliente;
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
                comando_login(argumento, cliente, user_db, resposta_str);
            }
            else if (!cliente.logado && str_comando != "sair") {
                resposta_str = "ERRO: Voce precisa fazer login primeiro.";
            }
            else if (str_comando == "ls") {
                resposta_str = comando_ls(cliente.diretorio_atual);
            }
            else if (str_comando == "cd") {
                comando_cd(cliente, argumento, resposta_str);
            }
            else if (str_comando == "cd..") {
                cliente.diretorio_atual = cliente.diretorio_atual.parent_path();
                resposta_str = "Diretorio alterado para: " + cliente.diretorio_atual.string();
            }
            else if (str_comando == "put") {
                
                if (argumento.empty()) {
                    resposta_str = "ERRO: Comando 'put' requer um nome de arquivo.";
                } else {
                    // 1. Avisa ao cliente que está pronto para receber
                    std::cout << "Recebendo arquivo '" << argumento << "'. Avisando cliente para iniciar envio..." << std::endl;
                    uint32_t ultimo_id_ok = enviar_dados(socket_server, endereco_cliente, id_sequencia_resposta, "OK");
                    id_sequencia_resposta = ultimo_id_ok > 0 ? ultimo_id_ok + 1 : id_sequencia_resposta + 2;

                    // 2. Agora, aguarda o conteúdo do arquivo
                    std::string conteudo_arquivo;
                    if (receber_dados(socket_server, conteudo_arquivo, endereco_cliente)) {
                        // 3. Salva o conteúdo recebido
                        std::ofstream arquivo_novo(cliente.diretorio_atual / argumento, std::ios::binary);
                        if (arquivo_novo.is_open()) {
                            arquivo_novo.write(conteudo_arquivo.c_str(), conteudo_arquivo.length());
                            arquivo_novo.close();
                            resposta_str = "Arquivo '" + argumento + "' recebido com sucesso no servidor.";
                        } else {
                            resposta_str = "ERRO: Nao foi possivel criar o arquivo no servidor.";
                        }
                    } else {
                        resposta_str = "ERRO: Falha ao receber o conteudo do arquivo do cliente.";
                    }
                }
            }
            else if (str_comando == "get") {
                resposta_str = comando_get(cliente.diretorio_atual, argumento);
            }
            else if (str_comando == "mkdir") {
                comando_mkdir(argumento, cliente, resposta_str);
            }
            else if (str_comando == "rmdir") {
                resposta_str = "Comando 'rmdir'  " + argumento + ". faltando implementar";
            }
            else if (str_comando == "sair") {
                resposta_str = "Cliente desconectando. Ate mais!";
                //tem que implementar o logout do cliente e verificar quantos clientes estão conectados
                cliente.logado = false;
            }
            else {
                resposta_str = "ERRO: Comando '" + str_comando + "' desconhecido.";
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