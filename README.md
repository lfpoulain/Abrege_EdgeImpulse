# 🎤 Détecteur Audio IA avec Edge Impulse et Home Assistant

Un système de détection audio intelligent utilisant l'apprentissage  pour reconnaître des sons spécifiques et déclencher des actions domotiques via Home Assistant.

## 📋 Fonctionnalités

- Détection audio en temps réel avec Edge Impulse
- Traitement local sur Arduino (pas de cloud)
- Intégration avec Home Assistant via webhooks
- Personnalisable pour différents types de sons
- Boîtier imprimé en 3D discret
- Faible consommation d'énergie

## 🛠️ Matériel Requis

- Arduino Nano RP2040 Connect

## 🔧 Installation

1. **Configurer Edge Impulse**
   - Créer un compte sur [Edge Impulse](https://edgeimpulse.com)
   - Créer un nouveau projet
   - Ajouter des échantillons audio pour l'entraînement
   - Configurer
   - Entraîner le modèle
   - Exporter la librairie Arduino

2. **Préparation Arduino**
   - Installer l'IDE Arduino
   - Installer les bibliothèques requises :
     ```
     - WiFiNINA
     - ArduinoHttpClient
     - EdgeImpulse Library (exportée plus tôt)
     ```

3. **Configuration Home Assistant**
   - Configurer une automatisation avec webhook GET comme déclencheur
   - Noter l'URL du webhook

## 💻 Configuration

### Code Arduino
Dans le code source principal, modifiez ces lignes avec vos informations :

```cpp
// Configuration WiFi
char ssid[] = "SSID";     // Remplacez par votre SSID WiFi
char pass[] = "PASSWORD"; // Remplacez par votre mot de passe WiFi

// Configuration serveur
char serverAddress[] = "X.X.X.X";  // Adresse IP de votre Home Assistant
int port = 8123;                   // Port de Home Assistant (8123 par défaut)
```

### Home Assistant
Dans Home Assistant, créez une nouvelle automatisation

L'URL du webhook sera : `http://[ADRESSE_HOME_ASSISTANT]:8123/api/webhook/votrecodewebhook`

Cette URL doit correspondre à la ligne dans le code Arduino :
```cpp
client.get("/api/webhook/votrecodewebhook");
```

Et aussi, suivant le nom de la librairie téléchargée via EdgeImpulse :
```cpp
#include <votrelib_inferencing.h> // Modèle Edge Impulse généré et ajouté en lib
```



## 🚀 Utilisation

1. Téléverser le code sur l'Arduino
2. Le système commencera automatiquement à :
   - Écouter les sons via le microphone PDM
   - Analyser les échantillons audio
   - Envoyer des requêtes à Home Assistant lors d'une détection

## 🔍 Calibration

- Ajuster le seuil de détection dans le code (par défaut 0.90)
- Augmenter le dataset d'entraînement pour améliorer la précision
- Tester dans différentes conditions acoustiques

## 📊 Performance

- Temps d'inférence : ~100ms
- Précision : >90% (avec un bon dataset)
- Latence webhook : <1s
- Faux positifs : <5% (avec calibration correcte)

## 🔒 Sécurité

- Tout le traitement audio est fait localement
- Aucune donnée audio n'est envoyée au cloud
- Communication uniquement avec Home Assistant en local


## 🙏 Remerciements

- Abrège et la communauté
- https://www.youtube.com/watch?v=x5MdpQb1MA0&t
