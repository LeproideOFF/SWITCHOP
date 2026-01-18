# 🎮 SWITCHOP
> **L'ultime Store Homebrew pour Nintendo Switch.**

SWITCHOP est une application homebrew moderne conçue avec **SDL2**. Elle permet aux utilisateurs de consoles hackées de parcourir un catalogue dynamique, de lire les actualités de la scène, et d'accumuler des points grâce à un système de gamification intégré.

---

## ✨ Fonctionnalités

### 🛒 Le Store
* **Catalogue Cloud** : Récupération en temps réel des applications et jeux via une API JSON.
* **Recherche Intelligente** : Moteur de recherche intégré (Bouton X) insensible à la casse.
* **Filtrage par Catégories** : Navigation simplifiée entre "Jeux", "Apps" et "Tout".
* **Système de Points** : Un mini-jeu "Clicker" intégré permet de farmer des points pour de futures récompenses.

### 📢 Communication & News
* **Bandeau de Broadcast** : Une barre défilante en haut de l'écran affiche les annonces serveurs en direct.
* **Flux d'Actualités** : Un menu dédié pour suivre les nouveautés, avec défilement complet.
* **Interface Réactive** : Feedback visuel (animations, particules) lors des clics.

### 🛠️ Technique & UX
* **Scrolling Hybride** : Navigation fluide au doigt (tactile) ou à la manette (Joy-Cons).
* **Sécurité** : Chaque console possède son propre profil basé sur un `deviceId` unique stocké sur la SD.
* **Musique d'ambiance** : Support de la lecture audio pendant la navigation.

---

## 🚀 Installation

1. Téléchargez le dernier fichier `.nro` depuis l'onglet [Releases](https://github.com/ton-pseudo/SWITCHOP/releases).
2. Placez-le sur votre carte SD : `sdmc:/switch/SWITCHOP.nro`.
3. Lancez l'application via le Homebrew Menu.

> **Note :** Un accès Wi-Fi est indispensable pour le chargement des données.

---

## 💻 Compilation (Développeurs)

Le projet nécessite **devkitPro** et la **libnx**.

### Prérequis
```bash
# Installez les librairies nécessaires via pacman (devkitPro)
dkp-pacman -S switch-sdl2 switch-sdl2_ttf switch-sdl2_mixer switch-sdl2_gfx switch-curl switch-nlohmann-json
Build
Bash
git clone [https://github.com/ton-pseudo/SWITCHOP.git](https://github.com/ton-pseudo/SWITCHOP.git)
cd SWITCHOP
make
📡 Backend (API)
SWITCHOP récupère ses données depuis une structure JSON spécifique.

URL Serveur : Défini dans app.hpp.

Champs JSON supportés : broadcast, news, apps, music_url.

📜 Crédits
SDL2 : Pour le rendu graphique.

libnx : Pour l'accès aux fonctions natives de la console.

CURL : Pour la gestion des requêtes réseau.

nlohmann/json : Pour le parsing des données.

⚠️ Avertissement : Ce projet est fourni à des fins éducatives. Ni l'auteur ni SWITCHOP ne sont responsables de l'utilisation que vous en faites ou des dommages causés à votre console.
