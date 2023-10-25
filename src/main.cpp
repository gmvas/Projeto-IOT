#include <arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

/* Definicoes para o MQTT */
#define TOPICO_PUBLISH_CONTROLE        "controle_geral"
#define TOPICO_SUBSCRIBE_MOVIMENTO     "controle_movimento"
#define TOPICO_SUBSCRIBE_LUMINOSIDADE  "controle_luminosidade"

#define ID_MQTT  "laptopPedro_mqtt"     //id mqtt (para identificação de sessão)

//Internet a se conectar
const char* SSID     = ""; // SSID / nome da rede WI-FI que deseja se conectar
const char* PASSWORD = "";  // Senha da rede WI-FI que deseja se conectar

const char* BROKER_MQTT = "test.mosquitto.org";
int BROKER_PORT = 1883; // Porta do Broker MQTT

//Variáveis e objetos globais
WiFiClient espClient; // Cria o objeto espClient
PubSubClient MQTT(espClient); // Instancia o Cliente MQTT passando o objeto espClient

/* Definicoes de variaveis para o ESP32 */
const int pirPin = 35; //Pino D35 (Input Only)
const int pinoLuz = 36; //Pino VP - ADC1 (Input only)
const int verde = 32; //Pino D32
const int vermelho = 33; //Pino D33
const int led_luz = 25; //Pino D25

int statusMovimento   = LOW; //Status para ler mudança de movimento
int statusMovimentoAnterior  = LOW;  
int statusLuz; //Status para guardar index da luz
int statusLuzAnterior;
int valorLuz; //Valor para leitura analógica

/* Prototypes */
void initWiFi(void);
void initMQTT(void);
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void reconnectMQTT(void);
void reconnectWiFi(void);
void VerificaConexoesWiFIEMQTT(void);
void movimento(void);
void parou(void);
void piscar(void);
void medirLuz(int valorLuz);
void movimentacao(int statusMovimentoAnterior, int statusMovimento);

/*
   Implementações
*/

/* Função: inicializa e conecta-se na rede WI-FI desejada
   Parâmetros: nenhum
   Retorno: nenhum
*/
void initWiFi(void)
{
  delay(10);
  Serial.println("------Conexao WI-FI------");
  Serial.print("Conectando-se na rede: ");
  Serial.println(SSID);
  Serial.println("Aguarde");

  reconnectWiFi();
}


/* Função: inicializa parâmetros de conexão MQTT(endereço do
           broker, porta e seta função de callback)
   Parâmetros: nenhum
   Retorno: nenhum
*/
void initMQTT(void)
{
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);   //informa qual broker e porta deve ser conectado
  MQTT.setCallback(mqtt_callback);            //atribui função de callback (função chamada quando qualquer informação de um dos tópicos subescritos chega)
}

/* Função: função de callback
           esta função é chamada toda vez que uma informação de
           um dos tópicos subescritos chega)
   Parâmetros: nenhum
   Retorno: nenhum
*/
void mqtt_callback(char* topic, byte* payload, unsigned int length)
{
  String msg;

  /* obtem a string do payload recebido */
  for (int i = 0; i < length; i++)
  {
    char c = (char)payload[i];
    msg += c;
  }

  Serial.print("Chegou a seguinte string via MQTT: ");
  Serial.println(msg);

  /* toma ação dependendo da string recebida */

  //Acionando Luz pelo comando MQTT
  if (msg.equals("L")){
    digitalWrite(led_luz, HIGH);
    Serial.println("Lampada ligada via MQTT!");
  }

  //Apagando Luz pelo comando MQTT
  else if(msg.equals("D")){
    digitalWrite(led_luz, LOW);
    Serial.println("Lampada desligada via MQTT!");
  }

  //Mandando status da luminosidade atual sob pedido do MQTT
  else if(msg.equalsIgnoreCase("Controle luminosidade")){
    char msgResposta[35];
    sprintf(msgResposta, "Controle de luminosidade: %d/4095", valorLuz);
    MQTT.publish(TOPICO_SUBSCRIBE_LUMINOSIDADE, msgResposta);
  }
  
}


