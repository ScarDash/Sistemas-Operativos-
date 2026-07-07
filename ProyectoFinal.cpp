// Proyecto UVI UNAH - Sistemas Operativos
// Este proyecto simula un sistema de emergencias con operadores y centrales de atención.

#include <sys/sem.h>  // semget, semctl, semop
#include <sys/shm.h>  // shmget, shmat, shmdt, shmctl
#include <sys/ipc.h>  // claves IPC (ftok)
#include <sys/wait.h> // waitpid
#include <unistd.h>   // fork, sleep, usleep, getpid
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <cstring>

using namespace std;

// --- CONFIGURACION DE SEMAFOROS (INDICES DENTRO DEL MISMO SET) ---
#define SEM_MONITOR 0   // Mutex: protege todo acceso a la memoria compartida (arranca en 1)
#define SEM_COMISARIA 1 // Semaforo contador: llamadas pendientes en Comisaria (arranca en 0)
#define SEM_HOSPITAL 2  // Semaforo contador: llamadas pendientes en Hospital  (arranca en 0)
#define SEM_FORENSE 3   // Semaforo contador: llamadas pendientes en Forense   (arranca en 0)

#define NUM_LLAMADAS_POR_OPERADOR 10

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

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

enum Centrales
{
    COMISARIA = 1 << 0, // bit 0 -> IDX_COMISARIA
    HOSPITAL = 1 << 1,  // bit 1 -> IDX_HOSPITAL
    FORENSE = 1 << 2    // bit 2 -> IDX_FORENSE
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

// Indices para acceder a colas/historiales/semaforos de cada central de forma

enum IndiceCentral
{
    IDX_COMISARIA = 0,
    IDX_HOSPITAL = 1,
    IDX_FORENSE = 2
};

// MAPEOS PARA IMPRESION EN CONSOLA / LOG
string nombre_incidentes[] = {"Accidente", "Crimen", "Enfermedad", "Muerte", "Robo", "Asesinato", "Herido", "Persecución", "Falsa Alarma"};
string nombre_prioridad[] = {"Alta", "Media", "Baja", "Falsa Alarma"};
string nombre_linea[] = {"Claro", "Tigo", "Hondutel"};
string nombre_tipo_llamada[] = {"Nacional", "Internacional"};
string nombreCentralIdx[] = {"Comisaria", "Hospital", "Forense"};
const int semDeCentral[] = {SEM_COMISARIA, SEM_HOSPITAL, SEM_FORENSE};

// ============================================================================
// Estructura de la llamada. Debe ser POD (sin std::string, sin punteros) para
// poder vivir dentro de arreglos en memoria compartida y copiarse tal cual
// entre procesos.
// ============================================================================
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
};

// Cola de prioridad que es mas segura para memoria compartida que la anterior
// Reemplaza al std::priority_queue (que usa heap
// dinamico y NO puede compartirse entre procesos) por un arreglo fijo
// ordenado por insercion.
struct ColaPrioridad
{
    static const int CAPACIDAD = 50;
    Llamada items[CAPACIDAD];
    int size; // Se debe poner en 0 manualmente tras el shmat (la memoria
              // compartida no ejecuta constructores en cada proceso).

    // Inserta manteniendo el arreglo ordenado (ALTA primero). Emula el
    // comportamiento del priority_queue original: el frente siempre es la
    // llamada más urgente pendiente en esa central.
    bool insertarOrdenado(const Llamada &ll)
    {
        if (size >= CAPACIDAD)
            return false;
        int pos = size;
        while (pos > 0 && items[pos - 1].prioridad > ll.prioridad)
        {
            items[pos] = items[pos - 1];
            pos--;
        }
        items[pos] = ll;
        size++;
        return true;
    }

    // Extrae siempre el elemento de mayor prioridad (el primero del arreglo)
    bool extraerFrente(Llamada &out)
    {
        if (size <= 0)
            return false;
        out = items[0];
        for (int i = 1; i < size; i++)
            items[i - 1] = items[i];
        size--;
        return true;
    }
};

