#include <Servo.h>
#include <LedControl.h>

// =====================================================
// PINES
// =====================================================

const byte PIN_JOYSTICK_X = A0;
const byte PIN_BUZZER = 8;
const byte PIN_SERVO = 9;

const byte PIN_MATRIZ_DIN = 12;
const byte PIN_MATRIZ_CLK = 11;
const byte PIN_MATRIZ_CS  = 10;

// =====================================================
// JOYSTICK
// =====================================================

int joystickX = 512;
const int CENTRO_JOYSTICK = 512;
const int ZONA_ACTIVACION = 55; // Cuanto hay que mover para que "cuente".

// =====================================================
// VELOCIDAD (parametro central, 0.0 a 1.0)
// =====================================================
// Sube mientras el joystick esta girado, baja de a poco cuando lo soltas.
// Todo (servo, sonido, matriz) se maneja con este mismo numero,
// asi nunca hay un corte brusco entre "girando" y "quieto".

float velocidad = 0.0;

const float INCREMENTO_VELOCIDAD_POR_MS = 1.0 / 2500.0; // ~2.5s hasta el maximo
const float DECREMENTO_VELOCIDAD_POR_MS = 1.0 / 5000.0; // ~5s hasta frenar del todo (tambaleo)

// =====================================================
// SERVO
// =====================================================

Servo servoMareo;

const int ANGULO_MINIMO = 45;
const int ANGULO_CENTRO = 90;
const int ANGULO_MAXIMO = 135;

float posicionServo = ANGULO_CENTRO;
float faseServo = 0.0;

const float AMPLITUD_SERVO_MAXIMA = 40.0;
const float VELOCIDAD_FASE_SERVO_MIN = 1.0;
const float VELOCIDAD_FASE_SERVO_MAX = 6.0;
const float FACTOR_INCLINACION_JOYSTICK = 0.12;
const float SUAVIZADO_SERVO = 0.3;

// =====================================================
// SONIDO: sirena suave (sube y baja continuo)
// =====================================================

const int FRECUENCIA_MINIMA = 300;
const int FRECUENCIA_MAXIMA = 650;

// Velocidad del "sube y baja". Con esto, un ciclo completo dura
// entre ~8s (quieto/arrancando) y ~2s (mareo a full) - tipo sirena tranquila.
const float VELOCIDAD_FASE_SONIDO_MIN = 0.8;
const float VELOCIDAD_FASE_SONIDO_MAX = 3.0;

const unsigned long INTERVALO_SONIDO = 20;
float faseSonido = 0.0;
unsigned long ultimoPasoSonido = 0;

// =====================================================
// MATRIZ DE LEDS (MAX7219, 8x8, LedControl)
// =====================================================

LedControl matriz = LedControl(PIN_MATRIZ_DIN, PIN_MATRIZ_CLK, PIN_MATRIZ_CS, 1);

const float CENTRO_MATRIZ = 3.5;
const float RADIO_MATRIZ = 3.0;
const byte CANTIDAD_PUNTOS_MATRIZ = 3;

const float VELOCIDAD_FASE_MATRIZ_MIN = 1.0;
const float VELOCIDAD_FASE_MATRIZ_MAX = 8.0;

float faseMatriz = 0.0;
const unsigned long INTERVALO_MATRIZ = 30;
unsigned long ultimoPasoMatriz = 0;

// =====================================================
// TIEMPOS GENERALES
// =====================================================

unsigned long ultimoCiclo = 0;
unsigned long ultimaImpresionSerie = 0;

// =====================================================
// CONFIGURACION
// =====================================================

void setup() {
  Serial.begin(9600);

  pinMode(PIN_BUZZER, OUTPUT);
  noTone(PIN_BUZZER);

  servoMareo.attach(PIN_SERVO);
  servoMareo.write(ANGULO_CENTRO);
  posicionServo = ANGULO_CENTRO;

  matriz.shutdown(0, false);
  matriz.setIntensity(0, 8);
  matriz.clearDisplay(0);

  ultimoCiclo = millis();

  Serial.println("===============================");
  Serial.println("SIMULADOR DE MAREO");
  Serial.println("===============================");
}

// =====================================================
// BUCLE PRINCIPAL
// =====================================================

void loop() {
  unsigned long ahora = millis();
  unsigned long dt = ahora - ultimoCiclo;
  ultimoCiclo = ahora;

  leerJoystick();
  bool activo = joystickFueraDelCentro();

  // --- Actualiza la velocidad central, sin cortes ---
  if (activo) {
    velocidad += INCREMENTO_VELOCIDAD_POR_MS * dt;
  } else {
    velocidad -= DECREMENTO_VELOCIDAD_POR_MS * dt;
  }
  velocidad = constrain(velocidad, 0.0, 1.0);

  // --- Todo se mueve en base a "velocidad", siempre ---
  actualizarServo(dt, activo);
  actualizarSonido();
  actualizarMatriz();

  mostrarDatosSerie(activo);
}

