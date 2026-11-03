#include <stdio.h>
#include <iostream>
#include <string>
#include <pthread.h>
#include <signal.h> //ctr+c
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>
#include <vector>
#include <atomic>
#include <algorithm>
#include <semaphore.h>
#include <fstream>
using namespace std;

// definicion de hilos 
#define perros 12
#define pumas 1
#define tablero 28   // 25 de la grilla + trono + 2 nodos del techo
#define trono 25
#define techo_i 26
#define techo_d 27
#define cap_ganar 6
// difinicion de estados de cassillas

enum estados_casillas {VACIA, PERRO, PUMA};

struct casilla{
    estados_casillas estado;
    pthread_mutex_t mutex;
    int idpie;
};

//variable globales (algunos le puse valores para probarlo sin el config aunque cuando ponga el config los valores correspondientes lo haran )

int total_jug=0;
int total_mesas=0;
atomic<int> servidor_act(1); //para lo de ctr+c (atomic: la leen todos los hilos y la escribe el handler)
struct saltos{
    int cas_med; // casilla saltada (donde hay un perro)
    int cas_des; // casilla destino
};
vector<int> ady[tablero];
vector <saltos>saltospum[tablero];
unsigned semi;

int tiem_in=50;     // retardo mínimo (ms) por defecto según el enunciado
int tiem_max=300;    // retardo máximo (ms) por defecto según el enunciado
int pos_puma=12;
vector<int> pos_perros = {10,14,15,16,17,18,19,20,21,22,23,24}; // ver esto toy con duda lo voy a probar idea rara

//parametros de config txt 

int lim_mov=600;
int prob_abn=0;
int llega_max=300;
int tab_ms=2000; // para impirmir el tablero 


struct Mesa;   // declaración adelantada
struct jugador{

int id;
int rol;
bool fallo;
unsigned sem;
Mesa *mesa;
int part;
sem_t emparejado;
sem_t fin;
pthread_t hilo;
bool lanzado;




};



struct Mesa{
int id;
int ocup;
int partida;

atomic<int> juegosAct;
atomic<int> perros_cap;
atomic<int> mov;
atomic<int> conflic;
atomic<int> aband;
casilla tablero_casillas[tablero];

pthread_mutex_t estmax;
jugador *jpuma;
jugador *jperro;
pthread_t hilo_juez;
bool estJuez;

};


// reglas de los hilos 

struct parametros{
    int id;
    Mesa *mesa_utl;
    int cassact;
    unsigned sem_local;
};

Mesa * mesas=NULL;
jugador *jugs=NULL;
sem_t ctr_mesas;

vector<jugador*> cola;
int llega;
bool cola_cerr=false;

pthread_mutex_t mtx_cola;
pthread_cond_t cond_cola;
pthread_mutex_t mtx_mesas;

pthread_mutex_t imprime;
pthread_mutex_t stat;
int cont_partida=0;
int st_partidas=0, st_puma=0, st_perros=0, st_empates=0, st_aband=0, st_ctrlc=0;
int st_activas=0, st_max_activas=0;
int st_movs=0, st_conflictos=0;



//utilidades

void imprimir(string mensaje){
pthread_mutex_lock(&imprime);
cout << mensaje << endl;
pthread_mutex_unlock(&imprime);

}

string nomb_casilla(int a){

if(a==trono) return "trono";
if(a==techo_i) return "techo izq";
if(a==techo_d) return "techo der";
return "casilla" + to_string(a/5) + ","+ to_string(a%5);

}

void esperar_sem(sem_t *s){

while(sem_wait(s)==-1 && errno==EINTR){}


}

void plazo_ms(timespec *ts, int ms){
    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec += ms/1000;
    ts->tv_nsec += (long)(ms%1000)*1000000L;
    if(ts->tv_nsec>=1000000000L){
        ts->tv_sec++;
        ts->tv_nsec-=1000000000L;
    }
}


int aleatorio(unsigned *s, int lo, int hi){
    if(hi<=lo) return lo;
    return lo+(int)(rand_r(s)%(unsigned)(hi-lo+1));
}

