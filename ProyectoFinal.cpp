

#include <sys/sem.h> // semget, semctl, semop
#include <iostream>
#include <sys/errno.h> // para errores
#include <sys/ipc.h>   // para IPC
#include <wait.h>
#include <unistd.h> // para fork
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <queue>

using namespace std;

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};
// Archivo de salida para registrar las llamadas de emergencia

ofstream RecursoCompartido;
// no implementado todavia

// REGLAS DEL SEMAFORO PARA EL PROCESO DE REGISTRO DE LLAMADAS DE EMERGENCIA
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

// Simulación de la generación de llamadas de emergencia aleatorias
string generarLlamadaAleatoria(int operador)
{
    const string prioridades[] = {"Alta", "Media", "Baja"};
    const string tipos[] = {"Nacional", "Internacional"};
    const string zonas[] = {"Urbana", "Rural"};
    const string refuerzos[] = {"Sí", "No"};
    const string incidentes[] = {"Accidente", "Crimen", "Enfermedad", "Muerte"};
    const string centrales[] = {"Comisaria", "Hospital", "Forense"};
    int p = rand() % 3;
    int t = rand() % 2;
    int z = rand() % 2;
    int r = rand() % 2;
    int c = rand() % 4;

    if (c == 0) // Accidente
    {
        cout << "Se requiere la presencia de la central de " << centrales[1] << endl; // Hospital
    }
    else if (c == 1) // Crimen
    {
        cout << "Se requiere la presencia de la central de " << centrales[0] << endl; // Comisaria
    }
    else if (c == 2) // Enfermedad
    {
        cout << "Se requiere la presencia de la central de " << centrales[1] << endl; // Hospital
    }
    else if (c == 3) // Muerte
    {
        cout << "Se requiere la presencia de la central de " << centrales[2] << endl; // Forense
    }

    return "Operador " + to_string(operador) + " registró una llamada " + tipos[t] +
           " de prioridad " + prioridades[p] + ", zona " + zonas[z] + ", refuerzos: " + refuerzos[r] + ", central: " + centrales[c] + ", incidente: " + incidentes[c];
}

enum Centrales
{
    COMISARIA = 1 << 0, // 1  (0001)
    HOSPITAL = 1 << 1,  // 2  (0010)
    FORENSE = 1 << 2,   // 4  (0100)

};

string nombre_centrales[] = {"Comisaria", "Hospital", "Forense"};
enum Incidentes
{
    ACCIDENTE,
    CRIMEN,
    ENFERMEDAD,
    MUERTE
};
string nombre_incidentes[] = {"Accidente", "Crimen", "Enfermedad", "Muerte"};
enum Prioridad
{
    ALTA,
    MEDIA,
    BAJA
};
string nombre_prioridad[] = {"Alta", "Media", "Baja"};

enum Linea
{
    CLARO,
    TIGO,
    HONDUTEL
};
string nombre_linea[] = {"Claro", "Tigo", "Hondutel"};

enum TipoLlamada
{
    Nacional,
    Internacional

};
string nombre_tipo_llamada[] = {"Nacional", "Internacional"};

enum zona
{
    rural,
    urbana
};
string nombre_zona[] = {"Rural", "Urbana"};

enum refuerzos
{
    SI,
    NO
};
string nombre_refuerzos[] = {"Sí", "No"};

struct Llamada
{
    int id;
    Prioridad prioridad;
    unsigned int Centrales = 0;
    string descripcion;
    TipoLlamada tipo;
    string descripcion_llamada;
    Linea linea_telefonica;
    zona zona_llamada;
    refuerzos refuerzos_llamada; // La central registra a comsiaria, hospital y forense si es necesario llevar mas de una unidad de refuerzo a la escena del incidente.
};

int main()
{
    cout << "Bienvenido al sistema de registro de llamadas de emergencia" << endl;

    // Codigo que garantiza los numeros aleatorios diferentes en cada ejecucion del programa
    srand(static_cast<unsigned>(time(nullptr)));

    RecursoCompartido.open("logs_uvi.txt", ios::app);
    if (!RecursoCompartido.is_open())
    {
        cerr << "No se pudo abrir el archivo logs_uvi.txt" << endl;
        return 1;
    }

    key_t clave = ftok("/bin", 65);
    if (clave == -1)
    {
        cerr << "Error al generar la clave unica" << endl;
        return 1;
    }

    int sem_id = semget(clave, 2, 0666 | IPC_CREAT);
    if (sem_id == -1)
    {
        cerr << "Error al crear el semáforo" << endl;
        return 1;
    }

    // Inicialización de los semáforos
    union semun init;
    init.val = 1;
    semctl(sem_id, 0, SETVAL, init);
    init.val = 0;
    semctl(sem_id, 1, SETVAL, init);

    pid_t pid = fork();
    if (pid == 0)
    {
        for (int i = 0; i < 5; ++i)
        {
            sem_wait(sem_id, 1); // Espera a que el operador 1 registre la llamada

            string texto = generarLlamadaAleatoria(2);
            RecursoCompartido << "[Operador 2] " << texto << endl;
            RecursoCompartido.flush();
            cout << "[Operador 2] registrando llamada" << endl;

            sem_signal(sem_id, 0);
            sleep(1);
        }
    }
    else
    {
        for (int i = 0; i < 5; ++i)
        {
            sem_wait(sem_id, 0); // Espera a que el operador 2 registre la llamada antes de registrar la suya

            string texto = generarLlamadaAleatoria(1);
            RecursoCompartido << "[Operador 1] " << texto << endl;
            RecursoCompartido.flush();
            cout << "[Operador 1] registrando llamada" << endl;

            sem_signal(sem_id, 1);
            sleep(1);
        }

        wait(nullptr);
        RecursoCompartido.close();
        semctl(sem_id, 0, IPC_RMID);
        cout << "Proceso finalizado. Revise el archivo logs_uvi.txt" << endl;
    }

    return 0;
}
