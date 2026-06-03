#ifndef TRANSLATIONMANAGER_H
#define TRANSLATIONMANAGER_H

#include <QString>
#include <QSettings>
#include <QLocale>
#include <cstring>

class TranslationManager {
public:
    enum Language { Turkish, English, German, French, Spanish, Japanese, Chinese, Russian, LangCount };

    static void init() {
        QSettings s("EShot", "EShot");
        int saved = s.value("language", -1).toInt();
        if (saved >= 0 && saved < LangCount) {
            m_lang = static_cast<Language>(saved);
        } else {
            QLocale loc = QLocale::system();
            switch (loc.language()) {
                case QLocale::Turkish:  m_lang = Turkish; break;
                case QLocale::German:   m_lang = German; break;
                case QLocale::French:   m_lang = French; break;
                case QLocale::Spanish:  m_lang = Spanish; break;
                case QLocale::Japanese: m_lang = Japanese; break;
                case QLocale::Chinese:  m_lang = Chinese; break;
                case QLocale::Russian:  m_lang = Russian; break;
                default: m_lang = English; break;
            }
            s.setValue("language", static_cast<int>(m_lang));
        }
    }

    static void setLanguage(Language lang) {
        m_lang = lang;
        QSettings s("EShot", "EShot");
        s.setValue("language", static_cast<int>(lang));
    }

    static Language currentLanguage() { return m_lang; }

    static QString langCode() {
        switch (m_lang) {
            case Turkish:  return "tr";
            case English:  return "en";
            case German:   return "de";
            case French:   return "fr";
            case Spanish:  return "es";
            case Japanese: return "ja";
            case Chinese:  return "zh";
            case Russian:  return "ru";
        }
        return "en";
    }

    struct Trans {
        const char *key;
        const char *vals[8]; // TR EN DE FR ES JP CN RU
    };

    static QString tr(const char *key) {
        for (int i = 0; i < s_transCount; ++i) {
            if (std::strcmp(s_trans[i].key, key) == 0) {
                return QString::fromUtf8(s_trans[i].vals[static_cast<int>(m_lang)]);
            }
        }
        return QString::fromUtf8(key);
    }

    // ─── Genel ───
    static QString appTitle()         { return tr("appTitle"); }
    static QString settingsTitle()    { return tr("settingsTitle"); }
    static QString save()             { return tr("save"); }
    static QString cancel()           { return tr("cancel"); }
    static QString reset()            { return tr("reset"); }
    static QString browse()           { return tr("browse"); }
    static QString language()         { return tr("language"); }

    // ─── Tab ───
    static QString tabGeneral()       { return tr("tabGeneral"); }
    static QString tabCapture()       { return tr("tabCapture"); }
    static QString tabAppearance()    { return tr("tabAppearance"); }
    static QString tabInterface()     { return tr("tabInterface"); }
    static QString tabHotkey()        { return tr("tabHotkey"); }
    static QString tabRecording()     { return tr("tabRecording"); }

    // ─── Genel tab ───
    static QString saveDir()          { return tr("saveDir"); }
    static QString saveDirDesc()      { return tr("saveDirDesc"); }
    static QString filenamePattern()  { return tr("filenamePattern"); }
    static QString patternPreview()   { return tr("patternPreview"); }
    static QString patternVars()      { return tr("patternVars"); }
    static QString generalOptions()   { return tr("generalOptions"); }
    static QString autoStart()        { return tr("autoStart"); }
    static QString showNotifications(){ return tr("showNotifications"); }
    static QString playSound()        { return tr("playSound"); }
    static QString copyPathAfterSave(){ return tr("copyPathAfterSave"); }

    // ─── Yakalama tab ───
    static QString fileFormat()       { return tr("fileFormat"); }
    static QString formatPng()        { return tr("formatPng"); }
    static QString formatJpeg()       { return tr("formatJpeg"); }
    static QString formatBmp()        { return tr("formatBmp"); }
    static QString jpegQuality()      { return tr("jpegQuality"); }
    static QString captureSettings()  { return tr("captureSettings"); }
    static QString delay()            { return tr("delay"); }
    static QString noDelay()          { return tr("noDelay"); }
    static QString copyAfterCapture() { return tr("copyAfterCapture"); }
    static QString closeAfterCopy()   { return tr("closeAfterCopy"); }
    static QString scrollingCapture() { return tr("scrollingCapture"); }
    static QString scrollingCaptureDesc() { return tr("scrollingCaptureDesc"); }

    // ─── Görünüm tab ───
    static QString theme()            { return tr("theme"); }
    static QString darkMode()         { return tr("darkMode"); }
    static QString overlaySettings()  { return tr("overlaySettings"); }
    static QString bgOpacity()        { return tr("bgOpacity"); }
    static QString crosshair()        { return tr("crosshair"); }
    static QString crossDash()        { return tr("crossDash"); }
    static QString crossSolid()       { return tr("crossSolid"); }
    static QString crossNone()        { return tr("crossNone"); }

    // ─── Arayüz tab ───
    static QString toolbarVisibility(){ return tr("toolbarVisibility"); }
    static QString toolbarDesc()      { return tr("toolbarDesc"); }
    static QString selectAll()        { return tr("selectAll"); }
    static QString deselectAll()      { return tr("deselectAll"); }

    // ─── Kısayol tab ───
    static QString hotkeyTitle()      { return tr("hotkeyTitle"); }
    static QString hotkeyDesc()       { return tr("hotkeyDesc"); }
    static QString hotkeyValid()      { return tr("hotkeyValid"); }
    static QString hotkeyReset()      { return tr("hotkeyReset"); }
    static QString hotkeyNote()       { return tr("hotkeyNote"); }
    static QString hotkeyInvalid()    { return tr("hotkeyInvalid"); }

    // ─── Kayıt tab ───
    static QString recordingSettings() { return tr("recordingSettings"); }
    static QString recordingFps()    { return tr("recordingFps"); }
    static QString recordingTimeLimit() { return tr("recordingTimeLimit"); }
    static QString recordingSeconds() { return tr("recordingSeconds"); }
    static QString recordingUnlimited() { return tr("recordingUnlimited"); }
    static QString recordingLoop()    { return tr("recordingLoop"); }
    static QString recordingLoopInfinite() { return tr("recordingLoopInfinite"); }
    static QString recordingQuality() { return tr("recordingQuality"); }

    // ─── Dil isimleri (her zaman kendi dilinde) ───
    static QString langTurkish()  { return "Türkçe"; }
    static QString langEnglish()  { return "English"; }
    static QString langGerman()   { return "Deutsch"; }
    static QString langFrench()   { return "Français"; }
    static QString langSpanish()  { return "Español"; }
    static QString langJapanese() { return "日本語"; }
    static QString langChinese()  { return "中文"; }
    static QString langRussian()  { return "Русский"; }

    // ─── Tray ───
    static QString trayCapture()      { return tr("trayCapture"); }
    static QString trayScrollingCapture() { return tr("trayScrollingCapture"); }
    static QString trayRecordGif()    { return tr("trayRecordGif"); }
    static QString trayStopRecording() { return tr("trayStopRecording"); }
    static QString traySettings()     { return tr("traySettings"); }
    static QString trayAbout()        { return tr("trayAbout"); }
    static QString trayQuit()         { return tr("trayQuit"); }

    // ─── Erişilebilirlik ───
    static QString accessibility()    { return tr("accessibility"); }
    static QString highContrast()     { return tr("highContrast"); }
    static QString trayIcon()         { return tr("trayIcon"); }
    static QString trayIconDark()     { return tr("trayIconDark"); }
    static QString trayIconLight()    { return tr("trayIconLight"); }