// formato del tablero requerido del enunciado


 int fila_de(int a){ //tablero: grilla 0..24 (fila*5+col), trono=25, techo_i=26, techo_d=27
    if(a==trono) return -2;
    if(a==techo_i || a==techo_d) return -1;
    return a/5;
}
int col_de(int a){ // coordenadas (fila,col): trono(-2,2) techo_i(-1,1) techo_d(-1,3)

    if(a==trono) return 2;
    if(a==techo_i) return 1;
    if(a==techo_d) return 3;
    return a%5;
}
int mcd(int a, int b){
    a=abs(a); b=abs(b);
    while(b){ int t=a%b; a=b; b=t; }
    return a;
}

void arist(int a, int b){

    if(find(ady[a].begin(), ady[a].end(),b) == ady[a].end()){

        ady[a].push_back(b);
        ady[b].push_back(a);
    }

}
void tablero_adys(){
    // grilla 5x5: solo horizontales y verticales (la figura no tiene diagonales)
    for(int f=0;f<5;f++)
        for(int c=0;c<5;c++){
            if(c<4) arist(f*5+c, f*5+c+1);
            if(f<4) arist(f*5+c, (f+1)*5+c);
        }
    // techo y trono
    arist(techo_i,0);
    arist(techo_i,1);
    arist(techo_d,3);
    arist(techo_d,4);
    arist(techo_i,techo_d);
    arist(trono,techo_i);
    arist(trono,techo_d);
    arist(trono,2);

    // saltos del puma: a -> b -> c en linea recta (misma direccion en ambos tramos)
    for(int a=0;a<tablero;a++){
        for(size_t k=0;k<ady[a].size();k++){
            int b=ady[a][k];
            int d1f=fila_de(b)-fila_de(a), d1c=col_de(b)-col_de(a);
            int g1=mcd(d1f,d1c); d1f/=g1; d1c/=g1;
            for(size_t m=0;m<ady[b].size();m++){
                int c=ady[b][m];
                if(c==a) continue;
                int d2f=fila_de(c)-fila_de(b), d2c=col_de(c)-col_de(b);
                int g2=mcd(d2f,d2c); d2f/=g2; d2c/=g2;
                if(d1f==d2f && d1c==d2c){
                    saltos s={b,c};
                    saltospum[a].push_back(s);
                }
            }
        }
    }
}

bool mov_val(int in, int des, int med){

    if(med<0) return find(ady[in].begin(),ady[in].end(),des) != ady[in].end();
    
    for(size_t i=0; i<saltospum[in].size(); i++) {
        if(saltospum[in][i].cas_med==med && saltospum[in][i].cas_des==des) return true;

    }
    return false;
}




void tiempo_de_espera(unsigned *d){
     int rango= tiem_max-tiem_in+1;

int milseg=tiem_in+(int)(rand_r(d)%(unsigned)rango);

usleep(milseg*1000);

}

void *actperro(void *arg){

    parametros *param= (parametros*) arg;
    int id= param->id;
    Mesa *mesa= param->mesa_utl;

    while(mesa->juegosAct==1 && servidor_act==1){

    tiempo_de_espera(&param->sem_local);
    if(mesa->juegosAct!=1 || servidor_act!=1) break;
    int act=param->cassact;
    int opc=ady[act].size();
       if (opc==0){
       
        continue;

    }
    
    int dest=ady[act][rand_r(&param->sem_local)%opc];
    if(!mov_val(act,dest,-1)) continue;

    
    int lock1= (act <dest) ? act:dest;
    int lock2= (act <dest) ? dest:act;


        pthread_mutex_lock(&mesa->tablero_casillas[lock1].mutex);
        pthread_mutex_lock(&mesa->tablero_casillas[lock2].mutex);

    
        if(mesa->tablero_casillas[act].estado != PERRO || mesa->tablero_casillas[act].idpie != id){
    pthread_mutex_unlock(&mesa->tablero_casillas[lock2].mutex);
    pthread_mutex_unlock(&mesa->tablero_casillas[lock1].mutex);
        break; 
        }

bool mov=false;

        if(mesa->tablero_casillas[dest].estado == VACIA){
            mesa->tablero_casillas[act].estado=VACIA;
            mesa->tablero_casillas[dest].estado=PERRO;
            mesa->tablero_casillas[dest].idpie=id;
            param->cassact=dest;
            mesa->mov++;
            mov=true;


        }
        else {
            mesa->conflic++;

        }
        
         pthread_mutex_unlock(&mesa->tablero_casillas[lock2].mutex);
        pthread_mutex_unlock(&mesa->tablero_casillas[lock1].mutex);

if (mov){
 imprimir("mesa"+to_string(mesa->id)+ " perro se movio "+ to_string(id) + " : "+ nomb_casilla(act)+" -> "+ nomb_casilla(dest));

}


    }

    
 
    return NULL; 

}

