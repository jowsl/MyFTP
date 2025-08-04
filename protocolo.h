#ifndef PROTOCOLO_H
#define PROTOCOLO_H
#include <cstdint> 
/* 
    Essa biblioteca (cstdint) permite que possamos usar tipos de dados inteiros com tamanhos fixos,
    como int8_t, int16_t, int32_t, e int64_t.
    Esses tipos são úteis para garantir que o tamanho dos dados seja consistente em diferentes plataformas,
    o que é importante para protocolos de comunicação, como o FTP. Grante que funcione em diferentes OS.
*/

constexpr int TAM_MAX_PACOTE = 1024;
constexpr int DADOS_STRUCT = sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint8_t); // ID, Tamanho, flag
constexpr int TAM_DADOS = TAM_MAX_PACOTE - DADOS_STRUCT; // Tamanho máximo dos dados que podem ser enviados

/*
    As flags serão usadas para identificar se:
    0b00000001 se o pacote é de dados
    0b00000010 se o pacote é de controle (confirmação)
    0b00000100 se o pacote é um pacote final
*/

const uint8_t FLAG_DADOS = 0b00000001;
const uint8_t FLAG_CONTROLE = 0b00000010;
const uint8_t FLAG_FINAL = 0b00000100;

struct Pacote {
    uint32_t id; 
    uint16_t tamanho;
    uint8_t flag;
    // ID unico do pacote, tamanho dos dados e flag de controle.
    char dados[TAM_DADOS]; //Dados em si, em char por ser 1byte por caractere.
};


#endif // PROTOCOLO_H