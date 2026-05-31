#ifndef TRANSLATIONMANAGER_H
#define TRANSLATIONMANAGER_H

#include <QString>
#include <QSettings>
#include <QLocale>
#include <cstring>

class TranslationManager {
public:
    enum Language { Turkish, English, German, French, Spanish, Japanese, Chinese, LangCount };

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
                default: m_lang = English; break;
            }
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
        }
        return "en";
    }

    struct Trans {
        const char *key;
        const char *vals[7]; // TR EN DE FR ES JP CN
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

    // ─── Dil isimleri (her zaman kendi dilinde) ───
    static QString langTurkish()  { return "Türkçe"; }
    static QString langEnglish()  { return "English"; }
    static QString langGerman()   { return "Deutsch"; }
    static QString langFrench()   { return "Français"; }
    static QString langSpanish()  { return "Español"; }
    static QString langJapanese() { return "日本語"; }
    static QString langChinese()  { return "中文"; }

    // ─── Tray ───
    static QString trayCapture()      { return tr("trayCapture"); }
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
    static QString actionPin()        { return tr("actionPin"); }
    static QString actionCopy()       { return tr("actionCopy"); }
    static QString actionSave()       { return tr("actionSave"); }
    static QString actionClose()      { return tr("actionClose"); }

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

    // ─── Bildirim ───
    static QString notifCaptureTitle(){ return tr("notifCaptureTitle"); }
    static QString notifCaptureMsg(int w, int h) { return tr("notifCaptureMsg").arg(w).arg(h); }

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

private:
    static inline Language m_lang = Turkish;

    // TR EN DE FR ES JP CN
    static inline const Trans s_trans[] = {
        // ─── Genel ───
        {"appTitle",       {"EShot - Ekran Görüntüsü Aracı", "EShot - Screenshot Tool", "EShot - Bildschirmfoto-Tool", "EShot - Outil de Capture d'Écran", "EShot - Herramienta de Captura", "EShot - スクリーンショットツール", "EShot - 截图工具"}},
        {"settingsTitle",  {"EShot Ayarları", "EShot Settings", "EShot Einstellungen", "Paramètres EShot", "Configuración de EShot", "EShot設定", "EShot设置"}},
        {"save",           {"Kaydet", "Save", "Speichern", "Enregistrer", "Guardar", "保存", "保存"}},
        {"cancel",         {"İptal", "Cancel", "Abbrechen", "Annuler", "Cancelar", "キャンセル", "取消"}},
        {"reset",          {"Varsayılana Sıfırla", "Reset to Default", "Auf Standard zurücksetzen", "Réinitialiser", "Restablecer predeterminado", "デフォルトにリセット", "恢复默认"}},
        {"browse",         {"Gözat...", "Browse...", "Durchsuchen...", "Parcourir...", "Examinar...", "参照...", "浏览..."}},
        {"language",       {"Dil", "Language", "Sprache", "Langue", "Idioma", "言語", "语言"}},

        // ─── Tab ───
        {"tabGeneral",     {"Genel", "General", "Allgemein", "Général", "General", "一般", "常规"}},
        {"tabCapture",     {"Yakalama", "Capture", "Erfassung", "Capture", "Captura", "キャプチャ", "捕获"}},
        {"tabAppearance",  {"Görünüm", "Appearance", "Darstellung", "Apparence", "Apariencia", "外観", "外观"}},
        {"tabInterface",   {"Arayüz", "Interface", "Oberfläche", "Interface", "Interfaz", "インターフェース", "界面"}},
        {"tabHotkey",      {"Kısayol Tuşu", "Hotkey", "Tastenkürzel", "Raccourci", "Atajo", "ホットキー", "快捷键"}},

        // ─── Genel tab ───
        {"saveDir",        {"Kayıt Dizini", "Save Directory", "Speicherordner", "Dossier de sauvegarde", "Directorio de guardado", "保存先フォルダ", "保存目录"}},
        {"saveDirDesc",    {"Ekran görüntülerinin kaydedileceği dizin", "Directory where screenshots will be saved", "Verzeichnis für Bildschirmfotos", "Dossier pour captures", "Directorio de capturas", "スクリーンショットの保存先", "截图保存目录"}},
        {"filenamePattern",{"Dosya Adı Şablonu", "Filename Pattern", "Dateinamensmuster", "Modèle de nom de fichier", "Patrón de nombre", "ファイル名パターン", "文件名模式"}},
        {"patternPreview", {"Önizleme", "Preview", "Vorschau", "Aperçu", "Vista previa", "プレビュー", "预览"}},
        {"patternVars",    {"<b>Değişkenler:</b> %Y (yıl), %M (ay), %D (gün), %h (saat), %m (dk), %s (sn), %T (başlık)", "<b>Variables:</b> %Y (year), %M (month), %D (day), %h (hour), %m (min), %s (sec), %T (title)", "<b>Variablen:</b> %Y (Jahr), %M (Monat), %D (Tag), %h (Stunde), %m (Min), %s (Sek), %T (Titel)", "<b>Variables:</b> %Y (année), %M (mois), %D (jour), %h (heure), %m (min), %s (sec), %T (titre)", "<b>Variables:</b> %Y (año), %M (mes), %D (día), %h (hora), %m (min), %s (seg), %T (título)", "<b>変数:</b> %Y (年), %M (月), %D (日), %h (時), %m (分), %s (秒), %T (タイトル)", "<b>变量:</b> %Y (年), %M (月), %D (日), %h (时), %m (分), %s (秒), %T (标题)"}},
        {"generalOptions", {"Genel Seçenekler", "General Options", "Allgemeine Optionen", "Options générales", "Opciones generales", "一般オプション", "常规选项"}},
        {"autoStart",      {"Windows ile başlat", "Start with Windows", "Mit Windows starten", "Démarrer avec Windows", "Iniciar con Windows", "Windowsと同時に開始", "随Windows启动"}},
        {"showNotifications",{"Bildirim göster", "Show notifications", "Benachrichtigungen", "Notifications", "Notificaciones", "通知を表示", "显示通知"}},
        {"playSound",      {"Ses çal", "Play sound", "Ton abspielen", "Jouer le son", "Reproducir sonido", "サウンド再生", "播放声音"}},
        {"copyPathAfterSave",{"Kaydettikten sonra yolu kopyala", "Copy path after save", "Pfad nach Speichern kopieren", "Copier le chemin", "Copiar ruta después de guardar", "保存後にパスをコピー", "保存后复制路径"}},

        // ─── Yakalama ───
        {"fileFormat",     {"Dosya Formatı", "File Format", "Dateiformat", "Format de fichier", "Formato de archivo", "ファイル形式", "文件格式"}},
        {"formatPng",      {"PNG (Kayıpsız)", "PNG (Lossless)", "PNG (Verlustfrei)", "PNG (Sans perte)", "PNG (Sin pérdida)", "PNG（可逆）", "PNG（无损）"}},
        {"formatJpeg",     {"JPEG (Küçük)", "JPEG (Smaller)", "JPEG (Kleiner)", "JPEG (Plus petit)", "JPEG (Pequeño)", "JPEG（小）", "JPEG（较小）"}},
        {"formatBmp",      {"BMP (Sıkıştırmasız)", "BMP (Uncompressed)", "BMP (Unkomprimiert)", "BMP (Non compressé)", "BMP (Sin compresión)", "BMP（无压缩）", "BMP（未压缩）"}},
        {"jpegQuality",    {"JPEG Kalitesi:", "JPEG Quality:", "JPEG-Qualität:", "Qualité JPEG:", "Calidad JPEG:", "JPEG品質:", "JPEG质量:"}},
        {"captureSettings",{"Yakalama Ayarları", "Capture Settings", "Erfassungseinstellungen", "Paramètres de capture", "Configuración de captura", "キャプチャ設定", "捕获设置"}},
        {"delay",          {"Gecikme:", "Delay:", "Verzögerung:", "Délai:", "Retraso:", "遅延:", "延迟:"}},
        {"noDelay",        {"Gecikme yok", "No delay", "Keine Verzögerung", "Pas de délai", "Sin retraso", "遅延なし", "无延迟"}},
        {"copyAfterCapture",{"Yakaladıktan sonra kopyala", "Copy after capture", "Nach Erfassung kopieren", "Copier après capture", "Copiar después de capturar", "キャプチャ後にコピー", "捕获后复制"}},
        {"closeAfterCopy", {"Kopyaladıktan sonra kapat", "Close after copy", "Nach Kopieren schließen", "Fermer après copie", "Cerrar después de copiar", "コピー後に閉じる", "复制后关闭"}},

        // ─── Görünüm ───
        {"theme",          {"Tema", "Theme", "Thema", "Thème", "Tema", "テーマ", "主题"}},
        {"darkMode",       {"Koyu tema", "Dark theme", "Dunkles Thema", "Thème sombre", "Tema oscuro", "ダークテーマ", "深色主题"}},
        {"overlaySettings",{"Overlay Ayarları", "Overlay Settings", "Overlay-Einstellungen", "Paramètres superposition", "Configuración superposición", "オーバーレイ設定", "覆盖层设置"}},
        {"bgOpacity",      {"Arka Plan Opaklığı:", "Background Opacity:", "Hintergrund-Deckkraft:", "Opacité de fond:", "Opacidad de fondo:", "背景の不透明度:", "背景不透明度:"}},
        {"crosshair",      {"Crosshair:", "Crosshair:", "Fadenkreuz:", "Viseur:", "Mira:", "クロスヘア:", "十字准星:"}},
        {"crossDash",      {"Kesikli çizgi", "Dashed line", "Gestrichelt", "Pointillés", "Discontinua", "破線", "虚线"}},
        {"crossSolid",     {"Düz çizgi", "Solid line", "Durchgezogen", "Pleine", "Sólida", "実線", "实线"}},
        {"crossNone",      {"Kapalı", "Off", "Aus", "Désactivé", "Desactivado", "オフ", "关闭"}},

        // ─── Arayüz ───
        {"toolbarVisibility",{"Araç Çubuğu Görünürlüğü", "Toolbar Visibility", "Symbolleiste", "Barre d'outils", "Barra de herramientas", "ツールバー表示", "工具栏可见性"}},
        {"toolbarDesc",    {"Toolbar'da gösterilecek araçları seçin:", "Select tools to show:", "Werkzeuge auswählen:", "Outils à afficher:", "Herramientas a mostrar:", "ツールを選択:", "选择显示的工具:"}},
        {"selectAll",      {"Tümünü Seç", "Select All", "Alle auswählen", "Tout sélectionner", "Seleccionar todo", "すべて選択", "全选"}},
        {"deselectAll",    {"Tümünü Kaldır", "Deselect All", "Alle abwählen", "Tout désélectionner", "Deseleccionar", "すべて解除", "取消全选"}},

        // ─── Kısayol ───
        {"hotkeyTitle",    {"Yakalama Kısayolu", "Capture Hotkey", "Erfassungskürzel", "Raccourci de capture", "Atajo de captura", "キャプチャホットキー", "捕获快捷键"}},
        {"hotkeyDesc",     {"Kısayol tuşunu ayarlayın.\nVarsayılan: Print Screen", "Set the hotkey.\nDefault: Print Screen", "Tastenkürzel festlegen.\nStandard: Druck", "Définir le raccourci.\nDéfaut : Impr. écran", "Configurar atajo.\nPredeterminado: Imp Pant", "ホットキーを設定。\nデフォルト: Print Screen", "设置快捷键。\n默认: Print Screen"}},
        {"hotkeyValid",    {"✅ Geçerli", "✅ Valid", "✅ Gültig", "✅ Valide", "✅ Válido", "✅ 有効", "✅ 有效"}},
        {"hotkeyReset",    {"🔄 Varsayılan (Print Screen)", "🔄 Default (Print Screen)", "🔄 Standard (Druck)", "🔄 Défaut (Impr. écran)", "🔄 Predeterminado (Imp Pant)", "🔄 デフォルト (Print Screen)", "🔄 默认 (Print Screen)"}},
        {"hotkeyNote",     {"⚠️ Bazı sistem kısayolları engellenemez. Ctrl/Alt/Shift kombinasyonu önerilir.", "⚠️ Some system shortcuts cannot be overridden. Modifier combos recommended.", "⚠️ Einige Systemkürzel können nicht überschrieben werden.", "⚠️ Certains raccourcis système ne peuvent pas être remplacés.", "⚠️ Algunos atajos de sistema no se pueden sobrescribir.", "⚠️ 一部のシステムホットキーは上書きできません。", "⚠️ 某些系统快捷键无法覆盖。"}},
        {"hotkeyInvalid",  {"⚠️ Geçersiz kısayol", "⚠️ Invalid hotkey", "⚠️ Ungültiges Kürzel", "⚠️ Raccourci invalide", "⚠️ Atajo no válido", "⚠️ 無効なホットキー", "⚠️ 无效快捷键"}},

        // ─── Dil isimleri (her zaman kendi dilinde) ───
        {"langTurkish",    {"Türkçe", "Türkçe", "Türkçe", "Türkçe", "Türkçe", "トルコ語", "土耳其语"}},
        {"langEnglish",    {"English", "English", "English", "English", "English", "英語", "英语"}},
        {"langGerman",     {"Deutsch", "Deutsch", "Deutsch", "Deutsch", "Deutsch", "ドイツ語", "德语"}},
        {"langFrench",     {"Français", "Français", "Français", "Français", "Français", "フランス語", "法语"}},
        {"langSpanish",    {"Español", "Español", "Español", "Español", "Español", "スペイン語", "西班牙语"}},
        {"langJapanese",   {"日本語", "日本語", "日本語", "日本語", "日本語", "日本語", "日语"}},
        {"langChinese",    {"中文", "中文", "中文", "中文", "中文", "中文", "中文"}},

        // ─── Tray ───
        {"trayCapture",    {"Yakala", "Capture", "Erfassen", "Capture", "Capturar", "キャプチャ", "捕获"}},
        {"traySettings",   {"Ayarlar", "Settings", "Einstellungen", "Paramètres", "Configuración", "設定", "设置"}},
        {"trayAbout",      {"Hakkında", "About", "Über", "À propos", "Acerca de", "概要", "关于"}},
        {"trayQuit",       {"Çıkış", "Quit", "Beenden", "Quitter", "Salir", "終了", "退出"}},

        // ─── Erişilebilirlik ───
        {"accessibility",  {"Erişilebilirlik", "Accessibility", "Barrierefreiheit", "Accessibilité", "Accesibilidad", "アクセシビリティ", "无障碍"}},
        {"highContrast",   {"Yüksek kontrast", "High contrast", "Hoher Kontrast", "Contraste élevé", "Alto contraste", "ハイコントラスト", "高对比度"}},
        {"trayIcon",       {"Tepsi Simgesi", "Tray Icon", "Taskbar-Symbol", "Icône de zone", "Icono de bandeja", "トレイアイコン", "托盘图标"}},
        {"trayIconDark",   {"Koyu", "Dark", "Dunkel", "Sombre", "Oscuro", "ダーク", "深色"}},
        {"trayIconLight",  {"Açık", "Light", "Hell", "Clair", "Claro", "ライト", "浅色"}},

        // ─── Toolbar ───
        {"toolPen",        {"Kalem (P)", "Pen (P)", "Stift (P)", "Stylo (P)", "Lápiz (P)", "ペン (P)", "画笔 (P)"}},
        {"toolArrow",      {"Ok (A)", "Arrow (A)", "Pfeil (A)", "Flèche (A)", "Flecha (A)", "矢印 (A)", "箭头 (A)"}},
        {"toolRect",       {"Kare (R)", "Rectangle (R)", "Rechteck (R)", "Rectangle (R)", "Rectángulo (R)", "長方形 (R)", "矩形 (R)"}},
        {"toolCircle",     {"Çember (C)", "Circle (C)", "Kreis (C)", "Cercle (C)", "Círculo (C)", "円 (C)", "圆形 (C)"}},
        {"toolText",       {"Metin (T)", "Text (T)", "Text (T)", "Texte (T)", "Texto (T)", "テキスト (T)", "文本 (T)"}},
        {"toolHighlighter",{"Vurgulayıcı (H)", "Highlighter (H)", "Textmarker (H)", "Surligneur (H)", "Resaltador (H)", "マーカー (H)", "荧光笔 (H)"}},
        {"toolBlur",       {"Bulanıklaştır (B)", "Blur (B)", "Unschärfe (B)", "Flou (B)", "Desenfocar (B)", "ぼかし (B)", "模糊 (B)"}},
        {"toolCounter",    {"Numara (#)", "Counter (#)", "Zähler (#)", "Compteur (#)", "Contador (#)", "カウンター (#)", "计数器 (#)"}},
        {"toolEraser",     {"Silgi (X)", "Eraser (X)", "Radierer (X)", "Gomme (X)", "Borrador (X)", "消しゴム (X)", "橡皮 (X)"}},
        {"toolLine",       {"Çizgi (L)", "Line (L)", "Linie (L)", "Ligne (L)", "Línea (L)", "線 (L)", "线条 (L)"}},
        {"toolColor",      {"Renk Seç", "Pick Color", "Farbe wählen", "Couleur", "Elegir color", "色を選択", "选择颜色"}},
        {"toolWidth",      {"Kalınlık", "Width", "Breite", "Épaisseur", "Grosor", "太さ", "粗细"}},
        {"toolUndo",       {"Geri Al (Ctrl+Z)", "Undo (Ctrl+Z)", "Rückgängig (Strg+Z)", "Annuler (Ctrl+Z)", "Deshacer (Ctrl+Z)", "元に戻す (Ctrl+Z)", "撤销 (Ctrl+Z)"}},
        {"toolRedo",       {"İleri Al (Ctrl+Y)", "Redo (Ctrl+Y)", "Wiederholen (Strg+Y)", "Rétablir (Ctrl+Y)", "Rehacer (Ctrl+Y)", "やり直し (Ctrl+Y)", "重做 (Ctrl+Y)"}},
        {"actionPin",      {"Ekrana Sabitle 📌", "Pin to Screen 📌", "Heften 📌", "Épingler 📌", "Fijar 📌", "ピン留め 📌", "固定 📌"}},
        {"actionCopy",     {"Kopyala (Ctrl+C)", "Copy (Ctrl+C)", "Kopieren (Strg+C)", "Copier (Ctrl+C)", "Copiar (Ctrl+C)", "コピー (Ctrl+C)", "复制 (Ctrl+C)"}},
        {"actionSave",     {"Kaydet (Ctrl+S)", "Save (Ctrl+S)", "Speichern (Strg+S)", "Enregistrer (Ctrl+S)", "Guardar (Ctrl+S)", "保存 (Ctrl+S)", "保存 (Ctrl+S)"}},
        {"actionClose",    {"Kapat (Esc)", "Close (Esc)", "Schließen (Esc)", "Fermer (Esc)", "Cerrar (Esc)", "閉じる (Esc)", "关闭 (Esc)"}},

        // ─── Araç listesi ───
        {"toolListPen",    {"✏️ Kalem", "✏️ Pen", "✏️ Stift", "✏️ Stylo", "✏️ Lápiz", "✏️ ペン", "✏️ 画笔"}},
        {"toolListArrow",  {"➡️ Ok", "➡️ Arrow", "➡️ Pfeil", "➡️ Flèche", "➡️ Flecha", "➡️ 矢印", "➡️ 箭头"}},
        {"toolListRect",   {"⬜ Kare", "⬜ Rectangle", "⬜ Rechteck", "⬜ Rectangle", "⬜ Rectángulo", "⬜ 長方形", "⬜ 矩形"}},
        {"toolListCircle", {"⭕ Çember", "⭕ Circle", "⭕ Kreis", "⭕ Cercle", "⭕ Círculo", "⭕ 円", "⭕ 圆形"}},
        {"toolListText",   {"🔤 Metin", "🔤 Text", "🔤 Text", "🔤 Texte", "🔤 Texto", "🔤 テキスト", "🔤 文本"}},
        {"toolListHighlight",{"🖍️ Vurgulayıcı", "🖍️ Highlighter", "🖍️ Textmarker", "🖍️ Surligneur", "🖍️ Resaltador", "🖍️ マーカー", "🖍️ 荧光笔"}},
        {"toolListBlur",   {"🔲 Bulanıklaştır", "🔲 Blur", "🔲 Unschärfe", "🔲 Flou", "🔲 Desenfocar", "🔲 ぼかし", "🔲 模糊"}},
        {"toolListCounter",{"🔢 Numara", "🔢 Counter", "🔢 Zähler", "🔢 Compteur", "🔢 Contador", "🔢 カウンター", "🔢 计数器"}},
        {"toolListEraser", {"🧹 Silgi", "🧹 Eraser", "🧹 Radierer", "🧹 Gomme", "🧹 Borrador", "🧹 消しゴム", "🧹 橡皮"}},
        {"toolListLine",   {"📏 Çizgi", "📏 Line", "📏 Linie", "📏 Ligne", "📏 Línea", "📏 線", "📏 线条"}},

        // ─── Pinned ───
        {"pinnedLabel",    {"Sabitlendi", "Pinned", "Gepinnt", "Épinglé", "Fijado", "ピン留め", "已固定"}},
        {"pinnedCopy",     {"Kopyala", "Copy", "Kopieren", "Copier", "Copiar", "コピー", "复制"}},
        {"pinnedSave",     {"Farklı Kaydet...", "Save As...", "Speichern unter...", "Enregistrer sous...", "Guardar como...", "名前を付けて保存...", "另存为..."}},
        {"pinnedClose",    {"Kapat", "Close", "Schließen", "Fermer", "Cerrar", "閉じる", "关闭"}},
        {"pinnedCloseAll", {"Tümünü Kapat", "Close All", "Alle schließen", "Tout fermer", "Cerrar todo", "すべて閉じる", "全部关闭"}},

        // ─── Hatalar ───
        {"errSaveDir",     {"Kayıt dizini oluşturulamadı: ", "Failed to create directory: ", "Ordner erstellen fehlgeschlagen: ", "Échec création dossier: ", "Error al crear directorio: ", "ディレクトリ作成失敗: ", "创建目录失败: "}},
        {"errInvalidHotkey",{"Geçersiz kısayol. Varsayılanı kullanın.", "Invalid hotkey. Use default.", "Ungültiges Kürzel.", "Raccourci invalide.", "Atajo no válido.", "無効なホットキー。", "无效快捷键。"}},
        {"errTitle",       {"Hata", "Error", "Fehler", "Erreur", "Error", "エラー", "错误"}},
        {"errInvalidHotkeyTitle",{"Geçersiz Kısayol", "Invalid Hotkey", "Ungültiges Kürzel", "Raccourci invalide", "Atajo no válido", "無効なホットキー", "无效快捷键"}},

        // ─── Sıfırlama ───
        {"resetTitle",     {"Sıfırla", "Reset", "Zurücksetzen", "Réinitialiser", "Restablecer", "リセット", "重置"}},
        {"resetConfirm",   {"Tüm ayarlar sıfırlansın mı?", "Reset all settings?", "Alle Einstellungen zurücksetzen?", "Tout réinitialiser ?", "¿Restablecer todo?", "すべてリセット？", "恢复所有设置？"}},

        // ─── About ───
        {"aboutTitle",     {"EShot Hakkında", "About EShot", "Über EShot", "À propos", "Acerca de", "概要", "关于EShot"}},
        {"aboutDesc",      {"Gelişmiş Ekran Alıntısı Aracı", "Advanced Screenshot Tool", "Fortgeschrittenes Screenshot-Tool", "Outil de Capture Avancé", "Herramienta de Captura Avanzada", "高度なスクリーンショットツール", "高级截图工具"}},

        // ─── Bildirim ───
        {"notifCaptureTitle",{"EShot", "EShot", "EShot", "EShot", "EShot", "EShot", "EShot"}},
        {"notifCaptureMsg",{"Ekran görüntüsü alındı (%1x%2)", "Screenshot taken (%1x%2)", "Bildschirmfoto (%1x%2)", "Capture (%1x%2)", "Captura (%1x%2)", "スクリーンショット (%1x%2)", "截图 (%1x%2)"}},

        // ─── Güncelleme ───
        {"updateTitle",    {"Güncelleme Mevcut", "Update Available", "Update verfügbar", "Mise à jour", "Actualización", "アップデート可能", "有可用更新"}},
        {"updateMessage",  {"Yeni sürüm: %1\nGitHub'dan indirin.", "New version: %1\nDownload from GitHub.", "Neue Version: %1\nVon GitHub herunterladen.", "Nouvelle version : %1\nTélécharger.", "Nueva versión: %1\nDescargar.", "新しいバージョン: %1\nGitHubからダウンロード。", "新版本: %1\n从GitHub下载。"}},

        // ─── Sihirbaz ───
        {"wizardTitle",    {"EShot'a Hoş Geldiniz!", "Welcome to EShot!", "Willkommen bei EShot!", "Bienvenue !", "¡Bienvenido!", "ようこそ！", "欢迎使用EShot！"}},
        {"wizardDesc",     {"Ayarları yapılandırın.", "Configure your settings.", "Einstellungen konfigurieren.", "Configurez vos paramètres.", "Configure sus ajustes.", "設定を構成。", "配置设置。"}},
        {"wizardNext",     {"İleri", "Next", "Weiter", "Suivant", "Siguiente", "次へ", "下一步"}},
        {"wizardBack",     {"Geri", "Back", "Zurück", "Retour", "Atrás", "戻る", "返回"}},
        {"wizardFinish",   {"Tamamla", "Finish", "Fertig", "Terminer", "Finalizar", "完了", "完成"}},
        {"wizardHotkeyDesc",{"Kısayol tuşunu ayarlayın.\nVarsayılan: Print Screen", "Set the hotkey.\nDefault: Print Screen", "Tastenkürzel festlegen.\nStandard: Druck", "Définir le raccourci.\nDéfaut: Impr. écran", "Configure el atajo.\nPredeterminado: Imp Pant", "ホットキーを設定。\nデフォルト: Print Screen", "设置快捷键。\n默认: Print Screen"}},

        // ─── Dışa/İçe ───
        {"settingsExportImport",{"Ayarları Dışa / İçe Aktar", "Export / Import Settings", "Einstellungen exportieren/importieren", "Exporter / Importer", "Exportar / Importar", "設定の書き出し/取り込み", "导出/导入设置"}},
        {"exportSettings", {"Dışa Aktar", "Export", "Exportieren", "Exporter", "Exportar", "書き出し", "导出"}},
        {"importSettings", {"İçe Aktar", "Import", "Importieren", "Importer", "Importar", "取り込み", "导入"}},
        {"exportSuccess",  {"Dışa aktarıldı.", "Exported.", "Exportiert.", "Exporté.", "Exportado.", "書き出し完了。", "导出成功。"}},
        {"importSuccess",  {"İçe aktarıldı.", "Imported.", "Importiert.", "Importé.", "Importado.", "取り込み完了。", "导入成功。"}},
        {"importError",    {"Geçersiz dosya.", "Invalid file.", "Ungültige Datei.", "Fichier invalide.", "Archivo no válido.", "無効なファイル。", "无效文件。"}},

        // ─── Tooltip ───
        {"tipLanguage",    {"Dil. Kaydettikten sonra etkili olur.", "Language. Takes effect after save.", "Sprache. Wirkt nach Speichern.", "Langue. Prend effet après enregistrement.", "Idioma. Efecto al guardar.", "言語。保存後に有効。", "语言。保存后生效。"}},
        {"tipSaveDir",     {"Kayıt dizini.", "Save directory.", "Speicherordner.", "Dossier de sauvegarde.", "Directorio de guardado.", "保存先。", "保存目录。"}},
        {"tipAutoStart",   {"Windows ile başlat.", "Start with Windows.", "Mit Windows starten.", "Démarrer avec Windows.", "Iniciar con Windows.", "Windowsと同時に開始。", "随Windows启动。"}},
        {"tipNotifications",{"Bildirim göster.", "Show notifications.", "Benachrichtigungen.", "Notifications.", "Notificaciones.", "通知を表示。", "显示通知。"}},
        {"tipPlaySound",   {"Ses çal.", "Play sound.", "Ton abspielen.", "Jouer le son.", "Reproducir sonido.", "サウンド再生。", "播放声音。"}},
        {"tipCopyPath",    {"Yolu kopyala.", "Copy path.", "Pfad kopieren.", "Copier le chemin.", "Copiar ruta.", "パスをコピー。", "复制路径。"}},
        {"tipHighContrast",{"Yüksek kontrast.", "High contrast.", "Hoher Kontrast.", "Contraste élevé.", "Alto contraste.", "ハイコントラスト。", "高对比度。"}},
        {"tipTrayIcon",    {"Tepsi simgesi rengi.", "Tray icon color.", "Taskbar-Symbolfarbe.", "Couleur de l'icône.", "Color del icono.", "トレイアイコンの色。", "托盘图标颜色。"}},
    };

    static constexpr int s_transCount = sizeof(s_trans) / sizeof(s_trans[0]);
};

#endif
