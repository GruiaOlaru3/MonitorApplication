//Acest proiect folosește biblioteca SFML, aflată sub licența zlib/png, care este compatibilă integral cu GNU GPL (poate fi folosită ca dependență, iar proiectul rămâne în continuare sub GPL; este necesară includerea licenței doar dacă se aduc modificări codului sursă al bibliotecii).

//Din câte am înțeles de la domnul profesor de laborator, este permisă utilizarea componentelor aflate sub licențe compatibile GPL, cum ar fi MIT.

//Dacă totuși utilizarea bibliotecilor externe nu este permisă, am inclus și varianta de client CLI ca alternativă de rezervă.

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <strings.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <deque>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <ctime>

#define MAX_SIZE 16384
#define IST_SIZE 60
#define LATIME 1280.0
#define INALTIME 720.0

const int DIM_FONT_NORMAL = 13;
const int DIM_FONT_LISTA = 10;
const int DIM_FONT_IST = 9;
const int DIM_FONT_MARE = 18;

using namespace std;

string elimina_spatii(const string& str) {
    size_t prim = str.find_first_not_of(' ');
    if (string::npos == prim) return "";
    size_t ultim = str.find_last_not_of(' ');
    return str.substr(prim, (ultim - prim + 1));
}

string obtine_timp() {
    time_t acum = time(0);
    tm* ltm = localtime(&acum);
    char buf[32];
    sprintf(buf, "%02d:%02d:%02d", ltm->tm_hour, ltm->tm_min, ltm->tm_sec);
    return string(buf);
}

string filtreaza_linii(string continut, string interogare) {
    if (interogare.empty()) return continut;
    stringstream ss(continut);
    string linie, rez = "";
    string q_mic = interogare;
    transform(q_mic.begin(), q_mic.end(), q_mic.begin(), ::tolower);

    while(getline(ss, linie)) {
        string l_mic = linie;
        transform(l_mic.begin(), l_mic.end(), l_mic.begin(), ::tolower);

        if (l_mic.find("procese") != string::npos || 
            l_mic.find("pid ") != string::npos || 
            l_mic.find("user ") != string::npos ||
            l_mic.find("usr") != string::npos ||
            l_mic.find("timestamp") != string::npos ||
            l_mic.find("conexiuni") != string::npos ||
            l_mic.find("porturi") != string::npos ||
            l_mic.find("recv-q") != string::npos ||
            l_mic.find("send-q") != string::npos ||
            l_mic.find("local address") != string::npos ||
            l_mic.find("peer address") != string::npos ||
            l_mic.find("address") != string::npos ||
            l_mic.find("state") != string::npos ||
            l_mic.find("----") != string::npos) {
            rez += linie + "\n";
        } else if (l_mic.find(q_mic) != string::npos) {
            rez += linie + "\n";
        }
    }
    return rez;
}

string compact_tabel(string intrare) {
    stringstream ss(intrare);
    string linie;
    vector<vector<string>> randuri;
    vector<size_t> latimi_max;
    string titlu_header = "";
    const size_t LATIME_MAX_COL = 35;

    while(getline(ss, linie)) {
        if (linie.find("----") != string::npos) continue;
        if (linie.find('|') == string::npos) {
            if (elimina_spatii(linie).length() > 0 && titlu_header.empty()) titlu_header = elimina_spatii(linie);
            continue;
        }
        vector<string> rand_act;
        stringstream linie_ss(linie);
        string segment;
        while(getline(linie_ss, segment, '|')) {
            string s = elimina_spatii(segment);
            if (s.length() > LATIME_MAX_COL) s = s.substr(0, LATIME_MAX_COL - 3) + "...";
            rand_act.push_back(s);
        }
        if (latimi_max.size() < rand_act.size()) latimi_max.resize(rand_act.size(), 0);
        for(size_t i = 0; i < rand_act.size(); ++i) {
            if (rand_act[i].length() > latimi_max[i]) latimi_max[i] = rand_act[i].length();
        }
        randuri.push_back(rand_act);
    }

    if (randuri.empty()) return intrare;
    string rez = "";
    if (!titlu_header.empty()) rez += titlu_header + "\n";
    rez += " ";
    for(size_t i = 0; i < latimi_max.size(); ++i) {
        rez += string(latimi_max[i], '-');
        if (i < latimi_max.size() - 1) rez += "-+-";
    }
    rez += "\n";
    for(size_t r = 0; r < randuri.size(); ++r) {
        rez += " ";
        for(size_t c = 0; c < randuri[r].size(); ++c) {
            rez += randuri[r][c];
            if (c < randuri[r].size() - 1) {
                size_t padding = latimi_max[c] - randuri[r][c].length();
                rez += string(padding, ' ') + " | ";
            }
        }
        rez += "\n";
        if (r == 0) {
             rez += " ";
             for(size_t i = 0; i < latimi_max.size(); ++i) {
                rez += string(latimi_max[i], '-');
                if (i < latimi_max.size() - 1) rez += "-+-";
            }
            rez += "\n";
        }
    }
    return rez;
}

struct notificator {
    string mesaj;
    sf::Clock ceas;
    bool activ = false;
    sf::Color culoare_fundal;

    void afiseaza(string msg, bool eroare = false) 
    {
        mesaj = msg;
        activ = true;
        ceas.restart();
        culoare_fundal = eroare ? sf::Color(220, 53, 69, 230) : sf::Color(40, 167, 69, 230);
    }

