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
##Ejecutar

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









