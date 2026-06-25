#define _GNU_SOURCE
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define PORT 2444
#define INTERVAL 10
#define MAX_SIZE 16384
#define SHM_NAME "/monitorandlogs_shm"

extern int errno;

// intervalul e global ca sa il vada si procesele copil
struct shm_data {
  int interval;
} *shm;

struct ports {
  char port[16];
  int nr_con;
  int aparitii;
} porturi[100];

char *conv_addr(struct sockaddr_in adr) 
{
  static char str[25];
  char port[7];
  strcpy(str, inet_ntoa(adr.sin_addr));
  sprintf(port, ":%d", ntohs(adr.sin_port));
  strcat(str, port);
  return (str);
}

void buff_app(char *dest, const char *src, int len) 
{
  if (strlen(dest) + strlen(src) < len) {
    strcat(dest, src);
  }
}

void scrie_log() 
{
  FILE *f;
  time_t tm;
  struct tm *info;
  char tm_str[80], conn_str[512], usr_str[256], serv_str[256], proc_str[256], net_str[256];
  time(&tm);
  info = localtime(&tm);
  strftime(tm_str, 80, "%Y-%m-%d %H:%M:%S", info);

  // inc sist
  float inc = 0;
  FILE *f_load = fopen("/proc/loadavg", "r");
  if (f_load) {
    fscanf(f_load, "%f", &inc);
    fclose(f_load);
  } else {
    inc = -1;
  }

  // nr proc
  int nr_proc = 0;
  DIR *dir_proc = opendir("/proc");
  if (dir_proc) {
    struct dirent *e;
    while ((e = readdir(dir_proc)) != NULL) {
      if (e->d_name[0] >= '0' && e->d_name[0] <= '9')
        nr_proc++;
    }
    closedir(dir_proc);
  }

  // mem ram folosita
  float mem_fol = 0;
  FILE *f_mem = fopen("/proc/meminfo", "r");
  if (f_mem) {
    char linii[200];
    int mem_total = 0, mem_disponibila = 0;
    while (fgets(linii, 200, f_mem)) {
      if (strstr(linii, "MemTotal:"))
        sscanf(linii, "MemTotal: %d", &mem_total);
      if (strstr(linii, "MemAvailable:"))
        sscanf(linii, "MemAvailable: %d", &mem_disponibila);
    }
    fclose(f_mem);
    if (mem_total > 0)
      mem_fol = 100.0 * (mem_total - mem_disponibila) / mem_total;
  }

  // utilizatori conectati
  FILE *f_who = popen("who", "r");
  if (f_who) {
    char linii[100];
    usr_str[0] = 0;
    while (fgets(linii, 100, f_who)) {
      char *usr = strtok(linii, " ");
      if (usr) {
        if (strlen(usr_str) > 0)
          buff_app(usr_str, "|", 256);
        buff_app(usr_str, usr, 256);
      }
    }
    pclose(f_who);
  }
  if (strlen(usr_str) == 0)
    strcpy(usr_str, "");

  // conexiuni
  FILE *f_conn = popen("ss -tn state established", "r");
  if (f_conn) {
    char linii[200];
    conn_str[0] = 0;
    fgets(linii, 200, f_conn); 
    int nr_port = 0;
    bzero(porturi, sizeof(porturi));
    while (fgets(linii, 200, f_conn)) {
      char *p = strtok(linii, " ");
      int col = 0;
      char ip_remote[64] = "";
      while (p) {
        col++;
        if (col == 4) strcpy(ip_remote, p);
        p = strtok(NULL, " ");
      }
      char *ptr = strrchr(ip_remote, ':');
      if (ptr) {
        *ptr = 0; // taiem portul ca sa ramanem doar cu IP-ul
        char *port = ptr + 1;
        int gasit = -1;
        for (int i = 0; i < nr_port; i++) {
          if (strcmp(porturi[i].port, port) == 0) {
            gasit = i;
            break;
          }
        }
        if (gasit >= 0) {
          porturi[gasit].nr_con++;
        } else if (nr_port < 50) {
          strncpy(porturi[nr_port].port, port, 15);
          porturi[nr_port].nr_con = 1;
          nr_port++;
        }
      }
    }
    pclose(f_conn); // inchidem pipe-ul ca sa nu lasam procese zombie
    for (int i = 0; i < nr_port; i++) {
      char temp[32];
      sprintf(temp, "%s:%d|", porturi[i].port, porturi[i].nr_con);
      buff_app(conn_str, temp, 500);
    }
    if (strlen(conn_str) > 0)
      conn_str[strlen(conn_str) - 1] = 0;
  }
  if (strlen(conn_str) == 0)
    strcpy(conn_str, "None");

  // servicii
  FILE *f_serv = popen("ss -ltn", "r");
  if (f_serv) {
    char linii[200];
    serv_str[0] = 0;
    fgets(linii, 200, f_serv);
    while (fgets(linii, 200, f_serv)) {
      char *p = strtok(linii, " ");
      int col = 0;
      while (p) {
        col++;
        if (col == 4) {
          char *ptr = strrchr(p, ':');
          if (ptr) {
            char *port = ptr + 1;
            char temp[32];
            sprintf(temp, "%s|", port);
            buff_app(serv_str, temp, 250);
          }
        }
        p = strtok(NULL, " ");
      }
    }
    pclose(f_serv);
  }
  if (strlen(serv_str) == 0)
    strcpy(serv_str, "");
  else if (strlen(serv_str) > 0)
    serv_str[strlen(serv_str) - 1] = 0;

  // tipuri conexiuni
  strcpy(net_str, "");
  FILE *f_sock = fopen("/proc/net/sockstat", "r");
  if (f_sock) {
    char linii[256];
    while(fgets(linii, 256, f_sock)) {
        char key[32];
        char *p_inuse = strstr(linii, "inuse");
        if(p_inuse) {
            char *sep = strchr(linii, ':');
            if(sep) {
                int len = sep - linii;
                if(len > 31) len = 31;
                strncpy(key, linii, len);
                key[len] = 0;
                int val = atoi(p_inuse + 6);
                char temp[64];
                sprintf(temp, "%s:%d|", key, val);
                buff_app(net_str, temp, 256);
            }
        }
    }
    if(strlen(net_str) > 0) net_str[strlen(net_str)-1] = 0;
    fclose(f_sock);
  } else {
      strcpy(net_str, "None");
  }

  // top procese
  FILE *f_proc = popen("ps -eo pid,comm,%cpu,%mem --sort=-%cpu", "r");
  if (f_proc) {
    char linii[200];
    proc_str[0] = 0;
    fgets(linii, 200, f_proc);
    int cnt = 0;
    while (fgets(linii, 200, f_proc) && cnt < 3) {
      char pid[10], com[40], cpu[10], mem[10];
      if (sscanf(linii, "%s %s %s %s", pid, com, cpu, mem) == 4) {
        char temp[100];
        sprintf(temp, "%s:%s:%s:%s|", pid, com, cpu, mem);
        buff_app(proc_str, temp, 256);
        cnt++; 
      }
    }
    pclose(f_proc);
  }
  if (strlen(proc_str) == 0)
    strcpy(proc_str, "");
  else if (strlen(proc_str) > 0)
    proc_str[strlen(proc_str) - 1] = 0;

  f = fopen("data/system_logs.csv", "a");
  if (f == NULL) {
    perror("[server] Eroare la deschiderea log");
    return;
  }
 
  // utilizare cpu
  double cpu_use = -1;
  static unsigned int prev_user = 0, prev_nice = 0, prev_system = 0, prev_idle = 0, prev_iowait = 0, prev_irq = 0, prev_softirq = 0, prev_steal = 0;
  unsigned int user, nice, system, idle, iowait, irq, softirq, steal;
  FILE *fp = fopen("/proc/stat", "r");
  if (!fp) cpu_use = -1.0;
    
  char buff[256];
  if (fgets(buff, sizeof(buff), fp)) {
    if (sscanf(buff, "cpu %d %d %d %d %d %d %d %d", &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) < 4) {
      fclose(fp);
      cpu_use = -1.0;
    }
    fclose(fp);

    if (prev_user == 0 && prev_idle == 0) {
        prev_user = user; prev_nice = nice; prev_system = system; prev_idle = idle;
        prev_iowait = iowait; prev_irq = irq; prev_softirq = softirq; prev_steal = steal;
        cpu_use = 0.0;
    }
    unsigned int prev_total = prev_user + prev_nice + prev_system + prev_idle + prev_iowait + prev_irq + prev_softirq + prev_steal;
    unsigned int total = user + nice + system + idle + iowait + irq + softirq + steal;
    unsigned int total_diff = total - prev_total;
    unsigned int idle_diff = (idle + iowait) - (prev_idle + prev_iowait);
    prev_user = user; prev_nice = nice; prev_system = system; prev_idle = idle;
    prev_iowait = iowait; prev_irq = irq; prev_softirq = softirq; prev_steal = steal;
    if (total_diff > 0) 
      cpu_use = 100.0 * (double)(total_diff - idle_diff) / total_diff;
    else
      cpu_use = 0.0;
  }

  fprintf(f, "%s,%.2f,%d,%.1f,%.1f,%s,%s,%s,%s,%s\n", tm_str, inc, nr_proc, mem_fol, cpu_use,
          usr_str, conn_str, serv_str, net_str, proc_str);
  fclose(f);
  printf("[server-main] Log salvat: %s | Load:%.2f | Procs:%d | Mem:%.1f%% | CPU:%.1f%% | "
         "Useri:%s | Conexiuni:OK | Servicii:OK | Types:%s\n",
         tm_str, inc, nr_proc, mem_fol, cpu_use, usr_str, net_str);
}