    void draw(sf::RenderWindow& window, sf::Font& font) 
    {
        if (!activ) return;
        float scurs = ceas.getElapsedTime().asSeconds();
        if (scurs > 4.0f) { activ = false; return; }

        float alpha = 255;
        if (scurs > 3.0f) alpha = 255 * (1.0f - (scurs - 3.0f));

        sf::RectangleShape cutie(sf::Vector2f(300, 60));
        cutie.setPosition(LATIME - 320, INALTIME - 80);
        
        sf::Color c = culoare_fundal;
        c.a = (sf::Uint8)alpha;
        cutie.setFillColor(c);
        cutie.setOutlineThickness(1);
        cutie.setOutlineColor(sf::Color(255,255,255, (sf::Uint8)alpha));

        sf::Text txt;
        txt.setFont(font);
        txt.setString(mesaj);
        txt.setCharacterSize(14);
        txt.setFillColor(sf::Color(255, 255, 255, (sf::Uint8)alpha));
        
        sf::FloatRect b = txt.getLocalBounds();
        txt.setPosition(cutie.getPosition().x + (300 - b.width)/2, cutie.getPosition().y + (60 - b.height)/2 - 5);

        window.draw(cutie);
        window.draw(txt);
    }
} notif_glb;

struct stare_app {
    string inc_medie = "Se incarca...";
    string info_mem = "Se incarca...";
    string info_procese = "Se asteapta date...";
    string usr_conectati = "Niciun usr";
    string conexiuni = "Se asteapta date...";
    string servicii = "Se asteapta date...";
    string interval_act = "--";
    
    string ist_mem_str;
    string ist_proc_str;
    string ist_useri_str;
    string ist_serv_str;
    string ist_lista_str;
    string ist_lista_compact;

    deque<string> jurnal_consola;
    deque<float> ist_cpu;
    deque<float> ist_mem;
    deque<float> ist_cpu_usage;
    deque<float> ist_nr_proc;

    bool auth = false;
    string usr_act = "";

    stare_app() {
        ist_cpu.resize(IST_SIZE, 0.0f);
        ist_mem.resize(IST_SIZE, 0.0f);
        ist_cpu_usage.resize(IST_SIZE, 0.0f);
        ist_nr_proc.resize(IST_SIZE, 0.0f);
    }
    
    void adauga_log_consola(string msg) 
    {
        stringstream ss(msg);
        string linie;
        while(getline(ss, linie)) {
            jurnal_consola.push_back(linie);
        }
        while(jurnal_consola.size() > 200) jurnal_consola.pop_front();
    }

    void exporta_raport()
    {
        ofstream iesire("raport_monitorizare.txt");
        if(iesire.is_open()) {
            iesire << "=== RAPORT MONITORIZARE ===" << endl;
            iesire << "Generat la: " << obtine_timp() << endl << endl;
            iesire << "[Load Average]" << endl << inc_medie << endl << endl;
            iesire << "[Memory Info]" << endl << info_mem << endl << endl;
            iesire << "[Procese Active]" << endl << info_procese << endl << endl;
            iesire << "[useri]" << endl << usr_conectati << endl << endl;
            iesire << "[Conexiuni Retea]" << endl << conexiuni << endl << endl;
            iesire << "[Servicii]" << endl << servicii << endl << endl;
            iesire.close();
         notif_glb.afiseaza("Raport salvat in raport_monitorizare.txt");
        } else {
         notif_glb.afiseaza("Eroare la salvare fisier!", true);
        }
    }
};

stare_app app;
bool ruleaza_app = true;
bool date_active = false;
bool pauza_date = false;

class retea {
    int sd;
    struct sockaddr_in server;
    string adresa_ip;
    int numar_port;

public:
    retea(const string& ip, int port) : adresa_ip(ip), numar_port(port), sd(-1) {
        conecteaza();
    }

    ~retea() {
        if(sd != -1) close(sd);
    }

    bool conecteaza() {
        if (sd != -1) close(sd);

        if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
         notif_glb.afiseaza("Eroare socket!", true);
            return false;
        }
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = inet_addr(adresa_ip.c_str());
        server.sin_port = htons(numar_port);

