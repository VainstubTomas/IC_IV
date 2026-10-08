CXX ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra
# Las pruebas del nodo viven con su fuente, en legacy/firmware/nodo_mesh_v3/tests.
# Aca queda solo la del gateway de banco (necesita ArduinoJson: ruta .../ArduinoJson/src).
NODO := ../../../../firmware/nodo_mesh_v3/tests
ARDUINOJSON_INCLUDE ?=
.PHONY: all clean ingest
all:
	$(MAKE) -C $(NODO)
ingest:
	$(CXX) $(CXXFLAGS) -I"$(ARDUINOJSON_INCLUDE)" test_ingest.cpp -o test_ingest
	./test_ingest
clean:
	$(MAKE) -C $(NODO) clean
	rm -f test_ingest *.exe
