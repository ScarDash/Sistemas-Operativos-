#include <sys/sem.h> // semget, semctl, semop
#include <sys/shm.h> // shmget, shmat, shmdt, shmctl
#include <sys/ipc.h> // IPC
#include <sys/errno.h>
#include <wait.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <queue>

using namespace std;

// --- CONFIGURACIÓN DE SEMÁFOROS (ÍNDICES) ---
#define SEM_COLA 0    // Controla el acceso a la cola de prioridad general
#define SEM_MONITOR 1 // Controla la exclusión mutua dentro del Monitor

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// REGLAS DEL SEMAFORO NATIVAS
void sem_wait(int semid, int sem_num)
{
    struct sembuf operacion = {(unsigned short)sem_num, -1, 0};
    semop(semid, &operacion, 1);
}

void sem_signal(int semid, int sem_num)
{
    struct sembuf operacion = {(unsigned short)sem_num, 1, 0};
    semop(semid, &operacion, 1);
}

// --- ENUMS ORIGINALES ---
enum Centrales
{
    COMISARIA = 1 << 0,
    HOSPITAL = 1 << 1,
    FORENSE = 1 << 2
};
enum Incidentes
{
    ACCIDENTE,
    CRIMEN,
    ENFERMEDAD,
    MUERTE,
    ROBO,
    ASESINATO,
    HERIDO,
    PERSECUCION,
    FALSA_ALARMA
};
enum Prioridad
{
    ALTA,
    MEDIA,
    BAJA,
    FALSA_ALARMA_P
};
enum Linea
{
    CLARO,
    TIGO,
    HONDUTEL
};
enum TipoLlamada
{
    Nacional,
    Internacional
};
enum zona
{
    rural,
    urbana
};
enum refuerzos
{
    SI,
    NO
};

// MAPEO PARA IMPRESIÓN en CONSOLA
string nombre_centrales[] = {"Comisaria", "Hospital", "Forense"};
string nombre_incidentes[] = {"Accidente", "Crimen", "Enfermedad", "Muerte", "Robo", "Asesinato", "Herido", "Persecución", "Falsa Alarma"};
string nombre_prioridad[] = {"Alta", "Media", "Baja", "Falsa Alarma"};
string nombre_linea[] = {"Claro", "Tigo", "Hondutel"};
string nombre_tipo_llamada[] = {"Nacional", "Internacional"};
string nombre_zona[] = {"Rural", "Urbana"};
string nombre_refuerzos[] = {"Sí", "No"};

// Estructura de la llamada
struct Llamada
{
    int id;
    Prioridad prioridad;
    unsigned int Centralesbits;
    char descripcion[150];
    TipoLlamada tipo;
    Linea linea_telefonica;
    zona zona_llamada;
    refuerzos refuerzos_llamada;

    // implementa prioridad de la llamada viendo la llamada actual con la otra llamada
    bool operator<(const Llamada &otra) const
    {
        return this->prioridad > otra.prioridad;
    }
};

// --- MONITOR DE CENTRALES (Alojado en MEMORIA COMPARTIDA) ---
struct MonitorCentrales
{
    Llamada historialComisaria[50];
    int sizeComisaria = 0;

    Llamada historialHospital[50];
    int sizeHospital = 0;

    Llamada historialForense[50];
    int sizeForense = 0;