        if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1) {
         notif_glb.afiseaza("Nu ma pot conecta la server!", true);
            return false;
        }
        
        char temp[MAX_SIZE];
        bzero(temp, MAX_SIZE);
        read(sd, temp, MAX_SIZE);
        return true;
    }

    bool login(string user, string pass, string& raspuns) 
    {
        char buffer[MAX_SIZE];
        string comanda = "login " + user + " " + pass;
        
        trimite_com(comanda.c_str(), buffer);
        raspuns = buffer;
        
        if (raspuns.find("reusita") != string::npos || raspuns.find("deja") != string::npos) {
         notif_glb.afiseaza("Autentificare reusita!");
            return true;
        }
     notif_glb.afiseaza("Autentificare esuata!", true);
        return false;
    }

    void logout() 
    {
        char dummy[MAX_SIZE];
        trimite_com("logout", dummy);
     notif_glb.afiseaza("Deconectat.");
    }

    void trimite_com(const char* cmd, char* dest) 
    {
        if (sd == -1) { 
            strcpy(dest, "Eroare conexiune"); 
            return; 
        }

        if (write(sd, cmd, strlen(cmd)) <= 0) {
             strcpy(dest, "Eroare write");
             notif_glb.afiseaza("Eroare comunicare server (write)!", true);
             return;
        }

        bzero(dest, MAX_SIZE);
        int rez = read(sd, dest, MAX_SIZE);
        if (rez < 0) {
            strcpy(dest, "Eroare read");
         notif_glb.afiseaza("Eroare la read() de la server!", true);
        } else if (rez == 0) {
             strcpy(dest, "Serverul a inchis conexiunea");
             notif_glb.afiseaza("Serverul a inchis conexiunea!", true);
        }
    }

    void trimite_comanda(const char* cmd, char* dest) 
    {
        trimite_com(cmd, dest);
    }

    void actualizeaza_date() 
    {
        static char buffer[MAX_SIZE];
        
        trimite_comanda("get_load_avg", buffer); 
        string load_s = buffer;
        
        trimite_comanda("get_memory", buffer);
        string mem_s = buffer;

        // 1 load avg
        float inc_acta = 0.0f;
        size_t pos = load_s.find("Average: ");
        if (pos != string::npos) inc_acta = atof(load_s.c_str() + pos + 9);

        // 2 ram usage
        float mem_acta = 0.0f;
        pos = mem_s.rfind('('); 
        if (pos != string::npos) mem_acta = atof(mem_s.c_str() + pos + 1);
        
        char t_mem_h[MAX_SIZE], t_proc[MAX_SIZE], t_proc_h[MAX_SIZE];
        char t_users[MAX_SIZE], t_users_h[MAX_SIZE], t_serv[MAX_SIZE];
        char t_conn[MAX_SIZE], t_serv_h[MAX_SIZE], t_list_h[MAX_SIZE], t_int[MAX_SIZE];

        char cmd[64]; sprintf(cmd, "get_sys_hist %d", IST_SIZE); trimite_comanda(cmd, t_mem_h);
        trimite_comanda("get_proc_info 32", t_proc); 
        
        // 4 nr procese active
        float nr_proc_act = 0.0f;
        if (sscanf(t_proc, "Procese active: %f", &nr_proc_act) != 1) nr_proc_act = 0;

        trimite_comanda("get_proc_hist 32", t_proc_h);
        trimite_comanda("get_logged_users", t_users);
        trimite_comanda("get_users_hist 20", t_users_h);
        trimite_comanda("get_services", t_serv);
        trimite_comanda("get_connections", t_conn);
        trimite_comanda("get_list_hist 15", t_list_h);
        trimite_comanda("get_interval", t_int);
        app.inc_medie = load_s;
        app.info_mem = mem_s;
        app.ist_mem_str = t_mem_h;
        app.info_procese = t_proc;
        app.ist_proc_str = t_proc_h;
        app.usr_conectati = t_users;
        app.ist_useri_str = t_users_h;
        app.servicii = t_serv;
        app.conexiuni = t_conn;
        app.ist_serv_str = t_serv_h;
        app.ist_lista_str = t_list_h;
        app.ist_lista_compact = compact_tabel(string(t_list_h));
        app.interval_act = t_int;

        app.ist_cpu.push_back(inc_acta);
        if(app.ist_cpu.size() > IST_SIZE) app.ist_cpu.pop_front();

        if (string(t_mem_h).find("|") != string::npos) {
            stringstream ss_h(t_mem_h);
            string line_h;
            app.ist_mem.clear();
            app.ist_cpu_usage.clear();
            while(getline(ss_h, line_h)) {
                if (line_h.find("Timestamp") != string::npos || line_h.find("----") != string::npos) continue;
                size_t p1 = line_h.find('|');
                if (p1 == string::npos) continue;
                size_t p2 = line_h.find('|', p1 + 1);
                if (p2 == string::npos) {
                    try {
                        string s_m = line_h.substr(p1 + 1);
                        s_m.erase(remove(s_m.begin(), s_m.end(), '%'), s_m.end());
                        app.ist_mem.push_back(stof(s_m));
                        app.ist_cpu_usage.push_back(0.0f); 
                    } catch(...) {}
                } else {
                    try {
                        string s_m = line_h.substr(p1 + 1, p2 - p1 - 1);
                        string s_c = line_h.substr(p2 + 1);
                        s_m.erase(remove(s_m.begin(), s_m.end(), '%'), s_m.end());
                        s_c.erase(remove(s_c.begin(), s_c.end(), '%'), s_c.end());
                        app.ist_mem.push_back(stof(s_m));
                        app.ist_cpu_usage.push_back(stof(s_c));
                    } catch(...) {}
                }
            }
        } else {
             app.ist_cpu_usage.push_back(0); 
             if(app.ist_cpu_usage.size() > IST_SIZE) app.ist_cpu_usage.pop_front();
             app.ist_mem.push_back(mem_acta);
             if(app.ist_mem.size() > IST_SIZE) app.ist_mem.pop_front();
        }

        app.ist_nr_proc.push_back(nr_proc_act);
        if(app.ist_nr_proc.size() > IST_SIZE) app.ist_nr_proc.pop_front();
    }
    
    void modifica_interval_logare(int sec) 
    {
        char cmd[32];
        sprintf(cmd, "set_interval %d", sec);
        char dummy[MAX_SIZE];
        trimite_comanda(cmd, dummy);
     notif_glb.afiseaza("Interval actualizat.");
    }
};

// frontend

sf::RenderWindow window(sf::VideoMode(LATIME, INALTIME), "MonitorAndLogS");
sf::Font font;

class caseta_text {
public:
    sf::RectangleShape cutie;
    string text_brut;
    sf::Text text_afisat;
    bool are_focus;
    double x, y, w, h;
    bool este_parola;
    caseta_text(int _x, int _y, int _w, int _h, bool parola=false) : x(_x), y(_y), w(_w), h(_h), are_focus(false), este_parola(parola) {
        cutie.setPosition(x, y);
        cutie.setSize(sf::Vector2f(w, h));
        cutie.setFillColor(sf::Color(33, 37, 43));
        cutie.setOutlineThickness(2);
        cutie.setOutlineColor(sf::Color(70, 75, 85));
        
        text_afisat.setFont(font);
        text_afisat.setCharacterSize(16);
        text_afisat.setFillColor(sf::Color::White);
        text_afisat.setPosition(x + 5, y + (h/2) - 10);
    }

    void proceseaza_event(sf::Event& ev) {
        if (!are_focus) return;
        if (ev.type == sf::Event::TextEntered) {
            if (ev.text.unicode == '\b') { 
                if (!text_brut.empty()) text_brut.pop_back();
            } else if (ev.text.unicode < 128 && ev.text.unicode > 31) {
                text_brut += static_cast<char>(ev.text.unicode);
            }
        }
    }

    bool verifica_click(sf::Vector2i mouse) {
        are_focus = (mouse.x >= x && mouse.x <= x+w && mouse.y >= y && mouse.y <= y+h);
        if(are_focus) cutie.setOutlineColor(sf::Color(97, 175, 239));
        else cutie.setOutlineColor(sf::Color(70, 75, 85));
        return are_focus;
    }

