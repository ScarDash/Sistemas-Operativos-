#include <iostream>
#include <thread>
#include <queue> // Incluye std::queue y std::priority_queue
#include <string>
#include <chrono>
#include <fstream>
#include <vector>

using namespace std;

// Estructura de la llamada recibida en la central de emergencias
struct Llamada
{
    int idLlamada;
    string lineaTelefonica;
    string zonaLlamada;
    string descripcionLlamada;
    int prioridad; // 1: Baja, 2: Media, 3: Alta

    // ordenea prioridad de MAYOR a MENOR prioridad (3 > 2 > 1)
    bool operator<(const Llamada &otra) const
    {
        return prioridad < otra.prioridad;
    }
};

// MODIFICADO: Las colas ahora son de prioridad (priority_queue)
priority_queue<Llamada> colaHospital;
priority_queue<Llamada> colaComisaría;

// Variables a utilizar en Peterson
bool bandera[2] = {false, false};
int turno;

// Contador a utilizar para el incremento de las llamadas en las colas
int contadorID = 0;

// Archivo para guardar el registro de las llamadas
ofstream logFile("logs_uvi.txt", ios::app);

// Estructura de datos para simular llamadas entrantes
struct LlamadaSimulada
{
    string linea;
    string zona;
    string descripcion;
    int prioridad; // 1: Baja, 2: Media, 3: Alta
};

// Banco de pruebas con múltiples llamadas simuladas
vector<LlamadaSimulada> llamadasSimuladas = {
    {"911-1111", "Zona Norte", "Accidente de transito", 1},
    {"911-2222", "Zona Sur", "Robo", 2},
    {"911-3333", "Zona Este", "Infarto", 3}, // Alta prioridad médica
    {"911-4444", "Zona Oeste", "Asalto", 3}, // Alta prioridad policial
    {"911-5555", "Centro", "Incendio doméstico", 2},
    {"911-6666", "Zona Sur", "Pelea callejera (Robo)", 1}};

// Metodos para la Sección Crítica (Algoritmo de Peterson)
void entrarSeccionCritica(int i)
{
    int j = 1 - i;
    bandera[i] = true;
    turno = j;
    while (bandera[j] && turno == j)
        ;
}

void salirSeccionCritica(int i)
{
    bandera[i] = false;
}

void registrar(string text)
{
    logFile << text << endl;
    logFile.flush();
}

// Método para la central de llamadas UVI
void centralUVI(int op)
{
    // El operador '0' toma las llamadas pares, el operador '1' toma las impares
    for (size_t i = op; i < llamadasSimuladas.size(); i += 2)
    {
        Llamada call;
        call.lineaTelefonica = llamadasSimuladas[i].linea;
        call.zonaLlamada = llamadasSimuladas[i].zona;
        call.descripcionLlamada = llamadasSimuladas[i].descripcion;
        call.prioridad = llamadasSimuladas[i].prioridad;

        // SECCIÓN CRÍTICA: Acceso a contadorID y a las Colas globales
        entrarSeccionCritica(op);

        contadorID++;
        call.idLlamada = contadorID;

        string registroDeLlamada =
            "[UVI - Op " + to_string(op) + "] ID: " + to_string(call.idLlamada) +
            " | Línea: " + call.lineaTelefonica +
            " | Zona: " + call.zonaLlamada +
            " | Prioridad: " + to_string(call.prioridad) +
            " | Tipo: " + call.descripcionLlamada;

        cout << "\nREGISTRANDO LLAMADA: " << registroDeLlamada << endl;
        registrar(registroDeLlamada);

        if (call.descripcionLlamada.find("Robo") != string::npos ||
            call.descripcionLlamada.find("Asalto") != string::npos)
        {
            colaComisaría.push(call); // Se inserta y auto-ordena por prioridad
            registrar("[UVI] ID " + to_string(call.idLlamada) + " -> Comisaria (Prio: " + to_string(call.prioridad) + ")");
            cout << "[UVI] Enviado a la Comisaría (Prio: " << call.prioridad << ")" << endl;
        }
        else
        {
            colaHospital.push(call); // Se inserta y auto-ordena por prioridad
            registrar("[UVI] ID " + to_string(call.idLlamada) + " -> Hospital (Prio: " + to_string(call.prioridad) + ")");
            cout << "[UVI] Enviado a Hospital (Prio: " << call.prioridad << ")" << endl;
        }

        salirSeccionCritica(op);

        // Simulamos un pequeño retraso entre la recepción de llamadas
        this_thread::sleep_for(chrono::milliseconds(200));
    }
}

// Método para la recepción de las llamadas en el Hospital
void recepcionHospital()
{
    while (true)
    {
        if (!colaHospital.empty())
        {
            // MODIFICADO: En priority_queue se extrae con .top() en lugar de .front()
            Llamada llamada = colaHospital.top();
            colaHospital.pop();

            string mensaje = "[Hospital] ID: " + to_string(llamada.idLlamada) +
                             " [Prioridad: " + to_string(llamada.prioridad) + "] | Ambulancia enviada";
            cout << "\n>>> ATENCIÓN HOSPITAL: " << mensaje << endl;
            registrar(mensaje);
        }
        this_thread::sleep_for(chrono::milliseconds(600));
    }
}

// Método para la recepción de las llamadas en la Comisaría
void recepcionComisaria()
{
    while (true)
    {
        if (!colaComisaría.empty())
        {
            // MODIFICADO: En priority_queue se extrae con .top() en lugar de .front()
            Llamada llamada = colaComisaría.top();
            colaComisaría.pop();

            string mensaje = "[Comisaria] ID: " + to_string(llamada.idLlamada) +
                             " [Prioridad: " + to_string(llamada.prioridad) + "] | Patrulla enviada";
            cout << "\n>>> ATENCIÓN POLICÍA: " << mensaje << endl;
            registrar(mensaje);
        }
        this_thread::sleep_for(chrono::milliseconds(600));
    }
}

int main()
{
    registrar("===== Inicio del Sistema UVI ====");
    cout << "=== SIMULACIÓN DE CENTRAL UVI CON PRIORIDADES INICIADA ===\n";

    // Creamos los hilos de los operadores
    thread t1(centralUVI, 0);
    thread t2(centralUVI, 1);

    // Creamos los hilos de despacho
    thread hospital(recepcionHospital);
    thread comisaria(recepcionComisaria);

    // Esperamos a que ambos operadores terminen de procesar el banco de llamadas
    t1.join();
    t2.join();

    // Damos un breve tiempo para que los despachos terminen de procesar lo que quede en cola
    this_thread::sleep_for(chrono::seconds(3));

    // Forzamos el cierre de los hilos de fondo de forma segura en la simulación
    hospital.detach();
    comisaria.detach();

    registrar("===== Final del Sistema UVI ====");
    cout << "\n=== Sistema Finalizado con Éxito ===\n";

    return 0;
}