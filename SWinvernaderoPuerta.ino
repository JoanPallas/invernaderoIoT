#include <PubSubClient.h>
#include <DHT.h>
#include <Stepper.h>
#include <ESP8266WiFi.h>

const char* ssid = "INVERNADERO"; // red wifi a la que nos conectamos
const char* password = "invernadero"; // contraseña de la red a la que nos conectamos
const char* mqtt_server = "192.168.1.148";  // IP de la Raspberry (broker) 

WiFiClient espClient; //declaramos el cliente Wifi
PubSubClient client(espClient);
bool primeraVez=true;
String clientId;

//obtener hora
String datoHora=""; //respuesta enviada desde la Raspi a mi solicitud.
unsigned long timeHour=0;
#define timeLecturaHour 900000 //15'

//motor puerta
Stepper motorPuerta(200,D5,D6);  // pasos/vuelta, cw+, clk+ 
#define velocidad 3000 //rpm
#define timeCierre 3500  
#define timeApertura 3500
#define timeSolenoide 500
bool cerrar=false;  //booleanos internos del arduino
bool abrir=false;
bool buttonCierre=false; //booleanos que se activan cuando se recibe un valor desde el server
bool buttonAbrir=false;
bool abriendoPuerta=false;
bool cerrandoPuerta=false;
unsigned long tiempoOC = 0;

//sensor Temperatura interno
#define DHTTYPE DHT22 
#define DHTPIN 13 // GPIO13--> D7
#define timeLecturaT 900000 //15'
#define timeError 5000 //5"
DHT dht(DHTPIN, DHTTYPE);  //creamos el objeto dht con sus parametros
float temperatura; // variables para almacenar valores de temperatura y humedad ofrecidas por el sensor
float humedad;
unsigned long timeTemp = 0;  //variable para saber cada cuanto enviar el valor
bool errorLecturaDHT=false;
bool inicioConexionT=true;

//sensore Humedad Suelo Bancal Corto
#define timeLecturaHum 900000 //15' 
unsigned long timeHum = 0;
int lecturaHumCorto;
int lecturaPorcentajeHumCorto;
bool inicioConexionH=true;
bool errorLecturaHum=false;

//cama caliente
#define rangeTemp 20  //rango temperatura
#define timeApagado 1800000 //30'  //[600000]-->10'   [1200000]-->'20  [1800000]-->30'  [2400000]-->40'  [3000000]-->50'  [3600000]-->60'
#define timeEncendido 1200000 //20'
unsigned long tiempoResistencia = 0;
bool encendido=true;

//auto apertura/cierre Puerta
#define timePuerta 3600000 //1h ; cada hora se comprueba el auto de la Puerta
unsigned long tiempoPuerta = 0;
bool primeraApertura=true;
bool primerCierre=true;


