#include <iostream>
#include <thread>
#include <queue>
#include <string>
#include <chrono>
#include <fstream>

using namespace std;

//Estructura de la llamada recibida en la central de emergencias
struct Llamada {
    int idLlamada;
    string lineaTelefonica;
    string zonaLlamada;
    string descripcionLlamada;
};

//Colas de llamadas 
queue<Llamada> colaHospital;
queue<Llamada> colaComisaría;
queue<Llamada> colaForense;


//Variables a utilizar en Peterson
bool bandera[2] = {false, false};
int turno;

//Contador a utilizar para el incremento de las llamadas en las colas
int contadorID = 0;

//Archivo para guardar el registro de las llamadas
ofstream logFile("logs_uvi.txt", ios::app);

//Metodos para la Sección Crítica
void entrarSeccionCritica(int i){
    int j = 1 - i;
    bandera[i] = true;
    turno = j;
    while(bandera[j] && turno == j);
}

void salirSeccionCritica(int i){
    bandera[i] = false;
}

//Función para ingresar los registros al archivo de texto
void registrar(string text){
    logFile << text << endl;
    logFile.flush();
}

//Metodo para la central de llamadas UVI
void centralUVI(int op){
    for(int i = 0; i < 2; i++){
        
        Llamada call;
        entrarSeccionCritica(op);

        cout << "\nOperador " << op << endl;
        cout << "Linea: "; getline(cin, call.lineaTelefonica);
        cout << "Zona: "; getline(cin, call.zonaLlamada);
        cout << "Descripcion: "; getline(cin, call.descripcionLlamada);

        contadorID++;
        call.idLlamada = contadorID;

        string registroDeLlamada = 
        "[UVI] ID: " + to_string(call.idLlamada) + 
        " | Línea Telefónica: " + call.lineaTelefonica + 
        " | Zona de la llamada: " + call.zonaLlamada;

        cout << "\nREGISTRANDO LLAMADA: " << registroDeLlamada;
        registrar(registroDeLlamada);

        if(call.descripcionLlamada == "Robo" ||
            call.descripcionLlamada == "Asalto"){
            colaComisaría.push(call);
            registrar("[UVI] ID " + to_string(call.idLlamada) + 
            " -> Comisaria");
            cout << "\nEnviado a la Comisaría";
        }else if(call.descripcionLlamada == "Homicidio"){
            colaForense.push(call);
            registrar("[UVI] ID " + to_string(call.idLlamada) + 
            " -> Forense");
            cout << "\nEnviado a Forense";
        }else{
            colaHospital.push(call);
             registrar("[UVI] ID " + to_string(call.idLlamada) + 
            " -> Hospital");
            cout << "\nEnviado a Hospital";
        }

        salirSeccionCritica(op);

        this_thread::sleep_for(chrono::milliseconds(300));
    }
}

//Método para la recepción de las llamadas en el Hospital
void recepcionHospital(){
    while(true){
        if(!colaHospital.empty()){
            entrarSeccionCritica(0);
            Llamada llamada = colaHospital.front();
            colaHospital.pop();
            salirSeccionCritica(0);

            string mensaje =
                "[Hospital] ID: " + to_string(llamada.idLlamada) + 
                " | Ambulancia enviada";
            cout << "\n" << mensaje;
            registrar(mensaje);    
        }
        this_thread::sleep_for(chrono::milliseconds(500));
    }
}

//Método para la recepción de las llamadas en la Comisaría
void recepcionComisaria(){
    while(true){
        if(!colaComisaría.empty()){
            entrarSeccionCritica(1);
            Llamada llamada = colaComisaría.front();
            colaComisaría.pop();
            salirSeccionCritica(1);

            string mensaje =
                "[Comisaria] ID: " + to_string(llamada.idLlamada) + 
                " | Patrulla enviada";
            cout << "\n" << mensaje;
            registrar(mensaje);    
        }
        this_thread::sleep_for(chrono::milliseconds(500));
    }
}

//Cola y variables propias para la sección crítica con Dekker (Forense)
bool banderaDekker[2] = {false, false};
int turnoDekker = 0;
 
//Métodos para la Sección Crítica usando el algoritmo de Dekker
void entrarSeccionCriticaDekker(int i){
    int j = 1 - i;
    banderaDekker[i] = true;
    while(banderaDekker[j]){
        if(turnoDekker == j){
            banderaDekker[i] = false;
            while(turnoDekker == j);
            banderaDekker[i] = true;
        }
    }
}
 
void salirSeccionCriticaDekker(int i){
    turnoDekker = 1 - i;
    banderaDekker[i] = false;
}
 
//Método para la recepción de las llamadas en Forense (usando Dekker)
void recepcionForense(){
    while(true){
        if(!colaForense.empty()){
            entrarSeccionCriticaDekker(0);
            Llamada llamada = colaForense.front();
            colaForense.pop();
            salirSeccionCriticaDekker(0);
 
            string mensaje =
                "[Forense] ID: " + to_string(llamada.idLlamada) + 
                " | Equipo forense enviado";
            cout << "\n" << mensaje;
            registrar(mensaje);    
        }
        this_thread::sleep_for(chrono::milliseconds(500));
    }
}

//Método principal
int main(){
    registrar("===== Inicio del Sistema UVI ====");
    thread t1(centralUVI, 0);
    thread t2(centralUVI, 1);
    thread hospital(recepcionHospital);
    thread comisaria(recepcionComisaria);
    thread forense(recepcionForense);


    t1.join();
    t2.join();

    hospital.detach();
    comisaria.detach();
    forense.detach();

    registrar("===== Final del Sistema UVI ====");

    cout << "\nSistema FInalizado\n";

    return 0;


}