/* Função: reconecta-se ao broker MQTT (caso ainda não esteja conectado ou em caso de a conexão cair)
           em caso de sucesso na conexão ou reconexão, o subscribe dos tópicos é refeito.
   Parâmetros: nenhum
   Retorno: nenhum
*/
void reconnectMQTT(void)
{
  while (!MQTT.connected())
  {
    Serial.print("* Tentando se conectar ao Broker MQTT: ");
    Serial.println(BROKER_MQTT);
    if (MQTT.connect(ID_MQTT))
    {
      Serial.println("Conectado com sucesso ao broker MQTT!");
      MQTT.subscribe(TOPICO_PUBLISH_CONTROLE);
    }
    else
    {
      Serial.println("Falha ao reconectar no broker.");
      Serial.println("Havera nova tentatica de conexao em 2s");
      delay(2000);
    }
  }  
}

/* Função: verifica o estado das conexões WiFI e ao broker MQTT.
           Em caso de desconexão (qualquer uma das duas), a conexão
           é refeita.
   Parâmetros: nenhum
   Retorno: nenhum
*/
void VerificaConexoesWiFIEMQTT(void)
{
  if (!MQTT.connected())
    reconnectMQTT(); //se não há conexão com o Broker, a conexão é refeita

  reconnectWiFi(); //se não há conexão com o WiFI, a conexão é refeita
}

/* Função: reconecta-se ao WiFi
   Parâmetros: nenhum
   Retorno: nenhum
*/
void reconnectWiFi(void)
{
  //se já está conectado a rede WI-FI, nada é feito.
  //Caso contrário, são efetuadas tentativas de conexão
  if (WiFi.status() == WL_CONNECTED)
    return;

  WiFi.begin(SSID, PASSWORD); // Conecta na rede WI-FI

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Conectado com sucesso na rede ");
  Serial.print(SSID);
  Serial.println("\nIP obtido: ");
  Serial.println(WiFi.localIP());
}

/* Função: mudar condições do Leds de indicação
   Parâmetros: nenhum
   Retorno: nenhum
   Remover antes de implementação final - somente para indicação em protoboard
*/
void movimento(void){
  digitalWrite(vermelho, HIGH);
  digitalWrite(verde, LOW);
}

/* Função: mudar condições do Leds de indicação
   Parâmetros: nenhum
   Retorno: nenhum
   Remover antes de implementação final - somente para indicação em protoboard
*/
void parou(void){
  digitalWrite(vermelho, LOW);
  digitalWrite(verde, HIGH);
}

/* Função: Realizar um sequencia de liga-desliga em Leds para demonstrar mudança de sinal
   Parâmetros: nenhum
   Retorno: nenhum
   Remover antes de implementação final - somente para indicação em protoboard
*/
void piscar(void){
  digitalWrite(vermelho, LOW);
  digitalWrite(verde, HIGH);
  delay(100);
  digitalWrite(vermelho, HIGH);
  digitalWrite(verde, LOW);
  delay(100);
  digitalWrite(vermelho, LOW);
  digitalWrite(verde, HIGH);
  delay(100);
  digitalWrite(vermelho, HIGH);
  digitalWrite(verde, LOW);
  delay(100);
}