// ============================================================================
// --- MONITOR DE CENTRALES (alojado en MEMORIA COMPARTIDA) ---
// Todo lo que estaba separado en 3 colas/3 historiales sueltos ahora vive en
// arreglos indexados por IDX_COMISARIA/IDX_HOSPITAL/IDX_FORENSE, así una sola
// funcion sirve para las 3 centrales.
// ============================================================================
struct UVI
{
    int contadorID;
    bool centralActiva;       // Bandera de apagado (protegida por SEM_MONITOR)
    ColaPrioridad colas[3];   // Llamadas pendientes por central
    Llamada historial[3][50]; // Llamadas ya atendidas por central
    int sizeHistorial[3];

    // Metodo para que un operador (productor) registre y clasifique una
    // llamada. Descarta las internacionales, asigna un ID unico y pone en cola la
    // llamada en cada central que le corresponda dependiendo su bitmask.
    // Devuelve false si la llamada fue descartada (internacional).
    bool registrarLlamada(int operadorId, Llamada llamada, int sem_id, ofstream &archivoLog)
    {
        bool internacional;
        string registro;
        bool enviado[3] = {false, false, false};

        // ---------------- SECCION CRITICA ----------------
        sem_wait(sem_id, SEM_MONITOR);

        internacional = (llamada.tipo == Internacional);
        if (!internacional)
        {
            llamada.id = ++contadorID;
            registro = "[Operador " + to_string(operadorId) + "] Registró ID: " + to_string(llamada.id) +
                       " | Tipo: " + nombre_tipo_llamada[llamada.tipo] +
                       " | Prioridad: " + nombre_prioridad[llamada.prioridad] +
                       " | " + string(llamada.descripcion) + " | Enviando a : ";

            for (int idx = 0; idx < 3; idx++)
            {
                if (llamada.Centralesbits & (1u << idx))
                {
                    enviado[idx] = colas[idx].insertarOrdenado(llamada);
                    registro += "[" + nombreCentralIdx[idx] + "] ";
                }
            }
        }

        sem_signal(sem_id, SEM_MONITOR);
        // ---------------- FIN SECCION CRITICA ----------------

        if (internacional)
        {
            string descarte = "[Monitor] LLAMADA INTERNACIONAL DESCARTADA (Operador: " +
                              to_string(operadorId) + ") - No califica como emergencia nacional.";
            registrarTexto(sem_id, archivoLog, descarte, "\033[1;31m"); // roja solo en terminal
            return false;
        }

        registrarTexto(sem_id, archivoLog, registro);

        // Señalizar FUERA del mutex: se avisa (sem_signal) unicamente a los
        // semaforos de las centrales que realmente recibieron la llamada.
        for (int idx = 0; idx < 3; idx++)
        {
            if (enviado[idx])
                sem_signal(sem_id, semDeCentral[idx]);
        }
        return true;
    }

    // Método para que un consumidor (Comisaria/Hospital/Forense) extraiga su
    // siguiente llamada pendiente y la mueva a su historial. Debe llamarse
    // DESPUÉS de haber hecho sem_wait sobre su semáforo correspondiente.
    bool atenderCentral(int idx, int sem_id, Llamada &out)
    {
        sem_wait(sem_id, SEM_MONITOR);
        bool hay = colas[idx].extraerFrente(out);
        if (hay && sizeHistorial[idx] < 50)
        {
            historial[idx][sizeHistorial[idx]++] = out;
        }
        sem_signal(sem_id, SEM_MONITOR);
        return hay;
    }

    // Escritura segura (thread/proceso-safe) en consola y en el log.
    // Si se pasa colorAnsi, la terminal muestra el texto en ese color, pero
    // el archivo de log siempre guarda texto plano (sin códigos de escape).
    static void registrarTexto(int sem_id, ofstream &archivoLog, const string &texto, const string &colorAnsi = "")
    {
        sem_wait(sem_id, SEM_MONITOR);
        if (!colorAnsi.empty())
            cout << colorAnsi << texto << "\033[0m" << "\n";
        else
            cout << texto << "\n";

        if (archivoLog.is_open())
        {
            archivoLog << texto << "\n";
            archivoLog.flush();
        }
        sem_signal(sem_id, SEM_MONITOR);
    }