void login_(char *rasp, int *auth, char *arg) 
{
  if (!arg || strlen(arg) == 0) {
    sprintf(rasp, "Utilizare: login <user> <parola>\n");
    return;
  }
  FILE *f = fopen("data/users.txt", "r");
  if (!f) {
    *auth = 1;
    sprintf(rasp, "debug\n");
    return;
  }
  char linii[200];
  int gasit = 0;
  while (fgets(linii, 200, f)) {
    linii[strcspn(linii, "\r\n")] = 0; // scapam de newline ca sa putem compara stringurile
    if (strcmp(linii, arg) == 0) {
      gasit = 1;
      break;
    }
  }
  fclose(f);
  if (gasit) {
    *auth = 1;
    sprintf(rasp, "Autentificare reusita. Bine ai venit!\n");
  } 
  else {
    sprintf(rasp, "Eroare: User sau parola incorecta.\n");
  }
}

void load_avg(char *rasp) 
{
  FILE *f = fopen("/proc/loadavg", "r");
  if (f == NULL) {
    sprintf(rasp, "Eroare: Nu s-a putut citi /proc/loadavg\n");
    return;
  }
  float load1, load5, load15;
  if (fscanf(f, "%f %f %f", &load1, &load5, &load15) == 3) {
    sprintf(rasp, "Load Average: %.2f (1 min), %.2f (5 min), %.2f (15 min)\n", load1, load5, load15);
  } 
  else {
    sprintf(rasp, "Eroare: Format invalid in /proc/loadavg\n");
  }
  fclose(f);
}