    void draw() {
        window.draw(cutie);
        if (este_parola) {
            string mascat(text_brut.length(), '*');
            text_afisat.setString(mascat);
        } else {
            text_afisat.setString(text_brut);
        }
        static int f = 0; f++;
        if (are_focus && (f % 60 < 30)) {
            sf::Text cursor = text_afisat;
            cursor.setString(text_afisat.getString() + "|");
            window.draw(cursor);
        } else {
            window.draw(text_afisat);
        }
    }
};

void (*grafica_acta)() = nullptr;
void (*event_act)(sf::Event&) = nullptr;
retea* ref_retea = nullptr;

void grafica_login(); void event_login(sf::Event& ev);
void grafica_panou_control(); void event_panou_control(sf::Event& ev);
void grafica_procese(); void event_procese(sf::Event& ev);
void grafica_retea(); void event_retea(sf::Event& ev);
void grafica_useri(); void event_useri(sf::Event& ev);
void grafica_consola(); void event_consola(sf::Event& ev);
void grafica_setari(); void event_setari(sf::Event& ev);

bool incarca_font() {
    const char* cai[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeMono.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 
        "DejaVuSansMono.ttf",
        "arial.ttf"
    };
    for (const char* cale : cai) {
        if (font.loadFromFile(cale)) return true;
    }
    return false;
}

struct buton_navigare { string text; void (*functie)(); int x; };
void draw_bara_navigare() 
{
    sf::RectangleShape bara(sf::Vector2f(LATIME, 55));
    bara.setFillColor(sf::Color(40, 44, 52));
    window.draw(bara);

    vector<buton_navigare> butoane = {
        {"Dashboard", [](){ grafica_acta = grafica_panou_control; event_act = event_panou_control; }, 10},
        {"Procese", [](){ grafica_acta = grafica_procese; event_act = event_procese; }, 120},
        {"Retea", [](){ grafica_acta = grafica_retea; event_act = event_retea; }, 230},
        {"Useri", [](){ grafica_acta = grafica_useri; event_act = event_useri; }, 340},
        {"Consola", [](){ grafica_acta = grafica_consola; event_act = event_consola; }, 450},
        {"Config", [](){ grafica_acta = grafica_setari; event_act = event_setari; }, 560}
    };

    sf::Vector2i mouse = sf::Mouse::getPosition(window);
    sf::Vector2f m_v = window.mapPixelToCoords(mouse);

    for(auto& b : butoane) {
        sf::RectangleShape cutie(sf::Vector2f(100, 30));
        cutie.setPosition(b.x, 12);
        bool activ = false;
        if(b.text == "Dashboard" && grafica_acta == grafica_panou_control) activ = true;
        if(b.text == "Procese" && grafica_acta == grafica_procese) activ = true;
        if(b.text == "Retea" && grafica_acta == grafica_retea) activ = true;
        if(b.text == "Useri" && grafica_acta == grafica_useri) activ = true;
        if(b.text == "Consola" && grafica_acta == grafica_consola) activ = true;
        if(b.text == "Config" && grafica_acta == grafica_setari) activ = true;

        if(activ) cutie.setFillColor(sf::Color(97, 175, 239));
        else if(m_v.x >= b.x && m_v.x <= b.x+100 && m_v.y >= 12 && m_v.y <= 42) cutie.setFillColor(sf::Color(70, 75, 85));
        else cutie.setFillColor(sf::Color(50, 54, 62));
        
        window.draw(cutie);
        
        sf::Text t; t.setFont(font); t.setString(b.text); t.setCharacterSize(14);
        sf::FloatRect limite = t.getLocalBounds();
        t.setOrigin((int)(limite.left + limite.width/2.0f), (int)(limite.top + limite.height/2.0f));
        t.setPosition((int)(b.x + 50), (int)(12 + 15));
        window.draw(t);
    }

    sf::RectangleShape b_p(sf::Vector2f(80, 30));
    b_p.setPosition(1080, 12);
    if(pauza_date) b_p.setFillColor(sf::Color(200, 100, 0)); // portocaliu cand e pauza
    else b_p.setFillColor(sf::Color(0, 150, 0)); // verde cand ruleaza
    window.draw(b_p);
    
    sf::Text tp; tp.setFont(font); 
    tp.setString(pauza_date ? "Reluare" : "Pauza"); tp.setCharacterSize(12);
    sf::FloatRect pb = tp.getLocalBounds();
    tp.setOrigin((int)(pb.left + pb.width/2.0f), (int)(pb.top + pb.height/2.0f));
    tp.setPosition(1120, 27);
    window.draw(tp);

    sf::RectangleShape b_q(sf::Vector2f(80, 30));
    b_q.setPosition(1170, 12); b_q.setFillColor(sf::Color(224, 108, 117));
    window.draw(b_q);
    sf::Text tq; tq.setFont(font); tq.setString("Quit"); tq.setCharacterSize(12);
    sf::FloatRect qb = tq.getLocalBounds();
    tq.setOrigin((int)(qb.left + qb.width/2.0f), (int)(qb.top + qb.height/2.0f));
    tq.setPosition(1210, 27);
    window.draw(tq);
}

void gestioneaza_navigare(sf::Vector2i m) 
{
    if (m.y < 12 || m.y > 42) return;
    if (m.x >= 10 && m.x <= 110) { grafica_acta = grafica_panou_control; event_act = event_panou_control; }
    if (m.x >= 120 && m.x <= 220) { grafica_acta = grafica_procese; event_act = event_procese; }
    if (m.x >= 230 && m.x <= 330) { grafica_acta = grafica_retea; event_act = event_retea; }
    if (m.x >= 340 && m.x <= 440) { grafica_acta = grafica_useri; event_act = event_useri; }
    if (m.x >= 450 && m.x <= 550) { grafica_acta = grafica_consola; event_act = event_consola; }
    if (m.x >= 560 && m.x <= 660) { grafica_acta = grafica_setari; event_act = event_setari; }
    if (m.x >= 1080 && m.x <= 1160) { pauza_date = !pauza_date; }
    if (m.x >= 1170 && m.x <= 1250) { ruleaza_app = false; window.close(); }
}