    // ─── Toolbar ───
    static QString toolPen()          { return tr("toolPen"); }
    static QString toolArrow()        { return tr("toolArrow"); }
    static QString toolRect()         { return tr("toolRect"); }
    static QString toolCircle()       { return tr("toolCircle"); }
    static QString toolText()         { return tr("toolText"); }
    static QString toolHighlighter()  { return tr("toolHighlighter"); }
    static QString toolBlur()         { return tr("toolBlur"); }
    static QString toolCounter()      { return tr("toolCounter"); }
    static QString toolEraser()       { return tr("toolEraser"); }
    static QString toolLine()         { return tr("toolLine"); }
    static QString toolColor()        { return tr("toolColor"); }
    static QString toolWidth()        { return tr("toolWidth"); }
    static QString toolUndo()         { return tr("toolUndo"); }
    static QString toolRedo()         { return tr("toolRedo"); }
    static QString toolEyedropper()   { return tr("toolEyedropper"); }
    static QString toolSemiRect()     { return tr("toolSemiRect"); }
    static QString toolBlurIntensity(){ return tr("toolBlurIntensity"); }
    static QString actionPin()        { return tr("actionPin"); }
    static QString actionCopy()       { return tr("actionCopy"); }
    static QString actionSave()       { return tr("actionSave"); }
    static QString actionClose()      { return tr("actionClose"); }
    static QString actionLock()       { return tr("actionLock"); }
    static QString actionOcr()        { return tr("actionOcr"); }

    // ─── Araç listesi ───
    static QString toolListPen()      { return tr("toolListPen"); }
    static QString toolListArrow()    { return tr("toolListArrow"); }
    static QString toolListRect()     { return tr("toolListRect"); }
    static QString toolListCircle()   { return tr("toolListCircle"); }
    static QString toolListText()     { return tr("toolListText"); }
    static QString toolListHighlight(){ return tr("toolListHighlight"); }
    static QString toolListBlur()     { return tr("toolListBlur"); }
    static QString toolListCounter()  { return tr("toolListCounter"); }
    static QString toolListEraser()   { return tr("toolListEraser"); }
    static QString toolListLine()     { return tr("toolListLine"); }
    static QString toolListSemiRect() { return tr("toolListSemiRect"); }

    // ─── Pinned ───
    static QString pinnedLabel()      { return tr("pinnedLabel"); }
    static QString pinnedCopy()       { return tr("pinnedCopy"); }
    static QString pinnedSave()       { return tr("pinnedSave"); }
    static QString pinnedClose()      { return tr("pinnedClose"); }
    static QString pinnedCloseAll()   { return tr("pinnedCloseAll"); }

    // ─── Hatalar ───
    static QString errSaveDir()       { return tr("errSaveDir"); }
    static QString errInvalidHotkey() { return tr("errInvalidHotkey"); }
    static QString errTitle()         { return tr("errTitle"); }
    static QString errInvalidHotkeyTitle() { return tr("errInvalidHotkeyTitle"); }

    // ─── Sıfırlama ───
    static QString resetTitle()       { return tr("resetTitle"); }
    static QString resetConfirm()     { return tr("resetConfirm"); }

    // ─── About ───
    static QString aboutTitle()       { return tr("aboutTitle"); }
    static QString aboutDesc()        { return tr("aboutDesc"); }
    static QString checkForUpdates()  { return tr("checkForUpdates"); }
    static QString upToDate()         { return tr("upToDate"); }
    static QString version()          { return tr("version"); }

    // ─── Bildirim ───
    static QString notifCaptureTitle(){ return tr("notifCaptureTitle"); }
    static QString notifCaptureMsg(int w, int h) { return tr("notifCaptureMsg").arg(w).arg(h); }
    static QString captureSaved()     { return tr("captureSaved"); }

    // ─── Güncelleme ───
    static QString updateTitle()      { return tr("updateTitle"); }
    static QString updateMessage(const QString &v) { return tr("updateMessage").arg(v); }

    // ─── Sihirbaz ───
    static QString wizardTitle()      { return tr("wizardTitle"); }
    static QString wizardDesc()       { return tr("wizardDesc"); }
    static QString wizardNext()       { return tr("wizardNext"); }
    static QString wizardBack()       { return tr("wizardBack"); }
    static QString wizardFinish()     { return tr("wizardFinish"); }
    static QString wizardHotkeyDesc() { return tr("wizardHotkeyDesc"); }

    // ─── Dışa/İçe ───
    static QString settingsExportImport() { return tr("settingsExportImport"); }
    static QString exportSettings()   { return tr("exportSettings"); }
    static QString importSettings()   { return tr("importSettings"); }
    static QString exportSuccess()    { return tr("exportSuccess"); }
    static QString importSuccess()    { return tr("importSuccess"); }
    static QString importError()      { return tr("importError"); }

    // ─── Tooltip ───
    static QString tipLanguage()      { return tr("tipLanguage"); }
    static QString tipSaveDir()       { return tr("tipSaveDir"); }
    static QString tipAutoStart()     { return tr("tipAutoStart"); }
    static QString tipNotifications() { return tr("tipNotifications"); }
    static QString tipPlaySound()     { return tr("tipPlaySound"); }
    static QString tipCopyPath()      { return tr("tipCopyPath"); }
    static QString tipHighContrast()  { return tr("tipHighContrast"); }
    static QString tipTrayIcon()      { return tr("tipTrayIcon"); }

    // ─── OCR ───
    static QString ocrTitle()         { return tr("ocrTitle"); }
    static QString ocrCopy()          { return tr("ocrCopy"); }
    static QString ocrCopied()        { return tr("ocrCopied"); }
    static QString ocrEmpty()         { return tr("ocrEmpty"); }
    static QString ocrFailed()        { return tr("ocrFailed"); }
    static QString ocrClose()         { return tr("ocrClose"); }
    static QString ocrRetry()         { return tr("ocrRetry"); }
    static QString ocrProcessing()    { return tr("ocrProcessing"); }
    static QString ocrNoText()        { return tr("ocrNoText"); }

    // ─── Kayıt (Recording) ───
    static QString recordingStart()   { return tr("recordingStart"); }
    static QString recordingStop()    { return tr("recordingStop"); }
    static QString recordingStartTitle() { return tr("recordingStartTitle"); }
    static QString recordingStartDesc() { return tr("recordingStartDesc"); }
    static QString recordingInProgress() { return tr("recordingInProgress"); }
    static QString recordingComplete() { return tr("recordingComplete"); }
    static QString recordingSaved()   { return tr("recordingSaved"); }
    static QString recordingFailed()  { return tr("recordingFailed"); }
    static QString recordingSelectArea() { return tr("recordingSelectArea"); }
    static QString recordingPressHotkey() { return tr("recordingPressHotkey"); }
    static QString recordingTimeLimitReached() { return tr("recordingTimeLimitReached"); }
    static QString recordingMaxTime() { return tr("recordingMaxTime"); }
    static QString recordingFpsLabel() { return tr("recordingFpsLabel"); }

    // ─── Kaydırmalı Yakalama ───
    static QString scrollSelectArea() { return tr("scrollSelectArea"); }
    static QString scrollScrolling()  { return tr("scrollScrolling"); }
    static QString scrollStitching()  { return tr("scrollStitching"); }
    static QString scrollComplete()   { return tr("scrollComplete"); }
    static QString scrollFailed()     { return tr("scrollFailed"); }
    static QString scrollCountdown()  { return tr("scrollCountdown"); }
    static QString scrollPressEsc()   { return tr("scrollPressEsc"); }

    // ─── Yükleme (Upload) ───
    static QString uploadToService()  { return tr("uploadToService"); }
    static QString uploadTitle()      { return tr("uploadTitle"); }
    static QString uploadProvider()   { return tr("uploadProvider"); }
    static QString upload()           { return tr("upload"); }
    static QString uploadUploading()  { return tr("uploadUploading"); }
    static QString uploadSuccess()    { return tr("uploadSuccess"); }
    static QString uploadFailed()     { return tr("uploadFailed"); }
    static QString uploadLinkCopied() { return tr("uploadLinkCopied"); }
    static QString uploadOpen()       { return tr("uploadOpen"); }
    static QString uploadLinkPlaceholder()   { return tr("uploadLinkPlaceholder"); }
    static QString uploadDeletePlaceholder() { return tr("uploadDeletePlaceholder"); }
    static QString copy()             { return tr("copy"); }
    static QString catboxUserHash()   { return tr("catboxUserHash"); }
    static QString catboxUserHashDesc() { return tr("catboxUserHashDesc"); }

private:
    static inline Language m_lang = Turkish;

