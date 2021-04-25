import paho.mqtt.client as mqtt
import datetime	#conjunto de funciones para trabajar con fecha y horas
from time import time
from flask import Flask, render_template, request 
app=Flask(__name__) #creamos un objeto Flask 

#inicializamos valores
electrovalvulaTodas=False
electrovalvulaLargo=False
electrovalvulaCorto=False
electrovalvulaFuera=False
resCalefactable=False
ventana=False
puerta=False
tempAmbInterior=""
tAintNumber=0.0
humAmbInterior=""
tempAmbExterior=""
humAmbExterior=""
humBancalLargo=""
humBancalCorto=""
posVentana=""
posPuerta=""
espPuerta=False
espVentana=False
startTime=time()

def on_connect(client, userdata, flags, rc):  #una conexión exitosa nos pone el rc=0
    print("Connected with result code "+str(rc))
    #se suscribe a todos los tópicos de los que quiere escuchar
    client.subscribe("invernadero/sensores/DHT/exterior")   #por defecto nos pone qos 0 --> qos[Quality of Service] existen la 0,1,2
    client.subscribe("invernadero/sensores/DHT/interior")
    client.subscribe("invernadero/conexion/esp8266zonaVentana")
    client.subscribe("invernadero/conexion/esp8266zonaPuerta")
    client.subscribe("invernadero/sensores/humedadSuelo/bancalLargo")
    client.subscribe("invernadero/sensores/humedadSuelo/bancalCorto")
    client.subscribe("invernadero/sensores/finalCarrera/ventana")  #1--> ventana cerrada ;; 0--> ventana abierta
    client.subscribe("invernadero/sensores/finalCarrera/puerta")
    client.subscribe("invernadero/time/hour")
    client.subscribe("invernadero/camaCaliente/estado")

def on_disconnect(client, userdata, rc):  #una desconexión exitosa nos pone el rc=0
    print("Disconnected with result code "+str(rc))  
    