void proc_info(char *rasp, char *arg) 
{
  int num = 0;
  DIR *proc_dir = opendir("/proc");
  if (proc_dir) {
    struct dirent *de;
    while ((de = readdir(proc_dir)) != NULL) {
      if (de->d_name[0] >= '0' && de->d_name[0] <= '9')
        num++;
    }
    closedir(proc_dir);
  } 
  else {
    num = -1;
  }
  sprintf(rasp, "Procese active: %d\n", num);
  int n = 0;
  if (arg && strlen(arg) > 0)
    n = atoi(arg);
  if (n > 0) {
    if (n > 30)
      n = 30; // limitam la 30 ca sa nu umplem ecranul clientului
    char com[256];
    sprintf(com, "ps -eo pid,user,comm,%%cpu,%%mem --sort=-%%cpu | head -n %d", n + 1); // +1 pt header
    FILE *f_top = popen(com, "r");
    if (f_top) {
      strcat(rasp, "\nTop consumatori CPU:\n");
      char linii[512];
      while (fgets(linii, 512, f_top)) {
        buff_app(rasp, linii, MAX_SIZE - 30);
      }
      pclose(f_top);
    }
  }
}

void logged_users(char *rasp) 
{
  FILE *f = popen("who", "r");
  if (f) {
    char linii[512];
    strcpy(rasp, "Utilizatori logati:\n");
    while (fgets(linii, 512, f)) {
      buff_app(rasp, linii, MAX_SIZE);
    }
    pclose(f);
  } 
  else {
    sprintf(rasp, "Eroare popen who");
  }
}

void services(char *rasp) 
{
  FILE *f = popen("ss -tlnp", "r");
  if (f) {
    char linii[512];
    strcpy(rasp, "Servicii (porturi in LISTEN):\n");
    
    while (fgets(linii, 512, f)) {
        char *p = linii;
        while (*p && *p != ' ' && *p != '\t') p++;
        while (*p && (*p == ' ' || *p == '\t')) p++;
        buff_app(rasp, p, MAX_SIZE);
    }
    pclose(f);
  } 
  else {
    sprintf(rasp, "Eroare popen ss");
  }
}

void connections(char *rasp) 
{
  FILE *f = popen("ss -tn state established", "r");
  if (f) {
    char linii[512];
    strcpy(rasp, "Conexiuni active (ESTABLISHED):\n");
    while (fgets(linii, 512, f)) {
      buff_app(rasp, linii, MAX_SIZE);
    }
    pclose(f);
  } else {
    sprintf(rasp, "Eroare popen ss");
  }

  char summary[512] = "";
  FILE *f_ss = popen("ss -s", "r");
  if(f_ss) {
      char linii[256];
      strcat(summary, "\nSumar Tipuri:\n");
      while(fgets(linii, 256, f_ss)) {
             linii[strcspn(linii, "\n")] = 0;
             strcat(summary, "  ");
             strcat(summary, linii);
             strcat(summary, "\n");
      }
      pclose(f_ss);
  }
  buff_app(rasp, summary, MAX_SIZE);
}



void memory_info(char *rasp) 
{
  FILE *f = fopen("/proc/meminfo", "r");
  if (!f) {
    sprintf(rasp, "Eroare: Nu se poate deschide meminfo\n");
    return;
  }
  char linii[256];
  int total = 0, disp = 0, buff = 0, cache = 0, swap_t = 0, swap_f = 0;
  while (fgets(linii, 256, f)) {
    if (strstr(linii, "MemTotal:"))
      sscanf(linii, "MemTotal: %d", &total);
    else if (strstr(linii, "MemAvailable:"))
      sscanf(linii, "MemAvailable: %d", &disp);
    else if (strstr(linii, "Buffers:"))
      sscanf(linii, "Buffers: %d", &buff);
    else if (strstr(linii, "Cached:"))
      sscanf(linii, "Cached: %d", &cache);
    else if (strstr(linii, "SwapTotal:"))
      sscanf(linii, "SwapTotal: %d", &swap_t);
    else if (strstr(linii, "SwapFree:"))
      sscanf(linii, "SwapFree: %d", &swap_f);
  }
  fclose(f);

  int fol = total - disp;
  float procent;
  if (total > 0)
    procent = (100.0 * fol / total);
  else
    procent = 0;
  int swap_fol = swap_t - swap_f;
  sprintf(rasp, "Memorie:\n"
                "Total:     %d MB\n"
                "Disponibil:%d MB\n"
                "Folosita:  %d MB (%.1f%%)\n"
                "Buffers:   %d MB\n"
                "Cached:    %d MB\n"
                "Swap:      %d/%d MB folosit\n",
                total / 1024, disp / 1024, fol / 1024, procent, buff / 1024, cache / 1024, 
                swap_fol / 1024, swap_t / 1024);
}

// 1 tmp
// 2 load
// 3 procs
// 4 mem
// 5 cpu
// 6 useri
// 7 conexiuni
// 8 servicii
// 9 tipuri conexiuni
// 10 top procs

