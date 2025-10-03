# 🎶 MusicBee Hardware – Agent Guide

## 📦 Project Scope

* Firmware progettato per **ESP32-S3**, funge da bridge tra la lettura di **tag NFC** e il backend **MusicBee**, permettendo ai bambini di avviare la riproduzione musicale in sicurezza tramite NFC.
* Supporta due famiglie di lettori NFC:

  * **MFRC522** – interfaccia SPI
  * **PN532** – interfaccia I²C (SPI opzionale)
* Feedback tramite **LED** indica stato Wi-Fi, lettura card e comunicazione col backend.

---

## 📁 Repository Layout

* **`src/`** – implementazioni principali (`main.cpp`, driver e logica effetti). Il punto di ingresso gestisce Wi-Fi, RFID, chiamate al backend e segnalazioni LED.
* **`include/`** – header pubblici e configurazioni (`Config.h`, `RfidReader.h`, `WifiManager.h`, effetti). La configurazione è a **compile-time** e i segreti sono caricati da `secrets.h`.
* **`lib/`** – librerie opzionali per PlatformIO (vuoto per default).
* **`test/`** – placeholder per test unitari PlatformIO.

---

## 🔧 Build & Flash Workflow

1. Copia `include/secrets.example.h` in `include/secrets.h` e inserisci **SSID**, **password Wi-Fi**, **host** e **porta** del backend.
   ⚠️ **Non** committare mai `secrets.h`.
2. Seleziona l’ambiente PlatformIO in base al lettore NFC:

   * `esp32-s3-devkitc-1-rc522`
   * `esp32-s3-devkitc-1-pn532`
     Per usare PN532 in modalità SPI aggiungi `-DUSE_PN532_SPI`.
3. Compila e flasha con:

   ```bash
   pio run
   pio run --target upload
   ```
4. Monitora i log con:

   ```bash
   pio device monitor -b 115200
   ```
5. Per supportare nuovo hardware:

   * Aggiungi una voce all’enum in `Config.h`
   * Definisci i pin corrispondenti
   * Estendi l’ambiente PlatformIO

---

## 🔐 Secrets & Configuration

* Definisci nuove opzioni come `static constexpr` in `Config.h` con commenti chiari per garantire build riproducibili.
* Non committare mai credenziali reali. Aggiorna sempre `secrets.example.h` con eventuali nuovi campi.

---

## ✍️ Coding Conventions

* Usa `#pragma once` negli header.
* Preferisci `static constexpr` alle macro.
* Indenta con **2 spazi**, usa **early return**, e mantieni le funzioni **brevi e single-purpose**.
* Per il logging usa messaggi taggati `Serial.printf` / `Serial.println` (es. `[Backend]`, `[RFID]`) per facilitare il debug.
* Usa `enum class` per gli state machine e misura il tempo con `unsigned long` e `millis()`.
* Utilizza RAII (`std::unique_ptr`) per gestire backend hardware e incapsula il codice specifico in classi o namespace dedicati.
* Ogni modulo deve avere un **commento di intestazione** con lo scopo del file.

---

## 🏗️ Firmware Architecture

* **`main.cpp`** gestisce il loop principale. Gli effetti LED sono centralizzati in `EffectManager`; gli stati temporanei (es. card letta, risposta backend) durano ~2.5s prima di tornare allo stato base Wi-Fi.
* **`WifiManager`** gestisce la connessione e i retry ogni 5s. Espone `isConnected()` per controlli prima delle chiamate di rete.
* **`BackendClient`** risolve host `.local` tramite **mDNS** e invia richieste JSON a `/api/v1/cards/{uid}/play`. Qualsiasi risposta non-2xx è considerata errore.
* **`RfidReader`** seleziona il backend corretto (RC522 o PN532) in base a `Config.h`. Per nuovi lettori implementa `IRfidBackend` e aggiungi la selezione nello `switch`.
* **`EffectManager`** fornisce effetti `SolidColor`, `Snake` e `Breathing`. Per crearne di nuovi, sottoclassa `Effect`, sovrascrivi `begin()` e `update()`, e lascia a `EffectManager` la gestione della strip LED.

---

## 🧪 Testing & Debugging

* Compila e flasha con:

  ```bash
  pio run
  pio run --target upload
  ```
* Osserva log di riconnessione Wi-Fi e messaggi `[DEBOUNCE]` per controllare la bontà del valore `CARD_DEBOUNCE_MS`.
* Valida nuove integrazioni backend tramite logging seriale e mantieni i messaggi di debug leggibili e informativi.

---

## 📚 Documentation & PR Checklist

* Aggiorna `README.md` se cambi pinout, aggiungi hardware o modifichi comportamenti visibili all’utente.
* Mantieni aggiornato `include/README` quando aggiungi nuove opzioni di configurazione.
* Aggiorna i placeholder in `secrets.example.h` se aggiungi nuovi campi.
* Aggiorna le dipendenze in `platformio.ini` quando necessario.
* Includi sempre i comandi di build e flash nella descrizione delle PR.

---

✅ **Tip:** Mantieni il flusso di lavoro semplice e documenta ogni nuovo modulo. Il progetto è progettato per essere estensibile, quindi aggiungere nuovi lettori NFC o effetti LED dovrebbe richiedere solo modifiche locali e minime.

