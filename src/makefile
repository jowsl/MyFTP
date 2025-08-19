# Compilador
CXX = g++
# Flags de Compilação
# NOVO: Adiciona o padrão C++17, necessário para <filesystem>, e -pthread para compilação e linkagem
CXXFLAGS = -Wall -Wextra -O2 -std=c++17 -pthread

# Fontes
CLIENTE_SRC = cliente_udp.cpp udp_seguro.cpp
SERVIDOR_SRC = servidor_udp.cpp udp_seguro.cpp

# Objetos (gerados automaticamente a partir das fontes)
CLIENTE_OBJ = $(CLIENTE_SRC:.cpp=.o)
SERVIDOR_OBJ = $(SERVIDOR_SRC:.cpp=.o)

# NOVO: Flags de linkagem para as bibliotecas
# O cliente usa <filesystem>
CLIENTE_LIBS = -lstdc++fs
# O servidor usa <filesystem> e <pthread>. A flag -pthread em CXXFLAGS já cuida da linkagem.
# Mas para ser explícito, poderíamos adicionar -lpthread aqui também. Por simplicidade, -pthread já resolve.

# Binários (executáveis finais)
CLIENTE_BIN = cliente
SERVIDOR_BIN = servidor

# Regra principal: compila tudo
all: $(CLIENTE_BIN) $(SERVIDOR_BIN)

# Regra de linkagem para o cliente
$(CLIENTE_BIN): $(CLIENTE_OBJ)
	# ALTERADO: Adiciona as bibliotecas na etapa final
	$(CXX) $(CXXFLAGS) -o $@ $(CLIENTE_OBJ) $(CLIENTE_LIBS)

# Regra de linkagem para o servidor
$(SERVIDOR_BIN): $(SERVIDOR_OBJ)
	# ALTERADO: A flag -pthread em CXXFLAGS já lida com a linkagem da biblioteca de threads
	$(CXX) $(CXXFLAGS) -o $@ $(SERVIDOR_OBJ) -lstdc++fs

# Regra de padrão para compilar arquivos .cpp em .o
# Se qualquer um dos headers mudar, os fontes serão recompilados.
%.o: %.cpp protocolo.h udp_seguro.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Regra para limpar os arquivos compilados
clean:
	rm -f $(CLIENTE_OBJ) $(SERVIDOR_OBJ) $(CLIENTE_BIN) $(SERVIDOR_BIN)