    void procesarLlamada(Llamada llamada, int operadorId, int sem_id, ofstream &archivoLog)
    {
        sem_wait(sem_id, SEM_MONITOR);

        // SI ES INTERNACIONAL SE DESCARTA ---
        if (llamada.tipo == Internacional)
        {
            string descarte = "[Monitor] LLAMADA INTERNACIONAL DESCARTADA (ID: " + to_string(llamada.id) +
                              " | Operador: " + to_string(operadorId) + ") - No califica como emergencia nacional.";

            if (archivoLog.is_open())
            {
                archivoLog << descarte << endl;
                archivoLog.flush();
            }
            cout << "\033[1;31m" << descarte << "\033[0m" << endl; // Imprime en rojo en la terminal

            sem_signal(sem_id, SEM_MONITOR);
            return; // Salimos de la función sin agregar la llamada a los arreglos
        }

        // --- SI ES NACIONAL, CONTINÚA EL PROCESO NORMAL ---
        string registro = "[Operador " + to_string(operadorId) + "] Procesó ID: " + to_string(llamada.id) +
                          " | Tipo: " + nombre_tipo_llamada[llamada.tipo] +
                          " | Prioridad: " + nombre_prioridad[llamada.prioridad] +
                          " | " + llamada.descripcion + " | ENVIANDO A: ";

        if (llamada.Centralesbits & COMISARIA)
        {
            if (sizeComisaria < 50)
                historialComisaria[sizeComisaria++] = llamada;
            registro += "[Comisaria] ";
        }
        if (llamada.Centralesbits & HOSPITAL)
        {
            if (sizeHospital < 50)
                historialHospital[sizeHospital++] = llamada;
            registro += "[Hospital] ";
        }
        if (llamada.Centralesbits & FORENSE)
        {
            if (sizeForense < 50)
                historialForense[sizeForense++] = llamada;
            registro += "[Forense] ";
        }

        if (archivoLog.is_open())
        {
            archivoLog << registro << endl;
            archivoLog.flush();
        }
        cout << registro << endl;

        sem_signal(sem_id, SEM_MONITOR);
    }

    void mostrarEstadisticas()
    {
        cout << "\n=============================================" << endl;
        cout << "   ESTADÍSTICAS FINALES DEL MONITOR (SHM)" << endl;
        cout << "   (Solo Emergencias Nacionales Atendidas)   " << endl;
        cout << "=============================================" << endl;
        cout << "Total llamadas registradas en Comisaría: " << sizeComisaria << endl;
        cout << "Total llamadas registradas en Hospital:  " << sizeHospital << endl;
        cout << "Total llamadas registradas en Forense:   " << sizeForense << endl;
        cout << "=============================================" << endl;
    }
};

// asigna prioridad y centrales a la llamada
Prioridad asignarPrioridadYCentrales(Incidentes incidente, unsigned int &centrales)
{
    switch (incidente)
    {
    case ACCIDENTE:
        centrales = HOSPITAL | COMISARIA;
        return ALTA;
    case ROBO:
    case PERSECUCION:
        centrales = COMISARIA;
        return ALTA;
    case ASESINATO:
    case MUERTE:
        centrales = COMISARIA | FORENSE;
        return ALTA;
    case CRIMEN:
        centrales = COMISARIA;
        return MEDIA;
    case HERIDO:
        centrales = HOSPITAL;
        return MEDIA;
    case ENFERMEDAD:
        centrales = HOSPITAL;
        return BAJA;
    default:
        centrales = COMISARIA;
        return FALSA_ALARMA_P;
    }
}

Llamada generarLlamadaAleatoria(int id)
{
    int i = rand() % 9;
    unsigned int deptoCentral = 0;
    Prioridad p = asignarPrioridadYCentrales(static_cast<Incidentes>(i), deptoCentral);

    Llamada ll;
    ll.id = id;
    ll.prioridad = p;
    ll.Centralesbits = deptoCentral;
    int probabilidadTipo = rand() % 10; // Genera un número de 0 a 9
    if (probabilidadTipo < 9)
    {
        ll.tipo = Nacional; // 90% de los casos (0, 1, 2, 3, 4, 5, 6, 7, 8)
    }
    else
    {
        ll.tipo = Internacional; // 10% de los casos ( 9)
    }

    ll.linea_telefonica = static_cast<Linea>(rand() % 3);
    ll.zona_llamada = static_cast<zona>(rand() % 2);
    ll.refuerzos_llamada = static_cast<refuerzos>(rand() % 2);

    string desc = "Incidente: " + nombre_incidentes[i] + " (Línea: " + nombre_linea[ll.linea_telefonica] + ")";
    strncpy(ll.descripcion, desc.c_str(), sizeof(ll.descripcion));

    return ll;
}