//logica del puma

void *actpuma(void *arg){
parametros *prmt=(parametros*) arg;
Mesa *mesa= prmt->mesa_utl;

 while(mesa->juegosAct==1 && servidor_act==1){
    tiempo_de_espera(&prmt->sem_local);
    if(mesa->juegosAct!=1 || servidor_act!=1) break;


    int act= prmt->cassact;
    bool comperro=false;

    int cantsal= saltospum[act].size();

    for(int i=0; i<cantsal; i++){

        int med= saltospum[act][i].cas_med;
        int des=saltospum[act][i].cas_des;

        int c1=act, c2=med, c3=des;

        if(c1>c2)swap(c1,c2);
        if(c2>c3)swap(c2,c3);
        if(c1>c2)swap(c1,c2);

        pthread_mutex_lock(&mesa->tablero_casillas[c1].mutex);
        pthread_mutex_lock(&mesa->tablero_casillas[c2].mutex);
        pthread_mutex_lock(&mesa->tablero_casillas[c3].mutex);

        if(mesa->tablero_casillas[med].estado==PERRO && mesa->tablero_casillas[des].estado==VACIA){

            mesa->tablero_casillas[act].estado=VACIA;
            mesa->tablero_casillas[med].estado=VACIA;
            mesa->tablero_casillas[des].estado=PUMA;
            mesa->tablero_casillas[des].idpie=0;
            prmt->cassact=des;

            mesa->perros_cap++;
            mesa->mov++;

            comperro=true;

            
        }

            pthread_mutex_unlock(&mesa->tablero_casillas[c3].mutex);
            pthread_mutex_unlock(&mesa->tablero_casillas[c2].mutex);
            pthread_mutex_unlock(&mesa->tablero_casillas[c1].mutex);

        if(comperro) {
        imprimir("mesa"+to_string(mesa->id)+ " puma salta "+ nomb_casilla(act)+" -> "+ nomb_casilla(des)+ " y se comio el perro de " + nomb_casilla(med)+ " capturas: " + to_string(mesa->perros_cap.load()));

            break;}
        

    }
if(!comperro){
            int opc= ady[act].size();
            if(opc==0)continue;
            int des=ady[act][rand_r(&prmt->sem_local)% opc];
            if(!mov_val(act,des,-1)) continue;

            int lock1= (act<des) ? act:des;
            int lock2= (act<des) ? des:act;

            pthread_mutex_lock(&mesa->tablero_casillas[lock1].mutex);
            pthread_mutex_lock(&mesa->tablero_casillas[lock2].mutex);

            bool mov=false;

            if(mesa->tablero_casillas[des].estado==VACIA){
                mesa->tablero_casillas[act].estado=VACIA;
                mesa->tablero_casillas[des].estado=PUMA;
                mesa->tablero_casillas[des].idpie=0;
                
                prmt->cassact=des;
                mesa->mov++;
                mov=true;
                
            }
            else{
                mesa->conflic++;
            }

            pthread_mutex_unlock(&mesa->tablero_casillas[lock2].mutex);
            pthread_mutex_unlock(&mesa->tablero_casillas[lock1].mutex);

            if(mov){
 imprimir("mesa"+to_string(mesa->id)+ " puma salta "+ nomb_casilla(act)+" -> "+ nomb_casilla(des));
            }
        }



 }

    return NULL;
}

// el cierre de juego de emerfencia y control del torneo

void cierre_aut(int sig){
(void)sig;
    int e=errno;      // el handler no debe alterar errno
    servidor_act=0;
    errno=e;
}
// juez de mesa todo
void bloquear(Mesa *m){
for(int i=0; i<tablero;i++) pthread_mutex_lock(&m->tablero_casillas[i].mutex);

}
void debloquear(Mesa *m){

for(int i=tablero-1;i>=0;i--) pthread_mutex_unlock(&m->tablero_casillas[i].mutex);

}

bool puma_atrapado(Mesa *m){

    int p=-1;
    for(int i=0; i<tablero;i++){
        if(m->tablero_casillas[i].estado==PUMA){
            p=i; 
            break;}}

        if(p<0) return false;

        for(size_t j = 0; j<ady[p].size();j++){
            if(m->tablero_casillas[ady[p][j]].estado==VACIA) return false;
        }

for(size_t k=0; k<saltospum[p].size();k++){
    if(m->tablero_casillas[saltospum[p][k].cas_med].estado==PERRO && m->tablero_casillas[saltospum[p][k].cas_des].estado==VACIA) return false;
}
    return true ;

    }

    char obt_sim(estados_casillas e){
        if(e == VACIA) return '.';
        if(e==PUMA)return 'P';
        return 'D';
    }