int citeste_log(char linii[100][512], int *start, int *output, int num, const char *start_tm, const char *end_tm) 
{
  FILE *f = fopen("data/system_logs.csv", "r");
  if (!f) return -1;
  *output = 0;
  *start = 0;
  if (start_tm) {
      char line[512];
      int gasit = 0;
      int total_read = 0; 
      while (fgets(line, 512, f)) {
          total_read++;
          int match = 0;
          if (strlen(line) >= 16) {
             if (end_tm == NULL) {
                 // zi specfica 
                 if (strncmp(line, start_tm, strlen(start_tm)) == 0) match = 1;
             } else {
                 // interval orar 
                 char hm[6];
                 strncpy(hm, line + 11, 5); // index 11 e inceputul HH:MM
                 hm[5] = 0;
                 if (strcmp(hm, start_tm) >= 0 && strcmp(hm, end_tm) <= 0) match = 1;
             }
          }
          if (match) {
              if (gasit < 100) {
                  strcpy(linii[gasit], line);
                  gasit++;
              }
          }
      }
      fclose(f);
      *output = gasit;
      return total_read;
  }

  // folosesc buffer circular ca sa retin ultimele 1000 linii
  char temp[1000][512];
  int count = 0;
  char line[512];
  while (fgets(line, 512, f)) {
    if (count < 1000) {
      strcpy(temp[count], line);
      count++;
    } else {
        for(int i=0; i<999; i++) strcpy(temp[i], temp[i+1]);
        strcpy(temp[999], line);
    }
  }
  fclose(f);
  
  if (count == 0) return 0;

  int read_n = (num < count) ? num : count;
  for (int i = 0; i < read_n; i++) {
    strcpy(linii[i], temp[count - read_n + i]);
  }
  *output = read_n;
  return count;
}

void sys_hist(char *rasp, char *arg) 
{ 
  char linii[100][512];
  int start, output;
  int total;

  if ((arg && strlen(arg) == 11 && arg[2] == ':' && arg[5] == '-' && arg[8] == ':')) {
    char s_tm[6], e_tm[6];
    strncpy(s_tm, arg, 5); s_tm[5]=0;
    strncpy(e_tm, arg+6, 5); e_tm[5]=0;
    total = citeste_log(linii, &start, &output, 0, s_tm, e_tm);
    if(total <= 0) {
        sprintf(rasp, "Nu exista date in intervalul %s-%s.\n", s_tm, e_tm);
        return;
    }
  }
  else if ((arg && strlen(arg) == 10 && arg[4] == '-' && arg[7] == '-')) {
      total = citeste_log(linii, &start, &output, 0, arg, NULL);
      if(total <= 0) {
          sprintf(rasp, "Nu exista date pentru data %s.\n", arg);
          return;
      }
  }
  else {
    int num = 10;
    if (strlen(arg) > 0) num = atoi(arg);
    if (num <= 0) num = 10;
    if (num > 100) num = 100;
    total = citeste_log(linii, &start, &output, num, NULL, NULL);
    if (total == -1) {
        sprintf(rasp, "Eroare: Nu exista loguri.\n");
        return;
    }
    if (total == 0) {
        sprintf(rasp, "Nu exista date in loguri.\n");
        return;
    }
  }

  char rez[MAX_SIZE] = "Istoric utilizare memorie si CPU:\n";
  strcat(rez, "  Timestamp           | Mem % | CPU %\n");
  strcat(rez, "  --------------------|-------|------\n");
  float mem_min = 100.0, mem_max = 0.0, mem_sum = 0.0;
  int count = 0;

  for (int i = 0; i < output; i++) {
    int idx = (start + i) % 100;
    char tm_str[32], copie_linii[512];
    float mem, cpu_v = 0.0;
    strcpy(copie_linii, linii[idx]);
    char *p = strtok(copie_linii, ","); // 1 tm
    if (!p) continue;
    strcpy(tm_str, p);
    p = strtok(NULL, ","); // 2 
    p = strtok(NULL, ","); // 3 
    p = strtok(NULL, ","); // 4 mem
    if (!p) continue;
    mem = atof(p);
    p = strtok(NULL, ","); // 5 cpu
    if (p) {
        if (strstr(p, "nan")) cpu_v = 0.0;
        else cpu_v = atof(p);
    }

    char linii_out[128];
    sprintf(linii_out, "  %s | %.1f%% | %.1f%%\n", tm_str, mem, cpu_v);
    buff_app(rez, linii_out, MAX_SIZE - 150);
    if (mem < mem_min) mem_min = mem;
    if (mem > mem_max) mem_max = mem;
    mem_sum += mem;
    count++;
  }
  if (count > 0) {
    char stat[128];
    strcat(rez, "  --------------------|-------|------\n");
    sprintf(stat, "  Min: %.1f%% | Max: %.1f%% | Avg: %.1f%%\n", mem_min,
            mem_max, mem_sum / count);
    strcat(rez, stat);
  }
  sprintf(rasp, "%s", rez);
}

