# Tarea 2 - Sistema Operativo - Torneo de Komikan 
Integrantes : Carlos Araya - Jasson Valverde 

Sección : 2 

## Descripción
El servidor de torneo del Komikan (un puma contra 12 perros) consiste en tiempo real y sin turnos. Un hilo matchmaker empareja a los N jugadores, se juegan hasta M partidas simultáneas (una por mesa) y cada mesa tiene un hilo juez que arbitra la partida. El puma y cada perro son hilos independientes que se mueven con retardos aleatorios.

## Como Compilar
En la terminal se debe ingresar el siguiente comando

```bash
g++ -Wall -Wextra -std=c++17 -pthread tarea2.cpp -o torneo
```
## Ejecutar

```bash
./torneo N M [config.txt]
```
Sea:
* N : Numero total de jugadores, cada partida se usa un maximo de 2 jugadores (un puma y un perro), si N es impar, el jugador se queda sin contrincante y se anula la partida
* M : Numero maximo de mesas (partidas) de manera simultánea  
ejemplo:

```bash
./torneo 8 2 config.txt
```
Debo recalcar que los valores de N clientes y M mesas es a su criterio, los valores mostrado en el comando es de ejemplo.  

## Formato del config.txt
Para el archivo config.txt , esta definido lo siguiente: 
* Semillas (seed)
* Retardo minimo por movimiento en ms
* Retardo maximo por movimiento en ms
* Posición inicial del Puma
* Posiciones iniciales de los 12 Perros
* Limites de movimiento antes del empate (por defecto son 600)
* porcentaje de jugadores que abandonan la partida, 0 - 100 (por defecto es 0)
* Tiempo maximo de llegada de los jugadores en ms (por defecto son 300)

Aunque los ultimos 3 puntos son opcionales. 
Ejemplo (nuestro archivo config.txt)

```
12345 // semillas
50 300 // retardo mínimo y máximo en ms
25 // posición inicial del puma
10 14 15 16 17 18 19 20 21 22 23 24 // posición inicial de los perros
600 0 300 // el limite de movimiento , porcentaje de jugadores que abandonan, tiempo máximo de llegada de los jugadores en ms
```
Numeración de las posiciones 

```
      25
   26    27
 0  1  2  3  4
 5  6  7  8  9
10 11 12 13 14
15 16 17 18 19
20 21 22 23 24
```
0 a 24 es la grilla 5x5, la casilla 25 es el trono y 26 y 27 son los nodos del techo. Las casillas no se -pueden repetirse.

## Decisiones de Sincronización
* Al usar un mutex por casilla (`pthread_mutex_t`), los movimientos toman el origen y el destino de manera ascendente del índice, el salto del puma utiliza 3 casillas, también de forma ordenada, de tal forma evitar el Deadlock y garantizar que haya una ficha por casilla.
* El Juez toma las 28 casillas ordenadamente para obtener una copia de manera consistente del tablero y evaluar la victoria.
* El máximo de M mesas es implementada mediante un semáforo (`ctr_mesas`) inicializando en M.
* La cola de jugadores es el uso mutuo de `mtx_cola` y `cond_cola` que avisa entre los jugadores y mesas, semaforos `emparejados` y `fin`.
* Los contadores de mesa y banderas (flags) de Ctrl + C utilizando `atomic<int>`.

### Fallos y Ctrl + C
* Abandono: Un jugador marcado como fallido (según el % en el archivo config.txt) abandona la partida, luego el juez le declara ganador al oponente, detiene las fichas y destruye los mutex de la mesa.
* SIGINT (Ctrl + C): Se dejan de aceptar a los jugadores, luego las partidas activas terminan con resultado "interrumpida", se hace un `join` de todos los hilos y se liberan mutex, semáforos y memoria. El programa termina con código 130.

### Limitaciones 
* Cada fichas deciden sus movimientos de manera al azar, por lo que los perros algunas veces logran acorralar al puma con la configuración (config.txt) por defecto.
* Si un movimiento ocurre justo en el momento en que el juez declara el resultado, el tablero final puede mostrar esa última jugada.























