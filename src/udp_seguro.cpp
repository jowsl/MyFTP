#include "protocolo.h" // onde esta a struct Pacote
#include "udp_seguro.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <unistd.h>
#include <iostream>


constexpr int TIMEOUT = 1; //timeout que vamos usar no select()

//envio seguro, socket, dados, endereço. 
bool envio_seguro(int socket, const struct sockaddr_in& endereco, const Pacote& dados) {
    // tenta enviar os dados dentro de um loop para garantir que todos os dados sejam enviados.

    int tentativas = 10;

    while(tentativas > 0){
        //sendto envia dados para o endereço especificado.
        ssize_t bytes_enviados = sendto(socket, &dados, sizeof(dados), 0,
                                        (const struct sockaddr *)&endereco, sizeof(endereco));
        
        // verifica se teve erro no sendto
        if (bytes_enviados < 0) { 
            perror("Erro ao enviar dados");
            return false; 
        }

        std::cout << "Dados: " << dados.id << " tentativa: " << tentativas << std::endl;

        //iniciando o socket para usar no select
        fd_set read_fds; // conjunto de sockets para ler
        FD_ZERO(&read_fds); // limpa o conjunto de sockets
        FD_SET(socket, &read_fds); // adiciona o socket ao conjunto

        // por quando tempo o select vai esperar a resposta
        struct timeval timeout;
        timeout.tv_sec = TIMEOUT; // segundos
        timeout.tv_usec = 0; // microssegundos

        //parametros do select: número máximo de sockets + 1, conjunto de leitura, conjunto de escrita (NULL),
        // conjunto de exceção (NULL), e o timeout.
        // oque o select faz é esperar até que o socket esteja pronto para leitura ou até que o timeout ocorra.
        int resultado = select(socket + 1, &read_fds, NULL, NULL, &timeout);
        
        if (resultado < 0) {
            perror("Erro no select [envio_seguro]");
            return false; // erro no select
        }

        if (resultado == 0) {
            tentativas--;
            std::cout << "Timeout no [envio_seguro], proxima tentativa: " << (tentativas-1) << std::endl;
            continue; // timeout, tenta novamente
        }   

        // select retornou com sucesso (resultado > 0), verificações dos dados recebidos
        if (FD_ISSET(socket, &read_fds)) { //socket de onde ler

                Pacote confirmacao_recebido;
                struct sockaddr_in remetente;
                socklen_t tam_remetente = sizeof(remetente);

                ssize_t bytes_recebidos = recvfrom(socket, &confirmacao_recebido, sizeof(confirmacao_recebido), 0,
                                                (struct sockaddr *)&remetente, &tam_remetente);

                if (bytes_recebidos > 0) {
                    // Verifica se a confirmação é do pacote que enviamos
                    if ((confirmacao_recebido.flag & FLAG_CONTROLE) && (confirmacao_recebido.id == dados.id)) {
                        std::cout << "[envio_seguro] pacote:  " << dados.id << " recebido com sucesso!" << std::endl;
                        return true; // certo
                    } else if ((confirmacao_recebido.flag & FLAG_CONTROLE) && (confirmacao_recebido.id < dados.id)) {
                        // Recebeu um ACK para um pacote anterior, reenvia o pacote atual
                        std::cout << "[envio_seguro] ACK para pacote anterior recebido. Reenviando pacote " << dados.id << std::endl;
                        tentativas--;
                        continue;
                    } else {
                        // Recebeu um pacote errado
                        std::cout << "[envio_seguro] Pacote errado recebido. Ignorando." << std::endl;
                    }
                }
            }
    }
    std::cerr << "[envio_seguro] Falha ao enviar dados após várias tentativas." << std::endl;
    return false; // falhou após várias tentativas
}