// revisar esto 
void imprimir_tablero(Mesa *m, const estados_casillas *esc, string t){
    pthread_mutex_lock(&imprime);   // agregar un mitex , esto evita que se mezclen
    cout << "\nMesa " << m->id << " " << t << endl;
    cout << string(14,' ') << obt_sim(esc[trono]) << "   <- trono\n"; // mejoramiento del tablero
    cout << string(12,' ') << obt_sim(esc[techo_i]) << "   " << obt_sim(esc[techo_d]) << "   <- techo\n"; // mejoramiento del tablero

    for(int f = 0; f < 5; f++){
        cout << "          ";
        for(int c = 0; c < 5; c++) {
            cout << obt_sim(esc[f * 5 + c]) << " ";
        }
        cout << "\n";
    }
    cout << endl;
    pthread_mutex_unlock(&imprime);
}

void inicio_mesa(Mesa *m, jugador *jp, jugador *jd){
m->partida=++cont_partida;
for(int i=0; i<tablero; i++){
    pthread_mutex_init(&m->tablero_casillas[i].mutex,NULL);
    m->tablero_casillas[i].estado=VACIA;
    m->tablero_casillas[i].idpie=-1;

}
m->tablero_casillas[pos_puma].estado=PUMA;
m->tablero_casillas[pos_puma].idpie=0;

for(int i =0; i<perros;i++){
    m->tablero_casillas[pos_perros[i]].estado=PERRO;
    m->tablero_casillas[pos_perros[i]].idpie=i+1;
}
m->perros_cap=0;
m->mov=0;
m->conflic=0;
m->aband=-1;
m->jperro=jd;
m->jpuma=jp;
m->juegosAct=1;
pthread_mutex_lock(&stat);
st_activas++;
if(st_activas>st_max_activas)st_max_activas=st_activas;
pthread_mutex_unlock(&stat);


}

enum fin_par{FIN_CAPT, FIN_ACRR, FIN_EMPT, FIN_ABN, FIN_CTR};

