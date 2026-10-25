#include <stdio.h>
#include <iostream>
#include <pthread.h>
#include <signal.h> //ctr+c
#include <unistd.h>
#include <stdlib.h>
#include<vector>
#include <semaphore.h>
#include<fstream>
using namespace std;

// definicion de hilos 
#define perros 12
#define pumas 1
#define tablero 26

// difinicion de estados de cassillas

enum estados_casillas {VACIA, PERRO, PUMA};

struct casilla{
    estados_casillas estado;
    pthread_mutex_t mutex;
};

//variable globales
int total_jug=0;
int total_mesas=0;
volatile sig_atomic_t servidor_act = 1; //para lo de ctr+c
vector<int> ady[tablero];
vector <saltos>saltospum[tablero];
int sem;
int tiem_in;
int tiem_max;
int pos_puma;
vector<int> pos_perros;




struct Mesa{
int id;
int juegosAct;
int perros_cap;
casilla tablero_casillas[tablero];
};

void tablero_adys(){
//medio-der
    ady[0].push_back(1); // derecha
    ady[0].push_back(5); // abajo
    ady[0].push_back(6); // diagonal abajo-derecha

//arriba al medio-izq
    ady[1].push_back(0); // izquierda
    ady[1].push_back(2); // derecha
    ady[1].push_back(6); // abajo
    ady[1].push_back(25); // ¡Conecta hacia arriba con el Trono!


//conecta el trono
    ady[25].push_back(1);
    ady[25].push_back(2);
    ady[25].push_back(3);

}

// reglas de los hilos 

struct parametros{
    int id;
    Mesa *mesa_utl;
    int cassact;

};

void tiempo_de_espera(){
     int rango= tiem_max-tiem_in+1;

int milseg=tiem_in+(rand()%rango);


usleep(milseg*1000);

}
void *actperro(void *arg){

    parametros *param= (parametros*) arg;
    int id= param->id;
    Mesa *mesa= param->mesa_utl;

    while(mesa->juegosAct==1 && servidor_act==1){

    tiempo_de_espera();
    int act=param->cassact;
    int opc=ady[act].size();
       if (opc==0){
       
        continue;

    }
    
    int dest=ady[act][rand()%opc];
    
    int lock1= (act <dest) ? act:dest;
    int lock2= (act <dest) ? dest:act;


        pthread_mutex_lock(&mesa->tablero_casillas[lock1].mutex);
        pthread_mutex_lock(&mesa->tablero_casillas[lock2].mutex);

    
        if(mesa->tablero_casillas[act].estado != PERRO){
    pthread_mutex_unlock(&mesa->tablero_casillas[lock2].mutex);
    pthread_mutex_unlock(&mesa->tablero_casillas[lock1].mutex);
        break; 
        }

        if(mesa->tablero_casillas[dest].estado == VACIA){
            mesa->tablero_casillas[act].estado=VACIA;
            mesa->tablero_casillas[dest].estado=PERRO;
            param->cassact=dest;

            cout<< "la mesa: "<< mesa->id << "Perro "<<id<< "se movio "<<act << "hacia" << dest<< endl;


        }
        
         pthread_mutex_unlock(&mesa->tablero_casillas[lock2].mutex);
        pthread_mutex_unlock(&mesa->tablero_casillas[lock1].mutex);



    }

    
 
    return NULL; 

}
//otras variables globales pumas
struct saltos{
    int cas_med; // si hay un perro
    int cas_des; // el destino

};
vector <saltos>saltospum[tablero];

//logica del puma

void *actpuma(void *arg){
parametros *prmt=(parametros*) arg;
int id= prmt->id;
Mesa *mesa= prmt->mesa_utl;

 while(mesa->juegosAct==1 && servidor_act==1){
    tiempo_de_espera();
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
            prmt->cassact=des;

            mesa->perros_cap++;

            comperro=true;

            cout<<"Mesa "<<mesa->id<< "puma se desplazo de "<< act<< "hacia" << des<< "se comio un perro en total lleva: "<<mesa->perros_cap<<endl;

            if(mesa->perros_cap>=6){

                cout<< "la MESA: "<< mesa->id<< "EL PUMA A GANADO con un total de "<<mesa->perros_cap <<  endl;
                mesa->juegosAct=0;

            }

        }

            pthread_mutex_unlock(&mesa->tablero_casillas[c3].mutex);
            pthread_mutex_unlock(&mesa->tablero_casillas[c2].mutex);
            pthread_mutex_unlock(&mesa->tablero_casillas[c1].mutex);

        if(comperro) break;
        

    }

if(!comperro){
            int opc= ady[act].size();
            if(opc==0)continue;
            int des=ady[act][rand()% opc];

            int lock1= (act<des) ? act:des;
            int lock2= (act<des) ? des:act;

            pthread_mutex_lock(&mesa->tablero_casillas[lock1].mutex);
            pthread_mutex_lock(&mesa->tablero_casillas[lock2].mutex);

            if(mesa->tablero_casillas[des].estado==VACIA){
                mesa->tablero_casillas[act].estado=VACIA;
                mesa->tablero_casillas[des].estado=PUMA;
                prmt->cassact=des;

                cout<< "mesa: "<< mesa->id << "el puma se desplazo: "<<act<< "hacia "<<des<<endl;



            }

            pthread_mutex_unlock(&mesa->tablero_casillas[lock2].mutex);
            pthread_mutex_unlock(&mesa->tablero_casillas[lock1].mutex);
        }



 }

    return NULL;
}

// el cierre de juego de emerfencia y control del torneo

sem_t ctr_mesas;
void cierre_aut(int sig){

    servidor_act=0;
    cout<< "CERRANDO EL TORNEO DEBIDO AL CTRL+C, CHAO TORNEO"<<endl;

}

void *juez_mesa(void*arg){
    Mesa *mesa=(Mesa*)arg;

    pthread_t hperro[12];
    pthread_t hpuma;

    parametros pperros[12];
    parametros ppumas;

    cout<< "inicio de partida en la mesa: "<<mesa->id<<endl;

    ppumas.id=1;
    ppumas.mesa_utl=mesa;
    ppumas.cassact=pos_puma;
    mesa->tablero_casillas[pos_puma].estado=PUMA;
    pthread_create(&hpuma, NULL , actpuma, &ppumas);

    for(int i=0; i<12; i++){
    pperros[i].id= i+1;
    pperros[i].mesa_utl=mesa;

    int cass=pos_perros[i];

    pperros[i].cassact=cass;
    mesa->tablero_casillas[cass].estado=PERRO;
    pthread_create(&hperro[i],NULL,actperro, &pperros[i]);
    
}
  
pthread_join(hpuma,NULL);

for(int i=0; i< tablero; i++){
    pthread_join(hperro[i], NULL);


}

mesa->juegosAct=0;
for(int i=0; i<tablero; i++){

    mesa->tablero_casillas[i].estado=VACIA;

}

cout << "MESA: "<< mesa->id<< " ha finalizado la partida mesa liberada"<<endl;

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