/* Função: Realizar medição do fotorresistor e interpretar seu valor
   Parâmetros: int valorLuz
   Retorno: nenhum
   TODO: Ajustar valores
*/
void medirLuz(int valorLuz){
  statusLuzAnterior = statusLuz; //Guardando status anterior para comparar

  //Interpretando o valor captado pelo Sensor
  if (valorLuz < 10) {
    statusLuz = 0;
  } else if (valorLuz < 200) {
    statusLuz = 1;
  } else if (valorLuz < 500) {
    statusLuz = 2;
  } else if (valorLuz < 800) {
    statusLuz = 3;
  } else {
    statusLuz = 4;
  }
  
  //Escrevendo mensagem
  char msg[] = {"Escuro"}; 
  char msg1[] = {"Pouco escuro"};
  char msg2[] = {"Iluminado"}; 
  char msg3[] = {"Claro"};
  char msg4[] = {"Muito claro"}; 


  //Comparando se houve mudança, caso não, nada será feito
  if(statusLuzAnterior != statusLuz){
    Serial.println("Medição de luz:");
    Serial.print(valorLuz + " - ");

    //Separando e dando diferentes funções para cada Status
    switch (statusLuz)
    {
    case 0:
      Serial.println(msg); //Enviando mensagem para a porta Serial
      MQTT.publish(TOPICO_SUBSCRIBE_LUMINOSIDADE, msg); //Publicando no MQTT
      break;
    case 1: 
      Serial.println(msg1); //Enviando mensagem para a porta Serial
      MQTT.publish(TOPICO_SUBSCRIBE_LUMINOSIDADE, msg1); //Publicando no MQTT
      break;
    case 2:
      Serial.println(msg2); //Enviando mensagem para a porta Serial
      MQTT.publish(TOPICO_SUBSCRIBE_LUMINOSIDADE, msg2); //Publicando no MQTT
      break;
    case 3:
      Serial.println(msg3); //Enviando mensagem para a porta Serial
      MQTT.publish(TOPICO_SUBSCRIBE_LUMINOSIDADE, msg3); //Publicando no MQTT
      break;
    case 4:
      Serial.println(msg4); //Enviando mensagem para a porta Serial
      MQTT.publish(TOPICO_SUBSCRIBE_LUMINOSIDADE, msg4); //Publicando no MQTT
      break;

    default:
      break;
    }
  }
}

/* Função: Verificar se houve mudança em movimento, caso tenha realizar as devidas mudanças na placa
   Parâmetros: int statusMovimentoAnterior & statusMovimento
   Retorno: nenhum
*/
void movimentacao(int statusMovimentoAnterior, int statusMovimento){
  if (statusMovimentoAnterior == LOW && statusMovimento == HIGH) {
    char msg[] = "Movimento detectado!";
    Serial.println(msg);
    MQTT.publish(TOPICO_SUBSCRIBE_MOVIMENTO, msg); //Enviando atualização ao topico controle_movimento
    piscar();
    movimento();
  }
  else
  if (statusMovimentoAnterior == HIGH && statusMovimento == LOW) {
    char msg[] = "Movimento parou!";
    Serial.println(msg);
    MQTT.publish(TOPICO_SUBSCRIBE_MOVIMENTO, msg); //Enviando atualização ao topico controle_movimento
    piscar();
    parou();
  }
}


void setup() {
  Serial.begin(9600); //Enviar e receber dados em 9600 baud
  delay(1000);
  Serial.println("Teste do Smart Kit com ESP32");
  delay(1000);

  //Configurando os pinos a serem utilizados
  pinMode(pirPin, INPUT);
  pinMode(pinoLuz, INPUT);
  pinMode(verde, OUTPUT);
  pinMode(vermelho, OUTPUT);
  pinMode(led_luz, OUTPUT);
  
  // Inicializa a conexao wi-fi
  initWiFi();

  // Inicializa a conexao ao broker MQTT
  initMQTT();
}

void loop() {
    // Guardando status anterior
    statusMovimentoAnterior = statusMovimento; 

    // Lendo novo status
    statusMovimento = digitalRead(pirPin); 

    //Lendo valor da luz
    valorLuz = analogRead(pinoLuz); 

    // garante funcionamento das conexões WiFi e ao broker MQTT
    VerificaConexoesWiFIEMQTT();

    // formata a temperatura aleatoria  como string
    // sprintf(temperatura_str, "%dC", numAleatorio);

    //  Publica a temperatura
    //MQTT.publish(TOPICO_PUBLISH_TEMPERATURA, temperatura_str);


    //Realizando checagem de mudança de status nos sensores
    movimentacao(statusMovimentoAnterior, statusMovimento);
    medirLuz(valorLuz);

    // keep-alive da comunicação com broker MQTT
    MQTT.loop();

    // Refaz o ciclo após 2 segundos
    delay(2000);
}