void *juez_mesa(void*arg){
    Mesa *mesa=(Mesa*)arg;

    pthread_t hperro[perros];
    pthread_t hpuma;

    parametros pperros[perros];
    parametros ppumas;
bool okpum=false, okperro[perros];

imprimir("Mesa "+ to_string(mesa->id)+ " Inicio la partida: Jugador "+ to_string(mesa->jpuma->id)+ " (PUMA) vs Jugador "+ to_string(mesa->jperro->id)+ " (PERROS)");

    ppumas.id=1;
    ppumas.mesa_utl=mesa;
    ppumas.cassact=pos_puma;
    ppumas.sem_local=semi+ mesa->partida*7919u+1u;
    mesa->tablero_casillas[pos_puma].estado=PUMA;
    okpum=(pthread_create(&hpuma, NULL , actpuma, &ppumas)==0);

    for(int i=0; i<perros; i++){
    pperros[i].id= i+1;
    pperros[i].mesa_utl=mesa;
    pperros[i].cassact=pos_perros[i];
    pperros[i].sem_local=semi+mesa->partida*7919u+(unsigned)(i+1)*131u+1u;
    okperro[i]=(pthread_create(&hperro[i],NULL,actperro, &pperros[i])==0);

    
}
 
fin_par fin=FIN_EMPT;
int gan=-1;
estados_casillas snap[tablero];
timespec t1;
clock_gettime(CLOCK_MONOTONIC, &t1);
long ultimo=-tab_ms;


while(true){

    if(servidor_act==0){
        fin=FIN_CTR;
        break;
    }
    int ab=mesa->aband;
    if(ab>=0){
        fin=FIN_ABN;
        gan=1-ab;
        break;

    }
    bloquear(mesa);
    for(int i=0; i<tablero; i++) snap[i]=mesa->tablero_casillas[i].estado;
     bool accr=puma_atrapado(mesa);
    debloquear(mesa);

    if(mesa->perros_cap >= cap_ganar){
    fin=FIN_CAPT;
    gan=0; 
    break;
    }
    if(accr){
        fin=FIN_ACRR;
        gan=1;
        break;
    
    }
    if(mesa->mov>=lim_mov){
        fin=FIN_EMPT;
        break;
    }
timespec t2;
clock_gettime(CLOCK_MONOTONIC,&t2);
long ms=(t2.tv_sec-t1.tv_sec)*1000L+(t2.tv_nsec-t1.tv_nsec)/1000000L;

if(ms-ultimo>=tab_ms){
            imprimir_tablero(mesa,snap,"estado");
            ultimo=ms;
        }
        usleep(20*1000);


}

pthread_mutex_lock(&mesa->estmax);
mesa->juegosAct=0;
pthread_mutex_unlock(&mesa->estmax);

for(int i =0; i<perros; i++){

    if(okperro[i]) pthread_join(hperro[i],NULL);
}
if(okpum) pthread_join(hpuma,NULL);   // faltaba esperar al puma antes de destruir los mutex

for(int i=0; i<tablero;i++)snap[i]=mesa->tablero_casillas[i].estado;
imprimir_tablero(mesa,snap, "tablero final");
switch(fin){
        case FIN_CAPT: imprimir(" Mesa " + to_string(mesa->id) + " RESULTADO: GANA EL PUMA (" + to_string(mesa->perros_cap.load()) + " capturas)"); break;
        case FIN_ACRR: imprimir(" Mesa " + to_string(mesa->id) + " RESULTADO: GANAN LOS PERROS (puma acorralado)"); break;
        case FIN_ABN: imprimir(" Mesa " + to_string(mesa->id) + " RESULTADO: " + string(gan==0?"GANA EL PUMA":"GANAN LOS PERROS") + " por abandono del oponente"); break;
        case FIN_CTR: imprimir(" Mesa " + to_string(mesa->id) + " RESULTADO: partida interrumpida por CTRL+C"); break;
        default: imprimir(" Mesa " + to_string(mesa->id) + " RESULTADO: EMPATE (limite de " + to_string(lim_mov) + " movimientos)"); break;
    }
for(int i=0; i<tablero; i++){
    pthread_mutex_destroy(&mesa->tablero_casillas[i].mutex);
    mesa->tablero_casillas[i].estado=VACIA;
}
pthread_mutex_lock(&stat);
    st_partidas++;
    st_movs+=mesa->mov;
    st_conflictos+=mesa->conflic;
    if(fin==FIN_CAPT)st_puma++;
    else if(fin==FIN_ACRR)st_perros++;
    else if(fin==FIN_ABN){
        st_aband++;
        if(gan==0) st_puma++;
        else st_perros++;
    }
    else if(fin==FIN_CTR)st_ctrlc++;
    else st_empates++;
    st_activas--;
    pthread_mutex_unlock(&stat);


    imprimir(" Mesa " + to_string(mesa->id) + "  ha finalizado la partida, mesa liberada");
    sem_post(&mesa->jpuma->fin);
    sem_post(&mesa->jperro->fin);

    pthread_mutex_lock(&mtx_mesas);
    mesa->ocup=false;
    pthread_mutex_unlock(&mtx_mesas);
    sem_post(&ctr_mesas); 
    return NULL;

}

// clientes y machmaking

    void *hiloclin(void *arg){

    jugador*j=(jugador*)arg;
    int esp= aleatorio(&j->sem,0,llega_max);
    for(int t=0; t<esp && servidor_act; t+=10){
    usleep(10*1000);
    }
    pthread_mutex_lock(&mtx_cola);
    llega++;
    if(!servidor_act || cola_cerr){
        pthread_cond_broadcast(&cond_cola);
        pthread_mutex_unlock(&mtx_cola);
        return NULL;
    }

    cola.push_back(j);
    pthread_cond_broadcast(&cond_cola);
    pthread_mutex_unlock(&mtx_cola);

    imprimir("servidor llega el jugador "+ to_string(j->id));
    esperar_sem(&j->emparejado);
    Mesa *m=j->mesa;
    
    if(m==NULL)return NULL;

    if(j->fallo){

        timespec ts;
        plazo_ms(&ts,aleatorio(&j->sem,300,1500));

        if(sem_timedwait(&j->fin,&ts)==0) return NULL;
        pthread_mutex_lock(&m->estmax);

        if(m->partida== j->part && m->juegosAct==1 && m->aband==-1){
            m->aband=j->rol;
            imprimir("mesa "+ to_string(m->id)+ " el jugador "+ to_string(j->id)+ " "+ string(j->rol==0?"puma":"perros")+ " falla o abandono");

        }
        pthread_mutex_unlock(&m->estmax);
    }
    esperar_sem(&j->fin);
    return NULL;

}