sf::Vector2i obtine_mouse_mapat() {
    return sf::Vector2i(window.mapPixelToCoords(sf::Mouse::getPosition(window)));
}

void draw_lista_colorata(float x, float y, float w, float h, string titlu, string continut, bool trunchiaza = true, int dimensiune_font = DIM_FONT_NORMAL) {

    sf::RectangleShape fundal(sf::Vector2f(w, h));
    fundal.setPosition(x, y);
    fundal.setFillColor(sf::Color(33, 37, 43));
    fundal.setOutlineThickness(1);
    fundal.setOutlineColor(sf::Color(60, 64, 72));
    window.draw(fundal);

    sf::RectangleShape header(sf::Vector2f(w, 25));
    header.setPosition(x, y);
    header.setFillColor(sf::Color(40, 44, 52));
    window.draw(header);

    sf::Text t; t.setFont(font); t.setString(titlu); t.setCharacterSize(14); t.setStyle(sf::Text::Bold);
    t.setFillColor(sf::Color(152, 195, 121)); t.setPosition(x + 10, y + 4);
    window.draw(t);

    sf::Text linie_text;
    linie_text.setFont(font);
    linie_text.setCharacterSize(dimensiune_font);
    
    int inaltime_linie = dimensiune_font + 4;
    int max_linii = (h - 35) / inaltime_linie;
    int linie_acta = 0;
    
    stringstream ss(continut);
    string temp;
    float y_act = y + 35;

    while(getline(ss, temp)) {
        if (trunchiaza && linie_acta >= max_linii) break;
        
        if (temp.find("PID") != string::npos || temp.find("Load") != string::npos || temp.find("Timestamp") != string::npos) {
            linie_text.setFillColor(sf::Color(97, 175, 239));
        } 
        else if (temp.find("root") != string::npos) {
            linie_text.setFillColor(sf::Color(224, 108, 117));
        }
        else if (temp.find("---") != string::npos) {
            linie_text.setFillColor(sf::Color(100, 100, 100)); // gri inchis pt separator
        }
        else {
            linie_text.setFillColor(sf::Color(171, 178, 191));
        }

        linie_text.setString(temp);
        linie_text.setPosition(x + 10, y_act);
        window.draw(linie_text);
        
        y_act += inaltime_linie;
        linie_acta++;
    }
}

void draw_grafic_liniar(float x, float y, float w, float h, deque<float>& date, float val_max, sf::Color culoare, string titlu) 
{
    sf::RectangleShape fundal(sf::Vector2f(w, h)); fundal.setPosition(x, y); fundal.setFillColor(sf::Color(33, 37, 43));
    fundal.setOutlineColor(sf::Color(60, 64, 72)); fundal.setOutlineThickness(1);
    window.draw(fundal);

    sf::Text t; t.setFont(font); t.setString(titlu); t.setCharacterSize(14); t.setFillColor(culoare); t.setPosition(x + 10, y + 5);
    window.draw(t);

    for(int i=1; i<4; i++) {
        float h_linie = y + h - (i * h / 4.0f);
        sf::RectangleShape linie(sf::Vector2f(w, 1)); linie.setPosition(x, h_linie); linie.setFillColor(sf::Color(50, 50, 50));
        window.draw(linie);
    }
    if (date.empty()) return;

    sf::VertexArray arie(sf::TrianglesStrip, date.size() * 2);
    sf::VertexArray linii(sf::LinesStrip, date.size());
    
    float pas_x = (w - 20) / (IST_SIZE - 1); if (pas_x < 1) pas_x = 1;

    for (size_t i = 0; i < date.size(); ++i) {
        float val = date[i]; if (val > val_max) val = val_max;
        
        float px = x + 10 + i * pas_x;
        float py = (y + h - 10) - (val / val_max) * (h - 40);
        
        linii[i].position = sf::Vector2f(px, py);
        linii[i].color = culoare;

        arie[2*i].position = sf::Vector2f(px, py);
        arie[2*i].color = sf::Color(culoare.r, culoare.g, culoare.b, 100); // semi-transparent
        
        arie[2*i+1].position = sf::Vector2f(px, y + h - 10);
        arie[2*i+1].color = sf::Color(culoare.r, culoare.g, culoare.b, 10); // foarte transparent
    }
    window.draw(arie);
    window.draw(linii);

    sf::Text tval; tval.setFont(font);
    stringstream ss; ss << fixed << setprecision(1) << date.back();
    tval.setString(ss.str()); tval.setCharacterSize(20); tval.setFillColor(sf::Color::White);
    tval.setOrigin(tval.getLocalBounds().width, 0); tval.setPosition(x + w - 10, y + 5);
    window.draw(tval);
}

struct buton {
    int x, y, w, h;
    string text;
    sf::Color culoare;
    void draw() {
        sf::RectangleShape s(sf::Vector2f(w, h)); s.setPosition(x, y);
        sf::Vector2i mp = sf::Mouse::getPosition(window); sf::Vector2f mv = window.mapPixelToCoords(mp);
        if (mv.x >= x && mv.x <= x+w && mv.y >= y && mv.y <= y+h)
            s.setFillColor(sf::Color(min(255, culoare.r+30), min(255, culoare.g+30), min(255, culoare.b+30)));
        else s.setFillColor(culoare);
        window.draw(s);
        sf::Text t; t.setFont(font); t.setString(text); t.setCharacterSize(14);
        sf::FloatRect limite = t.getLocalBounds();
        t.setOrigin((int)(limite.left + limite.width/2.0f), (int)(limite.top + limite.height/2.0f));
        t.setPosition((int)(x + w/2.0f), (int)(y + h/2.0f));
        window.draw(t);
    }
    bool este_apasat(sf::Vector2i m) { return (m.x >= x && m.x <= x+w && m.y >= y && m.y <= y+h); }
};

