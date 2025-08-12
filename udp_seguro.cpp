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
        // agora o select sabe que deve esperar por dados nesse socket

        // por quando tempo o select vai esperar a resposta
        struct timeval timeout;
        timeout.tv_sec = TIMEOUT; // segundos
        timeout.tv_usec = 0; // microssegundos

        //parametros do select: número máximo de sockets + 1, conjunto de leitura, conjunto de escrita (NULL),
        // conjunto de exceção (NULL), e o timeout.
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

    if (bytes_recebidos <= 0) {
        // Erro ou socket fechado
        return false;
    }

    // Verifica se o pacote recebido é um pacote de DADOS
    if (pacote_recebido.flag & FLAG_DADOS) {
        std::cout << "[recebimento_seguro] Pacote de DADOS " << pacote_recebido.id << " recebido. Enviando ACK..." << std::endl;

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