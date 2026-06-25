# MonitorAndLogS

A Linux client–server **system monitoring and logging** tool, written in C/C++ as a
university project for the *Computer Networks* course (Faculty of Computer Science,
"Alexandru Ioan Cuza" University of Iași — FII UAIC).

The server continuously samples the host's state from `/proc` and standard Linux tools
(`ss`, `who`, `ps`), appends each sample to a CSV log, and exposes both **live** and
**historical** queries over TCP. Two clients are provided: a graphical dashboard built
with **SFML** and a lightweight CLI fallback.

---

## Features

- **Concurrent TCP server** (`fork()` per client) on port `2444`.
- **Background logger** that snapshots system metrics on a configurable interval and
  appends them to `data/system_logs.csv`.
- **Shared memory** (`shm_open` / `mmap`) so the logging interval can be changed at
  runtime and is seen by every child process.
- **Simple authentication** gate (`login`) backed by `data/users.txt`.
- **Live metrics:** load average, active process count + top CPU consumers, memory
  usage, listening services, established connections, logged-in users.
- **Historical queries:** filter by record count, by a specific day (`YYYY-MM-DD`), or
  by an hourly interval (`HH:MM-HH:MM`).
- **SFML GUI client** with Dashboard / Procese / Retea / Useri / Consola / Config tabs,
  live charts, and a report export.

> **Platform:** Linux only. The server relies on `/proc` and POSIX APIs
> (sockets, shared memory, `fork`). It will not build or run natively on Windows.

---

## Project structure

```
.
├── src/
│   ├── server.c        # TCP server, system sampler, CSV logger, query handlers
│   ├── client_gui.cpp  # SFML graphical dashboard client
│   └── client_cli.c    # CLI fallback client
├── data/
│   ├── system_logs.csv # sample log data (so history queries work out of the box)
│   └── users.txt       # demo credentials — see security note below
├── docs/
│   └── Raport_tehnic.pdf  # technical report (Romanian)
├── Makefile
├── LICENSE             # GPLv3
└── README.md
```

---

## Building

Requires `gcc`, `g++`, `make`, and (for the GUI) the SFML development libraries.

```bash
# Debian / Ubuntu
sudo apt install build-essential libsfml-dev
```

Then, from the project root:

```bash
make          # builds server, cli, and gui into ./bin
make server   # server only
make cli      # CLI client only
make gui      # SFML GUI client only
make clean
```

---

## Running

The server reads and writes its data files using paths **relative to the current
directory**, so always launch it from the project root:

```bash
./bin/server
```

Then connect with either client (arguments are `<server_ip> <port>`):

```bash
./bin/client_gui 127.0.0.1 2444   # graphical dashboard
./bin/client_cli 127.0.0.1 2444   # CLI fallback
```

Log in first (see the demo credentials in `data/users.txt`, e.g. `admin admin`), then
issue commands. In the CLI client, type `help` for the full list.

---

## Command protocol

All commands are sent as plain text lines. History commands accept an optional argument:
a **count** (default `10`), a **day** `YYYY-MM-DD`, or an **hourly interval** `HH:MM-HH:MM`.

| Command | Description |
|---|---|
| `login <user> <pass>` | Authenticate (required before any query) |
| `logout` | Drop authentication |
| `quit` | Close the connection |
| `get_load_avg` | System load average (1 / 5 / 15 min) |
| `get_proc_info [n]` | Active process count (optionally top `n` by CPU) |
| `get_logged_users` | Currently logged-in users |
| `get_memory` | Current memory usage |
| `get_services` | Ports in `LISTEN` state |
| `get_connections` | Established connections |
| `get_sys_hist [arg]` | History of memory & CPU usage |
| `get_users_hist [arg]` | Per-user connection statistics |
| `get_serv_hist [arg]` | Connections per port/service (`<port> port` to filter) |
| `get_list_hist [arg]` | History of open (listening) ports |
| `get_proc_hist [arg]` | History of load, process count, top CPU consumers |
| `set_interval <sec>` | Change the logging interval (1–3600 s) |
| `get_interval` | Show the current logging interval |

---

## Security note

This is an educational project. `data/users.txt` stores **plaintext** username/password
pairs and is included only as demo data — do not reuse these credentials anywhere, and do
not expose the server on an untrusted network. The authentication is intentionally
minimal and is not meant for production use.

---

## License

Licensed under the **GNU General Public License v3.0** — see [LICENSE](LICENSE).

The GUI client uses [SFML](https://www.sfml-dev.org/), which is distributed under the
zlib/png license (GPL-compatible).
