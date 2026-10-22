# Droits, licences et éléments non redistribués

## Licence du dépôt

**Aucune licence open source n'est attribuée à ce dépôt pour l'instant.** Sans licence explicite, le contenu peut être consulté sur GitHub, mais pas réutilisé ni redistribué sans l'accord de l'auteur.

Ce choix est volontaire : une partie du contenu dépend de droits tiers qui ne sont pas encore clarifiés (voir ci-dessous). Une licence pourra être ajoutée plus tard pour le code écrit par l'auteur.

## Contenu produit par l'auteur

- Firmware ESP32 (`firmware/`), scripts Python (`edge/`), flux Node-RED (`node-red/`), fichiers de configuration (`infra/`).
- Schéma de câblage Fritzing (`hardware/`), avec les réserves de la section « Schéma Fritzing ».
- Captures d'écran, photos et vidéo de démonstration du prototype, diagramme d'architecture (`docs/`).

Les captures d'écran montrent des interfaces de logiciels tiers (Node-RED, Grafana) qui restent la propriété de leurs éditeurs respectifs.

## Composants et outils tiers

Le projet **utilise** les logiciels suivants **sans les redistribuer**. Chacun reste soumis à **sa propre licence** :

- **Ultralytics YOLO** : utilisé pour l'entraînement et l'inférence. Ultralytics est distribué sous licence AGPL-3.0, avec une licence commerciale séparée. Cela doit être pris en compte avant toute redistribution de modèles ou de code qui en dépend.
- **Node-RED** et les nœuds contribués listés dans `node-red/package.json`.
- **Bibliothèques Arduino** : `PubSubClient`, `DHT sensor library`, `ESP32Servo`, ainsi que le cœur ESP32 pour Arduino.
- **Mosquitto**, **InfluxDB**, **CouchDB**, **Grafana**, **OpenCV**, **libcamera**, **Fritzing**.

Consultez la licence de chaque projet avant de le réutiliser.

## Schéma Fritzing

Le fichier `hardware/Mon_montage_hydro_system_final.fzz` et ses exports (`montage_breadboard.png` / `.svg`) contiennent des définitions et des dessins de composants : ESP32 DevKitC, modules pH et TDS, relais, ventilateur, interrupteur à flotteur… Certains proviennent de la bibliothèque Fritzing, d'autres de contributeurs tiers. **Ces éléments graphiques restent soumis à leurs licences d'origine.** Seuls le choix des composants et le câblage représentés relèvent du travail de l'auteur.

## Éléments volontairement non redistribués

- **Jeux de données d'entraînement** : images, annotations et fichiers de configuration exportés. Ils proviennent de sources tierces, dont un jeu Roboflow déclaré sous licence « Private ».
- **Poids des modèles** (`.tflite`, `.pt`) : non distribués tant que leurs droits ne sont pas clarifiés, car ils dérivent de ces données et de l'outillage Ultralytics.
- **Secrets et configuration locale** : identifiants Wi-Fi, MQTT, CouchDB, InfluxDB et SMTP, fichiers d'identifiants Node-RED, historique système.

Voir aussi [`models/README.md`](models/README.md).
