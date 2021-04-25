#include <ESP8266WiFi.h>
#include <DHT.h>
#include <Stepper.h>
#include <PubSubClient.h>

const char* ssid = "INVERNADERO"; // red wifi a la que nos conectamos
const char* password = "invernadero"; // contraseña de la red a la que nos conectamos
const char* mqtt_server = "192.168.1.148";  // IP de la Raspberry (broker) 

WiFiClient espClient; //declaramos el cliente Wifi
PubSubClient client(espClient);
bool primeraVez=true;
String clientId;

//obtener hora
String datoHora; //respuesta enviada desde la Raspi a mi solicitud.
unsigned long timeHour=0;
#define timeLecturaHour 900000 //15'

//obtener temperatura interior
String tempInterior;
float tempInteriorNum;

//tiempo de riego 
#define tRiego 3600000 //60' 
unsigned long tRiegoLargo=0;  //variables para almacenar el tiempo que lleva cada linea regando
unsigned long tRiegoCorto=0;
unsigned long tRiegoFuera=0;

//motor ventana
Stepper motorVentana(200,D5,D6);  // pasos/vuelta, cw+, clk+
#define velocidad 4000 //rpm
#define timeApertura 23500
#define timeDesbloqueo 250
#define timeDesDesbloqueo 100
bool cerrar=false;  //booleanos internos 
bool abrir=false;
bool desbloqueo=false;  
bool buttonCierre=false; //booleanos que se activan cuando se recibe un valor desde el server
bool buttonAbrir=false;
unsigned long tiempoOC = 0;

//sensor Temperatura externo
#define DHTTYPE DHT22 
#define DHTPIN 13 // GPIO13-->D7 
#define timeLecturaT 900000 //15'
#define timeError 5000 //5"
DHT dht(DHTPIN, DHTTYPE);  //creamos el objeto dht con sus parametros
float temperatura; // variables para almacenar valores de temperatura y humedad ofrecidas por el sensor
float humedad;
unsigned long timeTemp = 0;  //variable para saber cada cuanto enviar el valor
bool errorLecturaDHT=false;
bool inicioConexionT=true;

//sensores Humedad Suelo
#define timeLecturaHum 900000 //15'
unsigned long timeHum = 0;
int lecturaHumLargo;  // variables para almacenar valores de humedad suelo y porcentaje
int lecturaPorcentajeHumLargo;
bool inicioConexionH=true;
bool errorLecturaHum=false;

