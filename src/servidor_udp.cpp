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
#include <pthread.h> // << NOVO: Biblioteca Pthreads
#include <mutex>     // << NOVO: Para proteger o acesso ao mapa de clientes

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
    uint32_t id_sequencia_resposta = 1;
};

// Uma thread em C só pode receber um argumento (um void*).
// Então, agrupamos tudo que a thread precisa saber em uma única struct.
struct ThreadData {
    int socket_escuta; // Socket de escuta compartilhado
    struct sockaddr_in endereco_cliente;
    std::string mensagem_inicial;
    std::map<std::string, std::string>* user_db;
    ClienteData* cliente_data; // Ponteiro para os dados do cliente
};
//dicionario para rastrear clientes ativos e um mutex para protegê-lo
std::map<std::string, bool> clientes_ativos;
std::mutex clientes_mutex;


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


void comando_rmdir(const std::string& argumento, const ClienteData& cliente, std::string& resposta) {
    if (argumento.empty()) {
        resposta = "ERRO: 'rmdir' requer o nome da pasta a ser removida.";
        return;
    }
    
    // Proíbe o uso de '..' para evitar ambiguidades e riscos de segurança.
    if (argumento == "." || argumento == "..") {
        resposta = "ERRO: Nome de diretorio invalido.";
        return;
    }

    try {
        std::filesystem::path diretorio_para_remover = cliente.diretorio_atual / argumento;
        
        // Normaliza o caminho para ter certeza do caminho real no sistema
        diretorio_para_remover = std::filesystem::canonical(diretorio_para_remover);

        // 1. CHECAGEM DE SEGURANÇA: Não deixar apagar a própria pasta raiz.
        if (diretorio_para_remover == cliente.diretorio_raiz) {
            resposta = "ERRO: Você não pode remover diretorio raiz.";
            return;
        }

        // 2. Verifica se o caminho existe e é um diretório
        if (!std::filesystem::exists(diretorio_para_remover) || !std::filesystem::is_directory(diretorio_para_remover)) {
            resposta = "ERRO: Diretorio '" + argumento + "' nao existe.";
            return;
        }

        // 3. Verifica se o diretório está vazio
        if (!std::filesystem::is_empty(diretorio_para_remover)) {
            resposta = "ERRO: Nao foi possivel remover '" + argumento + "'. O diretorio nao esta vazio.";
            return;
        }
        
        // 4. Se todas as checagens passaram, remove o diretório
        if (std::filesystem::remove(diretorio_para_remover)) {
            resposta = "Diretorio '" + argumento + "' removido com sucesso.";
        } else {
            resposta = "ERRO: Falha ao remover o diretorio '" + argumento + "'.";
        }

    } catch(const std::exception& e) {
        resposta = "ERRO: Caminho ou nome de diretorio invalido.";
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

void comando_cd(ClienteData& cliente, const std::filesystem::path& novo_caminho_relativo, std::string& resposta) {
    try {
        std::filesystem::path novo_caminho_abs = std::filesystem::canonical(cliente.diretorio_atual / novo_caminho_relativo);
        
        std::string root_str = cliente.diretorio_raiz.string();
        std::string new_path_str = novo_caminho_abs.string();

        // Checagem de segurança
        if (new_path_str.rfind(root_str, 0) == 0) {
            cliente.diretorio_atual = novo_caminho_abs;
            resposta = "Diretorio alterado para: " + cliente.diretorio_atual.string();
        } else {
            resposta = "ERRO: Acesso negado. Nao e permitido sair do seu diretorio raiz.";
        }
    } catch (const std::exception& e) {
        resposta = "ERRO: Caminho invalido ou inexistente.";
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

void* manipulador_de_clientes(void* args) {
    ThreadData* thread_args = (ThreadData*)args;
    
    // *** CRIAR SOCKET DEDICADO PARA ESTA THREAD ***
    int socket_dedicado = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_dedicado < 0) {
        perror("Erro ao criar socket dedicado para thread");
        delete thread_args->cliente_data;
        delete thread_args;
        pthread_exit(NULL);
    }
    
    // Bind em uma porta disponível (deixa o sistema escolher)
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = 0; // Sistema escolhe porta disponível
    
    if (bind(socket_dedicado, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erro no bind do socket dedicado");
        close(socket_dedicado);
        delete thread_args->cliente_data;
        delete thread_args;
        pthread_exit(NULL);
    }
    
    // *** REMOVIDO O connect() - NÃO NECESSÁRIO PARA UDP SERVIDOR ***
    
    // Resto do código da thread permanece igual, mas usando socket_dedicado
    ClienteData* cliente = thread_args->cliente_data;
    struct sockaddr_in endereco_cliente = thread_args->endereco_cliente;
    std::string mensagem_recebida = thread_args->mensagem_inicial;
    auto user_db = *thread_args->user_db;

    std::string cliente_id = std::string(inet_ntoa(endereco_cliente.sin_addr)) + ":" + 
                            std::to_string(ntohs(endereco_cliente.sin_port));
    std::cout << "Thread criada para o cliente: " << cliente_id << std::endl;

    while (true) {
        std::cout << "[" << cliente_id << "] Comando recebido: \"" << mensagem_recebida << "\"" << std::endl;

        std::stringstream ss(mensagem_recebida);
        std::string str_comando, argumento;
        ss >> str_comando;
        std::getline(ss >> std::ws, argumento);
        std::string resposta_str;

        // Processamento dos comandos (permanece igual)
        if (str_comando == "login") {
            comando_login(argumento, *cliente, user_db, resposta_str);
        }
        else if (str_comando == "sair") {
            resposta_str = "Cliente desconectando. Ate mais!";
            cliente->logado = false;
        }
        else if (!cliente->logado) {
            resposta_str = "ERRO: Voce precisa fazer login primeiro.";
        }
        else if (str_comando == "ls") {
            resposta_str = comando_ls(cliente->diretorio_atual);
        }
        else if (str_comando == "cd") {
            comando_cd(*cliente, argumento, resposta_str);
        }
        else if (str_comando == "cd..") {
            comando_cd(*cliente, "..", resposta_str);
        }
        else if (str_comando == "put") {
            if (argumento.empty()) {
                resposta_str = "ERRO: Comando \'put\' requer um nome de arquivo.";
            } else {
                uint32_t ultimo_id_ok = enviar_dados(socket_dedicado, endereco_cliente, 
                                                cliente->id_sequencia_resposta, "OK");
                cliente->id_sequencia_resposta = ultimo_id_ok > 0 ? ultimo_id_ok + 1 : cliente->id_sequencia_resposta + 2;

                std::string conteudo_arquivo;
                struct sockaddr_in remetente_temp;
                if (receber_dados(socket_dedicado, conteudo_arquivo, remetente_temp)) {
                    std::ofstream arquivo_novo(cliente->diretorio_atual / argumento, std::ios::binary);
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
            resposta_str = comando_get(cliente->diretorio_atual, argumento);
        }
        else if (str_comando == "mkdir") {
            comando_mkdir(argumento, *cliente, resposta_str);
        }
        else if (str_comando == "rmdir") {
            comando_rmdir(argumento, *cliente, resposta_str);
        }
        else {
            resposta_str = "ERRO: Comando \'" + str_comando + "\' desconhecido.";
        }

        // Envia resposta usando socket dedicado
        uint32_t ultimo_id = enviar_dados(socket_dedicado, endereco_cliente, 
                                        cliente->id_sequencia_resposta, resposta_str);
        if (ultimo_id > 0) {
            cliente->id_sequencia_resposta = ultimo_id + 1;
        } else {
            std::cerr << "[" << cliente_id << "] Falha ao enviar resposta completa." << std::endl;
        }

        if (str_comando == "sair") break;
        
        // *** IMPORTANTE: Usar variável temporária para o endereço ***
        struct sockaddr_in remetente_temp;
        if (!receber_dados(socket_dedicado, mensagem_recebida, remetente_temp)) {
            std::cerr << "[" << cliente_id << "] Erro ao receber proxima transferencia. Encerrando thread." << std::endl;
            break;
        }
        
        // *** VALIDAÇÃO DE SEGURANÇA: Verifica se é do mesmo cliente ***
        if (remetente_temp.sin_addr.s_addr != endereco_cliente.sin_addr.s_addr || 
            remetente_temp.sin_port != endereco_cliente.sin_port) {
            std::cerr << "[" << cliente_id << "] AVISO: Pacote de cliente diferente recebido. Ignorando." << std::endl;
            continue; // Ignora e continua esperando do cliente correto
        }
    }

    // Limpeza da thread
    close(socket_dedicado);
    std::cout << "Encerrando thread para o cliente: " << cliente_id << std::endl;
    {
        std::lock_guard<std::mutex> lock(clientes_mutex);
        clientes_ativos.erase(cliente_id);
    }
    delete cliente;
    delete thread_args;
    pthread_exit(NULL);
    return nullptr;
}



int main() {
    // ---- Setup do socket principal (de escuta) ----
    int socket_escuta = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_escuta < 0) { 
        perror("ERRO: ao tentar criar o socket de escuta!"); 
        return 1; 
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(socket_escuta, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("ERRO: no binding!"); close(socket_escuta); return 1;
    }
    
    auto user_db = InicializaBD("user.txt");
    std::filesystem::create_directory("server_files");
    std::cout << "Servidor pronto. Aguardando clientes na porta " << PORT << "..." << std::endl;

    // Loop do despachante de threads
 while (true) {
        struct sockaddr_in endereco_cliente;
        Pacote primeiro_pacote;

        // Ouve por um PRIMEIRO contato no socket principal
        if (recebimento_seguro(socket_escuta, primeiro_pacote, endereco_cliente)) {
            std::string cliente_id = std::string(inet_ntoa(endereco_cliente.sin_addr)) + ":" + std::to_string(ntohs(endereco_cliente.sin_port));
            
            std::lock_guard<std::mutex> lock(clientes_mutex);
            if (clientes_ativos.find(cliente_id) == clientes_ativos.end()) {
                clientes_ativos[cliente_id] = true;
                
                ThreadData* args = new ThreadData();
                args->socket_escuta = socket_escuta;
                args->endereco_cliente = endereco_cliente;
                args->mensagem_inicial = std::string(primeiro_pacote.dados, primeiro_pacote.tamanho);
                args->user_db = &user_db;
                args->cliente_data = new ClienteData(); // Aloca nova ClienteData para a thread

                pthread_t thread_id;
                if (pthread_create(&thread_id, NULL, manipulador_de_clientes, (void*)args) != 0) {
                    perror("Erro ao criar a thread");
                    delete args->cliente_data; // Libera se a criação da thread falhar
                    delete args;
                    clientes_ativos.erase(cliente_id);
                }
                pthread_detach(thread_id);
            }
        }
    }

    close(socket_escuta);
    return 0;
}

