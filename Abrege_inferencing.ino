/* Edge Impulse - Détection audio avec Arduino
 * Nécessite:
 * - Une carte Arduino compatible (ex: Nano 33 BLE)
 * - Une connexion WiFi
 * - Un modèle Edge Impulse entraîné pour l'audio
 */

#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>
#include "arduino_secrets.h" // Contient les credentials WiFi

// Configuration de la quantification du filterbank
#define EIDSP_QUANTIZE_FILTERBANK 0

#include <votrelib_inferencing.h> // Modèle Edge Impulse généré et ajouté en lib
#include <PDM.h> // Pour le microphone PDM

// Configuration WiFi
char ssid[] = "SSID";  // Nom du réseau WiFi
char pass[] = "PASSWORD";  // Mot de passe WiFi

// Configuration serveur
char serverAddress[] = "X.X.X.X";  // Adresse du serveur
int port = 8123;  // Port du serveur

// Initialisation des clients WiFi et HTTP
WiFiClient wifi;
HttpClient client = HttpClient(wifi, serverAddress, port);
int status = WL_IDLE_STATUS;

// Structure pour gérer l'inférence audio
typedef struct {
    int16_t *buffer;      // Buffer audio
    uint8_t buf_ready;    // Indicateur de buffer prêt
    uint32_t buf_count;   // Compteur d'échantillons
    uint32_t n_samples;   // Nombre total d'échantillons
} inference_t;

// Variables globales pour l'audio
static inference_t inference;
static signed short sampleBuffer[2048];
static bool debug_nn = false;  // Active le debug si true
static volatile bool record_ready = false;

/**
 * Configuration initiale
 */
void setup() {
    Serial.begin(115200);
    
    // Connexion WiFi
    while (status != WL_CONNECTED) {
        Serial.print("Connexion au réseau: ");
        Serial.println(ssid);
        status = WiFi.begin(ssid, pass);
    }
    
    // Affichage des informations de connexion
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    
    Serial.println("Edge Impulse Inferencing Demo");
    pinMode(LEDG, OUTPUT);
    
    // Affichage des paramètres d'inférence
    ei_printf("Paramètres d'inférence:\n");
    ei_printf("\tIntervalle: %.2f ms.\n", (float)EI_CLASSIFIER_INTERVAL_MS);
    ei_printf("\tTaille frame: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
    ei_printf("\tDurée échantillon: %d ms.\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT / 16);
    ei_printf("\tNombre de classes: %d\n", sizeof(ei_classifier_inferencing_categories) / sizeof(ei_classifier_inferencing_categories[0]));
    
    // Démarrage du microphone
    if (!microphone_inference_start(EI_CLASSIFIER_RAW_SAMPLE_COUNT)) {
        ei_printf("ERREUR: Impossible d'allouer le buffer audio\r\n");
        return;
    }
}

/**
 * Boucle principale
 */
void loop() {
    // Enregistrement audio
    ei_printf("Enregistrement...\n");
    digitalWrite(LEDG, HIGH);
    if (!microphone_inference_record()) {
        ei_printf("ERREUR: Échec enregistrement audio\n");
        return;
    }
    ei_printf("Enregistrement terminé\n");
    
    // Préparation du signal pour l'inférence
    signal_t signal;
    signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    signal.get_data = &microphone_audio_signal_get_data;
    ei_impulse_result_t result = { 0 };
    
    // Exécution de l'inférence
    EI_IMPULSE_ERROR res = run_classifier_continuous(&signal, &result, debug_nn);
    if (res != EI_IMPULSE_OK) {
        ei_printf("ERREUR: Échec classification (%d)\n", res);
        return;
    }
    
    print_inference_result(result);
}

/**
 * Callback PDM - Appelé quand le buffer PDM est plein
 */
static void pdm_data_ready_inference_callback(void) {
    int bytesAvailable = PDM.available();
    int bytesRead = PDM.read((char *)&sampleBuffer[0], bytesAvailable);
    
    if ((inference.buf_ready == 0) && (record_ready == true)) {
        for (int i = 0; i < bytesRead >> 1; i++) {
            inference.buffer[inference.buf_count++] = sampleBuffer[i];
            if (inference.buf_count >= inference.n_samples) {
                inference.buf_count = 0;
                inference.buf_ready = 1;
                break;
            }
        }
    }
}

/**
 * Initialisation de l'inférence et du PDM
 */
static bool microphone_inference_start(uint32_t n_samples) {
    inference.buffer = (int16_t *)malloc(n_samples * sizeof(int16_t));
    if (!inference.buffer) return false;
    
    inference.buf_count = 0;
    inference.n_samples = n_samples;
    inference.buf_ready = 0;
    
    PDM.onReceive(pdm_data_ready_inference_callback);
    PDM.setBufferSize(2048);
    delay(250);
    
    if (!PDM.begin(1, EI_CLASSIFIER_FREQUENCY)) {
        ei_printf("ERREUR: Échec démarrage PDM!");
        microphone_inference_end();
        return false;
    }
    
    return true;
}

/**
 * Enregistrement audio
 */
static bool microphone_inference_record(void) {
    record_ready = true;
    while (inference.buf_ready == 0) {
        delay(10);
    }
    inference.buf_ready = 0;
    record_ready = false;
    return true;
}

/**
 * Conversion des données audio
 */
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
    numpy::int16_to_float(&inference.buffer[offset], out_ptr, length);
    return 0;
}

/**
 * Arrêt du PDM et libération mémoire
 */
static void microphone_inference_end(void) {
    PDM.end();
    ei_free(inference.buffer);
}

/**
 * Affichage des résultats d'inférence
 */
void print_inference_result(ei_impulse_result_t result) {
    // Affichage des temps d'exécution
    ei_printf("Timing: DSP %d ms, inference %d ms, anomaly %d ms\r\n",
              result.timing.dsp, result.timing.classification, result.timing.anomaly);
    
    // Affichage des prédictions
    ei_printf("Prédictions:\r\n");
    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        ei_printf("  %s: %.5f\r\n", 
                 ei_classifier_inferencing_categories[i], 
                 result.classification[i].value);
    }
    
    // Si détection > 90%, envoi requête HTTP
    if (result.classification[0].value > 0.90) {
        ei_printf("DÉTECTION\r\n");
        Serial.println("Envoi requête GET");
        client.get("/api/webhook/votrecodewebhook");
        
        int statusCode = client.responseStatusCode();
        String response = client.responseBody();
        
        Serial.print("Code status: ");
        Serial.println(statusCode);
        Serial.print("Réponse: ");
        Serial.println(response);
    }
    
    // Affichage anomalie si disponible
    #if EI_CLASSIFIER_HAS_ANOMALY == 1
        ei_printf("Prédiction anomalie: %.3f\r\n", result.anomaly);
    #endif
}

// Vérification compatibilité capteur
#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_MICROPHONE
    #error "Modèle incompatible avec le capteur actuel."
#endif
