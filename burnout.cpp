#include <WiFi.h>          // Biblioteca para gerenciar a conexão Wi-Fi
#include <PubSubClient.h>  // Para comunicação MQTT
#include <DHTesp.h>        // Biblioteca para interagir com o sensor DHT

// --- DEFINIÇÕES DE HARDWARE E LIMITES ---
const int DHT_PIN = 15; 
const float TEMP_LIMITE_ALTA = 28.0; 	
const float UMIDADE_LIMITE_BAIXA = 35.0; 


DHTesp dht; 

// Credenciais de Rede (Wokwi)
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// Configurações do Broker MQTT (servidor: 20.150.218.100)
const char* mqtt_server = "20.150.218.100"; 
const int BROKER_PORT = 1883;

// IDENTIFICADORES FIWARE (Ajuste para o Monitor Ambiental)
const char* DEVICE_ID = "ambience001"; // Novo ID para este sensor
const char* API_KEY = "TEF"; // A chave do IoT Agent 
const char* TOPICO_PUBLISH = "/TEF/ambience001/attrs"; // Tópico Ultralight /<API_KEY>/<DEVICE_ID>/attrs

WiFiClient espClient; 
PubSubClient client(espClient); 
unsigned long lastMsg = 0; 
const unsigned long CHECK_INTERVAL = 30000UL; 

// --- FUNÇÕES DE CONEXÃO E PUBLICAÇÃO ---

void setup_wifi() { 
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
  Serial.println("\nWiFi connected"); 
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP()); 
}

void reconnect() { 
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection to FIWARE...");
    // Conectando com o DEVICE_ID como ID do cliente
    if (client.connect(DEVICE_ID)) { 
      Serial.println("Connected"); 
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state()); 
      Serial.println(" try again in 5 seconds"); 
      delay(5000);
    }
  }
}

// Publica a string no formato ULTRALIGHT
void publish_ultralight(const char* topic, const String& payload) {
    if (client.connected()) {
        client.publish(topic, payload.c_str());
        Serial.print("FIWARE ENVIO OK [UL]: "); Serial.println(payload);
    }
}

// --- SETUP E LOOP ---

void setup() {
  Serial.begin(115200); 
  
  setup_wifi();
  client.setServer(mqtt_server, BROKER_PORT); 
  dht.setup(DHT_PIN, DHTesp::DHT22); 
  Serial.println("Monitor Ambiental Inicializado.");
}

void loop() {
  // Tenta reconectar o Wi-Fi se cair
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi desconectado. Tentando reconectar...");
    setup_wifi();
  }

  // Tenta reconectar o MQTT se cair
  if (!client.connected()) {
    reconnect(); 
  }
  client.loop(); 

  unsigned long now = millis();
  
  if (now - lastMsg > CHECK_INTERVAL) { 
    lastMsg = now; 
    TempAndHumidity data = dht.getTempAndHumidity(); 

    if (dht.getStatus() != 0) {
        Serial.println("Falha ao ler o sensor DHT!");
        // Em caso de erro, publica apenas um alerta no FIWARE (se for mapeado)
        publish_ultralight(TOPICO_PUBLISH, "error|1");
        return; 
    }

    float temp = data.temperature;
    float hum = data.humidity;
    bool desconforto = (temp > TEMP_LIMITE_ALTA) || (hum < UMIDADE_LIMITE_BAIXA);
    
    // --- CONSTRUÇÃO DO PAYLOAD ULTRALIGHT ---
    // Atributos: t=temperatura, h=umidade, a=alerta (booleano)
    String payload = "t|" + String(temp, 1) + 
                     "|h|" + String(hum, 1) + 
                     "|a|" + String(desconforto ? "true" : "false"); 

    publish_ultralight(TOPICO_PUBLISH, payload);
    
    Serial.print("Status: Desconforto Detectado? ");
    Serial.println(desconforto ? "SIM" : "NÃO");
    Serial.println("---");
  }
}