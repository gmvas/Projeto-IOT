#include <arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <NTPClient.h>

/* Definicoes para o MQTT */
#define TOPICO_PUBLISH_CONTROLE        "controle_geral"
#define TOPICO_SUBSCRIBE_MOVIMENTO     "controle_movimento"
#define TOPICO_SUBSCRIBE_LUMINOSIDADE  "controle_luminosidade"
#define TOPICO_MENSAGENS               "mensagens_esp32"

#define ID_MQTT  "laptopPedro_mqtt"     //id mqtt (para identificação de sessão)


/*
* Variáveis para horário via internet
*/
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

//Internet a se conectar
const char* SSID     = ""; // SSID / nome da rede WI-FI que deseja se conectar
const char* PASSWORD = "";  // Senha da rede WI-FI que deseja se conectar

const char* BROKER_MQTT = "test.mosquitto.org";
int BROKER_PORT = 1883; // Porta do Broker MQTT

//Variáveis e objetos globais
WiFiClient espClient; // Cria o objeto espClient
PubSubClient MQTT(espClient); // Instancia o Cliente MQTT passando o objeto espClient

/* Definicao de variaveis para o ESP32 */
const int pirPin = 35; //Pino D35 (Input Only)
const int pinoSensorLuminosidade = 36; //Pino VP - ADC1 (Input only)
const int verde = 32; //Pino D32
const int vermelho = 33; //Pino D33
const int led_luz = 25; //Pino D25
const int interruptor = 34; //Pino D34 (Input Only)
const int luzQuarto = 26; //Pino D26 - Esse pino controla o Relé da luz
const int motorVentilador = 27; //Pino D27