void users_hist(char *rasp, char *arg) 
{
  int start, output;
  char linii[100][512];
  int total;
  
  if ((arg && strlen(arg) == 11 && arg[2] == ':' && arg[5] == '-' && arg[8] == ':')) {
    char s_tm[6], e_tm[6];
    strncpy(s_tm, arg, 5); s_tm[5]=0;
    strncpy(e_tm, arg+6, 5); e_tm[5]=0;
    total = citeste_log(linii, &start, &output, 0, s_tm, e_tm);
    if (total <= 0) {
       sprintf(rasp, "Nu exista date in intervalul %s-%s.\n", s_tm, e_tm);
       return;
    }
  } 
  else if ((arg && strlen(arg) == 10 && arg[4] == '-' && arg[7] == '-')) {
      total = citeste_log(linii, &start, &output, 0, arg, NULL);
      if(total <= 0) {
          sprintf(rasp, "Nu exista date pentru data %s.\n", arg);
          return;
      }
  } else {
    int num = 10;
    if (strlen(arg) > 0) num = atoi(arg);
    if (num <= 0) num = 10;
    if (num > 100) num = 100;
    total = citeste_log(linii, &start, &output, num, NULL, NULL);
    if (total <= 0) {
       if (total == -1) sprintf(rasp, "Eroare: Nu exista loguri.\n");
       else sprintf(rasp, "Nu exista date in loguri.\n");
       return;
    }
  }

  char nume_usr[50][64];
  int cnt_usr[50], numb_usr = 0;
  bzero(cnt_usr, sizeof(cnt_usr));

  for (int i = 0; i < output; i++) {
    int idx = (start + i) % 100;
    char copie_linii[512], useri[256];
    strcpy(copie_linii, linii[idx]);
    char *p = strtok(copie_linii, ","); // 1 tm
    p = strtok(NULL, ","); // 2
    p = strtok(NULL, ","); // 3
    p = strtok(NULL, ","); // 4
    p = strtok(NULL, ","); // 5
    p = strtok(NULL, ","); // 6 users
    if (p) strcpy(useri, p);
    else strcpy(useri, "None");
    char *token = strtok(useri, ";|");
    while (token != NULL) {
      if (strcmp(token, "()") != 0 && strcmp(token, "") != 0) {
        int gasit = -1;
        for (int j = 0; j < numb_usr; j++) {
          if (strcmp(nume_usr[j], token) == 0) {
            gasit = j;
            break;
          }
        }
        if (gasit >= 0)
          cnt_usr[gasit]++;
        else if (numb_usr < 50) {
          strncpy(nume_usr[numb_usr], token, 63);
          cnt_usr[numb_usr] = 1;
          numb_usr++;
        }
        token = strtok(NULL, ";|");
      }
    }
  }

  char antet[128], rez[MAX_SIZE] = "Statistici conectari per usr:\n";
  sprintf(antet, "  (Ultimele %d log-uri analizate)\n\n", output);
  strcat(rez, antet);
  strcat(rez, "  Utilizator     | Numar aparitii\n");
  strcat(rez, "  ---------------|-----------------\n");

  for (int i = 0; i < numb_usr; i++) {
    char linii_out[128];
    sprintf(linii_out, "  %-14s | %d\n", nume_usr[i],
            cnt_usr[i]);
    buff_app(rez, linii_out, MAX_SIZE - 100);
  }

  if (numb_usr == 0) {
    strcat(rez, "  (niciun user gasit in loguri)\n");
  } else {
    int tot_ap = 0;
    for (int i = 0; i < numb_usr; i++) {
      tot_ap += cnt_usr[i];
    }
    char stat[128];
    strcat(rez, "  ---------------|-----------------\n");
    sprintf(stat, "  Total: %d utilizatori unici, %d aparitii\n", numb_usr, tot_ap);
    strcat(rez, stat);
  }

  sprintf(rasp, "%s", rez);
}

void service_hist(char *rasp, char *arg) 
{
  int num = 10;
  char filt_port[16] = "";
  char linii[100][512];
  int start, output, total;

  if ((arg && strlen(arg) == 11 && arg[2] == ':' && arg[5] == '-' && arg[8] == ':')) {
    char s_tm[6], e_tm[6];
    strncpy(s_tm, arg, 5); s_tm[5]=0;
    strncpy(e_tm, arg+6, 5); e_tm[5]=0;
    total = citeste_log(linii, &start, &output, 0, s_tm, e_tm);
    if (total <= 0) {
       sprintf(rasp, "Nu exista date in intervalul %s-%s.\n", s_tm, e_tm);
       return;
    }
  } 

  else if ((arg && strlen(arg) == 10 && arg[4] == '-' && arg[7] == '-')) {
      total = citeste_log(linii, &start, &output, 0, arg, NULL);
      if(total <= 0) {
          sprintf(rasp, "Nu exista date pentru data %s.\n", arg);
          return;
      }
  } 
  else {
    if (strlen(arg) > 0) {
        char val_str[32] = "";
        char tip[16] = "";
        char buf_temp[64];
        strncpy(buf_temp, arg, 63);
        buf_temp[63] = 0;
        char *p_val = strtok(buf_temp, " ");
        if (p_val) {
            strncpy(val_str, p_val, 31);
            char *p_tip = strtok(NULL, " ");
            if (p_tip) {
                strncpy(tip, p_tip, 15);
            }
        }
        if (strcmp(tip, "port") == 0) {
            strncpy(filt_port, val_str, sizeof(filt_port) - 1);
        } else {
            num = atoi(val_str);
        }
    }
    if (num <= 0) num = 10;
    if (num > 100) num = 100;
    total = citeste_log(linii, &start, &output, num, NULL, NULL);
  }
  if (total == -1) {
    sprintf(rasp, "Eroare: Nu exista loguri.\n");
    return;
  }
  if (total == 0) {
    sprintf(rasp, "Nu exista date in loguri.\n");
    return;
  }
  char rez[MAX_SIZE] = "Istoric conexiuni per serviciu (port):\n";
  strcat(rez, "  Timestamp           | Conexiuni per port\n");
  strcat(rez, "  --------------------|-------------------\n");
  int num_port = 0;
  bzero(porturi, sizeof(porturi));
  for (int i = 0; i < output; i++) {
    int idx = (start + i) % 100;
    char tm_str[32], con[256], copie_linii[512];
    strcpy(copie_linii, linii[idx]);
    char *p = strtok(copie_linii, ","); // 1 tm
    if (!p) continue;
    strcpy(tm_str, p);
    p = strtok(NULL, ","); // 2
    p = strtok(NULL, ","); // 3
    p = strtok(NULL, ","); // 4
    p = strtok(NULL, ","); // 5
    p = strtok(NULL, ","); // 6
    p = strtok(NULL, ","); // 7 conexiuni
    if (p) strcpy(con, p);
    else strcpy(con, "None");
    char linii_out[384];
    if (strlen(filt_port) > 0) {
      char filtru[32];
      sprintf(filtru, "%s:", filt_port);
      char *gasit = strstr(con, filtru);
      if (gasit) {
        int port_conn = 0;
        char *sep = strchr(gasit, ':');
        if (sep) {
          port_conn = atoi(sep + 1);
        }
        sprintf(linii_out, "  %s | %s:%d\n", tm_str, filt_port, port_conn);
      } else {
        continue;
      }
    } else {
      sprintf(linii_out, "  %s | %s\n", tm_str,
              strlen(con) > 0 ? con : "(nicio conexiune)");
    }
    buff_app(rez, linii_out, MAX_SIZE - 300);
    char conn_aux[256];
    strncpy(conn_aux, con, sizeof(conn_aux) - 1);
    conn_aux[255] = 0;
    char *token = strtok(conn_aux, ";|");
    while (token != NULL) {
      char port_str[16];
      int count_conn;
      char *sep = strrchr(token, ':');
      if (sep) {
        *sep = 0;
        strncpy(port_str, token, sizeof(port_str) - 1);
        port_str[sizeof(port_str) - 1] = 0;
        count_conn = atoi(sep + 1);
        int gasit = -1;
        for (int p = 0; p < num_port; p++) {
          if (strcmp(porturi[p].port, port_str) == 0) {
            gasit = p;
            break;
          }
        }
        if (gasit >= 0) {
          porturi[gasit].nr_con += count_conn;
          porturi[gasit].aparitii++;
        } else if (num_port < 50) {
          strncpy(porturi[num_port].port, port_str, 15);
          porturi[num_port].nr_con = count_conn;
          porturi[num_port].aparitii = 1;
          num_port++;
        }
      }
      token = strtok(NULL, ";|");
    }
  }

  if (num_port > 0) {
    int max_idx = 0, total_con_glb = 0;
    char stat[256];
    for (int i = 1; i < num_port; i++) {
      if (porturi[i].nr_con >
          porturi[max_idx].nr_con) {
        max_idx = i;
      }
    }
    for (int i = 0; i < num_port; i++) {
      total_con_glb += porturi[i].nr_con;
    }    
    strcat(rez, "  --------------------|-------------------\n");
    sprintf(stat, "Total: %d porturi, %d conexiuni\nCel mai activ: port %s (%d conexiuni)\n",
            num_port, total_con_glb, porturi[max_idx].port,
            porturi[max_idx].nr_con);
    strcat(rez, stat);
  }
  sprintf(rasp, "%s", rez);
}