// =====================================================
// LECTURA DEL JOYSTICK
// =====================================================

void leerJoystick() {
  joystickX = analogRead(PIN_JOYSTICK_X);
}

bool joystickFueraDelCentro() {
  return abs(joystickX - CENTRO_JOYSTICK) > ZONA_ACTIVACION;
}

// =====================================================
// SERVO: sigue al joystick mientras giras, y "tambalea"
// (oscila cada vez mas lento y mas chico) cuando soltas
// =====================================================

void actualizarServo(unsigned long dt, bool activo) {
  float velocidadFase =
    VELOCIDAD_FASE_SERVO_MIN +
    (VELOCIDAD_FASE_SERVO_MAX - VELOCIDAD_FASE_SERVO_MIN) * velocidad;

  faseServo += velocidadFase * (dt / 1000.0);

  float amplitud = AMPLITUD_SERVO_MAXIMA * velocidad;
  float oscilacion = amplitud * sin(faseServo);

  // La inclinacion por el joystick solo aplica mientras lo estas moviendo.
  // Al soltar, queda solo la oscilacion (el tambaleo) muriendo de a poco.
  float inclinacion = 0.0;
  if (activo) {
    inclinacion = (joystickX - CENTRO_JOYSTICK) * FACTOR_INCLINACION_JOYSTICK;
  }

  float anguloObjetivo = ANGULO_CENTRO + oscilacion + inclinacion;
  anguloObjetivo = constrain(anguloObjetivo, ANGULO_MINIMO, ANGULO_MAXIMO);

  posicionServo += (anguloObjetivo - posicionServo) * SUAVIZADO_SERVO;
  servoMareo.write((int)posicionServo);
}

// =====================================================
// SONIDO: sirena continua, sube y baja, mas lenta y calma
// =====================================================

void actualizarSonido() {
  if (velocidad <= 0.005) {
    noTone(PIN_BUZZER);
    return;
  }

  if (millis() - ultimoPasoSonido < INTERVALO_SONIDO) {
    return;
  }
  ultimoPasoSonido = millis();

  float velocidadFase =
    VELOCIDAD_FASE_SONIDO_MIN +
    (VELOCIDAD_FASE_SONIDO_MAX - VELOCIDAD_FASE_SONIDO_MIN) * velocidad;

  faseSonido += velocidadFase * (INTERVALO_SONIDO / 1000.0);

  // sin() va de -1 a 1 -> lo llevamos al rango de frecuencias, suave y continuo.
  float t = (sin(faseSonido) + 1.0) / 2.0; // 0..1
  int frecuencia = FRECUENCIA_MINIMA + (int)(t * (FRECUENCIA_MAXIMA - FRECUENCIA_MINIMA));

  tone(PIN_BUZZER, frecuencia);
}

// =====================================================
// MATRIZ: 3 puntos girando en circulo, mas rapido con "velocidad"
// =====================================================

void actualizarMatriz() {
  if (millis() - ultimoPasoMatriz < INTERVALO_MATRIZ) {
    return;
  }
  ultimoPasoMatriz = millis();

  if (velocidad <= 0.005) {
    matriz.clearDisplay(0);
    return;
  }

  float velocidadFase =
    VELOCIDAD_FASE_MATRIZ_MIN +
    (VELOCIDAD_FASE_MATRIZ_MAX - VELOCIDAD_FASE_MATRIZ_MIN) * velocidad;

  faseMatriz += velocidadFase * (INTERVALO_MATRIZ / 1000.0);

  matriz.clearDisplay(0);

  for (byte i = 0; i < CANTIDAD_PUNTOS_MATRIZ; i++) {
    float angulo = faseMatriz + i * (2.0 * PI / CANTIDAD_PUNTOS_MATRIZ);

    int x = round(CENTRO_MATRIZ + RADIO_MATRIZ * cos(angulo));
    int y = round(CENTRO_MATRIZ + RADIO_MATRIZ * sin(angulo));

    x = constrain(x, 0, 7);
    y = constrain(y, 0, 7);

    matriz.setLed(0, y, x, true);
  }
}

// =====================================================
// MONITOR SERIE
// =====================================================

void mostrarDatosSerie(bool activo) {
  if (millis() - ultimaImpresionSerie < 250) {
    return;
  }
  ultimaImpresionSerie = millis();

  Serial.print("X: ");
  Serial.print(joystickX);

  Serial.print(" | Velocidad: ");
  Serial.print(velocidad, 2);

  Serial.print(" | Servo: ");
  Serial.print((int)posicionServo);

  Serial.print(" | Joystick activo: ");
  Serial.println(activo ? "SI" : "NO");
}