def on_message(client, userdata, message):   #cada mensaje recibido entra aqui
	global humAmbInterior
	global tempAmbInterior
	global humAmbExterior
	global tempAmbExterior
	global humBancalLargo
	global humBancalCorto
	global espPuerta
	global espVentana
	global timeIni
	global posVentana
	global posPuerta
	global electrovalvulaLargo
	global electrovalvulaCorto	
	global resCalefactable	
	global tAintNumber
	empezarAñadir=False
	mensajeStr=str(message.payload.decode("utf-8"))		#guardamos el mensaje recibido 
	print(message.topic + " : " + mensajeStr)
	#siempre que se reciba un mensaje entrará en una de estas condiciones
	if "ESP8266Client-zVentana conectado" in mensajeStr:	
		espVentana=True
	elif "ESP8266Client-zPuerta conectado" in mensajeStr:
		espPuerta=True
	elif "HumSueloBancalLargo" in mensajeStr: #leemos el valor recibido y lo guardamos; Formato de llegada: HumSueloBancalLargo--> 15.20 % 
		humBancalLargo=""
		for letter in mensajeStr:
			if empezarAñadir==True:
				humBancalLargo=humBancalLargo+letter
			if letter==" ":
				if empezarAñadir==False:
					empezarAñadir=True
				else:
					empezarAñadir=False
	elif "HumSueloBancalCorto" in mensajeStr:
		humBancalCorto=""
		for letter in mensajeStr:
			if empezarAñadir==True:
				humBancalCorto=humBancalCorto+letter
			if letter==" ":
				if empezarAñadir==False:
					empezarAñadir=True
				else:
					empezarAñadir=False
	elif "HumedadExt" in mensajeStr:		#Formato de llegada: HumedadExt--> 50.20 % 
		humAmbExterior=""
		for letter in mensajeStr:
			if empezarAñadir==True:
				humAmbExterior=humAmbExterior+letter
			if letter==" ":
				if empezarAñadir==False:
					empezarAñadir=True
				else:
					empezarAñadir=False
	elif "TemperaturaExt" in mensajeStr:	#Formato de llegada: TemperaturaExt--> 18.20 %
		tempAmbExterior=""
		for letter in mensajeStr:
			if empezarAñadir==True:
				tempAmbExterior=tempAmbExterior+letter
			if letter==" ":
				if empezarAñadir==False:
					empezarAñadir=True
				else:
					empezarAñadir=False
	elif "HumedadInt" in mensajeStr:
		humAmbInterior=""
		for letter in mensajeStr:
			if empezarAñadir==True:
				humAmbInterior=humAmbInterior+letter
			if letter==" ":
				if empezarAñadir==False:
					empezarAñadir=True
				else:
					empezarAñadir=False
	elif "TemperaturaInt" in mensajeStr:
		tempAmbInterior=""
		for letter in mensajeStr:
			if empezarAñadir==True:
				tempAmbInterior=tempAmbInterior+letter
			if letter==" ":
				if empezarAñadir==False:
					empezarAñadir=True
				else:
					empezarAñadir=False
		tAintNumber=float(tempAmbInterior)
	elif "PosicionVentana" in mensajeStr:		#Formato de llegada: PosicionVentana--> 0
		posVentana=""
		for letter in mensajeStr:
			if empezarAñadir==True:
				posVentana=posVentana+letter
			if letter==" ":
				if empezarAñadir==False:
					empezarAñadir=True
				else:
					empezarAñadir=False
	elif "PosicionPuerta" in mensajeStr:
		posPuerta=""
		for letter in mensajeStr:
			if empezarAñadir==True:
				posPuerta=posPuerta+letter
			if letter==" ":
				if empezarAñadir==False:
					empezarAñadir=True
				else:
					empezarAñadir=False
	elif "RegandoBancalLargo" in mensajeStr:  	#si se gestiona el riego de forma automatica en funcion del valor de humedad del suelo me actualiza el valor de electrovalvulaLargo
		for letter in mensajeStr:
			if empezarAñadir == True:
				if letter=="1":
					electrovalvulaLargo=True
				elif letter=="0":
					electrovalvulaLargo=False
			if letter==" ":
				if empezarAñadir==False:
					empezarAñadir=True
				else:
					empezarAñadir=False	
	elif "RegandoBancalCorto" in mensajeStr:  #Formato de llegada: RegandoBancalCorto--> 1 activaciónAutomática
		for letter in mensajeStr:
			if empezarAñadir == True:
				if letter=="1":
					electrovalvulaCorto=True
				elif letter=="0":
					electrovalvulaCorto=False
			if letter==" ":
				if empezarAñadir==False:
					empezarAñadir=True
				else:
					empezarAñadir=False	
	elif "EstadoR" in mensajeStr:  	#Formato de llegada: EstadoR--> 1
		for letter in mensajeStr:
			if empezarAñadir == True:
				if letter=="1":
					resCalefactable=True
				elif letter=="0":
					resCalefactable=False
			if letter==" ":
				if empezarAñadir==False:
					empezarAñadir=True
				else:
					empezarAñadir=False	
	elif "Peticion hora" in mensajeStr:
		x = datetime.datetime.now()
		x= ("%s" %x.hour)
		x= "Envio " + x
		client.publish("invernadero/time/hour", x)
			
client = mqtt.Client()
client.on_connect = on_connect  #la libreria ejecutará esta funcion cuando se conecte exitosamente al broker mqtt
client.on_message = on_message	#la libreria ejecutará esta función cuando reciba un mensaje
client.on_disconnect = on_disconnect
client.connect("192.168.1.148", 1883, 60) #también funciona con "localhost" como IP ; el 60 hace referencia al tiempo que mantenemos la conexión abierta
					
@app.route('/')		#se ejecuta cuando entramos en la web
def index():	
	client.loop_start()  #no bloquea el flujo del programa, trabaja como un hilo de fondo y vuelve
	client.publish("invernadero/conexion/esp8266zonaVentana", "Peticion")	#preguntamos a los esps si están concetados cada vez que refresquemos la pagina inicial
	client.publish("invernadero/conexion/esp8266zonaPuerta", "Peticion")	
	templateData={		#se genera un diccionario para enviar valores al html
		'valvulaTodas': electrovalvulaTodas,
		'valvulaLargo': electrovalvulaLargo,
		'valvulaCorto': electrovalvulaCorto,
		'valvulaFuera': electrovalvulaFuera,
		'Rcalefactable' : resCalefactable,
		'ventanaState' : ventana,
		'puertaState' : puerta,
		'tAint': tempAmbInterior,
		'tAintNum' : tAintNumber,
		'hAint': humAmbInterior,
		'tAext': tempAmbExterior,
		'hAext': humAmbExterior,
		'espZonaVentana': espVentana,
		'espZonaPuerta': espPuerta,
		'hBancalLargo' : humBancalLargo,
		'hBancalCorto' : humBancalCorto,
		'posiVentana' : posVentana,
		'posiPuerta' : posPuerta,
	}
	return render_template('index.html', **templateData)		#se pasan todas las variables al html
	