void *matchmaker(void *){
unsigned sm=semi*40503u+17u;
    while (true){
        jugador *a=NULL,*b=NULL;
        pthread_mutex_lock (&mtx_cola);

        while(servidor_act && cola.size()<2 && llega<total_jug){

            timespec ts;
            plazo_ms(&ts,100);
            pthread_cond_timedwait(&cond_cola, &mtx_cola, &ts);
        }

        if(servidor_act && cola.size()>=2){
            a=cola[0];
            b=cola[1];
cola.erase(cola.begin(),cola.begin()+2);

        }
        pthread_mutex_unlock(&mtx_cola);
        if(!a) break;
        jugador *jp=a, *jd=b;
        if(rand_r(&sm)%2) swap(jp,jd);
        jp->rol=0;
        jd->rol=1;

        bool tg=false;
        while (servidor_act)
        {
            timespec ts;
            plazo_ms(&ts,100);
            if(sem_timedwait(&ctr_mesas,&ts)==0){
                tg=true;
                break;
            }
            
        }
        if(!tg){
            jp->mesa=NULL;
            jd->mesa=NULL;
            sem_post(&jp->emparejado);
            sem_post(&jd->emparejado);
            break;
        }
        Mesa * mesa=NULL;
        pthread_mutex_lock(&mtx_mesas);
        for(int i=0; i<total_mesas;i++){
            if(!mesas[i].ocup){
                mesa= &mesas[i];
                mesa->ocup=true;
                break;
            }
        }
        pthread_mutex_unlock(&mtx_mesas);
        if(mesa->estJuez){
            pthread_join(mesa->hilo_juez,NULL);
            mesa->estJuez=false;

        }
        inicio_mesa(mesa,jp,jd);
        jp->mesa=jd->mesa=mesa;
    jp->part=jd->part=mesa->partida;
   
    if(pthread_create(&mesa->hilo_juez,NULL,juez_mesa,mesa)!=0){

            imprimir("servidor: no se pudo crear al juez correspondiente a la mesa " + to_string(mesa->id));

            for(int i=0;i<tablero;i++){
            pthread_mutex_destroy(&mesa->tablero_casillas[i].mutex);
            } 

            pthread_mutex_lock(&stat); st_activas--; pthread_mutex_unlock(&stat);
            pthread_mutex_lock(&mtx_mesas); mesa->ocup=false; pthread_mutex_unlock(&mtx_mesas);
            sem_post(&ctr_mesas);
            jp->mesa=jd->mesa=NULL;
        }
        else{
            mesa->estJuez=true;

        }
        sem_post(&jp->emparejado);
        sem_post(&jd->emparejado);

    }
    if(!servidor_act){

    imprimir("servidor: CTRL+C detectado. cerrando cola ");
} 
    int sob=0; 
    pthread_mutex_lock(&mtx_cola);
    cola_cerr=true
    ;
    for(size_t i=0; i<cola.size(); i++){
        cola[i]->mesa=NULL;
        sem_post(&cola[i]->emparejado);
        sob++;
    }

    cola.clear();
    pthread_mutex_unlock(&mtx_cola);
    if(sob>0) imprimir("servidor: " + to_string(sob) + " jugadores sin rivales se decartaron ");

    
    return NULL;
}
// configuraciones del txt

bool leerconfig(const char *ruta){
    ifstream config(ruta);
      if(!config.is_open()){
    cout<< "no funciono el config.txt"<<endl;

return false;
  }
    config >> semi;
    config >> tiem_in;
    config >> tiem_max;
    config >> pos_puma;
    pos_perros.clear();
      for(int i=0; i<12; i++){

    int pos;
    config >> pos;
    pos_perros.push_back(pos);
  }
    if(config.fail()){
        cout<<"config invalido faltan valores "<<endl;
        return false;
    }
    int p;
    if(config>>p)lim_mov=p;
    if(config>>p)prob_abn=p;
    if(config>>p)llega_max=p;
    config.close();
    return true;

}
bool validar(){
    vector<int> usd(tablero,0);

    if(tiem_in<0 || tiem_max<tiem_in){ 

        cout<<"rango de retardo invalido"<<endl; 
        return false; 
    }

    if(pos_puma<0 || pos_puma>=tablero){ 

        cout<<"posicion del puma fuera de rango(0,27)"<<endl; 
        return false; 
    }

    usd[pos_puma]=1;
    for(size_t i=0;i<pos_perros.size();i++){
        int p=pos_perros[i];

        if(p<0 || p>=tablero || usd[p]){ 

            cout<<"posicion invalida o repetida del perro "<<p<<endl; 
            return false;
         }

        usd[p]=1;
    }

    if(lim_mov<1 || prob_abn<0 || prob_abn>100 || llega_max<0){
        cout<<"parametro fuera de rango"<<endl;
        return false;
    }
    return true;
}


