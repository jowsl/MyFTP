CXX = g++
CXXFLAGS = -Wall -Wextra -O2

# Fontes
CLIENTE_SRC = cliente_udp.cpp udp_seguro.cpp
SERVIDOR_SRC = servidor_udp.cpp udp_seguro.cpp

# Objetos
CLIENTE_OBJ = $(CLIENTE_SRC:.cpp=.o)
SERVIDOR_OBJ = $(SERVIDOR_SRC:.cpp=.o)

# Binaries
CLIENTE_BIN = cliente
SERVIDOR_BIN = servidor

all: $(CLIENTE_BIN) $(SERVIDOR_BIN)

$(CLIENTE_BIN): $(CLIENTE_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(CLIENTE_OBJ)

$(SERVIDOR_BIN): $(SERVIDOR_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(SERVIDOR_OBJ)

%.o: %.cpp protocolo.h udp_seguro.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(CLIENTE_OBJ) $(SERVIDOR_OBJ) $(CLIENTE_BIN) $(SERVIDOR_BIN)