caseta_text input_user(540, 300, 200, 30);
caseta_text input_parola(540, 350, 200, 30, true);
buton buton_login{540, 400, 200, 40, "Autentificare", sf::Color(97, 175, 239)};
string status_login = "Introduceti datele de autentificare.";

void grafica_login() 
{
    window.clear(sf::Color(33, 37, 43));
    sf::Text titlu; titlu.setFont(font); titlu.setString("MonitorAndLogS - Login");
    titlu.setCharacterSize(24); titlu.setPosition(480, 200); window.draw(titlu);
    sf::Text lbl_u; lbl_u.setFont(font); lbl_u.setString("User:"); lbl_u.setPosition(490, 305); lbl_u.setCharacterSize(16); window.draw(lbl_u);
    sf::Text lbl_p; lbl_p.setFont(font); lbl_p.setString("Pass:"); lbl_p.setPosition(490, 355); lbl_p.setCharacterSize(16); window.draw(lbl_p);
    input_user.draw(); input_parola.draw(); buton_login.draw();
    sf::Text st; st.setFont(font); st.setString(status_login); st.setPosition(540, 450); 
    st.setCharacterSize(14); st.setFillColor(sf::Color(224, 108, 117)); window.draw(st);
 notif_glb.draw(window, font);
}

void event_login(sf::Event& ev) 
{
    input_user.proceseaza_event(ev); input_parola.proceseaza_event(ev);
    if (ev.type == sf::Event::MouseButtonPressed && ev.mouseButton.button == sf::Mouse::Left) {
        sf::Vector2i m = obtine_mouse_mapat();
        input_user.verifica_click(m); input_parola.verifica_click(m);
        if (buton_login.este_apasat(m)) {
            string rasp; status_login = "Se conecteaza..."; grafica_login(); window.display();
            if (ref_retea->login(input_user.text_brut, input_parola.text_brut, rasp)) {
                app.auth = true; app.usr_act = input_user.text_brut; 
                grafica_acta = grafica_panou_control; event_act = event_panou_control;
            } else { status_login = "Eroare: " + rasp; }
        }
    }
}

buton buton_export{980, 12, 90, 30, "Export", sf::Color(150, 100, 200)};

void grafica_panou_control() 
{
    window.clear(sf::Color(40, 44, 52));
    draw_bara_navigare();
    // graf rand 1
    draw_grafic_liniar(50, 60, 580, 180, app.ist_cpu, 4.0f, sf::Color(224, 108, 117), "CPU Load (Avg)");
    draw_grafic_liniar(650, 60, 580, 180, app.ist_mem, 100.0f, sf::Color(97, 175, 239), "RAM Usage (%)");
    // graf rand 2
    draw_grafic_liniar(50, 250, 580, 180, app.ist_cpu_usage, 100.0f, sf::Color(229, 192, 123), "CPU Usage (%)");
    draw_grafic_liniar(650, 250, 580, 180, app.ist_nr_proc, 200.0f, sf::Color(152, 195, 121), "Active Processes (#)");
    // liste
    draw_lista_colorata(50, 440, 580, 270, "Top Procese CPU", app.info_procese, true, DIM_FONT_LISTA);
    draw_lista_colorata(650, 440, 580, 270, "Informatii Sistem", app.info_mem + "\n\n" + app.inc_medie + "\n" + app.interval_act, true, 12);
    buton_export.draw();
 notif_glb.draw(window, font);
}

void event_panou_control(sf::Event& ev) 
{ 
    if (ev.type == sf::Event::MouseButtonPressed) {
        sf::Vector2i m = obtine_mouse_mapat();
        gestioneaza_navigare(m);
        if (buton_export.este_apasat(m)) {
            app.exporta_raport();
        }
    }
}

caseta_text input_cautare_proc(50, 70, 330, 30);
void grafica_procese() 
{
    window.clear(sf::Color(40, 44, 52)); draw_bara_navigare();
    input_cautare_proc.draw();
    if(input_cautare_proc.text_brut.empty() && !input_cautare_proc.are_focus) {
        sf::Text ph; ph.setFont(font); ph.setString("Cauta procese..."); ph.setCharacterSize(14); 
        ph.setFillColor(sf::Color(100, 100, 100)); ph.setPosition(55, 75); window.draw(ph);
    }
    string filtrare = filtreaza_linii(app.info_procese, input_cautare_proc.text_brut);
    draw_lista_colorata(50, 110, 330, 590, "Lista Procese", filtrare, true, 12);
    draw_lista_colorata(400, 70, 870, 630, "Istoric Load/Procese", app.ist_proc_str, true, 11);
 notif_glb.draw(window, font);
}
void event_procese(sf::Event& ev) 
{
    input_cautare_proc.proceseaza_event(ev);
    if (ev.type == sf::Event::MouseButtonPressed) {
        sf::Vector2i m = obtine_mouse_mapat(); gestioneaza_navigare(m); input_cautare_proc.verifica_click(m);
    }
}

caseta_text input_cautare_retea(50, 70, 610, 30);
void grafica_retea()
{
    window.clear(sf::Color(40, 44, 52)); draw_bara_navigare();
    input_cautare_retea.draw();
    if(input_cautare_retea.text_brut.empty() && !input_cautare_retea.are_focus) {
        sf::Text ph; ph.setFont(font); ph.setString("Cauta port/conexiune..."); ph.setCharacterSize(14); 
        ph.setFillColor(sf::Color(100, 100, 100)); ph.setPosition(55, 75); window.draw(ph);
    }

    string s_filt = filtreaza_linii(app.servicii, input_cautare_retea.text_brut);
    string c_filt = filtreaza_linii(app.conexiuni, input_cautare_retea.text_brut);
    draw_lista_colorata(50, 110, 610, 260, "Porturi Deschise", s_filt, true, DIM_FONT_LISTA);
    draw_lista_colorata(670, 70, 590, 300, "Conexiuni Active", c_filt, true, DIM_FONT_LISTA);
    draw_lista_colorata(50, 390, 1210, 300, "Istoric Activitate Retea", app.ist_lista_compact, false, 9);
 notif_glb.draw(window, font);
}
void event_retea(sf::Event& ev) 
{
    input_cautare_retea.proceseaza_event(ev);
    if (ev.type == sf::Event::MouseButtonPressed) {
        sf::Vector2i m = obtine_mouse_mapat(); gestioneaza_navigare(m); input_cautare_retea.verifica_click(m);
    }
}