    // TR EN DE FR ES JP CN RU
    static inline const Trans s_trans[] = {
        // ─── Genel ───
        {"appTitle",       {"EShot - Ekran Görüntüsü Aracı", "EShot - Screenshot Tool", "EShot - Bildschirmfoto-Tool", "EShot - Outil de Capture d'Écran", "EShot - Herramienta de Captura", "EShot - スクリーンショットツール", "EShot - 截图工具", "EShot - Инструмент скриншотов"}},
        {"settingsTitle",  {"EShot Ayarları", "EShot Settings", "EShot Einstellungen", "Paramètres EShot", "Configuración de EShot", "EShot設定", "EShot设置", "Настройки EShot"}},
        {"save",           {"Kaydet", "Save", "Speichern", "Enregistrer", "Guardar", "保存", "保存", "Сохранить"}},
        {"cancel",         {"İptal", "Cancel", "Abbrechen", "Annuler", "Cancelar", "キャンセル", "取消", "Отмена"}},
        {"reset",          {"Varsayılana Sıfırla", "Reset to Default", "Auf Standard zurücksetzen", "Réinitialiser", "Restablecer predeterminado", "デフォルトにリセット", "恢复默认", "Сбросить"}},
        {"browse",         {"Gözat...", "Browse...", "Durchsuchen...", "Parcourir...", "Examinar...", "参照...", "浏览...", "Обзор..."}},
        {"language",       {"Dil", "Language", "Sprache", "Langue", "Idioma", "言語", "语言", "Язык"}},

        // ─── Tab ───
        {"tabGeneral",     {"Genel", "General", "Allgemein", "Général", "General", "一般", "常规", "Общие"}},
        {"tabCapture",     {"Yakalama", "Capture", "Erfassung", "Capture", "Captura", "キャプチャ", "捕获", "Захват"}},
        {"tabAppearance",  {"Görünüm", "Appearance", "Darstellung", "Apparence", "Apariencia", "外観", "外观", "Внешний вид"}},
        {"tabInterface",   {"Arayüz", "Interface", "Oberfläche", "Interface", "Interfaz", "インターフェース", "界面", "Интерфейс"}},
        {"tabHotkey",      {"Kısayol Tuşu", "Hotkey", "Tastenkürzel", "Raccourci", "Atajo", "ホットキー", "快捷键", "Горячая клавиша"}},
        {"tabRecording",   {"Kayıt", "Recording", "Aufnahme", "Enregistrement", "Grabación", "録画", "录制", "Запись"}},

        // ─── Genel tab ───
        {"saveDir",        {"Kayıt Dizini", "Save Directory", "Speicherordner", "Dossier de sauvegarde", "Directorio de guardado", "保存先フォルダ", "保存目录", "Папка сохранения"}},
        {"saveDirDesc",    {"Ekran görüntülerinin kaydedileceği dizin", "Directory where screenshots will be saved", "Verzeichnis für Bildschirmfotos", "Dossier pour captures", "Directorio de capturas", "スクリーンショットの保存先", "截图保存目录", "Папка для сохранения скриншотов"}},
        {"filenamePattern",{"Dosya Adı Şablonu", "Filename Pattern", "Dateinamensmuster", "Modèle de nom de fichier", "Patrón de nombre", "ファイル名パターン", "文件名模式", "Шаблон имени файла"}},
        {"patternPreview", {"Önizleme", "Preview", "Vorschau", "Aperçu", "Vista previa", "プレビュー", "预览", "Предпросмотр"}},
        {"patternVars",    {"<b>Değişkenler:</b> %Y (yıl), %M (ay), %D (gün), %h (saat), %m (dk), %s (sn), %T (başlık)", "<b>Variables:</b> %Y (year), %M (month), %D (day), %h (hour), %m (min), %s (sec), %T (title)", "<b>Variablen:</b> %Y (Jahr), %M (Monat), %D (Tag), %h (Stunde), %m (Min), %s (Sek), %T (Titel)", "<b>Variables :</b> %Y (année), %M (mois), %D (jour), %h (heure), %m (min), %s (sec), %T (titre)", "<b>Variables:</b> %Y (año), %M (mes), %D (día), %h (hora), %m (min), %s (seg), %T (título)", "<b>変数:</b> %Y (年), %M (月), %D (日), %h (時), %m (分), %s (秒), %T (タイトル)", "<b>变量:</b> %Y (年), %M (月), %D (日), %h (时), %m (分), %s (秒), %T (标题)", "<b>Переменные:</b> %Y (год), %M (мес), %D (день), %h (час), %m (мин), %s (сек), %T (заголовок)"}},
        {"generalOptions", {"Genel Seçenekler", "General Options", "Allgemeine Optionen", "Options générales", "Opciones generales", "一般オプション", "常规选项", "Общие параметры"}},
        {"autoStart",      {"Windows ile başlat", "Start with Windows", "Mit Windows starten", "Démarrer avec Windows", "Iniciar con Windows", "Windowsと同時に開始", "随Windows启动", "Запускать с Windows"}},
        {"showNotifications",{"Bildirim göster", "Show notifications", "Benachrichtigungen", "Notifications", "Notificaciones", "通知を表示", "显示通知", "Показывать уведомления"}},
        {"playSound",      {"Ses çal", "Play sound", "Ton abspielen", "Jouer le son", "Reproducir sonido", "サウンド再生", "播放声音", "Звук"}},
        {"copyPathAfterSave",{"Kaydettikten sonra yolu kopyala", "Copy path after save", "Pfad nach Speichern kopieren", "Copier le chemin", "Copiar ruta después de guardar", "保存後にパスをコピー", "保存后复制路径", "Копировать путь после сохранения"}},

        // ─── Yakalama ───
        {"fileFormat",     {"Dosya Formatı", "File Format", "Dateiformat", "Format de fichier", "Formato de archivo", "ファイル形式", "文件格式", "Формат файла"}},
        {"formatPng",      {"PNG (Kayıpsız)", "PNG (Lossless)", "PNG (Verlustfrei)", "PNG (Sans perte)", "PNG (Sin pérdida)", "PNG（可逆）", "PNG（无损）", "PNG (без потерь)"}},
        {"formatJpeg",     {"JPEG (Küçük)", "JPEG (Smaller)", "JPEG (Kleiner)", "JPEG (Plus petit)", "JPEG (Pequeño)", "JPEG（小）", "JPEG（较小）", "JPEG (сжатый)"}},
        {"formatBmp",      {"BMP (Sıkıştırmasız)", "BMP (Uncompressed)", "BMP (Unkomprimiert)", "BMP (Non compressé)", "BMP (Sin compresión)", "BMP（无压缩）", "BMP（未压缩）", "BMP (без сжатия)"}},
        {"jpegQuality",    {"JPEG Kalitesi:", "JPEG Quality:", "JPEG-Qualität:", "Qualité JPEG:", "Calidad JPEG:", "JPEG品質:", "JPEG质量:", "Качество JPEG:"}},
        {"captureSettings",{"Yakalama Ayarları", "Capture Settings", "Erfassungseinstellungen", "Paramètres de capture", "Configuración de captura", "キャプチャ設定", "捕获设置", "Параметры захвата"}},
        {"delay",          {"Gecikme:", "Delay:", "Verzögerung:", "Délai:", "Retraso:", "遅延:", "延迟:", "Задержка:"}},
        {"noDelay",        {"Gecikme yok", "No delay", "Keine Verzögerung", "Pas de délai", "Sin retraso", "遅延なし", "无延迟", "Без задержки"}},
        {"copyAfterCapture",{"Yakaladıktan sonra kopyala", "Copy after capture", "Nach Erfassung kopieren", "Copier après capture", "Copiar después de capturar", "キャプチャ後にコピー", "捕获后复制", "Копировать после захвата"}},
        {"closeAfterCopy", {"Kopyaladıktan sonra kapat", "Close after copy", "Nach Kopieren schließen", "Fermer après copie", "Cerrar después de copiar", "コピー後に閉じる", "复制后关闭", "Закрыть после копирования"}},
        {"scrollingCapture",{"Kaydırmalı Yakalama", "Scrolling Capture", "Scrollaufnahme", "Capture défilante", "Captura con desplazamiento", "スクロールキャプチャ", "滚动截图", "Захват с прокруткой"}},
        {"scrollingCaptureDesc",{"Sayfayı otomatik kaydırarak uzun ekran görüntüsü al", "Auto-scroll and capture long screenshot", "Automatisch scrollen und langes Bildschirmfoto aufnehmen", "Défiler et capturer une longue capture", "Desplazamiento automático para captura larga", "自動スクロールで長いスクリーンショット", "自动滚动以截取长截图", "Автопрокрутка для длинного скриншота"}},

        // ─── Görünüm ───
        {"theme",          {"Tema", "Theme", "Thema", "Thème", "Tema", "テーマ", "主题", "Тема"}},
        {"darkMode",       {"Koyu tema", "Dark theme", "Dunkles Thema", "Thème sombre", "Tema oscuro", "ダークテーマ", "深色主题", "Тёмная тема"}},
        {"overlaySettings",{"Overlay Ayarları", "Overlay Settings", "Overlay-Einstellungen", "Paramètres superposition", "Configuración superposición", "オーバーレイ設定", "覆盖层设置", "Настройки оверлея"}},
        {"bgOpacity",      {"Arka Plan Opaklığı:", "Background Opacity:", "Hintergrund-Deckkraft:", "Opacité de fond:", "Opacidad de fondo:", "背景の不透明度:", "背景不透明度:", "Прозрачность фона:"}},
        {"crosshair",      {"Crosshair:", "Crosshair:", "Fadenkreuz:", "Viseur:", "Mira:", "クロスヘア:", "十字准星:", "Перекрестие:"}},
        {"crossDash",      {"Kesikli çizgi", "Dashed line", "Gestrichelt", "Pointillés", "Discontinua", "破線", "虚线", "Пунктир"}},
        {"crossSolid",     {"Düz çizgi", "Solid line", "Durchgezogen", "Pleine", "Sólida", "実線", "实线", "Сплошная"}},
        {"crossNone",      {"Kapalı", "Off", "Aus", "Désactivé", "Desactivado", "オフ", "关闭", "Выкл."}},

        // ─── Arayüz ───
        {"toolbarVisibility",{"Araç Çubuğu Görünürlüğü", "Toolbar Visibility", "Symbolleiste", "Barre d'outils", "Barra de herramientas", "ツールバー表示", "工具栏可见性", "Видимость панели"}},
        {"toolbarDesc",    {"Toolbar'da gösterilecek araçları seçin:", "Select tools to show:", "Werkzeuge auswählen:", "Outils à afficher:", "Herramientas a mostrar:", "ツールを選択:", "选择显示的工具:", "Выберите инструменты:"}},
        {"selectAll",      {"Tümünü Seç", "Select All", "Alle auswählen", "Tout sélectionner", "Seleccionar todo", "すべて選択", "全选", "Выбрать все"}},
        {"deselectAll",    {"Tümünü Kaldır", "Deselect All", "Alle abwählen", "Tout désélectionner", "Deseleccionar", "すべて解除", "取消全选", "Снять все"}},

        // ─── Kısayol ───
        {"hotkeyTitle",    {"Yakalama Kısayolu", "Capture Hotkey", "Erfassungskürzel", "Raccourci de capture", "Atajo de captura", "キャプチャホットキー", "捕获快捷键", "Горячая клавиша захвата"}},
        {"hotkeyDesc",     {"Kısayol tuşunu ayarlayın.\nVarsayılan: Print Screen", "Set the hotkey.\nDefault: Print Screen", "Tastenkürzel festlegen.\nStandard: Druck", "Définir le raccourci.\nDéfaut : Impr. écran", "Configurar atajo.\nPredeterminado: Imp Pant", "ホットキーを設定。\nデフォルト: Print Screen", "设置快捷键。\n默认: Print Screen", "Установите клавишу.\nПо умолчанию: Print Screen"}},
        {"hotkeyValid",    {"✅ Geçerli", "✅ Valid", "✅ Gültig", "✅ Valide", "✅ Válido", "✅ 有効", "✅ 有效", "✅ Готово"}},
        {"hotkeyReset",    {"🔄 Varsayılan (Print Screen)", "🔄 Default (Print Screen)", "🔄 Standard (Druck)", "🔄 Défaut (Impr. écran)", "🔄 Predeterminado (Imp Pant)", "🔄 デフォルト (Print Screen)", "🔄 默认 (Print Screen)", "🔄 По умолчанию (Print Screen)"}},
        {"hotkeyNote",     {"⚠️ Bazı sistem kısayolları engellenemez. Ctrl/Alt/Shift kombinasyonu önerilir.", "⚠️ Some system shortcuts cannot be overridden. Modifier combos recommended.", "⚠️ Einige Systemkürzel können nicht überschrieben werden.", "⚠️ Certains raccourcis système ne peuvent pas être remplacés.", "⚠️ Algunos atajos de sistema no se pueden sobrescribir.", "⚠️ 一部のシステムホットキーは上書きできません。", "⚠️ 某些系统快捷键无法覆盖。", "⚠️ Некоторые системные клавиши нельзя переопределить."}},
        {"hotkeyInvalid",  {"⚠️ Geçersiz kısayol", "⚠️ Invalid hotkey", "⚠️ Ungültiges Kürzel", "⚠️ Raccourci invalide", "⚠️ Atajo no válido", "⚠️ 無効なホットキー", "⚠️ 无效快捷键", "⚠️ Неверная клавиша"}},

        // ─── Kayıt tab ───
        {"recordingSettings",{"Kayıt Ayarları", "Recording Settings", "Aufnahmeeinstellungen", "Paramètres d'enregistrement", "Configuración de grabación", "録画設定", "录制设置", "Параметры записи"}},
        {"recordingFps",   {"Kare Hızı (FPS):", "Frame Rate (FPS):", "Bildrate (FPS):", "Images par seconde (FPS) :", "Fotogramas por segundo (FPS):", "フレームレート (FPS):", "帧率 (FPS):", "Частота кадров (FPS):"}},
        {"recordingTimeLimit",{"Süre Limiti:", "Time Limit:", "Zeitlimit:", "Limite de temps :", "Límite de tiempo:", "時間制限:", "时长限制:", "Лимит времени:"}},
        {"recordingSeconds",{"saniye (0 = sınırsız)", "seconds (0 = unlimited)", "Sekunden (0 = unbegrenzt)", "secondes (0 = illimité)", "segundos (0 = ilimitado)", "秒 (0 = 無制限)", "秒 (0 = 无限制)", "секунд (0 = без лимита)"}},
        {"recordingUnlimited",{"Sınırsız", "Unlimited", "Unbegrenzt", "Illimité", "Ilimitado", "無制限", "无限制", "Без лимита"}},
        {"recordingLoop",  {"Döngü:", "Loop:", "Schleife:", "Boucle :", "Bucle:", "ループ:", "循环:", "Повтор:"}},
        {"recordingLoopInfinite",{"Sonsuz", "Infinite", "Endlos", "Infini", "Infinito", "無限", "无限", "Бесконечно"}},
        {"recordingQuality",{"GIF Kalitesi:", "GIF Quality:", "GIF-Qualität:", "Qualité GIF :", "Calidad GIF:", "GIF品質:", "GIF 质量:", "Качество GIF:"}},

        // ─── Dil isimleri (her zaman kendi dilinde) ───
        {"langTurkish",    {"Türkçe", "Türkçe", "Türkçe", "Türkçe", "Türkçe", "トルコ語", "土耳其语", "Турецкий"}},
        {"langEnglish",    {"English", "English", "English", "English", "English", "英語", "英语", "Английский"}},
        {"langGerman",     {"Deutsch", "Deutsch", "Deutsch", "Deutsch", "Deutsch", "ドイツ語", "德语", "Немецкий"}},
        {"langFrench",     {"Français", "Français", "Français", "Français", "Français", "フランス語", "法语", "Французский"}},
        {"langSpanish",    {"Español", "Español", "Español", "Español", "Español", "スペイン語", "西班牙语", "Испанский"}},
        {"langJapanese",   {"日本語", "日本語", "日本語", "日本語", "日本語", "日本語", "日语", "Японский"}},
        {"langChinese",    {"中文", "中文", "中文", "中文", "中文", "中文", "中文", "Китайский"}},
        {"langRussian",    {"Русский", "Русский", "Русский", "Русский", "Русский", "Русский", "Русский", "Русский"}},

        // ─── Tray ───
        {"trayCapture",    {"Yakala", "Capture", "Erfassen", "Capture", "Capturar", "キャプチャ", "捕获", "Захват"}},
        {"trayScrollingCapture",{"Kaydırmalı Yakala", "Scrolling Capture", "Scrollaufnahme", "Capture défilante", "Captura con desplazamiento", "スクロールキャプチャ", "滚动截图", "Захват с прокруткой"}},
        {"trayRecordGif",  {"GIF Kaydı Başlat", "Start GIF Recording", "GIF-Aufnahme starten", "Démarrer enregistrement GIF", "Iniciar grabación GIF", "GIF録画開始", "开始 GIF 录制", "Начать запись GIF"}},
        {"trayStopRecording",{"Kaydı Durdur", "Stop Recording", "Aufnahme stoppen", "Arrêter l'enregistrement", "Detener grabación", "録画停止", "停止录制", "Остановить запись"}},
        {"traySettings",   {"Ayarlar", "Settings", "Einstellungen", "Paramètres", "Configuración", "設定", "设置", "Настройки"}},
        {"trayAbout",      {"Hakkında", "About", "Über", "À propos", "Acerca de", "概要", "关于", "О программе"}},
        {"trayQuit",       {"Çıkış", "Quit", "Beenden", "Quitter", "Salir", "終了", "退出", "Выход"}},

        // ─── Erişilebilirlik ───
        {"accessibility",  {"Erişilebilirlik", "Accessibility", "Barrierefreiheit", "Accessibilité", "Accesibilidad", "アクセシビリティ", "无障碍", "Доступность"}},
        {"highContrast",   {"Yüksek kontrast", "High contrast", "Hoher Kontrast", "Contraste élevé", "Alto contraste", "ハイコントラスト", "高对比度", "Высокий контраст"}},
        {"trayIcon",       {"Tepsi Simgesi", "Tray Icon", "Taskbar-Symbol", "Icône de zone", "Icono de bandeja", "トレイアイコン", "托盘图标", "Значок в трее"}},
        {"trayIconDark",   {"Koyu", "Dark", "Dunkel", "Sombre", "Oscuro", "ダーク", "深色", "Тёмный"}},
        {"trayIconLight",  {"Açık", "Light", "Hell", "Clair", "Claro", "ライト", "浅色", "Светлый"}},

        // ─── Toolbar ───
        {"toolPen",        {"Kalem (P)", "Pen (P)", "Stift (P)", "Stylo (P)", "Lápiz (P)", "ペン (P)", "画笔 (P)", "Карандаш (P)"}},
        {"toolArrow",      {"Ok (A)", "Arrow (A)", "Pfeil (A)", "Flèche (A)", "Flecha (A)", "矢印 (A)", "箭头 (A)", "Стрелка (A)"}},
        {"toolRect",       {"Kare (R)", "Rectangle (R)", "Rechteck (R)", "Rectangle (R)", "Rectángulo (R)", "長方形 (R)", "矩形 (R)", "Прямоугольник (R)"}},
        {"toolCircle",     {"Çember (C)", "Circle (C)", "Kreis (C)", "Cercle (C)", "Círculo (C)", "円 (C)", "圆形 (C)", "Круг (C)"}},
        {"toolText",       {"Metin (T)", "Text (T)", "Text (T)", "Texte (T)", "Texto (T)", "テキスト (T)", "文本 (T)", "Текст (T)"}},
        {"toolHighlighter",{"Vurgulayıcı (H)", "Highlighter (H)", "Textmarker (H)", "Surligneur (H)", "Resaltador (H)", "マーカー (H)", "荧光笔 (H)", "Маркер (H)"}},
        {"toolBlur",       {"Bulanıklaştır (B)", "Blur (B)", "Unschärfe (B)", "Flou (B)", "Desenfocar (B)", "ぼかし (B)", "模糊 (B)", "Размытие (B)"}},
        {"toolCounter",    {"Numara (#)", "Counter (#)", "Zähler (#)", "Compteur (#)", "Contador (#)", "カウンター (#)", "计数器 (#)", "Счётчик (#)"}},
        {"toolEraser",     {"Silgi (X)", "Eraser (X)", "Radierer (X)", "Gomme (X)", "Borrador (X)", "消しゴム (X)", "橡皮 (X)", "Ластик (X)"}},
        {"toolLine",       {"Çizgi (L)", "Line (L)", "Linie (L)", "Ligne (L)", "Línea (L)", "線 (L)", "线条 (L)", "Линия (L)"}},
        {"toolColor",      {"Renk Seç", "Pick Color", "Farbe wählen", "Couleur", "Elegir color", "色を選択", "选择颜色", "Выбрать цвет"}},
        {"toolWidth",      {"Kalınlık", "Width", "Breite", "Épaisseur", "Grosor", "太さ", "粗细", "Толщина"}},
        {"toolUndo",       {"Geri Al (Ctrl+Z)", "Undo (Ctrl+Z)", "Rückgängig (Strg+Z)", "Annuler (Ctrl+Z)", "Deshacer (Ctrl+Z)", "元に戻す (Ctrl+Z)", "撤销 (Ctrl+Z)", "Отменить (Ctrl+Z)"}},
        {"toolRedo",       {"İleri Al (Ctrl+Y)", "Redo (Ctrl+Y)", "Wiederholen (Strg+Y)", "Rétablir (Ctrl+Y)", "Rehacer (Ctrl+Y)", "やり直し (Ctrl+Y)", "重做 (Ctrl+Y)", "Повторить (Ctrl+Y)"}},
        {"actionPin",      {"Ekrana Sabitle 📌", "Pin to Screen 📌", "Heften 📌", "Épingler 📌", "Fijar 📌", "ピン留め 📌", "固定 📌", "Закрепить 📌"}},
        {"actionCopy",     {"Kopyala (Ctrl+C)", "Copy (Ctrl+C)", "Kopieren (Strg+C)", "Copier (Ctrl+C)", "Copiar (Ctrl+C)", "コピー (Ctrl+C)", "复制 (Ctrl+C)", "Копировать (Ctrl+C)"}},
        {"actionSave",     {"Kaydet (Ctrl+S)", "Save (Ctrl+S)", "Speichern (Strg+S)", "Enregistrer (Ctrl+S)", "Guardar (Ctrl+S)", "保存 (Ctrl+S)", "保存 (Ctrl+S)", "Сохранить (Ctrl+S)"}},
        {"actionClose",    {"Kapat (Esc)", "Close (Esc)", "Schließen (Esc)", "Fermer (Esc)", "Cerrar (Esc)", "閉じる (Esc)", "关闭 (Esc)", "Закрыть (Esc)"}},
        {"actionLock",     {"Seçimi Kilitle", "Lock Selection", "Auswahl sperren", "Verrouiller sélection", "Blocar selección", "選択をロック", "锁定选区", "Заблокировать выделение"}},
        {"actionOcr",      {"Metin Tanıma (OCR)", "Recognize Text (OCR)", "Texterkennung (OCR)", "Reconnaître le texte (OCR)", "Reconocer texto (OCR)", "テキスト認識 (OCR)", "识别文字 (OCR)", "Распознать текст (OCR)"}},
        {"toolEyedropper", {"Renk Seçici", "Eyedropper", "Pipette", "Pipette", "Cuentagotas", " eyedropper", "取色器", "Пипетка"}},
        {"toolSemiRect",   {"Saydam Kare", "Semi-Transparent", "Halbtransparent", "Semi-transparent", "Semi-transparente", "半透明", "半透明矩形", "Полупрозрачный"}},
        {"toolBlurIntensity",{"Bulanıklık Şiddeti", "Blur Intensity", "Unschärfeintensität", "Intensité du flou", "Intensidad de desenfoque", "ぼかし強度", "模糊强度", "Сила размытия"}},

        // ─── Araç listesi ───
        {"toolListPen",    {"✏️ Kalem", "✏️ Pen", "✏️ Stift", "✏️ Stylo", "✏️ Lápiz", "✏️ ペン", "✏️ 画笔", "✏️ Карандаш"}},
        {"toolListArrow",  {"➡️ Ok", "➡️ Arrow", "➡️ Pfeil", "➡️ Flèche", "➡️ Flecha", "➡️ 矢印", "➡️ 箭头", "➡️ Стрелка"}},
        {"toolListRect",   {"⬜ Kare", "⬜ Rectangle", "⬜ Rechteck", "⬜ Rectangle", "⬜ Rectángulo", "⬜ 長方形", "⬜ 矩形", "⬜ Прямоугольник"}},
        {"toolListCircle", {"⭕ Çember", "⭕ Circle", "⭕ Kreis", "⭕ Cercle", "⭕ Círculo", "⭕ 円", "⭕ 圆形", "⭕ Круг"}},
        {"toolListText",   {"🔤 Metin", "🔤 Text", "🔤 Text", "🔤 Texte", "🔤 Texto", "🔤 テキスト", "🔤 文本", "🔤 Текст"}},
        {"toolListHighlight",{"🖍️ Vurgulayıcı", "🖍️ Highlighter", "🖍️ Textmarker", "🖍️ Surligneur", "🖍️ Resaltador", "🖍️ マーカー", "🖍️ 荧光笔", "🖍️ Маркер"}},
        {"toolListBlur",   {"🔲 Bulanıklaştır", "🔲 Blur", "🔲 Unschärfe", "🔲 Flou", "🔲 Desenfocar", "🔲 ぼかし", "🔲 模糊", "🔲 Размытие"}},
        {"toolListCounter",{"🔢 Numara", "🔢 Counter", "🔢 Zähler", "🔢 Compteur", "🔢 Contador", "🔢 カウンター", "🔢 计数器", "🔢 Счётчик"}},
        {"toolListEraser", {"🧹 Silgi", "🧹 Eraser", "🧹 Radierer", "🧹 Gomme", "🧹 Borrador", "🧹 消しゴム", "🧹 橡皮", "🧹 Ластик"}},
        {"toolListLine",   {"📏 Çizgi", "📏 Line", "📏 Linie", "📏 Ligne", "📏 Línea", "📏 線", "📏 线条", "📏 Линия"}},
        {"toolListSemiRect",{"🔳 Saydam Kare", "🔳 Semi-Rect", "🔳 Halbtransparent", "🔳 Semi-rect", "🔳 Semi-rect", "🔳 半透明", "🔳 半透明矩形", "🔳 Полупрозрачный"}},

        // ─── Pinned ───
        {"pinnedLabel",    {"Sabitlendi", "Pinned", "Gepinnt", "Épinglé", "Fijado", "ピン留め", "已固定", "Закреплено"}},
        {"pinnedCopy",     {"Kopyala", "Copy", "Kopieren", "Copier", "Copiar", "コピー", "复制", "Копировать"}},
        {"pinnedSave",     {"Farklı Kaydet...", "Save As...", "Speichern unter...", "Enregistrer sous...", "Guardar como...", "名前を付けて保存...", "另存为...", "Сохранить как..."}},
        {"pinnedClose",    {"Kapat", "Close", "Schließen", "Fermer", "Cerrar", "閉じる", "关闭", "Закрыть"}},
        {"pinnedCloseAll", {"Tümünü Kapat", "Close All", "Alle schließen", "Tout fermer", "Cerrar todo", "すべて閉じる", "全部关闭", "Закрыть все"}},

        // ─── Hatalar ───
        {"errSaveDir",     {"Kayıt dizini oluşturulamadı: ", "Failed to create directory: ", "Ordner erstellen fehlgeschlagen: ", "Échec création dossier : ", "Error al crear directorio: ", "ディレクトリ作成失敗: ", "创建目录失败: ", "Не удалось создать папку: "}},
        {"errInvalidHotkey",{"Geçersiz kısayol. Varsayılanı kullanın.", "Invalid hotkey. Use default.", "Ungültiges Kürzel.", "Raccourci invalide.", "Atajo no válido.", "無効なホットキー。", "无效快捷键。", "Неверная клавиша. Используйте по умолчанию."}},
        {"errTitle",       {"Hata", "Error", "Fehler", "Erreur", "Error", "エラー", "错误", "Ошибка"}},
        {"errInvalidHotkeyTitle",{"Geçersiz Kısayol", "Invalid Hotkey", "Ungültiges Kürzel", "Raccourci invalide", "Atajo no válido", "無効なホットキー", "无效快捷键", "Неверная клавиша"}},

        // ─── Sıfırlama ───
        {"resetTitle",     {"Sıfırla", "Reset", "Zurücksetzen", "Réinitialiser", "Restablecer", "リセット", "重置", "Сброс"}},
        {"resetConfirm",   {"Tüm ayarlar sıfırlansın mı?", "Reset all settings?", "Alle Einstellungen zurücksetzen?", "Tout réinitialiser ?", "¿Restablecer todo?", "すべてリセット？", "恢复所有设置？", "Сбросить все настройки?"}},

        // ─── About ───
        {"aboutTitle",     {"EShot Hakkında", "About EShot", "Über EShot", "À propos", "Acerca de", "概要", "关于EShot", "О EShot"}},
        {"aboutDesc",      {"Gelişmiş Ekran Alıntısı Aracı", "Advanced Screenshot Tool", "Fortgeschrittenes Screenshot-Tool", "Outil de Capture Avancé", "Herramienta de Captura Avanzada", "高度なスクリーンショットツール", "高级截图工具", "Продвинутый инструмент скриншотов"}},
        {"checkForUpdates",{"Güncellemeleri Kontrol Et", "Check for Updates", "Nach Updates suchen", "Vérifier les mises à jour", "Buscar actualizaciones", "アップデートを確認", "检查更新", "Проверить обновления"}},
        {"upToDate",       {"Güncel", "Up to date", "Aktuell", "À jour", "Actualizado", "最新", "已是最新", "Актуальная версия"}},
        {"version",        {"Sürüm", "Version", "Version", "Version", "Versión", "バージョン", "版本", "Версия"}},

        // ─── Bildirim ───
        {"notifCaptureTitle",{"EShot", "EShot", "EShot", "EShot", "EShot", "EShot", "EShot", "EShot"}},
        {"notifCaptureMsg",{"Ekran görüntüsü alındı (%1x%2)", "Screenshot taken (%1x%2)", "Bildschirmfoto (%1x%2)", "Capture (%1x%2)", "Captura (%1x%2)", "スクリーンショット (%1x%2)", "截图 (%1x%2)", "Скриншот (%1x%2)"}},
        {"captureSaved",   {"Ekran görüntüsü kaydedildi:", "Screenshot saved:", "Screenshot gespeichert:", "Capture enregistrée :", "Captura guardada:", "スクリーンショット保存:", "截图已保存:", "Скриншот сохранён:"}},

        // ─── Güncelleme ───
        {"updateTitle",    {"Güncelleme Mevcut", "Update Available", "Update verfügbar", "Mise à jour", "Actualización", "アップデート可能", "有可用更新", "Доступно обновление"}},
        {"updateMessage",  {"Yeni sürüm: %1\nGitHub'dan indirin.", "New version: %1\nDownload from GitHub.", "Neue Version: %1\nVon GitHub herunterladen.", "Nouvelle version : %1\nTélécharger.", "Nueva versión: %1\nDescargar.", "新しいバージョン: %1\nGitHubからダウンロード。", "新版本: %1\n从GitHub下载。", "Новая версия: %1\nЗагрузите с GitHub."}},

        // ─── Sihirbaz ───
        {"wizardTitle",    {"EShot'a Hoş Geldiniz!", "Welcome to EShot!", "Willkommen bei EShot!", "Bienvenue !", "¡Bienvenido!", "ようこそ！", "欢迎使用EShot！", "Добро пожаловать в EShot!"}},
        {"wizardDesc",     {"Ayarları yapılandırın.", "Configure your settings.", "Einstellungen konfigurieren.", "Configurez vos paramètres.", "Configure sus ajustes.", "設定を構成。", "配置设置。", "Настройте параметры."}},
        {"wizardNext",     {"İleri", "Next", "Weiter", "Suivant", "Siguiente", "次へ", "下一步", "Далее"}},
        {"wizardBack",     {"Geri", "Back", "Zurück", "Retour", "Atrás", "戻る", "返回", "Назад"}},
        {"wizardFinish",   {"Tamamla", "Finish", "Fertig", "Terminer", "Finalizar", "完了", "完成", "Готово"}},
        {"wizardHotkeyDesc",{"Kısayol tuşunu ayarlayın.\nVarsayılan: Print Screen", "Set the hotkey.\nDefault: Print Screen", "Tastenkürzel festlegen.\nStandard: Druck", "Définir le raccourci.\nDéfaut: Impr. écran", "Configure el atajo.\nPredeterminado: Imp Pant", "ホットキーを設定。\nデフォルト: Print Screen", "设置快捷键。\n默认: Print Screen", "Установите клавишу.\nПо умолчанию: Print Screen"}},

        // ─── Dışa/İçe ───
        {"settingsExportImport",{"Ayarları Dışa / İçe Aktar", "Export / Import Settings", "Einstellungen exportieren/importieren", "Exporter / Importer", "Exportar / Importar", "設定の書き出し/取り込み", "导出/导入设置", "Экспорт/Импорт настроек"}},
        {"exportSettings", {"Dışa Aktar", "Export", "Exportieren", "Exporter", "Exportar", "書き出し", "导出", "Экспорт"}},
        {"importSettings", {"İçe Aktar", "Import", "Importieren", "Importer", "Importar", "取り込み", "导入", "Импорт"}},
        {"exportSuccess",  {"Dışa aktarıldı.", "Exported.", "Exportiert.", "Exporté.", "Exportado.", "書き出し完了。", "导出成功。", "Экспортировано."}},
        {"importSuccess",  {"İçe aktarıldı.", "Imported.", "Importiert.", "Importé.", "Importado.", "取り込み完了。", "导入成功。", "Импортировано."}},
        {"importError",    {"Geçersiz dosya.", "Invalid file.", "Ungültige Datei.", "Fichier invalide.", "Archivo no válido.", "無効なファイル。", "无效文件。", "Неверный файл."}},

        // ─── Tooltip ───
        {"tipLanguage",    {"Dil. Kaydettikten sonra etkili olur.", "Language. Takes effect after save.", "Sprache. Wirkt nach Speichern.", "Langue. Prend effet après enregistrement.", "Idioma. Efecto al guardar.", "言語。保存後に有効。", "语言。保存后生效。", "Язык. Применяется после сохранения."}},
        {"tipSaveDir",     {"Kayıt dizini.", "Save directory.", "Speicherordner.", "Dossier de sauvegarde.", "Directorio de guardado.", "保存先。", "保存目录。", "Папка сохранения."}},
        {"tipAutoStart",   {"Windows ile başlat.", "Start with Windows.", "Mit Windows starten.", "Démarrer avec Windows.", "Iniciar con Windows.", "Windowsと同時に開始。", "随Windows启动。", "Запускать с Windows."}},
        {"tipNotifications",{"Bildirim göster.", "Show notifications.", "Benachrichtigungen.", "Notifications.", "Notificaciones.", "通知を表示。", "显示通知。", "Показывать уведомления."}},
        {"tipPlaySound",   {"Ses çal.", "Play sound.", "Ton abspielen.", "Jouer le son.", "Reproducir sonido.", "サウンド再生。", "播放声音。", "Звук."}},
        {"tipCopyPath",    {"Yolu kopyala.", "Copy path.", "Pfad kopieren.", "Copier le chemin.", "Copiar ruta.", "パスをコピー。", "复制路径。", "Копировать путь."}},
        {"tipHighContrast",{"Yüksek kontrast.", "High contrast.", "Hoher Kontrast.", "Contraste élevé.", "Alto contraste.", "ハイコントラスト。", "高对比度。", "Высокий контраст."}},
        {"tipTrayIcon",    {"Tepsi simgesi rengi.", "Tray icon color.", "Taskbar-Symbolfarbe.", "Couleur de l'icône.", "Color del icono.", "トレイアイコンの色。", "托盘图标颜色。", "Цвет значка в трее."}},

        // ─── OCR ───
        {"ocrTitle",       {"Metin Tanıma (OCR)", "Text Recognition (OCR)", "Texterkennung (OCR)", "Reconnaissance de texte (OCR)", "Reconocimiento de texto (OCR)", "テキスト認識 (OCR)", "文字识别 (OCR)", "Распознавание текста (OCR)"}},
        {"ocrCopy",        {"Panoya Kopyala", "Copy to Clipboard", "In die Zwischenablage", "Copier dans le presse-papiers", "Copiar al portapapeles", "クリップボードにコピー", "复制到剪贴板", "Копировать в буфер"}},
        {"ocrCopied",      {"Metin panoya kopyalandı", "Text copied to clipboard", "Text in die Zwischenablage kopiert", "Texte copié dans le presse-papiers", "Texto copiado al portapapeles", "テキストをコピーしました", "文本已复制", "Текст скопирован"}},
        {"ocrEmpty",       {"Görüntüde metin bulunamadı", "No text found in image", "Kein Text im Bild gefunden", "Aucun texte trouvé dans l'image", "No se encontró texto en la imagen", "画像にテキストが見つかりません", "图像中未找到文字", "Текст не найден"}},
        {"ocrFailed",      {"OCR başarısız oldu", "OCR failed", "OCR fehlgeschlagen", "Échec de l'OCR", "OCR falló", "OCRに失敗しました", "OCR 失败", "Ошибка OCR"}},
        {"ocrClose",       {"Kapat", "Close", "Schließen", "Fermer", "Cerrar", "閉じる", "关闭", "Закрыть"}},
        {"ocrRetry",       {"Yeniden Dene", "Retry", "Erneut", "Réessayer", "Reintentar", "再試行", "重试", "Повторить"}},
        {"ocrProcessing",  {"Tanınıyor...", "Recognizing...", "Erkennung...", "Reconnaissance...", "Reconociendo...", "認識中...", "识别中...", "Распознавание..."}},
        {"ocrNoText",      {"Metin algılanmadı", "No text detected", "Kein Text erkannt", "Aucun texte détecté", "No se detectó texto", "テキストが検出されませんでした", "未检测到文字", "Текст не обнаружен"}},

        // ─── Kayıt (Recording) ───
        {"recordingStart", {"Kaydı Başlat", "Start Recording", "Aufnahme starten", "Démarrer l'enregistrement", "Iniciar grabación", "録画開始", "开始录制", "Начать запись"}},
        {"recordingStop",  {"Kaydı Durdur", "Stop Recording", "Aufnahme stoppen", "Arrêter l'enregistrement", "Detener grabación", "録画停止", "停止录制", "Остановить запись"}},
        {"recordingStartTitle",{"GIF Kaydı", "GIF Recording", "GIF-Aufnahme", "Enregistrement GIF", "Grabación GIF", "GIF録画", "GIF 录制", "Запись GIF"}},
        {"recordingStartDesc",{"Kaydedilecek alanı seçin ve başlatın", "Select area to record and start", "Bereich wählen und starten", "Sélectionnez la zone et démarrez", "Seleccione el área e inicie", "録画する範囲を選択", "选择录制区域并开始", "Выберите область и начните"}},
        {"recordingInProgress",{"Kayıt devam ediyor...", "Recording in progress...", "Aufnahme läuft...", "Enregistrement en cours...", "Grabación en curso...", "録画中...", "录制中...", "Идёт запись..."}},
        {"recordingComplete",{"Kayıt tamamlandı", "Recording complete", "Aufnahme abgeschlossen", "Enregistrement terminé", "Grabación completa", "録画完了", "录制完成", "Запись завершена"}},
        {"recordingSaved", {"GIF kaydedildi:", "GIF saved:", "GIF gespeichert:", "GIF enregistré :", "GIF guardado:", "GIF保存:", "GIF 已保存:", "GIF сохранён:"}},
        {"recordingFailed",{"Kayıt başarısız oldu", "Recording failed", "Aufnahme fehlgeschlagen", "Échec de l'enregistrement", "Grabación falló", "録画に失敗しました", "录制失败", "Ошибка записи"}},
        {"recordingSelectArea",{"Kayıt için alan seçin", "Select area to record", "Bereich für Aufnahme wählen", "Sélectionnez la zone à enregistrer", "Seleccione el área a grabar", "録画範囲を選択", "选择录制区域", "Выберите область записи"}},
        {"recordingPressHotkey",{"Kayıt için alan seçin veya iptal için Esc", "Select area or press Esc to cancel", "Bereich wählen oder Esc zum Abbrechen", "Sélectionnez la zone ou Esc pour annuler", "Seleccione el área o Esc para cancelar", "範囲を選択または Esc でキャンセル", "选择区域或按 Esc 取消", "Выберите область или Esc для отмены"}},
        {"recordingTimeLimitReached",{"Süre limiti doldu", "Time limit reached", "Zeitlimit erreicht", "Limite de temps atteinte", "Límite de tiempo alcanzado", "時間制限に達しました", "已达时长限制", "Достигнут лимит времени"}},
        {"recordingMaxTime",{"Maks. süre (sn):", "Max time (sec):", "Max. Zeit (Sek.):", "Temps max (sec) :", "Tiempo máx. (seg):", "最大時間 (秒):", "最大时长 (秒):", "Макс. время (сек):"}},
        {"recordingFpsLabel",{"FPS:", "FPS:", "FPS:", "FPS :", "FPS:", "FPS:", "FPS:", "FPS:"}},

        // ─── Kaydırmalı Yakalama ───
        {"scrollSelectArea",{"Kaydırmalı alan seçin", "Select scrolling area", "Scrollbereich wählen", "Sélectionnez la zone défilante", "Seleccione el área de desplazamiento", "スクロール範囲を選択", "选择滚动区域", "Выберите область прокрутки"}},
        {"scrollScrolling",{"Kaydırılıyor...", "Scrolling...", "Scrollen...", "Défilement...", "Desplazando...", "スクロール中...", "滚动中...", "Прокрутка..."}},
        {"scrollStitching",{"Birleştiriliyor...", "Stitching...", "Zusammenfügen...", "Assemblage...", "Uniendo...", "結合中...", "拼接中...", "Склейка..."}},
        {"scrollComplete",{"Kaydırmalı yakalama tamamlandı", "Scrolling capture complete", "Scrollaufnahme abgeschlossen", "Capture défilante terminée", "Captura con desplazamiento completa", "スクロールキャプチャ完了", "滚动截图完成", "Захват с прокруткой завершён"}},
        {"scrollFailed",  {"Kaydırmalı yakalama başarısız", "Scrolling capture failed", "Scrollaufnahme fehlgeschlagen", "Échec de la capture défilante", "Captura con desplazamiento falló", "スクロールキャプチャ失敗", "滚动截图失败", "Ошибка захвата с прокруткой"}},
        {"scrollCountdown",{"Başlıyor: %1", "Starting: %1", "Startet: %1", "Démarrage : %1", "Iniciando: %1", "開始: %1", "开始: %1", "Запуск: %1"}},
        {"scrollPressEsc",{"İptal için Esc", "Press Esc to cancel", "Esc zum Abbrechen", "Appuyez sur Esc pour annuler", "Pulse Esc para cancelar", "Esc でキャンセル", "按 Esc 取消", "Esc для отмены"}},

        // ─── Yükleme (Upload) ───
        {"uploadToService",{"Servise Yükle", "Upload to Service", "Zu Dienst hochladen", "Téléverser vers un service", "Subir al servicio", "サービスにアップロード", "上传到服务", "Загрузить в сервис"}},
        {"uploadTitle",    {"Görsel Yükle", "Upload Image", "Bild hochladen", "Téléverser l'image", "Subir imagen", "画像をアップロード", "上传图片", "Загрузить изображение"}},
        {"uploadProvider", {"Servis:", "Service:", "Dienst:", "Service :", "Servicio:", "サービス:", "服务:", "Сервис:"}},
        {"upload",         {"Yükle", "Upload", "Hochladen", "Téléverser", "Subir", "アップロード", "上传", "Загрузить"}},
        {"uploadUploading",{"Yükleniyor...", "Uploading...", "Wird hochgeladen...", "Téléversement...", "Subiendo...", "アップロード中...", "上传中...", "Загрузка..."}},
        {"uploadSuccess",  {"Yüklendi!", "Uploaded!", "Hochgeladen!", "Téléversé !", "¡Subido!", "アップロード完了！", "上传成功！", "Загружено!"}},
        {"uploadFailed",   {"Yükleme başarısız", "Upload failed", "Hochladen fehlgeschlagen", "Échec du téléversement", "Error al subir", "アップロード失敗", "上传失败", "Ошибка загрузки"}},
        {"uploadLinkCopied",{"Bağlantı panoya kopyalandı", "Link copied to clipboard", "Link in die Zwischenablage kopiert", "Lien copié", "Enlace copiado", "リンクをコピーしました", "链接已复制", "Ссылка скопирована"}},
        {"uploadOpen",     {"Bağlantıyı Aç", "Open Link", "Link öffnen", "Ouvrir le lien", "Abrir enlace", "リンクを開く", "打开链接", "Открыть ссылку"}},
        {"uploadLinkPlaceholder",{"Görsel bağlantısı burada görünecek", "Image link will appear here", "Bildlink erscheint hier", "Le lien apparaîtra ici", "El enlace aparecerá aquí", "リンクがここに表示されます", "链接将显示在此处", "Ссылка появится здесь"}},
        {"uploadDeletePlaceholder",{"Silme bağlantısı (opsiyonel)", "Delete link (optional)", "Löschlink (optional)", "Lien de suppression (optionnel)", "Enlace de borrado (opcional)", "削除リンク（任意）", "删除链接（可选）", "Ссылка на удаление (необязательно)"}},
        {"copy",           {"Kopyala", "Copy", "Kopieren", "Copier", "Copiar", "コピー", "复制", "Копировать"}},
        {"catboxUserHash", {"Catbox Kullanıcı Hash:", "Catbox User Hash:", "Catbox Benutzer-Hash:", "Hash utilisateur Catbox :", "Hash de usuario Catbox:", "Catbox ユーザーハッシュ:", "Catbox 用户哈希:", "Хэш пользователя Catbox:"}},
        {"catboxUserHashDesc",{"İsteğe bağlı. Hesabınızla yükleme/düzenleme/silme için catbox.moe → Hesap.", "Optional. For upload/edit/delete with your account: catbox.moe → Account.", "Optional. Für Hochladen/Bearbeiten/Löschen mit Konto: catbox.moe → Konto.", "Optionnel. Pour téléverser/modifier/supprimer avec votre compte : catbox.moe → Compte.", "Opcional. Para subir/editar/borrar con su cuenta: catbox.moe → Cuenta.", "任意。アカウントでアップロード/編集/削除する場合: catbox.moe → アカウント", "可选。使用您的账户上传/编辑/删除：catbox.moe → 账户", "Необязательно. Для загрузки/правки/удаления в своём аккаунте: catbox.moe → Аккаунт."}},
    };

    static constexpr int s_transCount = sizeof(s_trans) / sizeof(s_trans[0]);
};

#endif