    void mostrarEstadisticas()
    {
        cout << "\n=============================================\n";
        cout << "   ESTADÍSTICAS FINALES DEL MONITOR (SHM)\n";
        cout << "   (Solo Emergencias Nacionales Atendidas)   \n";
        cout << "=============================================\n";
        cout << "Total llamadas atendidas en Comisaría: " << sizeHistorial[IDX_COMISARIA] << "\n";
        cout << "Total llamadas atendidas en Hospital:  " << sizeHistorial[IDX_HOSPITAL] << "\n";
        cout << "Total llamadas atendidas en Forense:   " << sizeHistorial[IDX_FORENSE] << "\n";
        cout << "=============================================\n";
    }
};

// asigna prioridad y centrales de destino dependiendo del tipo de incidente
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

    int probabilidadTipo = rand() % 10;                          // 0..9
    ll.tipo = (probabilidadTipo < 9) ? Nacional : Internacional; // 90% nacional, 10% internacional

    ll.linea_telefonica = static_cast<Linea>(rand() % 3);
    ll.zona_llamada = static_cast<zona>(rand() % 2);
    ll.refuerzos_llamada = static_cast<refuerzos>(rand() % 2);

    string desc = "Incidente: " + nombre_incidentes[i] + " (Línea: " + nombre_linea[ll.linea_telefonica] + ")";
    strncpy(ll.descripcion, desc.c_str(), sizeof(ll.descripcion) - 1);
    ll.descripcion[sizeof(ll.descripcion) - 1] = '\0'; // asegura terminacion nula siempre

    return ll;
}

// Proceso generico (uno por central: Comisaria, Hospital, Forense)
// Cada uno duerme en su propio semaforo contador hasta que:
//   - llega una llamada real para su central (sem_signal desde el operador)
//   - el proceso padre ordena el apagado (un sem_signal extra al final).

void procesoCentral(int idx, UVI *monitor, int sem_id, ofstream &archivoLog)
{
    const string accion = (idx == IDX_HOSPITAL)    ? "Ambulancia enviada"
                          : (idx == IDX_COMISARIA) ? "Patrulla enviada"
                                                   : "Equipo forense enviado";
    int semNum = semDeCentral[idx];

    while (true)
    {
        sem_wait(sem_id, semNum); // duerme eficientemente hasta que haya trabajo o llegue el apagado

        Llamada ll;
        bool hay = monitor->atenderCentral(idx, sem_id, ll);

        if (hay)
        {
            string msg = "[" + nombreCentralIdx[idx] + "] ID: " + to_string(ll.id) +
                         " | Prioridad: " + nombre_prioridad[ll.prioridad] + " | " + accion;
            UVI::registrarTexto(sem_id, archivoLog, msg);
            usleep(200000); // simula el tiempo promedio de atencion
        }
        else
        {
            // No habia ninguna llamada real: este despertar solo puede venir
            // del apagado. Se confirma revisando la bandera bajo el mutex.
            sem_wait(sem_id, SEM_MONITOR);
            bool activa = monitor->centralActiva;
            sem_signal(sem_id, SEM_MONITOR);
            if (!activa)
                break;
        }
    }
}

