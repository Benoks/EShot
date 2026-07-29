#!/usr/bin/env bash
set -euo pipefail

sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  ninja-build \
  curl \
  qt6-base-dev \
  qt6-base-dev-tools \
  qt6-qpa-plugins \
  qt6-wayland \
  libqt6svg6 \
  libx11-dev \
  libgl1-mesa-dev \
  libxcb-cursor0 \
  libxkbcommon-dev \
  libsecret-1-dev \
  desktop-file-utils \
  appstream \
  ffmpeg \
  tesseract-ocr \
  tesseract-ocr-eng \
  gstreamer1.0-tools \
  gstreamer1.0-pipewire \
  gstreamer1.0-pulseaudio \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad \
  gstreamer1.0-plugins-ugly \
  gstreamer1.0-libav \
  xdg-desktop-portal \
  xdg-desktop-portal-gtk
