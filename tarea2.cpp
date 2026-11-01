#include <stdio.h>
#include <iostream>
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
#define tablero 26
#define trono 25
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
volatile sig_atomic_t servidor_act = 1; //para lo de ctr+c
vector<int> ady[tablero];
vector <saltos>saltospum[tablero];
unsigned semi;

int tiem_in;
int tiem_max;
int pos_puma;
vector<int> pos_perros = {10,14,15,16,17,18,19,20,21,22,23,24}; // ver esto toy con duda lo voy a probar idea rara

//parametros de config txt 

int lim_mov=600;
int prob_abn=0;
int llega_max=300;
int tab_ms=2000; // para impirmir el tablero 


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

Mesa * mesa=NULL;
jugador *jug=NULL;
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

vector <saltos>saltospum[tablero];


//utilidades

void imprimir(string mensaje){
pthread_mutex_lock(&imprime);
cout << mensaje << endl;
pthread_mutex_unlock(&imprime);

}

string nomb_casilla(int a){

if(a= trono) return "trono";
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

//tablero:

int ind(int f, int i){

if(f==-1 && i==2) return trono;
if(f<0 || f>4 || i<0 || i>4) return -1;
return f*5+i;


}

void arist(int a, int b){

    if(find(ady[a].begin(), ady[a].end(),b) == ady[a].end()){

        ady[a].push_back(0);
        ady[b].push_back(0);
    }

}
void tablero_adys(){
    for(int f=0;f<5;f++)
        for(int c=0;c<5;c++)
            for(int df=-1;df<=1;df++)
                for(int dc=-1;dc<=1;dc++){
                    if(df==0 && dc==0) continue;
                    if(df!=0 && dc!=0 && ((f+c)%2!=0)) continue;
                    int j=ind(f+df,c+dc);
                    if(j<0 || j==trono) continue;
                    arist(f*5+c,j);
                }
    arist(trono,1);
    arist(trono,2);
    arist(trono,3);

    for(int a=0;a<tablero;a++){
        int fa=(a==trono)?-1:a/5, ca=(a==trono)?2:a%5;
        for(size_t k=0;k<ady[a].size();k++){
            int b=ady[a][k];
            int fb=(b==trono)?-1:b/5, cb=(b==trono)?2:b%5;
            int c=ind(2*fb-fa, 2*cb-ca);
            if(c>=0 && c!=a && find(ady[b].begin(),ady[b].end(),c)!=ady[b].end()){
                saltos s={b,c};
                saltospum[a].push_back(s);
            }
        }
    }
}

bool mov_val(int in, int des, int med){

    if(med<0) return find(ady[in].begin(),ady[in].end(),des) != ady[in].end();
    
    for(size_t i=0; i<saltospum[in].size(); i++) {
        if(saltospum[in][i].cas_med=med && saltospum[in][i].cas_des=des) return true;

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

    
        if(mesa->tablero_casillas[act].estado != PERRO && mesa->tablero_casillas[act].idpie==id){
    pthread_mutex_unlock(&mesa->tablero_casillas[lock2].mutex);
    pthread_mutex_unlock(&mesa->tablero_casillas[lock1].mutex);
        break; 
        }

bool mov=false;

           // cout<< "la mesa: "<< mesa->id << "Perro "<<id<< "se movio "<<act << "hacia" << dest<< endl;

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
struct saltos{
    int cas_med; // si hay un perro
    int cas_des; // el destino

};

//logica del puma

void *actpuma(void *arg){
parametros *prmt=(parametros*) arg;
int id= prmt->id;
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

            
/*
            if(mesa->perros_cap>=6){

                cout<< "la MESA: "<< mesa->id<< "EL PUMA A GANADO con un total de "<<mesa->perros_cap <<  endl;
                mesa->juegosAct=0;

            }

        }*/

            pthread_mutex_unlock(&mesa->tablero_casillas[c3].mutex);
            pthread_mutex_unlock(&mesa->tablero_casillas[c2].mutex);
            pthread_mutex_unlock(&mesa->tablero_casillas[c1].mutex);

        if(comperro) {
        imprimir("mesa"+to_string(mesa->id)+ " puma salta "+ nomb_casilla(act)+" -> "+ nomb_casilla(des)+ " y se comio el perro de " + nomb_casilla(med)+ " capturas: " + to_string(mesa->perros_cap.load()));

            break;}
        

    }
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

                //cout<< "mesa: "<< mesa->id << "el puma se desplazo: "<<act<< "hacia "<<des<<endl;



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
    servidor_act=0;
   

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
        if(m->tablero_casillas[i].estado=PUMA){
            p=i; 
            break;}}

        if(p<0) return false;

        for(size_t j = 0; j<ady[p].size();j++){
            if(m->tablero_casillas[ady[p][j]].estado=VACIA) return false;
        }

for(size_t k=0; k<saltospum[p].size();k++){
    if(m->tablero_casillas[saltospum[p][k].cas_med].estado=PERRO && m->tablero_casillas[saltospum[p][k].cas_des].estado=VACIA) return false;
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
    cout << "\nMesa " << m->id << " " << t << endl;
    cout << "   trono: " << (esc[25] == VACIA ? '^' : obt_sim(esc[25])) << "\n";
    
    for(int f = 0; f < 5; f++){
        cout << "          ";
        for(int c = 0; c < 5; c++) {
            cout << obt_sim(esc[f * 5 + c]) << " ";
        }
        cout << "\n";
    }
    cout << endl;
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

imprimir("Mesa "+ to_string(mesa->id)+ " Inicio la partida: Jugador "+ to_string(mesa->jperro->id)+ " (PUMA) vs Jugador "+ to_string(mesa->jperro->id)+ " (PERROS)");

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

    if(mesa->perros_cap == cap_ganar){
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



int main(){

  ifstream config("config.txt");

  if(!config.is_open()){
    cout<< "no funciono el config.txt"<<endl;

return 1;
  }

  config >> sem;
  config >> tiem_in;
  config >> tiem_max;
  
  config >> pos_puma;

  for(int i=0; i<12; i++){

    int pos;
    config >> pos;
    pos_perros.push_back(pos);
  }

  config.close();

srand(sem);


    return 0;
   }