int main()
{
    cout << "BIENVENIDO AL SISTEMA DE EMERGENCIAS UVI UNAH\n";
    cout << "----------------------------------------------------------------------\n";

    srand(static_cast<unsigned>(time(nullptr)) ^ getpid());

    // El log se abre ANTES de crear cualquier proceso hijo: asi todos
    // (operadores y centrales) heredan el mismo descriptor de archivo y
    // comparten el offset real de escritura del sistema operativo.
    ofstream recursoCompartido("logs_uvi.txt", ios::app);
    if (!recursoCompartido.is_open())
    {
        cerr << "Error al abrir el archivo de logs." << endl;
        return 1;
    }

    key_t claveSem = ftok("/bin", 65);
    key_t claveShm = ftok("/bin", 66);

    int semId = semget(claveSem, 4, 0666 | IPC_CREAT); // 4 semaforos: mutex + las 3 centrales
    int shmId = shmget(claveShm, sizeof(UVI), 0666 | IPC_CREAT);

    if (semId == -1 || shmId == -1)
    {
        cerr << "Error al inicializar los recursos IPC del sistema." << endl;
        return 1;
    }

    union semun init;
    init.val = 1;
    semctl(semId, SEM_MONITOR, SETVAL, init); // mutex: arranca "libre"
    init.val = 0;
    semctl(semId, SEM_COMISARIA, SETVAL, init); // sin llamadas pendientes al inicio
    semctl(semId, SEM_HOSPITAL, SETVAL, init);
    semctl(semId, SEM_FORENSE, SETVAL, init);

    UVI *monitor = (UVI *)shmat(shmId, NULL, 0);
    if (monitor == (void *)-1)
    {
        cerr << "Error al adjuntar la memoria compartida." << endl;
        return 1;
    }

    // Inicializacion manual de la memoria compartida no ejecuta constructores,
    // asi que cada campo debe arrancar en su estado inicial a mano.
    monitor->contadorID = 0;
    monitor->centralActiva = true;
    for (int idx = 0; idx < 3; idx++)
    {
        monitor->colas[idx].size = 0;
        monitor->sizeHistorial[idx] = 0;
    }

    // IMPORTANTE: se vacia el buffer de cout antes de cualquier fork().
    // Si no se hace, el texto ya escrito pero no volcado (el banner de
    // bienvenida) queda copiado en el buffer de CADA proceso hijo, y se
    // terminaría imprimiendo una vez por cada uno de ellos al salir.
    cout.flush();

    // --- 1) Se lanzan primero los CONSUMIDORES ---
    // Así ya están esperando en su semaforo cuando lleguen las primeras
    // llamadas de los operadores.
    pid_t pidsConsumidores[3];
    for (int idx = 0; idx < 3; idx++)
    {
        pid_t pid = fork();
        if (pid == -1)
        {
            cerr << "Error al crear proceso consumidor." << endl;
            return 1;
        }
        if (pid == 0)
        {
            // Proceso hijo: consumidor de una central especifica
            procesoCentral(idx, monitor, semId, recursoCompartido);
            shmdt(monitor);
            recursoCompartido.close();
            // exit()  para que se vacíen los buffers de cout;
            exit(0);
        }
        pidsConsumidores[idx] = pid;
    }

    // --- 2) Se lanzan los 2 OPERADORES ---
    pid_t pidsOperadores[2];
    for (int op = 0; op < 2; op++)
    {
        pid_t pid = fork();
        if (pid == -1)
        {
            cerr << "Error al crear proceso operador." << endl;
            return 1;
        }
        if (pid == 0)
        {
            // Proceso hijo: operador que genera y clasifica llamadas
            for (int i = 0; i < NUM_LLAMADAS_POR_OPERADOR; i++)
            {
                Llamada nueva = generarLlamadaAleatoria(rand() % 9000 + 1000);
                monitor->registrarLlamada(op + 1, nueva, semId, recursoCompartido);
                sleep(1);
            }
            shmdt(monitor);
            recursoCompartido.close();
            exit(0);
        }
        pidsOperadores[op] = pid;
    }

    // El proceso principal actua solo como administrador: no genera ni atiende
    // llamadas, unicamente coordina el ciclo de vida de los demas procesos.

    // 3) Esperar a que ambos operadores terminen de generar llamadas
    for (int op = 0; op < 2; op++)
        waitpid(pidsOperadores[op], nullptr, 0);

    // 4) Avisar el apagado: se baja la bandera bajo el mutex...
    sem_wait(semId, SEM_MONITOR);
    monitor->centralActiva = false;
    sem_signal(semId, SEM_MONITOR);

    // se despierta una ultima vez a cada consumidor. Los sem_signal de
    // llamadas reales ya estan contados en cada semaforo; este post extra
    // solo garantiza que, tras vaciar su cola, cada consumidor pueda salir
    // del bucle en vez de quedar dormido para siempre.
    sem_signal(semId, SEM_COMISARIA);
    sem_signal(semId, SEM_HOSPITAL);
    sem_signal(semId, SEM_FORENSE);

    // 5) Esperar a que los 3 consumidores terminen de vaciar sus colas
    for (int idx = 0; idx < 3; idx++)
        waitpid(pidsConsumidores[idx], nullptr, 0);

    // 6) Estadisticas finales y limpieza de recursos IPC
    monitor->mostrarEstadisticas();

    recursoCompartido.close();
    shmdt(monitor);
    shmctl(shmId, IPC_RMID, NULL);
    semctl(semId, 0, IPC_RMID);

    cout << "Proceso e IPCs finalizados limpiamente." << endl;
    return 0;
}