void setup_wifi() {   // We start by connecting to a WiFi network
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  bool empieza=false;
  for (int i = 0; i < length; i++) {   //vamos imprimiendo el valor recibido caracter por caracter en el monitor serie
    Serial.print((char)payload[i]);
  }
  Serial.println();
  //condicionales para ejecutar en función del topic
  if ((String(topic))=="invernadero/ventilacion/puerta"){
    tiempoOC=millis();
    if ((char)payload[0] == '1') { //abrir puerta
      buttonAbrir=true;
    }
    else {  //cerrar puerta
      buttonCierre=true;   
    }
  }
  else if ((String(topic))=="invernadero/conexion/esp8266zonaPuerta"){ 
    if ((char)payload[0] == 'P') { 
      client.publish("invernadero/conexion/esp8266zonaPuerta",(clientId + " conectado exitosamente!").c_str());
    }
  }
  else if ((String(topic))=="invernadero/camaCaliente/OnOff"){
    if ((char)payload[0] == '1') { //encender resistencia calefactora
      if (temperatura<=rangeTemp){  //si la T interior me lo permite
        digitalWrite(D4,HIGH);  //aqui se enciende pero más adelante se tendrán que aplicar las condiciones 
        client.publish("invernadero/camaCaliente/estado","EstadoR--> 1"); //enviamos que acabamos de encenderla
        encendido=true;
        tiempoResistencia=millis();
      }
    }
    else {  //apagar resistencia
      digitalWrite(D4,LOW);  
      client.publish("invernadero/camaCaliente/estado","EstadoR--> 0"); //enviamos que acabamos de pararla
      encendido=false;
      tiempoResistencia=millis();
    }
  }
  else if ((String(topic))=="invernadero/time/hour"){
    if ((char)payload[0] == 'E') { //me envian la hora con este formato: Envio 18
      datoHora="";
      for (int y = 0; y < length; y++) {   //nos guardamos el valor de la hora que nos envia
        if(empieza){
          datoHora=datoHora+(char)payload[y];
        }
        if((char)payload[y]==' '){
          if(empieza){
            empieza=false;
            if(datoHora=="7"){
              primeraApertura=true;
              primerCierre=true;
            }
          }
          else{
            empieza=true;
          }
        }
      }
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    clientId = "ESP8266Client-zPuerta";    //nombre fijo para identidicar el esp
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      if(primeraVez){
        primeraVez=false;
        client.publish("invernadero/conexion/esp8266zonaPuerta", (clientId + " conectado exitosamente!").c_str());
        client.publish("invernadero/sensores/finalCarrera/puerta", ("PosicionPuerta--> " + (String(digitalRead(D1)))).c_str());
        client.publish("invernadero/time/hour","Peticion hora"); //peticion 
        client.publish("invernadero/camaCaliente/estado","EstadoR--> 1");
        if(digitalRead(D1)){
          abrir=true;
        }
        else{
          cerrar=true;
        }
      }
      else{
        client.publish("invernadero/conexion/esp8266zonaPuerta", (clientId + " reconectado exitosamente!").c_str());
        inicioConexionT=true; //cada vez que se reconecte nos vuelva a enviar estos valores.
        inicioConexionH=true;
      }
      
      // ... and resubscribe
      client.subscribe("invernadero/ventilacion/puerta");
      client.subscribe("invernadero/conexion/esp8266zonaPuerta");  //tambien para publicar, seria del estilo peticion/respuesta
      client.subscribe("invernadero/time/hour");  //para recibir Tinterior y así abrir/cerrar puerta
      client.subscribe("invernadero/camaCaliente/OnOff"); 
    } 
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  //D7 ocupado por DHT22
  pinMode(BUILTIN_LED, OUTPUT);     // led interno para hacer comprobaciones
  digitalWrite(BUILTIN_LED,HIGH);  //inicialmente apagado el led interno
  pinMode(D4, OUTPUT); //cama caliente
  digitalWrite(D4,HIGH); //lo ponemos en high para que dentro del loop no ejecute esta accion siempre que temperatura<=rangeTemp y encendido
  pinMode(D1,INPUT);  //final de carrera 
  pinMode(D2,OUTPUT);  //cierre eléctrico
  pinMode(A0,INPUT);  //humedadSueloCorto
  Serial.begin(9600);  //iniciamos comunicacion serie
  setup_wifi();  //llamamos a esta función para conectarnos a la red wifi
  client.setServer(mqtt_server, 1883);  //decimos la IP y el puerto por el que se comunica 
  client.setCallback(callback);  //cuando recibe un valor a uno de sus tópics va a la funcion callback
  motorPuerta.setSpeed(velocidad); //ajustamos el valor de velocidad de la ventana inicial
  dht.begin();  //iniciamos el "objeto" dht para poder utilizarlo
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if((millis()>=(timeHour+timeLecturaHour))){ //cada 15' hago peticion de hora, ahora cada 10" de prueba
    timeHour=millis();
    client.publish("invernadero/time/hour","Peticion hora"); //peticion 
  }

  if((millis()>=(tiempoPuerta+timePuerta))){ //cada hora hago comprobación auto de Ventana
    tiempoPuerta=millis();
    if (temperatura>=20 && primeraApertura && datoHora!="7"){
      primeraApertura=false;
      buttonAbrir=true; //simulamos pulsación de apertura desde el server
    }
    else if(temperatura<=18 && primerCierre){
      primerCierre=false;
      buttonCierre=true; //simulamos pulsación de cierre desde el server
    }
  }
  
  //proceso de apertura/cierre puerta  
  if (buttonAbrir && digitalRead(D1) && abrir){  //si recibimos pulsacion desde el server, el final de carrera detecta como cerrado y abrir==true
    abriendoPuerta=true;
  }
  else if(cerrar && buttonCierre && !digitalRead(D1)){  //si bool interno dice que toca cerrar y recibimos valor desde server y no detecta el final de carrera
    cerrandoPuerta=true;
  }
  if(abriendoPuerta){
    digitalWrite(D2,HIGH);  //se desbloquea el cierre electrico
    if(millis()>=(tiempoOC+timeSolenoide)){ // pasados unos ms se abre la puerta
      motorPuerta.step(-200);
    }
    if(millis()>=(tiempoOC+timeApertura)){  //si se cumple el tiempo cambiamos de estado y sabemos que ya habremos abierto
      abriendoPuerta=false;
      abrir=false;
      cerrar=true;
      digitalWrite(D2,LOW);
      buttonAbrir=false;
      tiempoOC=millis();
      client.publish("invernadero/sensores/finalCarrera/puerta", ("PosicionPuerta--> " + (String(digitalRead(D1)))).c_str()); //enviamos estado del final de carrera
    }
  }
  else if(cerrandoPuerta){
    motorPuerta.step(200);
    if(digitalRead(D1)){  //si el final de carrera detecta acabamos //en high esta cerrada
      cerrandoPuerta=false;
      cerrar=false;
      abrir=true;
      buttonCierre=false;
      tiempoOC=millis();
      client.publish("invernadero/sensores/finalCarrera/puerta", ("PosicionPuerta--> " + (String(digitalRead(D1)))).c_str());
    }
  }

  //condicion para saber si nos toca enviar mensaje de temperatura
  if ((millis()>=(timeTemp+timeLecturaT)) || errorLecturaDHT || inicioConexionT){ //si ha transcurrido el tiempo establecido o ha habido error de lectura en la anterior o acabamos de conectarnos 
    if(errorLecturaDHT){  //hacemos este condicional para que en caso de error no nos pete a msn, así podemos poner un tiempo entre medias
        if(millis()>=(timeTemp+timeError)){  //tiempo que se esèra entre msn y msn de error
          temperaturaDHT();   //llamamos a la función que lee el valor y envia
        }
    }
    else{
      temperaturaDHT();  //llamamos a la función que lee el valor y envia
    }
  }

  //condicion para saber si nos toca eviar valor de humedad suelo
  if((millis()>=(timeHum+timeLecturaHum)) || inicioConexionH || errorLecturaHum){  //si por tiempo toca lo leo, lo guardo y lo envio
    if(errorLecturaHum){  //hacemos este condicional para que en caso de error no nos pete a msn, así podemos poner un tiempo entre medias
        if(millis()>=(timeHum+timeError)){  //tiempo que se esèra entre msn y msn de error
          humedadSuelo();   //llamamos a la función que lee el valor y envia
        }
    }
    else{
      humedadSuelo();  //llamamos a la función que lee el valor y envia
    }
  }

  //condicion para saber si activamos o no la cama caliente
  if (temperatura<=rangeTemp){
    if(encendido){
      if(millis()>=(tiempoResistencia+timeEncendido)){
        tiempoResistencia=millis();
        digitalWrite(D4,LOW);
        client.publish("invernadero/camaCaliente/estado","EstadoR--> 0"); //enviamos que acabamos de pararla
        encendido=false;
      }
    }
    else{
      if(millis()>=(tiempoResistencia+timeApagado)){
        tiempoResistencia=millis();
        digitalWrite(D4,HIGH);
        client.publish("invernadero/camaCaliente/estado","EstadoR--> 1"); //enviamos que acabamos de endencerla
        encendido=true;
      }
    }
  }
  else{ //si la T supera el rango impuesta se para
    if(digitalRead(D4)){  //si venimos de estar encendidos, apagamos
      digitalWrite(D4,LOW);
      client.publish("invernadero/camaCaliente/estado","EstadoR--> 0"); //enviamos que acabamos de pararla
      encendido=false;
      tiempoResistencia=millis();
    }
  }
  
}

void temperaturaDHT(){
  timeTemp=millis();
  inicioConexionT=false;
  temperatura = dht.readTemperature();
  humedad = dht.readHumidity();
  if (isnan(humedad) || isnan(temperatura)) { //si no ha podido leer valor correcto de una de las dos variables del sensor...
    errorLecturaDHT=true;
    Serial.println("Failed to read from DHT22 sensor!");
    client.publish("invernadero/sensores/DHT/interior","Failed to read from DHT22 sensor inside!");  //nos manda mensaje de error y se va del programa
    return;
  }
  errorLecturaDHT=false;
  client.publish("invernadero/sensores/DHT/interior",("TemperaturaInt--> " + (String(temperatura)) + " ºC").c_str()); //leer desde espVentana
  client.publish("invernadero/sensores/DHT/interior",("HumedadInt--> " + (String(humedad)) + " %").c_str());
}

void humedadSuelo(){
  timeHum=millis();
  inicioConexionH=false;
  lecturaHumCorto=analogRead(A0);
  if(lecturaHumCorto>785 || lecturaHumCorto<350){
    client.publish("invernadero/sensores/humedadSuelo/bancalCorto","HumSueloBancalCorto--> Error"); 
    errorLecturaHum=true;
  }
  else{ //si es un valor coherente miramos condiciones de riego y enviamos msn
    errorLecturaHum=false;
    lecturaPorcentajeHumCorto=map(lecturaHumCorto,785,350,0,100); 
    //condicion para saber en funcion del valor de humedad del suelo si tenemos que encender el riego  
    if(lecturaHumCorto>=650 && (datoHora=="7" || datoHora=="18")){ //si sube de 650 y la hora está entre una de estas le tocaría regar 
      //650(30%) si valor<=30% humedad 
      client.publish("invernadero/sensores/humedadSuelo/bancalCorto",("HumSueloBancalCorto--> " + (String(lecturaPorcentajeHumCorto)) + " %").c_str());
      client.publish("invernadero/sensores/humedadSuelo/bancalCorto/riego","1"); //enviado la info a espVentana de que toca regar
    }
    //condicion para saber en funcion del valor de humedad del suelo si tenemos que apagar el riego
    else if(lecturaHumCorto<=500){ 
      //500(65%) si valor>=65% humedad   
      client.publish("invernadero/sensores/humedadSuelo/bancalCorto",("HumSueloBancalCorto--> " + (String(lecturaPorcentajeHumCorto)) + " %").c_str());
      client.publish("invernadero/sensores/humedadSuelo/bancalCorto/riego","0"); //enviado la info a espVentana de que toca dejar de regar
    }
    else {
      client.publish("invernadero/sensores/humedadSuelo/bancalCorto",("HumSueloBancalCorto--> " + (String(lecturaPorcentajeHumCorto)) + " %").c_str());
    }
  }
}