@app.route("/<deviceName>/<action>")
def action(deviceName, action):		
	global electrovalvulaTodas
	global electrovalvulaLargo
	global resCalefactable
	global electrovalvulaCorto
	global electrovalvulaFuera
	global ventana
	global puerta
	client.loop_start() #si entramos al server desde p.e --> http://192.168.1.148/ventana/off (por ejemplo) que también se ponga a escuchar los mensajes de llegada
	client.publish("invernadero/conexion/esp8266zonaVentana", "Peticion")	#preguntamos al ESP si está conectado por cada acción
	client.publish("invernadero/conexion/esp8266zonaPuerta", "Peticion")	

	if espVentana==True:	
		if deviceName == 'electrovalvulaTodas':    
			if action == "on":
				client.publish("invernadero/electrovalvulas/general", "1")   
				electrovalvulaTodas=True
				electrovalvulaLargo=True
				electrovalvulaCorto=True
				electrovalvulaFuera=True
			elif action == "off":	
				client.publish("invernadero/electrovalvulas/general", "0")
				electrovalvulaTodas=False
				electrovalvulaLargo=False
				electrovalvulaCorto=False
				electrovalvulaFuera=False
		elif deviceName == 'electrovalvulaLargo':	
			if action == "on":
				client.publish("invernadero/electrovalvulas/bancalLargo", "1")	
				electrovalvulaLargo=True
			elif action == "off":	
				client.publish("invernadero/electrovalvulas/bancalLargo", "0")
				electrovalvulaLargo=False
		elif deviceName == 'electrovalvulaCorto':
			if action == "on":
				client.publish("invernadero/electrovalvulas/bancalCorto", "1")	
				electrovalvulaCorto=True
			elif action == "off":	
				client.publish("invernadero/electrovalvulas/bancalCorto", "0")
				electrovalvulaCorto=False
		elif deviceName == 'electrovalvulaFuera':
			if action == "on":
				client.publish("invernadero/electrovalvulas/bancalFuera", "1")	
				electrovalvulaFuera=True
			elif action == "off":	
				client.publish("invernadero/electrovalvulas/bancalFuera", "0")
				electrovalvulaFuera=False
		elif deviceName == 'ventana':
			if action == "on":
				client.publish("invernadero/ventilacion/ventana", "1")	
				ventana=True
			elif action == "off":	
				client.publish("invernadero/ventilacion/ventana", "0")
				ventana=False
	else:
		client.publish("invernadero/conexion/esp8266zonaVentana", "Desconectado")
		
	if espPuerta==True:
		if deviceName == 'puerta':
			if action == "on":
				client.publish("invernadero/ventilacion/puerta", "1")	
				puerta=True
			elif action == "off":	
				client.publish("invernadero/ventilacion/puerta", "0")
				puerta=False
		elif deviceName == 'resistenciaCalefactable':
			if action == "on":
				client.publish("invernadero/camaCaliente/OnOff", "1")	
				resCalefactable=True
			elif action == "off":	
				client.publish("invernadero/camaCaliente/OnOff", "0")
				resCalefactable=False
	else:
		client.publish("invernadero/conexion/esp8266zonaPuerta", "Desconectado")
				
	templateData={
		'valvulaTodas': electrovalvulaTodas,
		'valvulaLargo': electrovalvulaLargo,
		'valvulaCorto': electrovalvulaCorto,
		'valvulaFuera': electrovalvulaFuera,
		'Rcalefactable' : resCalefactable,
		'ventanaState': ventana,
		'puertaState' : puerta,
		'tAint': tempAmbInterior,
		'tAintNum' : tAintNumber,
		'hAint': humAmbInterior,
		'tAext' : tempAmbExterior,
		'hAext': humAmbExterior,
		'espZonaVentana': espVentana,
		'espZonaPuerta': espPuerta,
		'hBancalLargo' : humBancalLargo,
		'hBancalCorto' : humBancalCorto,
		'posiVentana' : posVentana,
		'posiPuerta' : posPuerta,
	}
	return render_template('index.html', **templateData)
	
if __name__=="__main__":
	app.run(host='0.0.0.0', port=80, debug=True)  #el server escucha a través del puerto 80 
	
		
	

 