//auto apertura/cierre Ventana
#define timeVentana 3600000 //1h ; cada hora se comprueba el auto de la Ventana
unsigned long tiempoVentana = 0;
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
  bool empieza=false; //auxiliar para leer datos de algunos mensajes que nos envian
  for (int i = 0; i < length; i++) {   //vamos imprimiendo el valor recibido caracter por caracter en el monitor serie
    Serial.print((char)payload[i]);
  }
  Serial.println();
  //condicionales para ejecutar en función del topic
  if ((String(topic))=="invernadero/electrovalvulas/general"){  //si recibo un mensaje con este tópico
    if ((char)payload[0] == '1') {  // si el valor es este
      tRiegoLargo=millis();
      tRiegoCorto=millis();
      tRiegoFuera=millis();  
      digitalWrite(D3, HIGH);
      digitalWrite(D2, HIGH);
      digitalWrite(D4, HIGH);
    }
    else {
      digitalWrite(D3, LOW);
      digitalWrite(D2, LOW);
      digitalWrite(D4, LOW);
    }
  }
  else if ((String(topic))=="invernadero/electrovalvulas/bancalLargo"){
    if ((char)payload[0] == '1') {
      tRiegoLargo=millis();
      digitalWrite(D3, HIGH);
    } 
    else {
      digitalWrite(D3, LOW);
    }
  }
  else if ((String(topic))=="invernadero/electrovalvulas/bancalCorto"){
    if ((char)payload[0] == '1') { 
      tRiegoCorto=millis();
      digitalWrite(D2, HIGH);
    }
    else {
      digitalWrite(D2, LOW);
    }
  }
  else if ((String(topic))=="invernadero/electrovalvulas/bancalFuera"){
    if ((char)payload[0] == '1') {  
      tRiegoFuera=millis();
      digitalWrite(D4, HIGH);
    }
    else {
      digitalWrite(D4, LOW);
    }
  }
  else if ((String(topic))=="invernadero/sensores/humedadSuelo/bancalCorto/riego"){ //valor de hum recibido desde el otro ESP
    if ((char)payload[0] == '1') {  //condiciones indican que toca regar
      if(!digitalRead(D2)){ //no está regando
        tRiegoCorto=millis();
        digitalWrite(D2, HIGH); //riego
        client.publish("invernadero/sensores/humedadSuelo/bancalCorto", "RegandoBancalCorto--> 1 activaciónAutomática");
      }
    }
    else {  //condiciones indican que toca parar el riego
      digitalWrite(D2, LOW);
      client.publish("invernadero/sensores/humedadSuelo/bancalCorto", "RegandoBancalCorto--> 0 desactivaciónAutomática");
    }
  }
  else if ((String(topic))=="invernadero/ventilacion/ventana"){
    tiempoOC=millis();
    if ((char)payload[0] == '1') { //abrir ventana
      if (!buttonCierre && !cerrar){  //si bool interno no dice que toca cerrar y no estoy cerrando
        buttonAbrir=true; //toca abrir
      }
    }
    else {  //cerrar ventana
      if (!buttonAbrir && !desbloqueo && !abrir){ //si bool interno no dice que toca abrir y no estoy abriendo ni desbloqueando
        buttonCierre=true;  //toca cerrar
      }  
    }
  }
  else if ((String(topic))=="invernadero/conexion/esp8266zonaVentana"){ 
    if ((char)payload[0] == 'P') { 
      client.publish("invernadero/conexion/esp8266zonaVentana",(clientId + " conectado exitosamente!").c_str());
    }
  }
  else if ((String(topic))=="invernadero/time/hour"){
    if ((char)payload[0] == 'E') { //me envian la hora con este formato: Envio 18
      datoHora="";
      for (int y = 0; y < length; y++) {    //nos guardamos el valor de la hora que nos envia
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
  else if ((String(topic))=="invernadero/sensores/DHT/interior"){
    if ((char)payload[0] == 'T') { //me envian la temperatura interior con este formato: "TemperaturaInt--> 18.50 ºC
      tempInterior="";
      for (int y = 0; y < length; y++) {    //nos guardamos el valor de la temperatura que nos envia
        if(empieza){
          tempInterior=tempInterior+(char)payload[y];
        }
        if((char)payload[y]==' '){
          if(empieza){
            empieza=false;
            tempInteriorNum=tempInterior.toFloat();  //lo convertimos a numero una vez leido el valor entero
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
    clientId = "ESP8266Client-zVentana";    //nombre fijo para identidicar el esp
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      if(primeraVez){
        primeraVez=false;
        client.publish("invernadero/conexion/esp8266zonaVentana", (clientId + " conectado exitosamente!").c_str()); //publicamos conexion y valores iniciales
        client.publish("invernadero/sensores/finalCarrera/ventana", ("PosicionVentana--> " + (String(digitalRead(D1)))).c_str());
        client.publish("invernadero/time/hour","Peticion hora"); //peticion 
        if(digitalRead(D1)){  //en función del final de carrera asbemos el estado de la ventana
          desbloqueo=true;
        }
        else{
          cerrar=true;
        }
      }
      else{ //si se reconecta
        client.publish("invernadero/conexion/esp8266zonaVentana", (clientId + " reconectado exitosamente!").c_str());
        inicioConexionT=true; //cada vez que se reconecte nos vuelva a enviar estos valores.
        inicioConexionH=true;
      }
      
      // ... and resubscribe
      client.subscribe("invernadero/electrovalvulas/general");
      client.subscribe("invernadero/electrovalvulas/bancalLargo");
      client.subscribe("invernadero/electrovalvulas/bancalCorto");
      client.subscribe("invernadero/electrovalvulas/bancalFuera");
      client.subscribe("invernadero/ventilacion/ventana");
      client.subscribe("invernadero/conexion/esp8266zonaVentana");  //tambien para publicar, seria del estilo peticion/respuesta
      client.subscribe("invernadero/sensores/DHT/interior");  //para recibir Tinterior y así abrir/cerrar ventana
      client.subscribe("invernadero/time/hour");  //para recibir Tinterior y así abrir/cerrar ventana
      client.subscribe("invernadero/sensores/humedadSuelo/bancalCorto/riego"); //para recibir valores de humedad del bancal corto enviados desde el otro ESP8266
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
  pinMode(D3, OUTPUT); //electroválvula bancalLargo
  pinMode(D2, OUTPUT);  //electroválvula bancalCorto
  pinMode(D4, OUTPUT); //electroválvula bancalFuera
  digitalWrite(D4,LOW);
  pinMode(D1,INPUT);  //final de carrera 
  pinMode(A0,INPUT);  //humedadSueloLargo
  Serial.begin(9600);  //iniciamos comunicacion serie
  setup_wifi();  //llamamos a esta función para conectarnos a la red wifi
  client.setServer(mqtt_server, 1883);  //decimos la IP y el puerto por el que se comunica 
  client.setCallback(callback);  //cuando recibe un valor a uno de sus tópics va a la funcion callback
  motorVentana.setSpeed(velocidad); //ajustamos el valor de velocidad de la ventana inicial
  dht.begin();  //iniciamos el "objeto" dht para poder utilizarlo
}

void loop() {
  if (!client.connected()) {  //si no estamos conectados al broker seguimos reconectando hasta conseguirlo
    reconnect();
  }
  client.loop();
  
  if((millis()>=(timeHour+timeLecturaHour))){ //cada 15' hago peticion de hora
    timeHour=millis();
    client.publish("invernadero/time/hour","Peticion hora"); //peticion 
  }

  if((millis()>=(tiempoVentana+timeVentana))){ //cada hora hago comprobación auto de Ventana
    tiempoVentana=millis();
    if (tempInteriorNum>=20 && primeraApertura && datoHora!="7"){
      primeraApertura=false;
      buttonAbrir=true; //simulamos pulsación de apertura desde el server
    }
    else if(tempInteriorNum<=18 && primerCierre){
      primerCierre=false;
      buttonCierre=true; //simulamos pulsación de cierre desde el server
    }
  }
  
  //proceso de apertura/cierre ventana
  if (buttonAbrir && digitalRead(D1)){  //si recibimos pulsacion desde el server y el final de carrera detecta como cerrado...
    while(desbloqueo){//tira hacia atrás para desbloquear cierre
      motorVentana.step(200);
      if(millis()>=(tiempoOC+timeDesbloqueo)){
        tiempoOC=millis();
        desbloqueo=false;
        buttonAbrir=false;
        abrir=true;  //toca abrir
      }
    }
  }
  else if(abrir){ //si bool interno dice que toca abrir...
    motorVentana.step(-200);
    if(millis()>=(tiempoOC+timeApertura)){
      abrir=false;
      cerrar=true;
      client.publish("invernadero/sensores/finalCarrera/ventana", ("PosicionVentana--> " + (String(digitalRead(D1)))).c_str());
    }
  }
  else if(cerrar && buttonCierre){  //si bool interno dice que toca cerrar y recibimos valor desde server...
    motorVentana.step(200);
    if(digitalRead(D1)){  //si el final de carrera detecta...  //en high esta cerrada
      tiempoOC=millis();
      while(millis()<=(tiempoOC+timeDesbloqueo)){   //una vez detecta el final de carrera tira x segundos atrás para bloquear la puerta
        motorVentana.step(200);
      }
      tiempoOC=millis();
      while(millis()<=(tiempoOC+timeDesDesbloqueo)){   //una vez detecta el final de carrera tira x segundos atrás para bloquear la puerta
        motorVentana.step(-200);
      }
      cerrar=false;
      buttonCierre=false;
      desbloqueo=true;
      client.publish("invernadero/sensores/finalCarrera/ventana", ("PosicionVentana--> " + (String(digitalRead(D1)))).c_str());
    }
  }
  
  //proceso para cerrar el riego atomaticamente de cada linea (transcurridos 80') //funcionalidad modo manual
  if (tRiegoLargo!=0){
    if (millis()>=(tRiegoLargo+tRiego)){  //si ha transcurrido el tiempo lo para
      tRiegoLargo=0;
      digitalWrite(D3,LOW);
      client.publish("invernadero/sensores/humedadSuelo/bancalLargo", "RegandoBancalLargo--> 0 tiempoConsumido");
    }
  }
  if (tRiegoCorto!=0){
    if (millis()>=(tRiegoCorto+tRiego)){  //si ha transcurrido el tiempo lo para
      tRiegoCorto=0;
      digitalWrite(D2,LOW);
      client.publish("invernadero/sensores/humedadSuelo/bancalCorto", "RegandoBancalCorto--> 0 tiempoConsumido");
    }
  }
  if (tRiegoFuera!=0){
    if (millis()>=(tRiegoFuera+tRiego)){  //si ha transcurrido el tiempo lo para
      tRiegoFuera=0;
      digitalWrite(D4,LOW);
      client.publish("invernadero/sensores/humedadSuelo/bancalFuera", "RegandoBancalFuera--> 0 tiempoConsumido");
    }
  }
  
  //condicion para saber si nos toca enviar mensaje de temperatura
  if ((millis()>=(timeTemp+timeLecturaT)) || errorLecturaDHT || inicioConexionT){ //si ha transcurrido el tiempo establecido o ha habido error de lectura en la anterior o acabamos de conectarnos 
    if(errorLecturaDHT){  //hacemos este condicional para que en caso de error no nos pete a msn, así podemos poner un tiempo entre medias
        if(millis()>=(timeTemp+timeError)){  //tiempo que se espera entre msn y msn de error
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
  
}

void temperaturaDHT(){
  timeTemp=millis();
  inicioConexionT=false;
  temperatura = dht.readTemperature();
  humedad = dht.readHumidity();
  if (isnan(humedad) || isnan(temperatura)) { //si no ha podido leer valor correcto de una de las dos variables del sensor...
    errorLecturaDHT=true;
    Serial.println("Failed to read from DHT22 sensor!");
    client.publish("invernadero/sensores/DHT/exterior","Failed to read from DHT22 sensor!");  //nos manda mensaje de error y se va del programa
    return;
  }
  errorLecturaDHT=false;
  client.publish("invernadero/sensores/DHT/exterior",("TemperaturaExt--> " + (String(temperatura)) + " ºC").c_str());
  client.publish("invernadero/sensores/DHT/exterior",("HumedadExt--> " + (String(humedad)) + " %").c_str());
}

void humedadSuelo(){  //trabajando con sensor impermeable; me da valores de 30% de humedad cuando me deberia adr 50% --> comprobar si es por ruido electrico
  timeHum=millis();
  inicioConexionH=false;
  lecturaHumLargo=analogRead(A0);
  if(lecturaHumLargo>1000 || lecturaHumLargo<0){  //si está fuera del rango + histeresis
    client.publish("invernadero/sensores/humedadSuelo/bancalLargo","HumSueloBancalLargo--> Error"); 
    errorLecturaHum=true;
  }
  else{ //si es un valor coherente
    errorLecturaHum=false;
    lecturaPorcentajeHumLargo=map(lecturaHumLargo,960,0,0,100); 
    //condicion para saber en funcion del valor de humedad del suelo si tenemos que encender el riego  
    if(lecturaHumLargo>=380 && !digitalRead(D3) && (datoHora=="7" || datoHora=="18")){ //si sube de 380 y no está regando y la hora está entre una de estas empieza a regar 
      //380(60%) si valor<=60% humedad
      digitalWrite(D3,HIGH);
      tRiegoLargo=millis();
      client.publish("invernadero/sensores/humedadSuelo/bancalLargo",("HumSueloBancalLargo--> " + (String(lecturaPorcentajeHumLargo)) + " %").c_str());
      client.publish("invernadero/sensores/humedadSuelo/bancalLargo", "RegandoBancalLargo--> 1 activaciónAutomática");  // 1, por tanto esta regando
    }
    //condicion para saber en funcion del valor de humedad del suelo si tenemos que apagar el riego
    else if(lecturaHumLargo<=300 && digitalRead(D3)){ //si baja de 300 y está regando apaga el riego
      //300(69%) si valor>=69% humedad
      digitalWrite(D3,LOW);  
      tRiegoLargo=0;
      client.publish("invernadero/sensores/humedadSuelo/bancalLargo",("HumSueloBancalLargo--> " + (String(lecturaPorcentajeHumLargo)) + " %").c_str());
      client.publish("invernadero/sensores/humedadSuelo/bancalLargo", "RegandoBancalLargo--> 0 desactivaciónAutomática"); //0 , por tanto no riega
    }
    else {
      client.publish("invernadero/sensores/humedadSuelo/bancalLargo",("HumSueloBancalLargo--> " + (String(lecturaPorcentajeHumLargo)) + " %").c_str());
    }
  }
}
