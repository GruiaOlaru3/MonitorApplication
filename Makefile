# MonitorAndLogS - build file
# Build all binaries into ./bin. Run the server from the project root
# so it can find ./data/users.txt and ./data/system_logs.csv.

CC      := gcc
CXX     := g++
CFLAGS  := -Wall -Wextra -O2
SFML    := -lsfml-graphics -lsfml-window -lsfml-system
BIN     := bin

.PHONY: all server cli gui clean

all: server cli gui

$(BIN):
	mkdir -p $(BIN)

# TCP server + logger
server: | $(BIN)
	$(CC) $(CFLAGS) src/server.c -o $(BIN)/server -lrt

# CLI fallback client
cli: | $(BIN)
	$(CC) $(CFLAGS) src/client_cli.c -o $(BIN)/client_cli

# SFML GUI client (requires libsfml-dev)
gui: | $(BIN)
	$(CXX) $(CFLAGS) src/client_gui.cpp -o $(BIN)/client_gui $(SFML)

clean:
	rm -rf $(BIN)
