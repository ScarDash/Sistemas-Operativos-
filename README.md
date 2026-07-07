
````markdown
# Exposición: Sistema de Emergencias UVI
## Sistemas Operativos



# Introducción

Este proyecto simula un **Sistema de Emergencias UVI**, cuyo propósito es demostrar la aplicación práctica de conceptos fundamentales de **Sistemas Operativos**, como:

- Procesos
- Memoria Compartida
- Semáforos
- Exclusión Mutua
- Secciones Críticas
- Sincronización
- Productor-Consumidor
- IPC (Inter Process Communication)

Aunque el programa representa un sistema de atención de emergencias, su verdadero objetivo es demostrar cómo varios procesos pueden trabajar de forma concurrente compartiendo recursos de manera segura.

# PARTE 1. Arquitectura General del Sistema

## ¿Qué representa el programa?

Este programa simula un Sistema de Emergencias donde existen cuatro entidades principales:

- Operadores Telefónicos
-  Central Comisaría
-  Central Hospital
-  Central Medicina Forense

Todos estos procesos trabajan concurrentemente como sucede en un sistema operativo real.

El programa utiliza el patrón clásico:

# Productor – Consumidor

Los operadores producen llamadas.

Las centrales consumen las llamadas.



## Arquitectura General


// ================================================================
//              ARQUITECTURA GENERAL DEL PROGRAMA
//
//                    Proceso Padre
//                          │
//          ┌───────────────┴───────────────┐
//          │                               │
//    Operador 1                      Operador 2
//          │                               │
//          └───────────────┬───────────────┘
//                          │
//                 Memoria Compartida
//                          │
//          ┌───────────────┼───────────────┐
//          │               │               │
//      Comisaría       Hospital       Forense
//
// ================================================================

## DIAGRAMA DE SEMAFOROS  FUNCIONAMIENTO
// SEM_MONITOR = 1
//
// Operador 1
//      |
//      | sem_wait()
//      v
// SEM_MONITOR = 0
//
// Operador 2 intenta entrar
//      |
//      v
// BLOQUEADO
//
// Operador 1 termina
//      |
//      | sem_signal()
//      v
// SEM_MONITOR = 1
//
// Operador 2 entra


# Procesos del Sistema

## Proceso Padre

Existe un único proceso principal.

Su función es: 

- Crear todos los procesos
- Crear la memoria compartida
- Crear los semáforos
- Esperar la finalización
- Liberar todos los recursos

No atiende llamadas.

No genera llamadas.

Es únicamente el administrador.


````
## Procesos Operadores

El programa crea dos operadores.


for (int op = 0; op < 2; op++)
{
    pid_t pid = fork();
}

Cada operador genera llamadas aleatorias.

Cada operador produce:

```cpp
NUM_LLAMADAS_POR_OPERADOR = 10;
```

Por lo tanto,

Operador 1 → 10 llamadas

Operador 2 → 10 llamadas

Total → 20 llamadas

---

## Procesos Consumidores

El programa crea tres consumidores.

```cpp
for (int idx = 0; idx < 3; idx++)
{
    fork();
}
```

Uno para:

- Comisaría
- Hospital
- Forense

Cada consumidor espera llamadas para atender.

---

## Total de Procesos

```text
Proceso Padre          1

Operadores             2

Consumidores           3

-------------------------

TOTAL                  6 Procesos
```

---

# ¿Hay Hilos?

No.

Este programa **NO utiliza hilos**.

No aparecen:

```cpp
thread

pthread

std::thread
```

Todo el paralelismo ocurre mediante:

```cpp
fork()
```

Cada proceso posee:

- Su propio PID
- Su propia pila
- Su propio espacio de memoria

Lo único compartido es la memoria creada mediante:

```cpp
shmget()

shmat()
```

---

# PARTE 2. Memoria Compartida

Normalmente los procesos **NO comparten memoria**.

Cada proceso tendría una copia distinta.

Para resolver ese problema se utiliza Memoria Compartida.

---

## Creación de Memoria Compartida

```cpp
int shmId = shmget(claveShm, sizeof(UVI), 0666 | IPC_CREAT);
```

Luego todos los procesos ejecutan

```cpp
shmat()
```

y obtienen un puntero al mismo objeto.

```cpp
monitor
```

Todos trabajan exactamente sobre la misma estructura.

---

# ¿Qué contiene la Memoria Compartida?

La estructura principal es:

```cpp
struct UVI
```

Contiene:

- contadorID
- colas
- historial
- centralActiva
- estadísticas

---

## Representación


            MEMORIA COMPARTIDA

+------------------------------------+

contadorID

Cola Comisaría

Cola Hospital

Cola Forense

Historial Comisaría

Historial Hospital

Historial Forense

centralActiva

+------------------------------------+


Todos los procesos modifican esta estructura.

---

# Problema: Condición de Carrera

Supongamos:

Operador 1

y

Operador 2

registran una llamada exactamente al mismo tiempo.

Sin sincronización:

```text
Operador1 lee contador = 8

Operador2 lee contador = 8

Operador1 guarda 9

Operador2 guarda 9
```

Resultado:

Dos llamadas con el mismo ID.

Este problema recibe el nombre de:

## Race Condition (Condición de Carrera)

---

# PARTE 3. Mutex, Semáforos y Sección Crítica

Esta es la parte más importante del programa.

---

# ¿Qué es una Sección Crítica?