void grafica_useri() 
{
    window.clear(sf::Color(40, 44, 52)); draw_bara_navigare();
    draw_lista_colorata(240, 70, 800, 250, "Useri Conectati", app.usr_conectati, true, DIM_FONT_MARE);
    draw_lista_colorata(240, 340, 800, 350, "Istoric Conectari", app.ist_useri_str, true, 14);
 notif_glb.draw(window, font);
}
void event_useri(sf::Event& ev) { if (ev.type == sf::Event::MouseButtonPressed) gestioneaza_navigare(obtine_mouse_mapat()); }

caseta_text input_consola(50, 650, 1050, 30);
buton buton_trimite{1120, 650, 100, 30, "Trimite", sf::Color(152, 195, 121)};
int scroll_consola = 0; const int MAX_LINII_VIZIBILE = 30;

void grafica_consola() 
{
    window.clear(sf::Color(40, 44, 52)); draw_bara_navigare();
    sf::RectangleShape fundal_log(sf::Vector2f(1180, 560)); fundal_log.setPosition(50, 70);
    fundal_log.setFillColor(sf::Color(33, 37, 43)); fundal_log.setOutlineThickness(1); fundal_log.setOutlineColor(sf::Color(60,64,72));
    window.draw(fundal_log);
    sf::Text t; t.setFont(font); t.setCharacterSize(14); t.setFillColor(sf::Color(171, 178, 191));
    float y = 80;
    int total_linii = app.jurnal_consola.size();
    int max_scroll = std::max(0, total_linii - MAX_LINII_VIZIBILE);
    scroll_consola = std::clamp(scroll_consola, 0, max_scroll);
    int start_index = std::max(0, total_linii - MAX_LINII_VIZIBILE - scroll_consola);
    int count = 0;
    for (int i = start_index; i < total_linii && count < MAX_LINII_VIZIBILE; ++i) {
        t.setString(app.jurnal_consola[i]); t.setPosition(60, y); window.draw(t); y += 18; count++;
    }
    input_consola.draw(); buton_trimite.draw(); notif_glb.draw(window, font);
}

void stats() 
{
    app.adauga_log_consola("\n=== STATISTICI ===");
    char rasp[MAX_SIZE];
    app.adauga_log_consola("[mem]"); ref_retea->trimite_comanda("get_sys_hist 5", rasp); app.adauga_log_consola(string(rasp));
    app.adauga_log_consola("[useri]"); ref_retea->trimite_comanda("get_users_hist 20", rasp); app.adauga_log_consola(string(rasp));
    app.adauga_log_consola("[servicii]"); ref_retea->trimite_comanda("get_serv_hist 5", rasp); app.adauga_log_consola(string(rasp));
    app.adauga_log_consola("==================");
}
void help() 
{
    app.adauga_log_consola("\nComenzi disponibile:\n");
    app.adauga_log_consola("  Informatii live:\n");
    app.adauga_log_consola("    get_load_avg       - load average sistem\n");
    app.adauga_log_consola("    get_proc_info [n]  - numar procese active (optional: top n)\n");
    app.adauga_log_consola("    get_logged_users   - useri conectati\n");
    app.adauga_log_consola("    get_memory         - utilizare mem_acta\n");
    app.adauga_log_consola("    get_services       - porturi in LISTEN\n");
    app.adauga_log_consola("    get_connections    - conexiuni active\n");
    app.adauga_log_consola("\n  Statistici (<count> nr. inreg, default 10, <data> filtru dupa ziua sau intervalul orar):\n");
    app.adauga_log_consola("    get_sys_hist <count>/<data>        - ist utilizare mem si cpu\n");
    app.adauga_log_consola("    get_users_hist <count>/<data>         - statistici conectari per user\n");
    app.adauga_log_consola("    get_serv_hist <count/port>/<data> cnt/port - conexiuni per port/serviciu\n");
    app.adauga_log_consola("    get_list_hist <count>/<data>          - ist porturi deschise (LISTEN)\n");
    app.adauga_log_consola("    get_proc_hist <count>/<data>          - ist top consumatori CPU\n");
    app.adauga_log_consola("\n  Configurare:\n");
    app.adauga_log_consola("    set_interval <sec> - schimba intervalul de logare\n");
    app.adauga_log_consola("    get_interval       - afiseaza intervalul_act\n");
    app.adauga_log_consola("\n  Altele:\n");
    app.adauga_log_consola("    help               - afiseaza acest mesaj\n");
    app.adauga_log_consola("    stats              - afiseaza toate statisticile\n");
    app.adauga_log_consola("    quit               - inchide conexiunea\n\n");
}

void proceseaza_comanda_consola(string cmd) 
{
    if (cmd.length() == 0) return;
    { app.adauga_log_consola("> " + cmd); }
    if (cmd == "stats") stats();
    else if (cmd == "help") help();
    else { char rasp[MAX_SIZE]; ref_retea->trimite_comanda(cmd.c_str(), rasp); app.adauga_log_consola(string(rasp)); }
}