void list_hist(char *rasp, char *arg) 
{ 
  int start, output;
  char linii[100][512];
  int total;
  
  if ((arg && strlen(arg) == 11 && arg[2] == ':' && arg[5] == '-' && arg[8] == ':')) {
    char s_tm[6], e_tm[6];
    strncpy(s_tm, arg, 5); s_tm[5]=0;
    strncpy(e_tm, arg+6, 5); e_tm[5]=0;
    total = citeste_log(linii, &start, &output, 0, s_tm, e_tm);
    if (total <= 0) {
       sprintf(rasp, "Nu exista date in intervalul %s-%s.\n", s_tm, e_tm);
       return;
    }
  } 
  else if ((arg && strlen(arg) == 10 && arg[4] == '-' && arg[7] == '-')) {
      total = citeste_log(linii, &start, &output, 0, arg, NULL);
      if(total <= 0) {
          sprintf(rasp, "Nu exista date pentru data %s.\n", arg);
          return;
      }
  } else {
    int num = 10;
    if (strlen(arg) > 0) num = atoi(arg);
    if (num <= 0) num = 10;
    if (num > 100) num = 100;
    total = citeste_log(linii, &start, &output, num, NULL, NULL);
    if (total <= 0) {
       if (total == -1) sprintf(rasp, "Eroare: Nu exista loguri.\n");
       else sprintf(rasp, "Nu exista date in loguri.\n");
       return;
    }
  }
  if (total == 0) {
    sprintf(rasp, "Nu exista date in loguri.\n");
    return;
  }
  char rez[MAX_SIZE] = "Istoric porturi deschise (LISTEN):\n";
  strcat(rez, "  Timestamp           | Porturi\n");
  strcat(rez, "  --------------------|-----------------------------\n");
  for (int i = 0; i < output; i++) {
    int idx = (start + i) % 100;
    char tm_str[32];
    char serv[256];
    char copie_linii[512];
    strcpy(copie_linii, linii[idx]);
    char *p = strtok(copie_linii, ","); // 1 tm
    if (!p) continue;
    strcpy(tm_str, p);
    p = strtok(NULL, ","); // 2
    p = strtok(NULL, ","); // 3
    p = strtok(NULL, ","); // 4
    p = strtok(NULL, ","); // 5 
    p = strtok(NULL, ","); // 6
    p = strtok(NULL, ","); // 7 
    p = strtok(NULL, ","); // 8 servicii
    if (p) strcpy(serv, p);
    else strcpy(serv, "None");
    char net_types[256] = "";
    p = strtok(NULL, ","); // 9 tipuri conexiuni
    if (p) strcpy(net_types, p);

    if (strlen(serv) > 0 && strcmp(serv, "()") != 0 &&
        strcmp(serv, "") != 0) {
      if (strlen(serv) > 50)
        strcpy(serv + 47, "...");
      char line_out[1024];
      sprintf(line_out, "  %s | %-20s | %s\n", tm_str, serv, net_types);
      buff_app(rez, line_out, MAX_SIZE - 50);
    }
  }
  sprintf(rasp, "%s", rez);
}