/* Definicao de variaveis de controle */
boolean luzPorPresenca = false; //Variável para controlar o efeito da presença sobre a luz
boolean acionamentoLuzMQTT = false; //Varável de controle de luz caso usuario acione via app
boolean acionamentoDiurnoMQTT = false;
boolean luzPorIluminacao = false; //Variavel para contrle de luz por luminosidade
boolean interruptorLuz = false;
boolean isOn = false;
boolean ledDiurno = false;

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
void calculaTensao();
void startupGreetings(void);

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
    acionamentoDiurnoMQTT = true;
    Serial.println("Lampada ligada via MQTT!");
  }

  //Apagando Luz pelo comando MQTT
  else if(msg.equals("D")){
    acionamentoDiurnoMQTT = false;
    Serial.println("Lampada desligada via MQTT!");
  }

  else if(msg.equalsIgnoreCase("Ligar luz")){
    acionamentoLuzMQTT = true;
    Serial.println("Lampada do quarto ligada via MQTT!");
    MQTT.publish(TOPICO_MENSAGENS, "Luz do quarto ligada.");
  }

  else if(msg.equalsIgnoreCase("Desligar luz")){
    acionamentoLuzMQTT = false;
    digitalWrite(luzQuarto, LOW);
    Serial.println("Lampada do quarto desligada via MQTT!");
    MQTT.publish(TOPICO_MENSAGENS, "Luz do quarto desigada.");
  }

  //Mandando status da luminosidade atual sob pedido do MQTT
  else if(msg.equalsIgnoreCase("Controle luminosidade")){
    char msgResposta[35];
    sprintf(msgResposta, "Controle de luminosidade: %d/4095", valorLuz);
    MQTT.publish(TOPICO_SUBSCRIBE_LUMINOSIDADE, msgResposta);
  }

  else if (msg.equalsIgnoreCase("Horario"))
  {
    String msgStr = timeClient.getFormattedTime(); //Pegando o horario em formato de String
    char msg[msgStr.length()]; //Criando um array de char do tamanho da String
    strcpy(msg, msgStr.c_str()); //Copiando conteudo da String para o char array
    MQTT.publish(TOPICO_MENSAGENS, msg); //Enviando horario para um topico de mensagens geral
  } 

  else if (msg.equalsIgnoreCase("Acionamento presença ON"))
  {
    luzPorPresenca = true;
    MQTT.publish(TOPICO_MENSAGENS, "Luz agora acionará por presença!");     
  }

  else if (msg.equalsIgnoreCase("Acionamento presença OFF"))
  {
    luzPorPresenca = false;
    MQTT.publish(TOPICO_MENSAGENS, "Luz não acionará mais por presença!");
  }

  else if (msg.equalsIgnoreCase("Acionamento luminosidade ON"))
  {
    luzPorIluminacao = true;
    MQTT.publish(TOPICO_MENSAGENS, "Luz agora acionará quando estiver escuro!");
    
  }

  else if (msg.equalsIgnoreCase("Acionamento luminosidade OFF"))
  {
    luzPorPresenca = false;
    MQTT.publish(TOPICO_MENSAGENS, "Luz não acionará mais quando estive escuro!");
  }

  else if (msg.equalsIgnoreCase("Ventilador ON"))
  {
    digitalWrite(motorVentilador, HIGH);
    MQTT.publish(TOPICO_MENSAGENS, "Ventilador de teto ligado.");
  } 
  else if (msg.equalsIgnoreCase("Ventilador OFF"))
  {
    digitalWrite(motorVentilador, LOW);
    MQTT.publish(TOPICO_MENSAGENS, "Ventilador de teto desligado.");
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

/* Função: Realizar medição do fotorresistor e interpretar seu valor controlando a luz
   Parâmetros: int valorLuz
   Retorno: nenhum
   TODO: Ajustar valores
*/
void medirLuz(int valorLuz){
  statusLuzAnterior = statusLuz; //Guardando status anterior para comparar

  //Interpretando o valor captado pelo Sensor
  if (valorLuz < 200) {
    statusLuz = 0;
  } else if (valorLuz < 350) {
    statusLuz = 1;
  } else if (valorLuz < 700) {
    statusLuz = 2;
  } else if (valorLuz < 1200) {
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

  Serial.println("Valor luz: " + valorLuz);

  //Comparando se houve mudança, caso não, nada será feito
  if(statusLuzAnterior != statusLuz){
    Serial.println("Medição de luz:");
    Serial.print(" - ");

    //Separando e dando diferentes funções para cada Status
    switch (statusLuz)
    {
    case 0:
      Serial.println(msg); //Enviando mensagem para a porta Serial
      MQTT.publish(TOPICO_SUBSCRIBE_LUMINOSIDADE, msg); //Publicando no MQTT
      if(luzPorIluminacao){
        isOn = true;
      }
      break;
    case 1: 
      Serial.println(msg1); //Enviando mensagem para a porta Serial
      MQTT.publish(TOPICO_SUBSCRIBE_LUMINOSIDADE, msg1); //Publicando no MQTT
      if(luzPorIluminacao){
        isOn = true;
      }
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
    ledDiurno = true;
  }
  else
  if (statusMovimentoAnterior == HIGH && statusMovimento == LOW) {
    char msg[] = "Movimento parou!";
    Serial.println(msg);
    MQTT.publish(TOPICO_SUBSCRIBE_MOVIMENTO, msg); //Enviando atualização ao topico controle_movimento
    piscar();
    parou();
    ledDiurno = false;
  }
}


/* Função: Verificar se o interruptor foi mexido pelo usuario
   Parâmetros: nenhum
   Retorno: nenhum
*/
void calculaTensao(){
  float tensao_rms;
  float tensao_pico;
  double menor_valor = 0;
  double valor_inst;

  menor_valor = 4095;
  for(int i = 0; i < 500; i++){ //Pegando 500 amostras da voltagem para pegar sua menor voltagem
    valor_inst = analogRead(interruptor);
    if(menor_valor > valor_inst){
      menor_valor = valor_inst;
    }
    delayMicroseconds(10);
  }

  tensao_pico = map(menor_valor, 3250, 2760, 0, 315);

  tensao_rms = tensao_pico/1.4;
  tensao_rms = tensao_rms - 150;

  if(tensao_rms > 80){
    interruptorLuz = true;
  }
  else{
    interruptorLuz = false;
  }

  Serial.println(tensao_rms);
}

void startupGreetings(void){
  digitalWrite(led_luz, LOW);
  digitalWrite(vermelho, LOW);
  digitalWrite(verde, LOW);
  delay(150);
  
  digitalWrite(led_luz, HIGH);;
  delay(150);

  digitalWrite(vermelho, HIGH);;
  delay(150);

  digitalWrite(verde, HIGH);;
  delay(150);

  digitalWrite(led_luz, LOW);;
  delay(150);

  digitalWrite(vermelho, LOW);;
  delay(150);

  digitalWrite(verde, LOW);;
  delay(150);
}

void setup() {
  //Configurando os pinos a serem utilizados
  pinMode(pirPin, INPUT);
  pinMode(pinoSensorLuminosidade, INPUT);
  pinMode(interruptor, INPUT);
  pinMode(verde, OUTPUT);
  pinMode(vermelho, OUTPUT);
  pinMode(led_luz, OUTPUT);
  pinMode(motorVentilador, OUTPUT);
  pinMode(luzQuarto, OUTPUT);

  //Mensagens de inicialização
  digitalWrite(led_luz, HIGH); //Luz debug 1 - ESP iniciado
  Serial.begin(9600); //Enviar e receber dados em 9600 baud
  delay(1000);
  Serial.println("Teste do Smart Kit com ESP32");
  delay(1000);  
  
  // Inicializa a conexao wi-fi
  initWiFi();

  // Inicializa a conexao ao broker MQTT
  initMQTT();

  digitalWrite(vermelho, HIGH); //Luz debug 2 - Internet conectada

  //Initializa tempo via NTP
  timeClient.begin(); 

  //Fuso horario de UTC -3h
  timeClient.setTimeOffset(-10800); 

  digitalWrite(verde, HIGH); //Luz debug 3 - Horario via internet conectado

  startupGreetings(); //Indicação visual que o ESP inicalizou normalmente
}

void loop() {
  //Realizando atualizacao do horario
  if (!timeClient.update()){
    timeClient.forceUpdate();
  } 
  isOn = false; //Resentando viarável

  // Gerenciando status de movimentação no quarto
  statusMovimentoAnterior = statusMovimento; 
  statusMovimento = digitalRead(pirPin); 
  movimentacao(statusMovimentoAnterior, statusMovimento);

  //Realizando leitura da luminosidade local
  valorLuz = analogRead(pinoSensorLuminosidade); //Lendo valor de luminosidade local
  medirLuz(valorLuz); //Mudando a luz conforme necessário

  //Vendo se houve acionamento do interruptor
  calculaTensao();

  //Controlando luz do quarto
  if(interruptorLuz == HIGH){
    isOn = true; //Caso o interruptor de luz esteja ativo, ele será priorizado comparado aos outros parâmetros
  }
  else{
    if(acionamentoLuzMQTT){ //Checando se o usuario ligou a luz no app
      isOn = true;
    }
    else{
      //Fazendo controle de luz por presença caso a opção tenha sido escolhida
      if(luzPorPresenca){  
        if(statusMovimento){
          isOn = true;
        }
      }
    }
  }

  //Verificando se o led diurno foi ativado via app
  if(acionamentoDiurnoMQTT || ledDiurno){
    digitalWrite(led_luz, HIGH);
  }
  else{
    digitalWrite(led_luz, LOW);
  }

  ///Lendo luz do quarto
  if(isOn){
    digitalWrite(luzQuarto, HIGH);
  }
  else if(!isOn){
    digitalWrite(luzQuarto, LOW);
  }

  // Garante funcionamento das conexões WiFi e ao broker MQTT
  VerificaConexoesWiFIEMQTT();

  // keep-alive da comunicação com broker MQTT
  MQTT.loop();

  // Refaz o ciclo após 0,75 segundos
  delay(750);
}