void event_consola(sf::Event& ev) 
{
    if (ev.type == sf::Event::MouseButtonPressed) {
        sf::Vector2i m = obtine_mouse_mapat(); gestioneaza_navigare(m); input_consola.verifica_click(m);
        if (buton_trimite.este_apasat(m)) { string cmd = input_consola.text_brut; proceseaza_comanda_consola(cmd); input_consola.text_brut = ""; scroll_consola = 0; }
    }
    if (ev.type == sf::Event::MouseWheelScrolled) { 
        if (ev.mouseWheelScroll.delta > 0) { int ms = std::max(0, (int)app.jurnal_consola.size()-MAX_LINII_VIZIBILE); if(scroll_consola<ms) scroll_consola++; }
        else if (scroll_consola > 0) scroll_consola--;
    }
    input_consola.proceseaza_event(ev);
    if (ev.type == sf::Event::KeyPressed && ev.key.code == sf::Keyboard::Enter && input_consola.are_focus) {
        string cmd = input_consola.text_brut; proceseaza_comanda_consola(cmd); input_consola.text_brut = ""; scroll_consola = 0;
    }
}

buton buton_set_5{470, 250, 100, 40, "5 sec", sf::Color(0, 100, 50)};
buton buton_set_10{590, 250, 100, 40, "10 sec", sf::Color(150, 100, 0)};
buton buton_set_60{710, 250, 100, 40, "60 sec", sf::Color(100, 0, 0)};

caseta_text input_interval_custom(470, 320, 230, 30);
buton buton_set_custom{710, 320, 100, 30, "Schimba", sf::Color(97, 175, 239)};

buton buton_logout{540, 500, 200, 40, "Logout", sf::Color(224, 108, 117)};

void grafica_setari() 
{
    window.clear(sf::Color(40, 44, 52)); draw_bara_navigare();
    sf::Text t; t.setFont(font); t.setString("Setari"); t.setCharacterSize(24);
    sf::FloatRect tb = t.getLocalBounds();
    t.setPosition(LATIME/2 - tb.width/2, 100); 
    t.setFillColor(sf::Color::White); window.draw(t);
    sf::RectangleShape panel(sf::Vector2f(400, 60));
    panel.setPosition((LATIME - 400)/2, 150); panel.setFillColor(sf::Color(33, 37, 43));
    panel.setOutlineThickness(1); panel.setOutlineColor(sf::Color(60, 64, 72));
    window.draw(panel);
    sf::Text s; s.setFont(font); s.setString(app.interval_act);
    s.setCharacterSize(16); s.setFillColor(sf::Color(152, 195, 121));
    sf::FloatRect sb = s.getLocalBounds();
    s.setPosition((LATIME - 400)/2 + (400 - sb.width)/2, 150 + (60 - sb.height)/2 - 5); 
    window.draw(s);
    sf::Text l1; l1.setFont(font); l1.setString("Modifica intervalul de logare:"); 
    l1.setCharacterSize(14); 
    sf::FloatRect l1b = l1.getLocalBounds();
    l1.setPosition(LATIME/2 - l1b.width/2, 225); 
    l1.setFillColor(sf::Color(171, 178, 191));
    window.draw(l1);
    buton_set_5.draw(); buton_set_10.draw(); buton_set_60.draw();
    input_interval_custom.draw(); buton_set_custom.draw();
    if(input_interval_custom.text_brut.empty() && !input_interval_custom.are_focus) {
        sf::Text ph; ph.setFont(font); ph.setString("ex: 15"); ph.setCharacterSize(14); 
        ph.setFillColor(sf::Color(100, 100, 100)); 
        ph.setPosition(input_interval_custom.x + 10, input_interval_custom.y + 5); 
        window.draw(ph);
    }
    buton_logout.draw();
 notif_glb.draw(window, font);
}

void event_setari(sf::Event& ev) 
{
    input_interval_custom.proceseaza_event(ev);
    if (ev.type == sf::Event::MouseButtonPressed) {
        sf::Vector2i m = obtine_mouse_mapat(); gestioneaza_navigare(m);
        input_interval_custom.verifica_click(m);
        
        if(buton_set_5.este_apasat(m)) ref_retea->modifica_interval_logare(5);
        if(buton_set_10.este_apasat(m)) ref_retea->modifica_interval_logare(10);
        if(buton_set_60.este_apasat(m)) ref_retea->modifica_interval_logare(60);
        
        if(buton_set_custom.este_apasat(m)) {
            if(!input_interval_custom.text_brut.empty()) {
                int val = atoi(input_interval_custom.text_brut.c_str());
                if (val > 0) ref_retea->modifica_interval_logare(val);
                else notif_glb.afiseaza("Introdu un numar valid!", true);
            }
        }

        if(buton_logout.este_apasat(m)) { ref_retea->logout(); app.auth = false; grafica_acta = grafica_login; event_act = event_login; input_parola.text_brut = ""; }
    }
}

int main(int argc, char* argv[]) 
{
    if(argc != 3) { cout << "Utilizare: " << argv[0] << " <ip> <port>" << endl; return 1; }
    if (!incarca_font()) { cerr << "Eroare font!" << endl; return 1; }
    retea manager(argv[1], atoi(argv[2]));
    ref_retea = &manager;
    sf::View view(sf::FloatRect(0, 0, LATIME, INALTIME));
    window.setView(view);
    grafica_acta = grafica_login; event_act = event_login;
    window.setFramerateLimit(60);
    sf::Clock ceas_update;
    while (window.isOpen() && ruleaza_app) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) { ruleaza_app = false; window.close(); }
            if (event.type == sf::Event::Resized) window.setView(sf::View(sf::FloatRect(0, 0, LATIME, INALTIME)));
            if (event_act) (*event_act)(event);
        }
        // actualizarea de date are loc o data pe secunda si doar daca e auth
        if (app.auth && !pauza_date && ceas_update.getElapsedTime().asSeconds() >= 1.0f) {
            manager.actualizeaza_date();
            ceas_update.restart();
        }

        if (grafica_acta) (*grafica_acta)();
        window.display();
    }
    return 0;
}