Es una parte del código donde únicamente puede entrar un proceso al mismo tiempo.

Ejemplo:

```cpp
contadorID++;
```

Si dos procesos ejecutan esta línea simultáneamente:

Se pierde información.

---

# ¿Cómo protege el programa la Sección Crítica?

Utilizando un semáforo binario.

```cpp
SEM_MONITOR
```

Inicialización:

```cpp
init.val = 1;

semctl(semId, SEM_MONITOR, SETVAL, init);
```

Valor inicial:

```text
1
```

Significa:

```text
Recurso Libre
```

---

# Entrada a la Sección Crítica

Antes de modificar memoria compartida:

```cpp
sem_wait(sem_id, SEM_MONITOR);
```

El semáforo cambia:

```text
1 → 0
```

Ahora ningún otro proceso puede entrar.

---

# Salida de la Sección Crítica

Cuando termina:

```cpp
sem_signal(sem_id, SEM_MONITOR);
```

El semáforo cambia:

```text
0 → 1
```

Ahora otro proceso puede ingresar.

---

# Ejemplo

Operador 1 entra.

```text
SEM_MONITOR = 1

↓

sem_wait()

↓

SEM_MONITOR = 0
```

Llega Operador 2.

También ejecuta:

```cpp
sem_wait()
```

Pero el semáforo vale:

```text
0
```

El Sistema Operativo lo bloquea.

Cuando Operador 1 termina:

```cpp
sem_signal()
```

El semáforo vuelve a:

```text
1
```

Ahora entra Operador 2.

Nunca modifican la memoria al mismo tiempo.

---

# ¿Dónde aparece la Sección Crítica?

## registrarLlamada()

```cpp
sem_wait()

contadorID++

insertarOrdenado()

sem_signal()
```

---

## atenderCentral()

Porque elimina llamadas de la cola.

---

## registrarTexto()

Porque protege el archivo de log.

---

# Semáforos Contadores

Además del mutex existen tres semáforos contadores.

```cpp
SEM_COMISARIA

SEM_HOSPITAL

SEM_FORENSE
```

Su función es despertar únicamente a la central correspondiente.

Ejemplo:

Accidente

↓

Hospital

↓

Comisaría

↓

Forense permanece dormido.

---

# PARTE 4. Funcionamiento Completo

## Paso 1

El proceso principal crea:

- Memoria Compartida
- Semáforos

---

## Paso 2

Crea las centrales.

Todas ejecutan:

```cpp
sem_wait()
```

Quedan bloqueadas.

No consumen CPU.

---

## Paso 3

Se crean los operadores.

Cada operador ejecuta:

```cpp
generarLlamadaAleatoria()
```

Genera:

- Prioridad
- Incidente
- Destino

---

## Paso 4

El operador registra la llamada.

Obtiene el mutex.

Asigna un ID.

Inserta la llamada.

Libera el mutex.

---

## Paso 5

Después despierta únicamente la central correspondiente.

Ejemplo:

Accidente

↓

Hospital

↓

Comisaría

```cpp
sem_signal(Hospital);

sem_signal(Comisaría);
```

---

## Paso 6

La central despierta.

Extrae la llamada.

La mueve al historial.

Simula atención.

```cpp
usleep(200000);
```

---

## Paso 7

Cuando terminan los operadores.

El padre ejecuta:

```cpp
waitpid()
```

---

## Paso 8

El padre cambia:

```cpp
centralActiva = false;
```

Despierta a las tres centrales.

Cada central verifica:

```text
¿Hay llamadas?

NO

↓

¿centralActiva?

false

↓

Salir
```

---

## Paso 9

El padre imprime estadísticas.

Después elimina:

- Memoria Compartida

```cpp
shmctl()
```

- Semáforos

```cpp
semctl()
```

El sistema termina correctamente.

---

# Conceptos de Sistemas Operativos Aplicados

| Concepto | Implementación |
|-----------|----------------|
| Procesos | `fork()` |
| Hilos | No utilizados |
| Memoria Compartida | `shmget()` y `shmat()` |
| Sección Crítica | `registrarLlamada()`, `atenderCentral()`, `registrarTexto()` |
| Mutex | `SEM_MONITOR` |
| Semáforos Contadores | `SEM_COMISARIA`, `SEM_HOSPITAL`, `SEM_FORENSE` |
| Productor-Consumidor | Operadores producen, centrales consumen |
| Sincronización | `sem_wait()` y `sem_signal()` |
| Exclusión Mutua | `SEM_MONITOR` |
| Finalización Ordenada | `waitpid()`, `shmctl()`, `semctl()` |

---

# Conclusión

Desde el punto de vista de Sistemas Operativos, este proyecto integra varios conceptos fundamentales en una sola aplicación.

Demuestra cómo un sistema operativo coordina múltiples procesos independientes mediante mecanismos de comunicación y sincronización.

Los operadores producen información y las centrales la consumen utilizando memoria compartida.

Las condiciones de carrera se evitan mediante un mutex implementado con un semáforo binario.

Finalmente, el proceso padre coordina el ciclo de vida de todos los procesos y libera correctamente los recursos IPC, evitando fugas de memoria y recursos del sistema operativo.

Este proyecto constituye un ejemplo práctico del uso de:

- Procesos
- IPC
- Memoria Compartida
- Semáforos
- Exclusión Mutua
- Sincronización
- Productor-Consumidor
- Finalización Ordenada
- Administración de Recursos por parte del Sistema Operativo.
````