int main(int argc, char **argv){

    if(argc<3 || argc>4){
        cout<<"Uso: "<<argv[0]<<" N M [config.txt]"<<endl;
        return 1;
    }
    char *e1;
    char *e2;
    long n=strtol(argv[1],&e1,10), m=strtol(argv[2],&e2,10);

    if(*e1 || *e2 || n<1 || m<1 || n>10000|| m>10000){

        cout << "n y m deben ser positivos" << endl;
        return 1;
    }

    total_jug=(int)n;
    total_mesas=(int)m;
    if(argc==4 && !leerconfig(argv[3])) return 1;
    if(!validar()) return 1;
    tablero_adys();
    
        sem_init(&ctr_mesas,0,total_mesas);
        mesas=new Mesa[total_mesas];
        jugs= new jugador[total_jug];
        for(int i=0; i<total_mesas;i++){

            mesas[i].id=i+1;
            mesas[i].ocup=false;
            mesas[i].estJuez=false;
            mesas[i].aband=-1;
            mesas[i].juegosAct=0;
            pthread_mutex_init(&mesas[i].estmax,NULL);

        }


struct sigaction sa;
    sa.sa_handler=cierre_aut;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags=0;
    sigaction(SIGINT,&sa,NULL);

    imprimir(" Torneo Komikan: N=" + to_string(total_jug) + " jugadores  M=" + to_string(total_mesas) + " mesas, semilla=" + to_string(semi) + ", retardo= " + to_string(tiem_in) + " " + to_string(tiem_max) + " ms ");

pthread_t h;
bool nm=(pthread_create(&h,NULL,matchmaker,NULL)==0);
        for(int i=0; i<total_jug;i++){

        jugador *j=&jugs[i];

        j->id=i+1;
        j->rol=0;
        j->mesa=NULL;
        j->part=0;
        j->sem=semi*2654435761u+(unsigned)i*97u+7u;

        j->fallo=(int)(rand_r(&j->sem)%100u)<prob_abn;

        sem_init(&j->emparejado,0,0);
        sem_init(&j->fin,0,0);

        j->lanzado=nm && (pthread_create(&j->hilo,NULL,hiloclin,j)==0);

        if(!j->lanzado){
            pthread_mutex_lock(&mtx_cola);
            llega++;

            pthread_cond_broadcast(&cond_cola);
            pthread_mutex_unlock(&mtx_cola);
        }
        }
    if(nm) pthread_join(h,NULL);
    for(int i=0; i<total_jug;i++){
        if(jugs[i].lanzado) pthread_join(jugs[i].hilo,NULL);

    }
    
    for(int i=0; i<total_mesas;i++){
        if(mesas[i].estJuez) pthread_join(mesas[i].hilo_juez,NULL);
    }

        cout<< " resumen "<<endl;
        cout <<"partidas: "<<st_partidas<< " ganan los pumas "<< st_puma << " ganan los perros: "<<st_perros<< " empates: "<< st_empates<< " abandonos: "<< st_aband << " interrumpidas: "<< st_ctrlc 
        << " mesas simultaneas: "<< st_max_activas<< " | limite de M: "<< total_mesas<< " movimientos totales: "<< st_movs << " intento de casillas ocupadas: "<< st_conflictos<< endl;


    for(int i=0;i<total_jug;i++){
        sem_destroy(&jugs[i].emparejado);
        sem_destroy(&jugs[i].fin);
    }
    for(int i=0;i<total_mesas;i++){

    pthread_mutex_destroy(&mesas[i].estmax);

    } 

    sem_destroy(&ctr_mesas);
    delete[] jugs;
    delete[] mesas;
    return servidor_act ? 0 : 130;

   }