bool recebimento_seguro(int socket, Pacote& pacote_recebido, struct sockaddr_in& endereco_remetente) {
    socklen_t tam_remetente = sizeof(endereco_remetente);

    ssize_t bytes_recebidos = recvfrom(socket, &pacote_recebido, sizeof(pacote_recebido), 0,
                                       (struct sockaddr *)&endereco_remetente, &tam_remetente);
    // recvfrom é usado para receber dados de um socket UDP.

    if (bytes_recebidos <= 0) return false; // Erro ou socket fechado

    // precisamos verificar se o pacote recebido é um pacote de dados ou controle
    bool eh_pacote_de_dados = pacote_recebido.flag & FLAG_DADOS;
    bool eh_pacote_final = pacote_recebido.flag & FLAG_FINAL;

    // Verifica se o pacote recebido é um pacote de DADOS
    if (eh_pacote_de_dados || eh_pacote_final) {
        std::cout << "------------------------------------------------------" << std::endl;
        std::cout << "[recebimento_seguro] Pacote ID " << pacote_recebido.id << " recebido. Enviando resposta..." << std::endl;

        // monta o pacote de confirmação
        Pacote pacote_confirmacao;
        memset(&pacote_confirmacao, 0, sizeof(pacote_confirmacao));
        pacote_confirmacao.id = pacote_recebido.id; //para o mesmo ID
        pacote_confirmacao.flag = FLAG_CONTROLE;  // Define a flag de Controle
        pacote_confirmacao.tamanho = 0; // não tem dados, é so a confirmação

        // Envia o pacote de volta para quem enviou os dados
        sendto(socket, &pacote_confirmacao, sizeof(pacote_confirmacao), 0,
               (const struct sockaddr*)&endereco_remetente, tam_remetente);

        return true; // Sucesso, um pacote de dados foi recebido e processado.
    }

    // Retornamos false, nenhum dado chegou.
    return false;
}

// Esta função envia uma string completa, fatiando-a em pacotes se necessário.
// Retorna o ID do último pacote enviado ou 0 se falhar
uint32_t enviar_dados(int socket, const struct sockaddr_in& endereco, uint32_t id_inicial, const std::string& dados_completos) {
    size_t ponteiro = 0;
    uint32_t id_atual = id_inicial;

    // Loop enquanto houver dados restantes
    while (ponteiro < dados_completos.length()) {
        Pacote pacote_atual;
        memset(&pacote_atual, 0, sizeof(pacote_atual));
        pacote_atual.id = id_atual;
        pacote_atual.flag = FLAG_DADOS;

        size_t tamanho_fatia = std::min((size_t)TAM_DADOS, dados_completos.length() - ponteiro);
        pacote_atual.tamanho = tamanho_fatia;
        memcpy(pacote_atual.dados, dados_completos.c_str() + ponteiro, tamanho_fatia);
        // memcpy copia os dados da string para o pacote, respeitando o tamanho máximo de TAM_DADOS.
        // parâmetros: ponteiro de destino, ponteiro de origem, número de bytes a copiar.
        
        // caso o envio falhe, retornamos 0
        if (!envio_seguro(socket, endereco, pacote_atual)) {
            std::cerr << "Falha ao enviar fatia " << id_atual << std::endl;
            return 0;
        }
        ponteiro += tamanho_fatia;// o ponteiro vai incrementando para a próxima fatia
        id_atual++; // igual para o id do pacote
    }

    // Envia um pacote com a flag FINAL
    Pacote pacote_final;
    memset(&pacote_final, 0, sizeof(pacote_final));
    pacote_final.id = id_atual;
    pacote_final.flag = FLAG_FINAL;
    pacote_final.tamanho = 0;

    if (!envio_seguro(socket, endereco, pacote_final)) {
        std::cerr << "Falha ao enviar o pacote FLAG_FINAL." << std::endl;
        return 0;
    }

    std::cout << "[enviar_dados] Transferencia concluida com sucesso." << std::endl;
    return id_atual;
}

// Esta função recebe pacotes de dados e os monta em uma string completa.
 // Retorna true se a transferência foi concluída com sucesso (pacote FINAL recebido).
 // Retorna false se não conseguir receber os dados.
bool receber_dados(int socket, std::string& dados_remontados, struct sockaddr_in& remetente) {
    dados_remontados.clear(); // Limpa a string para receber novos dados

    while (true) {
        Pacote pacote_recebido;

        if (recebimento_seguro(socket, pacote_recebido, remetente)) {
            if (pacote_recebido.flag & FLAG_DADOS) {
                dados_remontados.append(pacote_recebido.dados, pacote_recebido.tamanho);
            } else if (pacote_recebido.flag & FLAG_FINAL) {
                std::cout << "[receber_dados] Pacote final recebido. Transferencia completa." << std::endl;
                return true;
            } else if (pacote_recebido.flag & FLAG_CONTROLE) {
                // Ignora ACKs recebidos aqui, eles são tratados por envio_seguro
                continue;
            }
        }
    }
    return false;
}