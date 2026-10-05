CXX ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Ifakes
# Solo test_ingest necesita ArduinoJson; indicar la ruta .../ArduinoJson/src.
ARDUINOJSON_INCLUDE ?=
.PHONY: all clean ingest
all: test_mesh test_storage
	./test_mesh
	./test_storage
test_mesh: test_mesh.cpp mesh_assertions.h ../../../firmware/mesh_comun/mesh_core.h
	$(CXX) $(CXXFLAGS) test_mesh.cpp -o $@
test_storage: test_storage.cpp fakes/Preferences.h ../../../firmware/mesh_comun/mesh_storage.h
	$(CXX) $(CXXFLAGS) test_storage.cpp -o $@
ingest:
	$(CXX) $(CXXFLAGS) -I"$(ARDUINOJSON_INCLUDE)" test_ingest.cpp -o test_ingest
	./test_ingest
clean:
	rm -f test_mesh test_storage test_ingest *.exe