void proc_hist(char *rasp, char *arg) 
{ 
  int start, output;
  char linii[100][512];
  int total;
  
  if ((arg && strlen(arg) == 11 && arg[2] == ':' && arg[5] == '-' && arg[8] == ':')) {
    char s_tm[6], e_tm[6];
    strncpy(s_tm, arg, 5); s_tm[5]=0;
    strncpy(e_tm, arg+6, 5); e_tm[5]=0;
    total = citeste_log(linii, &start, &output, 0, s_tm, e_tm);
    if(total <= 0) {
        sprintf(rasp, "Nu exista date in intervalul %s-%s.\n", s_tm, e_tm);
        return;
    }
  } 
  else if ((arg && strlen(arg) == 10 && arg[4] == '-' && arg[7] == '-')) {
      total = citeste_log(linii, &start, &output, 0, arg, NULL);
      if(total <= 0) {
          sprintf(rasp, "Nu exista date pentru data %s.\n", arg);
          return;
      }
  } else {
    int num = 10;
    if (strlen(arg) > 0) num = atoi(arg);
    if (num <= 0) num = 10;
    if (num > 100) num = 100;
    total = citeste_log(linii, &start, &output, num, NULL, NULL);
    if (total <= 0) {
       if (total == -1) sprintf(rasp, "Eroare: Nu exista loguri.\n");
       else sprintf(rasp, "Nu exista date in loguri.\n");
       return;
    }
  }
  if (total == 0) {
    sprintf(rasp, "Nu exista date in loguri.\n");
    return;
  }
  char rez[MAX_SIZE] = "Istoric Stare Sistem (Load, Procese, Top CPU):\n";
  strcat(rez, "  Timestamp           | Load | Procs | Top Consumatori CPU (pid, command, %cpu, %mem)\n");
  strcat(rez,
         "  --------------------|------|-------|----------------------------------\n");
  float load_sum = 0;
  int load_cnt = 0, proc_sum = 0;
  for (int i = 0; i < output; i++) {
    int idx = (start + i) % 100;
    char tm_str[32];
    float inc = 0;
    int nr_proc = 0;
    char top[256];
    char copie_linii[512];
    strcpy(copie_linii, linii[idx]);
    char *p = strtok(copie_linii, ","); // 1 tm
    if (!p) continue;
    strcpy(tm_str, p);
    p = strtok(NULL, ","); // 2 load
    if (p) inc = atof(p);
    p = strtok(NULL, ","); // 3 nr_proc
    if (p) nr_proc = atoi(p);
    p = strtok(NULL, ","); // 4 mem
    p = strtok(NULL, ","); // 5 cpu (skip)
    p = strtok(NULL, ","); // 6 usr
    p = strtok(NULL, ","); // 7 conn
    p = strtok(NULL, ","); // 8 serv
    p = strtok(NULL, ","); // 9 net
    p = strtok(NULL, "\n"); // 10 top proc
    if (p) strcpy(top, p); 
    else strcpy(top, "None");    
    if (strlen(top) > 0 && strcmp(top, "") != 0 && strcmp(top, "()") != 0) {
      char line_out[350];
      sprintf(line_out, "  %s | %.2f | %3d   | %s\n", tm_str, inc, nr_proc, top);
      buff_app(rez, line_out, MAX_SIZE - 50);
      load_sum += inc;
      load_cnt++;
      proc_sum += nr_proc;
    }
  }  
  if (load_cnt > 0) {
    char stats[200];
    strcat(rez, "  --------------------|------|-------|----------------------------------\n");
    sprintf(stats, "  Avg Load: %.2f | Avg Procs: %d\n", 
            load_sum / load_cnt, proc_sum / load_cnt);
    strcat(rez, stats);
  }
  sprintf(rasp, "%s", rez);
}

void set_interval_(char *rasp, char *arg) 
{
  if (!arg || strlen(arg) == 0) {
    sprintf(rasp,
            "Utilizare: set_interval <secunde>\nInterval curent: %d secunde\n",
            shm->interval);
    return;
  }
  int val = atoi(arg);
  if (val < 1)
    sprintf(rasp, "Eroare: Minim 1 secunda.\n");
  else if (val > 3600)
    sprintf(rasp, "Eroare: Maxim 3600 secunde.\n");
  else {
    shm->interval = val;
    sprintf(rasp, "Interval setat la %d secunde.\n", val);
    printf("[server] Interval schimbat: %d s\n", val);
  }
}

void get_interval_(char *rasp) 
{
  sprintf(rasp, "Interval curent: %d secunde\n", shm->interval);
}

void logout_(char *rasp, int *auth) 
{
  *auth = 0;
  sprintf(rasp, "Deconectat. (Folositi quit pentru a iesi)\n");
}

const char *lista_com[] = {
"login","logout","quit","get_load_avg","get_proc_info","get_logged_users","get_services",
"get_connections","get_memory","get_sys_hist","get_users_hist","get_serv_hist",
"get_list_hist","get_proc_hist","set_interval","get_interval",NULL
};

