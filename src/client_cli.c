// Alternativa de rezerva in caz ca nu este valida varianta GUI cu SFML

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_SIZE 16384
extern int errno;

void print_help() 
{
  printf("\nComenzi disponibile:\n");
  printf("  Informatii live:\n");
  printf("    get_load_avg       - load average sistem\n");
  printf("    get_proc_info [n]  - numar procese active (optional: top n)\n");
  printf("    get_logged_users   - utilizatori conectati\n");
  printf("    get_memory         - utilizare memorie curenta\n");
  printf("    get_services       - porturi in LISTEN\n");
  printf("    get_connections    - conexiuni active\n");
  printf("\n  Statistici (<count> nr. inreg, default 10, <data> filtru dupa ziua sau intervalul orar):\n");
  printf("    get_memory_hist <count>/<data>        - istoric utilizare memorie\n");
  printf("    get_users_hist <count>/<data>         - statistici conectari per user\n");
  printf("    get_serv_hist <count/port>/<data> cnt/port - conexiuni per port/serviciu\n");
  printf("    get_list_hist <count>/<data>          - istoric porturi deschise (LISTEN)\n");
  printf("    get_proc_hist <count>/<data>          - istoric top consumatori CPU\n");
  printf("\n  Configurare:\n");
  printf("    set_interval <sec> - schimba intervalul de logare\n");
  printf("    get_interval       - afiseaza intervalul curent\n");
  printf("\n  Altele:\n");
  printf("    help               - afiseaza acest mesaj\n");
  printf("    stats              - afiseaza toate statisticile\n");
  printf("    quit               - inchide conexiunea\n\n");
}

// functie pentru trimis comenzi
int tr_cmd(int sd, const char *cmd, char *rasp) 
{
  if (write(sd, cmd, strlen(cmd)) <= 0) 
  {
    perror("Eroare la write() spre server.\n");
    return -1;
  }
  bzero(rasp, MAX_SIZE);
  int rez = read(sd, rasp, MAX_SIZE);
  if (rez < 0) 
  {
    perror("Eroare la read() de la server.\n");
    return -1;
  }
  if (rez == 0) 
  {
    printf("Serverul a inchis conexiunea.\n");
    return -1;
  }
  return 0;
}

int main(int argc, char *argv[]) 
{
  int sd;
  struct sockaddr_in server;
  char com[MAX_SIZE];
  char rasp[MAX_SIZE];
  
  if (argc != 3) 
  {
    printf("MonitorAndLogS - Client\n");
    printf("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
    printf("Exemplu: %s 0 2444\n", argv[0]);
    return -1;
  }

  int port = atoi(argv[2]);
  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
  {
    perror("Eroare la socket().\n");
    return errno;
  }

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(argv[1]);
  server.sin_port = htons(port);
  
  printf("Conectare la %s:%d...\n", argv[1], port);
  
  if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1) 
  {
    perror("Eroare la connect().\n");
    return errno;
  }
  
  // citeste mesajul de bun venit ca sa fie sigur ca este conectat
  bzero(rasp, MAX_SIZE);
  if (read(sd, rasp, MAX_SIZE) > 0) 
  {
    printf("Server: %s", rasp);
  }
  
  printf("Tastati 'help' pentru lista de comenzi.\n\n");
  
  while (1) 
  {
    printf("> ");
    fflush(stdout);
    
    bzero(com, MAX_SIZE);
    if (fgets(com, MAX_SIZE, stdin) == NULL) break;
    com[strcspn(com, "\r\n")] = 0;
    
    if (strlen(com) == 0) 
    {
      continue;
    }
    
    // comenzi locale
    if (strcmp(com, "help") == 0) 
    {
      print_help();
      continue;
    }
    
    if (strcmp(com, "stats") == 0) 
    {
      printf("\n=== STATISTICI ===\n");
      
      printf("\n[Memorie]\n");
      if (tr_cmd(sd, "get_memory_hist 5", rasp) == 0) 
        printf("%s", rasp);
      
      printf("\n[Utilizatori]\n");
      if (tr_cmd(sd, "get_users_hist 20", rasp) == 0) 
        printf("%s", rasp);
      
      printf("\n[Conexiuni per serviciu]\n");
      if (tr_cmd(sd, "get_serv_hist 5", rasp) == 0) 
        printf("%s", rasp);

      printf("\n[Istoric Servicii Oferite (LISTEN)]\n");
      if (tr_cmd(sd, "get_list_hist 5", rasp) == 0)
        printf("%s", rasp);

      printf("\n[Istoric Procese (Top CPU)]\n");
      if (tr_cmd(sd, "get_proc_hist 5", rasp) == 0)
        printf("%s", rasp);
      
      printf("==================\n\n");
      continue;
    }
    
    if (strcmp(com, "quit") == 0) 
    {
      tr_cmd(sd, "quit", rasp);
      printf("La revedere!\n");
      break;
    }
    
    if (tr_cmd(sd, com, rasp) < 0) 
    {
      break;
    }
    
    printf("%s\n", rasp);
  }
  
  close(sd);
  return 0;
}
