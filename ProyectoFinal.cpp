

#include <sys/sem.h> // semget, semctl, semop
#include <iostream>
#include <sys/errno.h> // para errores
#include <sys/ipc.h>   // para IPC
#include <wait.h>
#include <unistd.h> // para fork
#include <thread>   // mutex
#include <mutex>

using namespace std;

enum Centrales
{
    COMISARIA = 1 << 0, // 1  (0001)
    HOSPITAL = 1 << 1,  // 2  (0010)
    FORENSE = 1 << 2,   // 4  (0100)

};
// Se guardan los tipos de incidentes en un arreglo de strings para poder imprimirlos en pantalla.
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
    cout << "Iniciando el sistema de gestión de llamadas..." << endl;

    // Aquí iría la lógica principal del sistema, como la creación de procesos,
    // manejo de semáforos, y procesamiento de llamadas.

    return 0;
}

