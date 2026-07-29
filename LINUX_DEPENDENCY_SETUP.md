# EShot Linux dependency setup

Bu not CachyOS/Arch tabanli sistemlerde EShot'un tek seferde bagimliliklarini kurup calismasi icindir.

## Tek komutla kurulum

EShot klasorunde su komutu calistir:

```bash
bash scripts/linux/install-cachyos-deps.sh
```

Bu script `sudo pacman -S --needed ...` kullanir. Parola isteyebilir. Paket zaten kuruluysa tekrar kurmaz.

Kurulan ana paketler:

```text
base-devel
cmake
ninja
qt6-base
qt6-wayland
libx11
libxkbcommon
desktop-file-utils
appstream
ffmpeg
tesseract
tesseract-data-eng
pipewire
wireplumber
gst-plugin-pipewire
gst-plugins-base
gst-plugins-good
gst-plugins-bad
gst-plugins-ugly
xdg-desktop-portal
xdg-desktop-portal-kde
```

## Kontrol

Kurulumdan sonra kontrol icin:

```bash
bash scripts/linux/check-linux-runtime.sh
```

Eksik kalmadiysa `desktop-launch.sh` ilk acilista Linux build'i alir ve uygulamayi baslatir:

```bash
bash scripts/linux/desktop-launch.sh
```

## Grafik arayuzden istege bagli kurulum

AppImage ilk acilista FFmpeg/GIF-video, Tesseract/OCR dilleri ve masaustu
portal entegrasyonunu ayri ayri secen bir kurulum ekrani acar. Paket kurulumu
`pkexec` ile yonetici onayi isteyebilir. Bu adim atlanabilir; daha sonra
**Ayarlar > Linux bagimlilik kurulumunu ac** dugmesinden ayni secim ekrani
yeniden acilir.

## AppImage guncellemesi

AppImage olarak calisirken Ayarlar veya sistem tepsindeki **Simdi guncelle**
eylemi yeni surumu yerinde kurar. EShot yalnizca GitHub Release icindeki
`EShot-v<surum>-x86_64.AppImage` dosyasini kullanir, GitHub'in SHA-256 digest
degerini zorunlu olarak dogrular, `APPIMAGE` ortam degiskeninin gosterdigi
mevcut dosyayi degistirir ve yeniden baslatir. `.deb` ya da kaynak build
kurulumlari kendi dosyasini degistirmez; bunlar paket yoneticisi veya kaynak
build akisi ile guncellenmelidir.

## Neden tek tikta kurulum bazen duruyor?

`desktop-launch.sh` eksik bagimlilik gorurse paket kurmayi dener. GUI icinden calistiginda `pkexec` veya `sudo` parola/onay bekleyebilir. Bu pencere gorunmezse uygulama acilmiyor gibi durur.

En temiz cozum ilk seferde terminalden su komutu calistirmaktir:

```bash
cd "/run/media/emirhan/Samsung 990 PRO/Users/emirh/OneDrive/Masaüstü/Code/VSCODE/EShot"
bash scripts/linux/install-cachyos-deps.sh
```

Bundan sonra `.desktop` dosyasina cift tiklamak yeterli olmalidir.