void proc_client(int fd)
{
  char com[MAX_SIZE], rasp[MAX_SIZE];
  int bytes, auth = 0;
  bzero(rasp, MAX_SIZE);
  strcat(rasp, "MonitorAndLogS este pornit\n");
  write(fd, rasp, strlen(rasp));
  while (1) {
    bzero(com, MAX_SIZE);
    bytes = read(fd, com, sizeof(com));
    if (bytes <= 0) {
      break;
    }
    com[strcspn(com, "\r\n")] = 0;
    printf("[server-copil] Comanda primita: %s\n", com);
    bzero(rasp, MAX_SIZE);
    char *cmd = strtok(com, " ");
    char *arg = strtok(NULL, "");
    if (!arg)
      arg = "";
    if (!cmd)
      continue;
    int cmd_id = -1;
    for (int i = 0; lista_com[i]; i++)
      if (strcmp(com, lista_com[i]) == 0)
        cmd_id = i;
    if (!auth && cmd_id != 0 && cmd_id != 2) {
      sprintf(rasp, "Eroare: Nu sunteti autentificat. Folositi login.\n");
      write(fd, rasp, strlen(rasp));
      continue;
    }
    switch (cmd_id)
    {
    case 0: if (auth) sprintf(rasp, "Sunteti deja autentificat.\n");
            else login_(rasp, &auth, arg);
            break;
    case 1: logout_(rasp, &auth); break;
    case 2: break;
    case 3: load_avg(rasp); break;
    case 4: proc_info(rasp, arg); break;
    case 5: logged_users(rasp); break;
    case 6: services(rasp); break;
    case 7: connections(rasp); break;
    case 8: memory_info(rasp); break;
    case 9: sys_hist(rasp, arg); break;
    case 10: users_hist(rasp, arg); break;
    case 11: service_hist(rasp, arg); break;
    case 12: list_hist(rasp, arg); break;
    case 13: proc_hist(rasp, arg); break;
    case 14: set_interval_(rasp, arg); break;
    case 15: get_interval_(rasp); break;
    default: sprintf(rasp, "Comanda invalida.\n"); break;
    }
    if (cmd_id == 2)
      break;
    if (write(fd, rasp, strlen(rasp)) < 0)
    {
      perror("[server-copil] Eroare la write() catre client.\n");
      break;
    }
  }
  printf("[server-copil] Client deconectat.\n");
  close(fd);
  exit(0);
}

// ajuta cu eliberarea resurselor copiilor care devin zombie
void zombie(int semnal) 
{
  while (waitpid(-1, NULL, WNOHANG) > 0); 
}

int main() 
{
  struct sockaddr_in adr_sv;
  struct sockaddr_in adr_cl;
  fd_set readfds;
  struct timeval tv_wait;
  int sd_sv, sd_cl;
  int optval = 1;

  int fd_shm = shm_open(SHM_NAME, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd_shm == -1) {
    perror("[server] Eroare shm_open");
    return errno;
  }
  if (ftruncate(fd_shm, sizeof(struct shm_data)) == -1) {
    perror("[server] Eroare ftruncate");
    return errno;
  }
  shm = mmap(NULL, sizeof(struct shm_data), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
  if (shm == MAP_FAILED) {
    perror("[server] Eroare mmap");
    return errno;
  }
  close(fd_shm);
  shm->interval = INTERVAL;
  printf("[server] Shm init. Interval: %d s\n", shm->interval);
  signal(SIGCHLD, zombie); 

  if ((sd_sv = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("[server] Eroare la socket().\n");
    return errno;
  }
  setsockopt(sd_sv, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)); 
  bzero(&adr_sv, sizeof(adr_sv));
  adr_sv.sin_family = AF_INET;
  adr_sv.sin_addr.s_addr = htonl(INADDR_ANY);
  adr_sv.sin_port = htons(PORT);
  if (bind(sd_sv, (struct sockaddr *)&adr_sv, sizeof(struct sockaddr)) == -1) 
  {
    perror("[server] Eroare la bind().\n");
    return errno;
  }
  if (listen(sd_sv, 5) == -1) 
  {
    perror("[server] Eroare la listen().\n");
    return errno;
  }
  time_t ult_log = time(NULL);
  printf("[server] MonitorAndLogS pornit la portul %d...\n", PORT);
  fflush(stdout);
  while (1) {
    FD_ZERO(&readfds);
    FD_SET(sd_sv, &readfds);
    // un workaround ca sa fie mereu sincronizat cu intervalul
    time_t timp_act = time(NULL);
    int delta_timp = timp_act - ult_log;
    int interval_act = shm->interval;
    if (delta_timp >= interval_act) {
      scrie_log();
      ult_log = timp_act;
      delta_timp = 0;
    }
    tv_wait.tv_sec = 1; 
    tv_wait.tv_usec = 0;

    int rez = select(sd_sv + 1, &readfds, NULL, NULL, &tv_wait);
    if (rez < 0) {
      if (errno == EINTR) {
        continue; // daca cumva se intampla sa fie intrerupt de un semnal, continua
      }
      perror("[server] Eroare la select().\n");
      return errno;
    }
    if (rez == 0) {
      continue;
    }
    // client nou
    if (FD_ISSET(sd_sv, &readfds)) {
      socklen_t len;
      len = sizeof(adr_cl);
      bzero(&adr_cl, sizeof(adr_cl));
      sd_cl = accept(sd_sv, (struct sockaddr *)&adr_cl, &len);
      if (sd_cl < 0) {
        perror("[server] Eroare la accept().\n");
        continue;
      }
      printf("[server] S-a conectat clientul de la adr %s.\n", conv_addr(adr_cl));
      fflush(stdout);
      // concurenta
      int pid = fork(); 
      if (pid == 0) 
      {
        close(sd_sv); // copilul nu mai trebuie sa asculte serverul
        proc_client(sd_cl);
      } 
      else 
      {
        close(sd_cl); // serverul si-a terminat treaba cu clientul
      }
    }
  }
  shm_unlink(SHM_NAME);
  close(sd_sv); // serverul elibereaza socket-ul
  printf("\n[server] Server oprit. Memorie partajata (shm) stearsa.\n");
  return 0;
}
