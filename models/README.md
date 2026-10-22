# Modèles de vision

Ce dossier documente les modèles de détection utilisés par le prototype. **Les jeux de données et les poids des modèles ne sont pas distribués dans ce dépôt.**

## Modèles entraînés pendant le mémoire

| Modèle | Objectif | Classe détectée | Fichier utilisé sur le Raspberry Pi (non fourni) | Script |
|---|---|---|---|---|
| YOLOv9t « ravageurs » | Repérer des pucerons sur les feuilles | `Pest` | `best_float32_pest_detect.tflite` | `edge/raspberry/nuisible_inference.py` |
| YOLOv9t « maladies » | Repérer une laitue présentant des symptômes de maladie | `Lettuce-Diseased` | `best_float32_disease_detect.tflite` | `edge/raspberry/plant_inference.py` |

Chaque modèle détecte **une seule classe**. Les deux ont été entraînés sur Google Colab avec Ultralytics, par apprentissage par transfert, puis exportés au format TFLite (float32) pour l'inférence sur Raspberry Pi.

## Tailles des données réellement utilisées

| Modèle | Entraînement | Validation | Instances annotées (validation) |
|---|---|---|---|
| Ravageurs | 490 images | 170 images | 583 (après réannotation) |
| Maladies | 800 images | 141 images | 141 (un objet par image) |

Un jeu de test séparé existe pour chaque modèle. Il n'a pas encore servi à une évaluation finale.

## Résultats du modèle « ravageurs »

Mesurés sur le **jeu de validation** avec le meilleur checkpoint :

| Precision | Recall | mAP50 | mAP50-95 |
|---|---|---|---|
| 0,821 | 0,787 | 0,865 | 0,433 |

Paramètres d'entraînement : 100 epochs, SGD, lr0 = 0,01, batch 16, images de 640 px.

Ces métriques ont servi à **sélectionner le modèle**. Elles ne remplacent pas une évaluation finale sur le jeu de test, qui reste à faire. La démarche et le rôle de la réannotation sont décrits dans le [README principal](../README.md#9-limpact-de-la-qualité-des-données).

Le modèle « maladies » n'est pas présenté avec des métriques de référence. Ses données correspondaient à une tâche simplifiée (un seul objet par image, proche d'une classification), et il généralisait mal sur les images réelles de la caméra du Pi.

## Ce qui n'est pas distribué, et pourquoi

- **Jeux de données** (images, annotations, fichiers de configuration exportés) : ils proviennent de sources tierces. Le jeu Roboflow utilisé pour les pucerons est déclaré sous licence « Private » dans ses métadonnées. Rien n'en est redistribué, pas même le fichier de configuration exporté.
- **Poids des modèles** (`.tflite`, `.pt`) : ils dérivent de ces données et ont été produits avec Ultralytics, qui a sa propre licence (voir [`NOTICE.md`](../NOTICE.md)). Leurs droits de redistribution ne sont pas clarifiés.

Cette décision pourra être revue si les droits sont clarifiés.