int main()
{
    cout << "BIENVENIDO AL SISTEMA DE EMERGENCIAS UVI UNAH" << endl;
    cout << "----------------------------------------------------------------------" << endl;

    srand(static_cast<unsigned>(time(nullptr)));

    ofstream RecursoCompartido("logs_uvi.txt", ios::app);
    if (!RecursoCompartido.is_open())
    {
        cerr << "Error al abrir el archivo de logs." << endl;
        return 1;
    }

    key_t clave_sem = ftok("/bin", 65);
    key_t clave_shm = ftok("/bin", 66);

    int sem_id = semget(clave_sem, 2, 0666 | IPC_CREAT);
    int shm_id = shmget(clave_shm, sizeof(MonitorCentrales), 0666 | IPC_CREAT);

    if (sem_id == -1 || shm_id == -1)
    {
        cerr << "Error al inicializar los recursos IPC del sistema." << endl;
        return 1;
    }

    union semun init;
    init.val = 1;
    semctl(sem_id, SEM_COLA, SETVAL, init);
    semctl(sem_id, SEM_MONITOR, SETVAL, init);

    MonitorCentrales *monitor = (MonitorCentrales *)shmat(shm_id, NULL, 0);
    monitor->sizeComisaria = 0;
    monitor->sizeHospital = 0;
    monitor->sizeForense = 0;

    priority_queue<Llamada> colaPrioridadGeneral;

    pid_t pid = fork();

    if (pid == -1)
    {
        cerr << "Error al ejecutar fork." << endl;
        return 1;
    }

    if (pid == 0)
    {
        // operador 2
        for (int i = 0; i < 10; ++i)
        {
            Llamada nuevaLlamada;

            sem_wait(sem_id, SEM_COLA);
            nuevaLlamada = generarLlamadaAleatoria(rand() % 9000 + 1000);
            colaPrioridadGeneral.push(nuevaLlamada);
            sem_signal(sem_id, SEM_COLA);

            usleep(100000);

            sem_wait(sem_id, SEM_COLA);
            if (!colaPrioridadGeneral.empty())
            {
                Llamada ll = colaPrioridadGeneral.top();
                colaPrioridadGeneral.pop();
                sem_signal(sem_id, SEM_COLA);

                monitor->procesarLlamada(ll, 2, sem_id, RecursoCompartido);
            }
            else
            {
                sem_signal(sem_id, SEM_COLA);
            }

            sleep(1);
        }
        shmdt(monitor);
        exit(0);
    }
    else
    {
        for (int i = 0; i < 10; ++i)
        {
            Llamada nuevaLlamada;

            sem_wait(sem_id, SEM_COLA);
            nuevaLlamada = generarLlamadaAleatoria(rand() % 9000 + 1000);
            colaPrioridadGeneral.push(nuevaLlamada);
            sem_signal(sem_id, SEM_COLA);

            usleep(100000);

            sem_wait(sem_id, SEM_COLA);
            if (!colaPrioridadGeneral.empty())
            {
                Llamada ll = colaPrioridadGeneral.top();
                colaPrioridadGeneral.pop();
                sem_signal(sem_id, SEM_COLA);

                monitor->procesarLlamada(ll, 1, sem_id, RecursoCompartido);
            }
            else
            {
                sem_signal(sem_id, SEM_COLA);
            }

            sleep(1);
        }

        wait(nullptr);

        monitor->mostrarEstadisticas();

        RecursoCompartido.close();
        shmdt(monitor);
        shmctl(shm_id, IPC_RMID, NULL);
        semctl(sem_id, 0, IPC_RMID);

        cout << "Proceso e IPCs finalizados limpiamente." << endl;
    }

    return 0;
}
