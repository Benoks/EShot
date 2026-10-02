#ifndef TRANSLATIONMANAGER_H
#define TRANSLATIONMANAGER_H

#include <QString>
#include <QSettings>
#include <QLocale>
#include <QHash>
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

    static void setLanguage(Language lang, bool persist = true) {
        m_lang = lang;
        if (persist) {
            QSettings s("EShot", "EShot");
            s.setValue("language", static_cast<int>(lang));
        }
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
        // Lazy-built hash lookup: forward iteration + insert makes later
        // duplicate rows overwrite earlier ones, so the last duplicate wins —
        // the same precedence as the previous reverse linear scan.
        static const QHash<QByteArray, const Trans *> cache = [] {
            QHash<QByteArray, const Trans *> table;
            table.reserve(s_transCount);
            for (int i = 0; i < s_transCount; ++i)
                table.insert(s_trans[i].key, &s_trans[i]);
            return table;
        }();
        const auto it = cache.constFind(key);
        if (it != cache.constEnd())
            return QString::fromUtf8(it.value()->vals[static_cast<int>(m_lang)]);
        return QString::fromUtf8(key);
    }

    // ─── Genel ───
    static QString appTitle()         { return tr("appTitle"); }
    static QString settingsTitle()    { return tr("settingsTitle"); }
    static QString save()             { return tr("save"); }
    static QString cancel()           { return tr("cancel"); }
    static QString reset()            { return tr("reset"); }
    static QString browse()           { return tr("browse"); }
    static QString openFolder()       { return tr("openFolder"); }
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
    static QString notifyCopy()       { return tr("notifyCopy"); }
    static QString notifySave()       { return tr("notifySave"); }
    static QString notifyGif()        { return tr("notifyGif"); }
    static QString notifyVideo()      { return tr("notifyVideo"); }
    static QString notificationOpenFolder() { return tr("notificationOpenFolder"); }
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
    static QString captureHintDrag()  { return tr("captureHintDrag"); }
    static QString captureHintScreen(){ return tr("captureHintScreen"); }
    static QString captureHintRecording() { return tr("captureHintRecording"); }
    static QString captureHintCopy()  { return tr("captureHintCopy"); }
    static QString captureHintSave()  { return tr("captureHintSave"); }
    static QString captureHintCancel(){ return tr("captureHintCancel"); }
    static QString captureHintQuickSettings() { return tr("captureHintQuickSettings"); }
    static QString showCaptureHints() { return tr("showCaptureHints"); }
    static QString showCaptureHintsTip() { return tr("showCaptureHintsTip"); }
    static QString rememberLastAnnotationTool() { return tr("rememberLastAnnotationTool"); }
    static QString rememberLastAnnotationToolHint() { return tr("rememberLastAnnotationToolHint"); }
    static QString drawingTools() { return tr("drawingTools"); }
    static QString bottomToolbarControls() { return tr("bottomToolbarControls"); }

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
    static QString gifFpsLabel()     { return tr("gifFpsLabel"); }
    static QString videoFpsLabel()   { return tr("videoFpsLabel"); }
    static QString recordingUnlimited() { return tr("recordingUnlimited"); }
    static QString recordingLoop()    { return tr("recordingLoop"); }
    static QString recordingLoopInfinite() { return tr("recordingLoopInfinite"); }

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
    static QString trayWindowCapture() { return tr("trayWindowCapture"); }
    static QString trayCancelRecording() { return tr("trayCancelRecording"); }
    static QString traySettings()     { return tr("traySettings"); }
    static QString trayAbout()        { return tr("trayAbout"); }
    static QString trayQuit()         { return tr("trayQuit"); }

    // ─── Erişilebilirlik ───
    static QString accessibility()    { return tr("accessibility"); }
    static QString highContrast()     { return tr("highContrast"); }
    static QString trayIcon()         { return tr("trayIcon"); }
    static QString trayIconDark()     { return tr("trayIconDark"); }

    // ─── Toolbar ───
    static QString toolPen()          { return tr("toolPen"); }
    static QString toolArrow()        { return tr("toolArrow"); }
    static QString toolRect()         { return tr("toolRect"); }
    static QString toolCircle()       { return tr("toolCircle"); }
    static QString toolText()         { return tr("toolText"); }
    static QString toolHighlighter()  { return tr("toolHighlighter"); }
    static QString toolBlur()         { return tr("toolBlur"); }
    static QString toolPixelate()     { return tr("toolPixelate"); }
    static QString toolCounter()      { return tr("toolCounter"); }
    static QString toolEraser()       { return tr("toolEraser"); }
    static QString toolLine()         { return tr("toolLine"); }
    static QString toolColor()        { return tr("toolColor"); }
    static QString toolFont()         { return tr("toolFont"); }
    static QString toolFontSize()     { return tr("toolFontSize"); }
    static QString toolMove()         { return tr("toolMove"); }
    static QString toolUndo()         { return tr("toolUndo"); }
    static QString toolRedo()         { return tr("toolRedo"); }
    static QString toolEyedropper()   { return tr("toolEyedropper"); }
    static QString toolSemiRect()     { return tr("toolSemiRect"); }
    static QString actionPin()        { return tr("actionPin"); }
    static QString actionCopy()       { return tr("actionCopy"); }
    static QString actionSave()       { return tr("actionSave"); }
    static QString actionClose()      { return tr("actionClose"); }
    static QString actionLock()       { return tr("actionLock"); }
    static QString actionOcr()        { return tr("actionOcr"); }
    static QString visualSearchTitle() { return tr("visualSearchTitle"); }
    static QString visualSearchProvider() { return tr("visualSearchProvider"); }
    static QString visualSearchGoogleLens() { return tr("visualSearchGoogleLens"); }
    static QString visualSearchYandexImages() { return tr("visualSearchYandexImages"); }
    static QString visualSearchAction() { return tr("visualSearchAction"); }
    static QString visualSearchGoogleTooltip() { return tr("visualSearchGoogleTooltip"); }
    static QString visualSearchYandexTooltip() { return tr("visualSearchYandexTooltip"); }
    static QString visualSearchFailed() { return tr("visualSearchFailed"); }
    static QString visualSearchTempCreateError() { return tr("visualSearchTempCreateError"); }
    static QString visualSearchTempPrepareError() { return tr("visualSearchTempPrepareError"); }
    static QString visualSearchUploaderCreateError() { return tr("visualSearchUploaderCreateError"); }
    static QString visualSearchUploadUnavailable(const QString &details) {
        return tr("visualSearchUploadUnavailable").arg(details);
    }

    // ─── Araç listesi ───
    static QString toolListPen()      { return tr("toolListPen"); }
    static QString toolListArrow()    { return tr("toolListArrow"); }
    static QString toolListRect()     { return tr("toolListRect"); }
    static QString toolListCircle()   { return tr("toolListCircle"); }
    static QString toolListText()     { return tr("toolListText"); }
    static QString toolListHighlight(){ return tr("toolListHighlight"); }
    static QString toolListBlur()     { return tr("toolListBlur"); }
    static QString toolListPixelate() { return tr("toolListPixelate"); }
    static QString toolListCounter()  { return tr("toolListCounter"); }
    static QString toolListEraser()   { return tr("toolListEraser"); }
    static QString toolListLine()     { return tr("toolListLine"); }
    static QString toolListSemiRect() { return tr("toolListSemiRect"); }

    // ─── Pinned ───
    static QString pinnedCopy()       { return tr("pinnedCopy"); }
    static QString pinnedSave()       { return tr("pinnedSave"); }
    static QString pinnedClose()      { return tr("pinnedClose"); }

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
    static QString version()          { return tr("version"); }

    // ─── Bildirim ───
    static QString notifCaptureTitle(){ return tr("notifCaptureTitle"); }
    static QString notifCaptureMsg(int w, int h) { return tr("notifCaptureMsg").arg(w).arg(h); }
    static QString captureSaved()     { return tr("captureSaved"); }

    // ─── Güncelleme ───
    static QString updateTitle()      { return tr("updateTitle"); }
    static QString updateNow()        { return tr("updateNow"); }
    static QString updateStatusIdle() { return tr("updateStatusIdle"); }
    static QString updateStatusChecking() { return tr("updateStatusChecking"); }
    static QString updateStatusUpToDate() { return tr("updateStatusUpToDate"); }
    static QString updateStatusAvailable(const QString &v) { return tr("updateStatusAvailable").arg(v); }
    static QString updateStatusDownloading() { return tr("updateStatusDownloading"); }
    static QString updateStatusInstalling() { return tr("updateStatusInstalling"); }
    static QString updateStatusRestarting() { return tr("updateStatusRestarting"); }
    static QString updateStatusFailed(const QString &reason) { return tr("updateStatusFailed").arg(reason); }
    static QString updateNoInstaller() { return tr("updateNoInstaller"); }
    static QString updateInvalidResponse() { return tr("updateInvalidResponse"); }
    static QString updateInvalidDownload() { return tr("updateInvalidDownload"); }
    static QString updateCannotLaunchInstaller() { return tr("updateCannotLaunchInstaller"); }
    static QString updateStatusAur(const QString &v) { return tr("updateStatusAur").arg(v); }
    static QString updateStatusPackageManager(const QString &v) { return tr("updateStatusPackageManager").arg(v); }

    // ─── Sihirbaz ───
    static QString wizardTitle()      { return tr("wizardTitle"); }
    static QString wizardDesc()       { return tr("wizardDesc"); }
    static QString wizardFinish()     { return tr("wizardFinish"); }
    static QString wizardHotkeyDesc() { return tr("wizardHotkeyDesc"); }
    static QString printScreenConflictTitle() { return tr("printScreenConflictTitle"); }
    static QString printScreenConflictMessage() { return tr("printScreenConflictMessage"); }
    static QString printScreenConflictFix() { return tr("printScreenConflictFix"); }
    static QString printScreenConflictDisabled() { return tr("printScreenConflictDisabled"); }
    static QString hotkeyMayBeInUse() { return tr("hotkeyMayBeInUse"); }
    static QString recordingHotkeyMayBeInUse() { return tr("recordingHotkeyMayBeInUse"); }
    static QString directCaptureHotkeyMayBeInUse() { return tr("directCaptureHotkeyMayBeInUse"); }
    static QString hotkeyNotActiveTitle() { return tr("hotkeyNotActiveTitle"); }
    static QString hotkeyNotActiveBody() { return tr("hotkeyNotActiveBody"); }
    static QString hotkeyCaptureFallback() { return tr("hotkeyCaptureFallback"); }
    static QString hotkeyNoneActive() { return tr("hotkeyNoneActive"); }
    static QString hotkeyConflictWith() { return tr("hotkeyConflictWith"); }
    static QString hotkeyConflictSave() { return tr("hotkeyConflictSave"); }
    static QString autoStartSaveFailed() { return tr("autoStartSaveFailed"); }
    static QString removeFromSystemTitle() { return tr("removeFromSystemTitle"); }
    static QString removeFromSystem() { return tr("removeFromSystem"); }
    static QString removeFromSystemConfirm() { return tr("removeFromSystemConfirm"); }
    static QString removeFromSystemConfirmPackage() { return tr("removeFromSystemConfirmPackage"); }
    static QString removeFromSystemDone() { return tr("removeFromSystemDone"); }
    static QString removeFromSystemDonePackage() { return tr("removeFromSystemDonePackage"); }
    static QString removeFromSystemErrors() { return tr("removeFromSystemErrors"); }

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
    static QString ocrAutomatic()     { return tr("ocrAutomatic"); }
    static QString ocrLanguagePackMissing() { return tr("ocrLanguagePackMissing"); }

    // ─── Kayıt (Recording) ───
    static QString recordingStart()   { return tr("recordingStart"); }
    static QString recordingStop()    { return tr("recordingStop"); }
    static QString recordingStopShort() { return tr("recordingStopShort"); }
    static QString recordingPauseResume() { return tr("recordingPauseResume"); }
    static QString recordingDetails() { return tr("recordingDetails"); }
    static QString recordingDrag() { return tr("recordingDrag"); }
    static QString recordingStartTitle() { return tr("recordingStartTitle"); }
    static QString recordingSaved()   { return tr("recordingSaved"); }
    static QString recordingFailed()  { return tr("recordingFailed"); }
    static QString recordingMaxTime() { return tr("recordingMaxTime"); }
    static QString quickSettings() { return tr("quickSettings"); }
    static QString quickPenWidth() { return tr("quickPenWidth"); }
    static QString quickBlurIntensity() { return tr("quickBlurIntensity"); }
    static QString quickGifRecording() { return tr("quickGifRecording"); }
    static QString quickMaxSeconds() { return tr("quickMaxSeconds"); }
    static QString videoRecordingTitle() { return tr("videoRecordingTitle"); }
    static QString videoSaved() { return tr("videoSaved"); }
    static QString videoFailed() { return tr("videoFailed"); }
    static QString videoFfmpegMissing() { return tr("videoFfmpegMissing"); }
    static QString videoGstreamerMissing() { return tr("videoGstreamerMissing"); }
    static QString videoWaylandPortalMissing() { return tr("videoWaylandPortalMissing"); }
    static QString videoWaylandPermissionDenied() { return tr("videoWaylandPermissionDenied"); }
    static QString videoWaylandWrongSource() { return tr("videoWaylandWrongSource"); }
    static QString videoGstreamerStartFailed() { return tr("videoGstreamerStartFailed"); }
    static QString videoPipeWireRemoteFailed() { return tr("videoPipeWireRemoteFailed"); }
    static QString gifSettings() { return tr("gifSettings"); }
    static QString recordingCancel() { return tr("recordingCancel"); }
    static QString recordingStartsIn() { return tr("recordingStartsIn"); }
    static QString videoQualityCrf() { return tr("videoQualityCrf"); }
    static QString videoCrfHint() { return tr("videoCrfHint"); }
    static QString audioMode() { return tr("audioMode"); }
    static QString audioNone() { return tr("audioNone"); }
    static QString audioDesktop() { return tr("audioDesktop"); }
    static QString audioMicrophone() { return tr("audioMicrophone"); }
    static QString audioDesktopMic() { return tr("audioDesktopMic"); }
    static QString audioSource() { return tr("audioSource"); }
    static QString audioMicrophoneDevice() { return tr("audioMicrophoneDevice"); }
    static QString audioSystemLoopback() { return tr("audioSystemLoopback"); }

    // ─── Yükleme (Upload) ───
    static QString uploadToService()  { return tr("uploadToService"); }
    static QString uploadTitle()      { return tr("uploadTitle"); }
    static QString uploadProvider()   { return tr("uploadProvider"); }
    static QString upload()           { return tr("upload"); }
    static QString uploadUploading()  { return tr("uploadUploading"); }
    static QString uploadSuccess()    { return tr("uploadSuccess"); }
    static QString uploadFailed()     { return tr("uploadFailed"); }
    static QString visualSearchBrowserLaunchTitle() { return tr("visualSearchBrowserLaunchTitle"); }
    static QString visualSearchBrowserLaunchError() { return tr("visualSearchBrowserLaunchError"); }
    static QString uploadLinkCopied() { return tr("uploadLinkCopied"); }
    static QString uploadOpen()       { return tr("uploadOpen"); }
    static QString uploadLinkPlaceholder()   { return tr("uploadLinkPlaceholder"); }
    static QString uploadDeletePlaceholder() { return tr("uploadDeletePlaceholder"); }
    static QString yandexAuthPlaceholder() { return tr("yandexAuthPlaceholder"); }
    static QString googleDriveAuthPlaceholder() { return tr("googleDriveAuthPlaceholder"); }
    static QString catboxUserHashPlaceholder() { return tr("catboxUserHashPlaceholder"); }
    static QString uploadAuthHelpYandex() { return tr("uploadAuthHelpYandex"); }
    static QString uploadAuthHelpGoogleDrive() { return tr("uploadAuthHelpGoogleDrive"); }
    static QString uploadAuthHelpCatbox() { return tr("uploadAuthHelpCatbox"); }
    static QString uploadAuthSaved()  { return tr("uploadAuthSaved"); }
    static QString uploadAuthSaveFailed() { return tr("uploadAuthSaveFailed"); }
    static QString uploadAuthHelpApiKey(const QString &service) { return tr("uploadAuthHelpApiKey").arg(service); }
    static QString uploadErrorInProgress() { return tr("uploadErrorInProgress"); }
    static QString uploadErrorImageMissing() { return tr("uploadErrorImageMissing"); }
    static QString uploadErrorCannotReadImage() { return tr("uploadErrorCannotReadImage"); }
    static QString uploadErrorServerRejected() { return tr("uploadErrorServerRejected"); }
    static QString uploadErrorNetwork(const QString &detail) { return tr("uploadErrorNetwork").arg(detail); }
    static QString uploadErrorHttp(int code) { return tr("uploadErrorHttp").arg(code); }
    static QString uploadErrorUnexpectedResponse(const QString &detail) { return tr("uploadErrorUnexpectedResponse").arg(detail); }
    static QString uploadErrorYandexTokenMissing() { return tr("uploadErrorYandexTokenMissing"); }
    static QString uploadErrorYandexUnsupportedToken() { return tr("uploadErrorYandexUnsupportedToken"); }
    static QString uploadErrorYandexAuthFailed() { return tr("uploadErrorYandexAuthFailed"); }
    static QString uploadErrorYandexScopeMissing() { return tr("uploadErrorYandexScopeMissing"); }
    static QString uploadErrorYandexStep(const QString &step, int code) { return tr("uploadErrorYandexStep").arg(step).arg(code); }
    static QString uploadErrorYandexUploadUrlMissing() { return tr("uploadErrorYandexUploadUrlMissing"); }
    static QString uploadErrorYandexPublicLinkMissing() { return tr("uploadErrorYandexPublicLinkMissing"); }
    static QString uploadErrorGoogleTokenMissing() { return tr("uploadErrorGoogleTokenMissing"); }
    static QString uploadErrorGoogleAuthFailed() { return tr("uploadErrorGoogleAuthFailed"); }
    static QString uploadErrorGoogleFileIdMissing() { return tr("uploadErrorGoogleFileIdMissing"); }
    static QString uploadApiKeyPlaceholder(const QString &service) { return tr("uploadApiKeyPlaceholder").arg(service); }
    static QString uploadErrorApiKeyMissing(const QString &service) { return tr("uploadErrorApiKeyMissing").arg(service); }
    static QString copy()             { return tr("copy"); }

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
        {"openFolder",     {"Klasörü Aç", "Open Folder", "Ordner öffnen", "Ouvrir le dossier", "Abrir carpeta", "フォルダーを開く", "打开文件夹", "Открыть папку"}},
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
        {"autoStart",      {"Sistemle başlat", "Start with system", "Mit dem System starten", "Démarrer avec le système", "Iniciar con el sistema", "システムと同時に開始", "随系统启动", "Запускать вместе с системой"}},
        {"showNotifications",{"Bildirim göster", "Show notifications", "Benachrichtigungen", "Notifications", "Notificaciones", "通知を表示", "显示通知", "Показывать уведомления"}},
        {"notifyCopy",{"Görsel kopyalama", "Image copy", "Bild kopieren", "Copie d'image", "Copiar imagen", "画像コピー", "图片复制", "Копирование изображения"}},
        {"notifySave",{"Görsel kayıt", "Image save", "Bild speichern", "Enregistrement d'image", "Guardar imagen", "画像保存", "图片保存", "Сохранение изображения"}},
        {"notifyGif",{"GIF kaydı", "GIF recording", "GIF-Aufnahme", "Enregistrement GIF", "Grabación GIF", "GIF録画", "GIF 录制", "Запись GIF"}},
        {"notifyVideo",{"Video kaydı", "Video recording", "Videoaufnahme", "Enregistrement vidéo", "Grabación de video", "動画録画", "视频录制", "Запись видео"}},
        {"notificationOpenFolder",{"Bildirimden klasör açmaya izin ver", "Allow opening the folder from notifications", "Öffnen des Ordners über Benachrichtigungen erlauben", "Autoriser l’ouverture du dossier depuis les notifications", "Permitir abrir la carpeta desde las notificaciones", "通知からフォルダーを開くことを許可", "允许从通知中打开文件夹", "Разрешить открывать папку из уведомлений"}},
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
        {"captureHintDrag", {"Alan seçmek için sürükleyin", "Drag to select an area", "Ziehen, um einen Bereich auszuwählen", "Faites glisser pour sélectionner une zone", "Arrastra para seleccionar un área", "ドラッグして範囲を選択", "拖动以选择区域", "Перетащите, чтобы выбрать область"}},
        {"captureHintScreen", {"Ekranı seçmek için çift tıklayın", "Double-click to select a screen", "Doppelklicken, um einen Bildschirm auszuwählen", "Double-cliquez pour sélectionner un écran", "Haz doble clic para seleccionar una pantalla", "ダブルクリックで画面を選択", "双击以选择屏幕", "Дважды щёлкните, чтобы выбрать экран"}},
        {"captureHintRecording", {"Kaydedilecek alanı seçmek için sürükleyin", "Drag to select the recording area", "Ziehen, um den Aufnahmebereich auszuwählen", "Faites glisser pour sélectionner la zone d'enregistrement", "Arrastra para seleccionar el área de grabación", "ドラッグして録画範囲を選択", "拖动以选择录制区域", "Перетащите, чтобы выбрать область записи"}},
        {"captureHintCopy", {"Kopyala", "Copy", "Kopieren", "Copier", "Copiar", "コピー", "复制", "Копировать"}},
        {"captureHintSave", {"Kaydet", "Save", "Speichern", "Enregistrer", "Guardar", "保存", "保存", "Сохранить"}},
        {"captureHintCancel", {"İptal", "Cancel", "Abbrechen", "Annuler", "Cancelar", "キャンセル", "取消", "Отмена"}},
        {"captureHintQuickSettings", {"Alanı seçtikten sonra çizim ve kayıt ayarları için soldaki Hızlı Ayarlar sekmesini kullanın.", "After selecting, use the Quick Settings tab on the left for drawing and recording options.", "Nutzen Sie nach der Auswahl die Registerkarte Schnelleinstellungen links für Zeichen- und Aufnahmeoptionen.", "Après la sélection, utilisez l'onglet Réglages rapides à gauche pour les options de dessin et d'enregistrement.", "Después de seleccionar, usa la pestaña Ajustes rápidos de la izquierda para las opciones de dibujo y grabación.", "選択後、左側のクイック設定タブで描画と録画のオプションを設定できます。", "选择后，使用左侧的快速设置标签调整绘图和录制选项。", "После выбора используйте вкладку быстрых настроек слева для параметров рисования и записи."}},
        {"showCaptureHints", {"Yakalama ipuçlarını göster", "Show capture hints", "Aufnahmehinweise anzeigen", "Afficher les conseils de capture", "Mostrar consejos de captura", "キャプチャのヒントを表示", "显示截图提示", "Показывать подсказки захвата"}},
        {"showCaptureHintsTip", {"Alan seçmeden önce temel hareketleri ve kısayolları gösterir.", "Shows basic gestures and shortcuts before you select an area.", "Zeigt grundlegende Gesten und Tastenkürzel vor der Bereichsauswahl.", "Affiche les gestes et raccourcis essentiels avant la sélection d'une zone.", "Muestra gestos y atajos básicos antes de seleccionar un área.", "範囲を選択する前に基本操作とショートカットを表示します。", "在选择区域前显示基本操作和快捷键。", "Показывает основные жесты и сочетания клавиш до выбора области."}},
        {"rememberLastAnnotationTool", {"Son kullanılan anotasyon aracını hatırla", "Remember the last annotation tool", "Letztes Anmerkungswerkzeug merken", "Mémoriser le dernier outil d’annotation", "Recordar la última herramienta de anotación", "最後に使った注釈ツールを記憶", "记住上次使用的标注工具", "Запоминать последний инструмент аннотации"}},
        {"rememberLastAnnotationToolHint", {"Varsayılan olarak kapalıdır. Açıldığında yeni yakalamalar son kullandığınız araçla başlar.", "Off by default. When enabled, new captures start with the last tool you used.", "Standardmäßig aus. Neue Aufnahmen starten dann mit dem zuletzt verwendeten Werkzeug.", "Désactivé par défaut. Les nouvelles captures démarrent avec le dernier outil utilisé.", "Desactivado de forma predeterminada. Las capturas nuevas comienzan con la última herramienta usada.", "既定ではオフです。有効にすると新しいキャプチャは最後に使ったツールで開始します。", "默认关闭。启用后，新截图会以上次使用的工具开始。", "По умолчанию выключено. Новые снимки будут открываться с последним использованным инструментом."}},
        {"rememberSettingsWindowSize", {"Ayarlar penceresi boyutunu hatırla", "Remember settings window size", "Größe des Einstellungsfensters merken", "Mémoriser la taille de la fenêtre des paramètres", "Recordar el tamaño de la ventana de configuración", "設定ウィンドウのサイズを記憶", "记住设置窗口大小", "Запоминать размер окна настроек"}},
        {"drawingTools", {"Çizim araçları", "Drawing tools", "Zeichenwerkzeuge", "Outils de dessin", "Herramientas de dibujo", "描画ツール", "绘图工具", "Инструменты рисования"}},
        {"bottomToolbarControls", {"Alt araç çubuğu kontrolleri", "Bottom toolbar controls", "Steuerelemente der unteren Werkzeugleiste", "Commandes de la barre d’outils inférieure", "Controles de la barra inferior", "下部ツールバーのコントロール", "底部工具栏控件", "Элементы нижней панели инструментов"}},

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
        {"gifFpsLabel",    {"GIF FPS:", "GIF FPS:", "GIF-FPS:", "FPS GIF :", "FPS de GIF:", "GIF フレームレート:", "GIF 帧率:", "FPS GIF:"}},
        {"videoFpsLabel",  {"Video FPS:", "Video FPS:", "Video-FPS:", "FPS vidéo :", "FPS de video:", "動画フレームレート:", "视频帧率:", "FPS видео:"}},
        {"recordingUnlimited",{"Sınırsız", "Unlimited", "Unbegrenzt", "Illimité", "Ilimitado", "無制限", "无限制", "Без лимита"}},
        {"recordingLoop",  {"Döngü:", "Loop:", "Schleife:", "Boucle :", "Bucle:", "ループ:", "循环:", "Повтор:"}},
        {"recordingLoopInfinite",{"Sonsuz", "Infinite", "Endlos", "Infini", "Infinito", "無限", "无限", "Бесконечно"}},

        // ─── Tray ───
        {"trayCapture",    {"Yakala", "Capture", "Erfassen", "Capture", "Capturar", "キャプチャ", "捕获", "Захват"}},
        {"trayWindowCapture",{"Pencere Yakala", "Capture Window", "Fenster erfassen", "Capturer une fenêtre", "Capturar ventana", "ウィンドウをキャプチャ", "捕获窗口", "Захват окна"}},
        {"trayCancelRecording",{"Kaydı İptal Et", "Cancel Recording", "Aufnahme abbrechen", "Annuler l'enregistrement", "Cancelar grabación", "録画をキャンセル", "取消录制", "Отменить запись"}},
        {"traySettings",   {"Ayarlar", "Settings", "Einstellungen", "Paramètres", "Configuración", "設定", "设置", "Настройки"}},
        {"trayAbout",      {"Hakkında", "About", "Über", "À propos", "Acerca de", "概要", "关于", "О программе"}},
        {"trayQuit",       {"Çıkış", "Quit", "Beenden", "Quitter", "Salir", "終了", "退出", "Выход"}},

        // ─── Erişilebilirlik ───
        {"accessibility",  {"Erişilebilirlik", "Accessibility", "Barrierefreiheit", "Accessibilité", "Accesibilidad", "アクセシビリティ", "无障碍", "Доступность"}},
        {"highContrast",   {"Yüksek kontrast", "High contrast", "Hoher Kontrast", "Contraste élevé", "Alto contraste", "ハイコントラスト", "高对比度", "Высокий контраст"}},
        {"trayIcon",       {"Tepsi Simgesi", "Tray Icon", "Taskbar-Symbol", "Icône de zone", "Icono de bandeja", "トレイアイコン", "托盘图标", "Значок в трее"}},
        {"trayIconDark",   {"Siyah tepsi simgesi", "Black tray icon", "Schwarzes Taskbar-Symbol", "Icône de zone noire", "Icono de bandeja negro", "黒いトレイアイコン", "黑色托盘图标", "Чёрный значок в трее"}},

        // ─── Toolbar ───
        {"toolPen",        {"Kalem (P)", "Pen (P)", "Stift (P)", "Stylo (P)", "Lápiz (P)", "ペン (P)", "画笔 (P)", "Карандаш (P)"}},
        {"toolArrow",      {"Ok (A)", "Arrow (A)", "Pfeil (A)", "Flèche (A)", "Flecha (A)", "矢印 (A)", "箭头 (A)", "Стрелка (A)"}},
        {"toolRect",       {"Kare (R)", "Rectangle (R)", "Rechteck (R)", "Rectangle (R)", "Rectángulo (R)", "長方形 (R)", "矩形 (R)", "Прямоугольник (R)"}},
        {"toolCircle",     {"Çember (C)", "Circle (C)", "Kreis (C)", "Cercle (C)", "Círculo (C)", "円 (C)", "圆形 (C)", "Круг (C)"}},
        {"toolText",       {"Metin (T)", "Text (T)", "Text (T)", "Texte (T)", "Texto (T)", "テキスト (T)", "文本 (T)", "Текст (T)"}},
        {"toolHighlighter",{"Vurgulayıcı (H)", "Highlighter (H)", "Textmarker (H)", "Surligneur (H)", "Resaltador (H)", "マーカー (H)", "荧光笔 (H)", "Маркер (H)"}},
        {"highlighterStraightTooltip", {"Shift: düz çizgi", "Shift: straight line", "Umschalt: gerade Linie", "Maj : ligne droite", "Mayús: línea recta", "Shift: 直線", "Shift：直线", "Shift: прямая линия"}},
        {"highlighterStraightHint", {"Shift basılıyken yatay veya dikey vurgula", "Hold Shift to highlight horizontally or vertically", "Mit Umschalt horizontal oder vertikal markieren", "Maintenez Maj pour surligner horizontalement ou verticalement", "Mantén Mayús para resaltar horizontal o verticalmente", "Shift を押しながら水平または垂直にマーク", "按住 Shift 可水平或垂直高亮", "Удерживайте Shift для горизонтального или вертикального выделения"}},
        {"toolBlur",       {"Bulanıklaştır (B)", "Blur (B)", "Unschärfe (B)", "Flou (B)", "Desenfocar (B)", "ぼかし (B)", "模糊 (B)", "Размытие (B)"}},
        {"textBold",       {"Kalın", "Bold", "Fett", "Gras", "Negrita", "太字", "粗体", "Жирный"}},
        {"textBackground", {"Metin arka planı", "Text background", "Texthintergrund", "Fond du texte", "Fondo del texto", "テキストの背景", "文字背景", "Фон текста"}},
        {"textBackgroundBox", {"Kutu", "Box", "Kasten", "Encadré", "Cuadro", "ボックス", "底框", "Плашка"}},
        {"textBackgroundPlain", {"Sade", "Plain", "Ohne", "Aucun", "Sin fondo", "なし", "无", "Без фона"}},
        {"textBackgroundOutline", {"Kontur", "Outline", "Kontur", "Contour", "Contorno", "縁取り", "描边", "Обводка"}},
        {"captureHintShortcuts", {"Kısayollar", "Shortcuts", "Tastenkürzel", "Raccourcis", "Atajos", "ショートカット", "快捷键", "Горячие клавиши"}},
        {"sheetTitle", {"Kısayollar ve ipuçları", "Shortcuts and tips", "Tastenkürzel und Tipps", "Raccourcis et astuces", "Atajos y consejos", "ショートカットとヒント", "快捷键与提示", "Горячие клавиши и советы"}},
        {"sheetClose", {"Kapatmak için ? veya Esc", "Press ? or Esc to close", "? oder Esc zum Schließen", "? ou Échap pour fermer", "Pulsa ? o Esc para cerrar", "? または Esc で閉じる", "按 ? 或 Esc 关闭", "? или Esc — закрыть"}},
        {"sheetSectionSelection", {"Seçim", "Selection", "Auswahl", "Sélection", "Selección", "選択", "选区", "Выделение"}},
        {"sheetSectionTools", {"Araçlar", "Tools", "Werkzeuge", "Outils", "Herramientas", "ツール", "工具", "Инструменты"}},
        {"sheetSectionEditing", {"Düzenleme", "Editing", "Bearbeiten", "Édition", "Edición", "編集", "编辑", "Редактирование"}},
        {"sheetSectionText", {"Metin yazarken", "While typing", "Beim Tippen", "Pendant la saisie", "Al escribir", "入力中", "输入文本时", "При вводе текста"}},
        {"sheetSectionMore", {"Diğer", "More", "Weitere", "Plus", "Más", "その他", "更多", "Ещё"}},
        {"keyDrag", {"Sürükle", "Drag", "Ziehen", "Glisser", "Arrastrar", "ドラッグ", "拖动", "Перетащить"}},
        {"keyDoubleClick", {"Çift tık", "Double-click", "Doppelklick", "Double-clic", "Doble clic", "ダブルクリック", "双击", "Двойной щелчок"}},
        {"keyCtrlClick", {"Ctrl+tık", "Ctrl+click", "Strg+Klick", "Ctrl+clic", "Ctrl+clic", "Ctrl+クリック", "Ctrl+单击", "Ctrl+щелчок"}},
        {"keyShiftDrag", {"Shift+sürükle", "Shift+drag", "Umschalt+Ziehen", "Maj+glisser", "Mayús+arrastrar", "Shift+ドラッグ", "Shift+拖动", "Shift+перетащить"}},
        {"sheetSelectArea", {"Alan seç", "Select an area", "Bereich auswählen", "Sélectionner une zone", "Seleccionar un área", "範囲を選択", "选择区域", "Выделить область"}},
        {"sheetSelectScreen", {"Tüm ekranı seç", "Select a whole screen", "Ganzen Bildschirm wählen", "Sélectionner tout l'écran", "Seleccionar toda la pantalla", "画面全体を選択", "选择整个屏幕", "Выделить весь экран"}},
        {"sheetCancel", {"Seçimi temizle / kapat", "Clear selection / close", "Auswahl löschen / schließen", "Effacer la sélection / fermer", "Borrar selección / cerrar", "選択を解除 / 閉じる", "清除选区 / 关闭", "Сбросить выделение / закрыть"}},
        {"sheetMoveObject", {"Nesneyi seç ve taşı", "Select and move an object", "Objekt wählen und verschieben", "Sélectionner et déplacer un objet", "Seleccionar y mover un objeto", "オブジェクトを選択して移動", "选择并移动对象", "Выбрать и переместить объект"}},
        {"sheetDeleteSelected", {"Seçili nesneyi sil", "Delete the selected object", "Ausgewähltes Objekt löschen", "Supprimer l'objet sélectionné", "Eliminar el objeto seleccionado", "選択したオブジェクトを削除", "删除所选对象", "Удалить выбранный объект"}},
        {"sheetStraight", {"Düz çizgi, kare veya tam daire", "Straight line, square or circle", "Gerade, Quadrat oder Kreis", "Ligne droite, carré ou cercle", "Línea recta, cuadrado o círculo", "直線・正方形・正円", "直线、正方形或正圆", "Прямая, квадрат или круг"}},
        {"sheetEditText", {"Metni düzenle", "Edit text", "Text bearbeiten", "Modifier le texte", "Editar texto", "テキストを編集", "编辑文本", "Изменить текст"}},
        {"sheetTextSize", {"Yazı boyutu", "Text size", "Textgröße", "Taille du texte", "Tamaño del texto", "文字サイズ", "文字大小", "Размер текста"}},
        {"sheetNewLine", {"Yeni satır", "New line", "Neue Zeile", "Nouvelle ligne", "Nueva línea", "改行", "换行", "Новая строка"}},
        {"sheetConfirm", {"Onayla", "Confirm", "Bestätigen", "Valider", "Confirmar", "確定", "确认", "Подтвердить"}},
        {"sheetQuickSettings", {"Kalınlık, blur gücü ve yazı tipi soldaki Hızlı Ayarlar sekmesinde.", "Width, blur strength and fonts are in the Quick Settings tab on the left.", "Linienstärke, Unschärfe und Schrift findest du links im Tab „Schnelleinstellungen“.", "Épaisseur, intensité du flou et police : onglet Réglages rapides à gauche.", "Grosor, intensidad del desenfoque y fuente: pestaña Ajustes rápidos a la izquierda.", "線の太さ・ぼかし強度・フォントは左のクイック設定タブにあります。", "线宽、模糊强度和字体在左侧的“快速设置”标签中。", "Толщина, сила размытия и шрифт — во вкладке «Быстрые настройки» слева."}},
        {"tipQuickSettings", {"Kalem kalınlığı, blur gücü ve yazı tipi ayarları burada.", "Pen width, blur strength and font settings are here.", "Hier findest du Linienstärke, Unschärfe und Schrift.", "Épaisseur du trait, intensité du flou et police se règlent ici.", "Aquí están el grosor, la intensidad del desenfoque y la fuente.", "線の太さ、ぼかし強度、フォントはここで設定できます。", "画笔粗细、模糊强度和字体都在这里设置。", "Здесь настраиваются толщина, сила размытия и шрифт."}},
        {"tipTextTool", {"Mevcut bir metni düzenlemek için üstüne tıklayın ya da F2'ye basın. Ctrl+B kalın, Ctrl+ +/− boyut.", "Click an existing label or press F2 to edit it. Ctrl+B bold, Ctrl+ +/− size.", "Klicke auf einen vorhandenen Text oder drücke F2 zum Bearbeiten. Strg+B fett, Strg+ +/− Größe.", "Cliquez sur un texte existant ou appuyez sur F2 pour le modifier. Ctrl+B gras, Ctrl+ +/− taille.", "Haz clic en un texto existente o pulsa F2 para editarlo. Ctrl+B negrita, Ctrl+ +/− tamaño.", "既存のテキストをクリックするか F2 で編集できます。Ctrl+B で太字、Ctrl+ +/− でサイズ。", "单击现有文本或按 F2 进行编辑。Ctrl+B 加粗，Ctrl+ +/− 调整大小。", "Щёлкните по тексту или нажмите F2, чтобы изменить его. Ctrl+B — жирный, Ctrl+ +/− — размер."}},
        {"tipCtrlMove", {"Çizim araçları diğer nesnelerin üstüne de çizer. Bir nesneyi taşımak için Ctrl+tık.", "Drawing tools draw over other objects too. Ctrl+click an object to move it.", "Zeichenwerkzeuge zeichnen auch über andere Objekte. Strg+Klick verschiebt ein Objekt.", "Les outils de dessin dessinent aussi par-dessus les objets. Ctrl+clic pour en déplacer un.", "Las herramientas dibujan también sobre otros objetos. Ctrl+clic para mover uno.", "描画ツールは他のオブジェクトの上にも描けます。移動するには Ctrl+クリック。", "绘图工具也会在其他对象上绘制。按住 Ctrl 单击可移动对象。", "Инструменты рисуют и поверх объектов. Ctrl+щелчок — переместить объект."}},
        {"tipGotIt", {"Anladım", "Got it", "Verstanden", "Compris", "Entendido", "了解", "知道了", "Понятно"}},
        {"tipsReset", {"İpuçlarını tekrar göster", "Show tips again", "Tipps erneut anzeigen", "Réafficher les astuces", "Volver a mostrar consejos", "ヒントを再表示", "重新显示提示", "Снова показывать советы"}},
        {"tipsResetDone", {"İpuçları bir sonraki yakalamada tekrar gösterilecek.", "Tips will show again on the next capture.", "Tipps werden bei der nächsten Aufnahme wieder angezeigt.", "Les astuces réapparaîtront à la prochaine capture.", "Los consejos se mostrarán en la próxima captura.", "次のキャプチャでヒントが再表示されます。", "下次截图时将重新显示提示。", "Советы снова появятся при следующем снимке."}},
        {"trayWelcomeTitle", {"EShot çalışıyor", "EShot is running", "EShot läuft", "EShot est lancé", "EShot está en ejecución", "EShot は実行中です", "EShot 正在运行", "EShot запущен"}},
        {"trayWelcomeBody", {"EShot sistem tepsisinde bekliyor. Ekran görüntüsü almak için %1 tuşuna basın.", "EShot is waiting in the system tray. Press %1 to take a screenshot.", "EShot wartet im Infobereich. Drücke %1 für einen Screenshot.", "EShot attend dans la zone de notification. Appuyez sur %1 pour faire une capture.", "EShot espera en la bandeja del sistema. Pulsa %1 para hacer una captura.", "EShot はシステムトレイで待機中です。%1 でスクリーンショットを撮れます。", "EShot 已在系统托盘中运行。按 %1 截图。", "EShot работает в системном трее. Нажмите %1, чтобы сделать снимок."}},
        {"trayWelcomeBodyNoHotkey", {"EShot sistem tepsisinde bekliyor. Etkin bir yakalama kısayolu yok; tepsi menüsünü kullanın veya Ayarlar'dan bir kısayol belirleyin.", "EShot is waiting in the system tray. No capture hotkey is active; use the tray menu or set a hotkey in Settings.", "EShot wartet im Infobereich. Kein Aufnahmekürzel ist aktiv; nutze das Tray-Menü oder lege in den Einstellungen ein Kürzel fest.", "EShot attend dans la zone de notification. Aucun raccourci de capture n'est actif ; utilisez le menu de l'icône ou définissez un raccourci dans les Paramètres.", "EShot espera en la bandeja del sistema. No hay ningún atajo de captura activo; usa el menú de la bandeja o define un atajo en Ajustes.", "EShot はシステムトレイで待機中です。有効なキャプチャホットキーがありません。トレイメニューを使うか、設定でホットキーを指定してください。", "EShot 已在系统托盘中运行。当前没有可用的截图快捷键；请使用托盘菜单，或在设置中指定快捷键。", "EShot работает в системном трее. Нет активной горячей клавиши захвата; используйте меню трея или задайте клавишу в настройках."}},
        {"descPen", {"Serbest çizim", "Draw freehand", "Freihand zeichnen", "Dessin à main levée", "Dibujo a mano alzada", "フリーハンドで描く", "自由绘制", "Рисование от руки"}},
        {"descArrow", {"Bir yeri işaret et", "Point at something", "Auf etwas zeigen", "Pointer un élément", "Señalar algo", "何かを指し示す", "指向某处", "Указать на что-то"}},
        {"descLine", {"Düz çizgi çiz", "Draw a straight line", "Gerade Linie zeichnen", "Tracer une ligne droite", "Dibujar una línea recta", "直線を描く", "绘制直线", "Нарисовать прямую"}},
        {"descRect", {"Bir alanı çerçevele (Shift: kare)", "Frame an area (Shift: square)", "Bereich umrahmen (Umschalt: Quadrat)", "Encadrer une zone (Maj : carré)", "Enmarcar un área (Mayús: cuadrado)", "範囲を囲む（Shift: 正方形）", "框选区域（Shift：正方形）", "Обвести область (Shift — квадрат)"}},
        {"descCircle", {"Bir alanı daire içine al (Shift: tam daire)", "Circle an area (Shift: perfect circle)", "Bereich einkreisen (Umschalt: Kreis)", "Entourer une zone (Maj : cercle)", "Rodear un área (Mayús: círculo)", "範囲を丸で囲む（Shift: 正円）", "圈出区域（Shift：正圆）", "Обвести кругом (Shift — ровный круг)"}},
        {"descText", {"Etiket ekle: tıkla ve yaz", "Add a label: click and type", "Beschriftung: klicken und tippen", "Ajouter un texte : cliquer puis taper", "Añadir texto: haz clic y escribe", "ラベルを追加：クリックして入力", "添加标签：单击后输入", "Добавить подпись: щёлкните и печатайте"}},
        {"descHighlighter", {"Metni vurgula (Shift: düz)", "Highlight text (Shift: straight)", "Text markieren (Umschalt: gerade)", "Surligner du texte (Maj : droit)", "Resaltar texto (Mayús: recto)", "テキストを強調（Shift: 直線）", "高亮文本（Shift：直线）", "Выделить текст (Shift — прямо)"}},
        {"descSemiRect", {"Bir alanı yarı saydam renkle kapla", "Cover an area with a see-through colour", "Bereich halbtransparent abdecken", "Couvrir une zone d'une couleur translucide", "Cubrir un área con color translúcido", "範囲を半透明の色で覆う", "用半透明颜色覆盖区域", "Закрыть область полупрозрачным цветом"}},
        {"descBlur", {"Yumuşak bulanıklaştırma", "Soft blur", "Weiches Weichzeichnen", "Flou doux", "Desenfoque suave", "柔らかいぼかし", "柔和模糊", "Мягкое размытие"}},
        {"descPixelate", {"Hassas bilgiyi gizle", "Hide sensitive information", "Vertrauliches verbergen", "Masquer des informations sensibles", "Ocultar información sensible", "機密情報を隠す", "隐藏敏感信息", "Скрыть конфиденциальные данные"}},
        {"descCounter", {"Adımları numarala", "Number the steps", "Schritte nummerieren", "Numéroter les étapes", "Numerar los pasos", "手順に番号を付ける", "为步骤编号", "Пронумеровать шаги"}},
        {"descEraser", {"Tıklanan nesneyi sil", "Remove the clicked object", "Angeklicktes Objekt entfernen", "Supprimer l'objet cliqué", "Quitar el objeto pulsado", "クリックしたオブジェクトを削除", "删除单击的对象", "Удалить объект по щелчку"}},
        {"descUndo", {"Son değişikliği geri al", "Undo the last change", "Letzte Änderung rückgängig", "Annuler la dernière modification", "Deshacer el último cambio", "直前の変更を取り消す", "撤销上一次更改", "Отменить последнее изменение"}},
        {"descRedo", {"Geri alınanı yeniden uygula", "Redo the undone change", "Rückgängig gemachte Änderung wiederholen", "Rétablir la modification annulée", "Rehacer el cambio deshecho", "取り消した変更をやり直す", "重做已撤销的更改", "Повторить отменённое действие"}},
        {"descColor", {"Çizim rengi; seçili nesneyi de boyar", "Drawing colour; also recolours the selected object", "Zeichenfarbe; färbt auch das ausgewählte Objekt", "Couleur du dessin ; recolore aussi l'objet sélectionné", "Color de dibujo; también recolorea el objeto seleccionado", "描画色（選択中のオブジェクトの色も変更）", "绘图颜色；也会重新着色所选对象", "Цвет рисования; меняет и цвет выбранного объекта"}},
        {"descEyedropper", {"Ekrandan renk al", "Pick a colour from the screen", "Farbe vom Bildschirm aufnehmen", "Prélever une couleur à l'écran", "Tomar un color de la pantalla", "画面から色を取得", "从屏幕取色", "Взять цвет с экрана"}},
        {"descLock", {"Seçimin yanlışlıkla kaymasını engelle", "Keep the selection from moving by accident", "Auswahl gegen Verschieben sperren", "Empêcher la sélection de bouger", "Evitar que la selección se mueva", "選択範囲の移動を防ぐ", "防止选区被意外移动", "Защитить выделение от сдвига"}},
        {"descOcr", {"Görüntüdeki metni kopyala", "Copy the text in the image", "Text im Bild kopieren", "Copier le texte de l'image", "Copiar el texto de la imagen", "画像内のテキストをコピー", "复制图像中的文字", "Скопировать текст с изображения"}},
        {"descUpload", {"Görüntüyü yükle ve bağlantı al", "Upload the image and get a link", "Bild hochladen und Link erhalten", "Téléverser l'image et obtenir un lien", "Subir la imagen y obtener un enlace", "画像をアップロードしてリンクを取得", "上传图像并获取链接", "Загрузить изображение и получить ссылку"}},
        {"descLens", {"Görüntüyle internette ara", "Search the web with the image", "Mit dem Bild im Web suchen", "Rechercher sur le web avec l'image", "Buscar en la web con la imagen", "画像でウェブ検索", "用图像进行网络搜索", "Искать в интернете по изображению"}},
        {"descGif", {"Seçili alanı GIF olarak kaydet", "Record the area as a GIF", "Bereich als GIF aufnehmen", "Enregistrer la zone en GIF", "Grabar el área como GIF", "範囲を GIF として録画", "将区域录制为 GIF", "Записать область в GIF"}},
        {"descVideo", {"Seçili alanı video olarak kaydet", "Record the area as a video", "Bereich als Video aufnehmen", "Enregistrer la zone en vidéo", "Grabar el área como vídeo", "範囲を動画として録画", "将区域录制为视频", "Записать область в видео"}},
        {"runElevated", {"Yönetici olarak başlat", "Start as administrator", "Als Administrator starten", "Démarrer en tant qu'administrateur", "Iniciar como administrador", "管理者として起動", "以管理员身份启动", "Запускать от имени администратора"}},
        {"runElevatedTip", {"Yönetici olarak çalışan uygulamaların ekran görüntüsünü alabilmek için. Açarken bir kez yönetici onayı istenir; sonra EShot her açılışta onay sormadan yönetici olarak başlar.", "Needed to capture apps that run as administrator. You approve it once; EShot then starts as administrator without prompting.", "Nötig, um Apps aufzunehmen, die als Administrator laufen. Einmal bestätigen, danach startet EShot ohne Nachfrage als Administrator.", "Nécessaire pour capturer les applications lancées en administrateur. Une seule confirmation, puis EShot démarre en administrateur sans demande.", "Necesario para capturar aplicaciones que se ejecutan como administrador. Se aprueba una vez; luego EShot se inicia como administrador sin preguntar.", "管理者として実行中のアプリをキャプチャするために必要です。一度承認すると、以降は確認なしで管理者として起動します。", "用于截取以管理员身份运行的应用。只需确认一次，之后 EShot 会以管理员身份启动而不再询问。", "Нужно для снимков приложений, запущенных от имени администратора. Подтвердите один раз — дальше EShot будет запускаться с правами администратора без запроса."}},
        {"runElevatedNote", {"Yönetici olarak açılan uygulamaları yakalamak için. Ayrı bir yönetici hesabı kullanıyorsanız önerilmez.", "For capturing apps opened as administrator. Not recommended if you use a separate admin account.", "Zum Aufnehmen von Apps, die als Administrator geöffnet sind. Nicht empfohlen bei separatem Administratorkonto.", "Pour capturer les applications ouvertes en administrateur. Déconseillé avec un compte administrateur séparé.", "Para capturar aplicaciones abiertas como administrador. No recomendado si usas una cuenta de administrador aparte.", "管理者として開いたアプリをキャプチャするためのものです。別の管理者アカウントを使う場合はおすすめしません。", "用于截取以管理员身份打开的应用。如果使用单独的管理员账户，不建议开启。", "Для снимков приложений, открытых от имени администратора. Не рекомендуется при отдельной учётной записи администратора."}},
        {"runElevatedFailed", {"Yönetici olarak başlatma ayarlanamadı. Yönetici onayı verilmedi ya da görev oluşturulamadı.", "Could not set up starting as administrator. The administrator prompt was declined or the task could not be created.", "Start als Administrator konnte nicht eingerichtet werden. Die Abfrage wurde abgelehnt oder die Aufgabe konnte nicht erstellt werden.", "Impossible de configurer le démarrage en administrateur. La demande a été refusée ou la tâche n'a pas pu être créée.", "No se pudo configurar el inicio como administrador. Se rechazó la solicitud o no se pudo crear la tarea.", "管理者としての起動を設定できませんでした。承認が拒否されたか、タスクを作成できませんでした。", "无法设置以管理员身份启动。管理员确认被拒绝，或无法创建任务。", "Не удалось настроить запуск от имени администратора. Запрос был отклонён или задачу не удалось создать."}},
        {"runElevatedRestart", {"EShot şimdi yönetici olarak yeniden başlatılsın mı?", "Restart EShot as administrator now?", "EShot jetzt als Administrator neu starten?", "Redémarrer EShot en tant qu'administrateur maintenant ?", "¿Reiniciar EShot como administrador ahora?", "今すぐ EShot を管理者として再起動しますか？", "现在以管理员身份重新启动 EShot？", "Перезапустить EShot от имени администратора сейчас?"}},
        {"toolPixelate",   {"Pikselleştir (M)", "Pixelate (M)", "Verpixeln (M)", "Pixeliser (M)", "Pixelar (M)", "モザイク (M)", "像素化 (M)", "Пикселизация (M)"}},
        {"toolCounter",    {"Numara (N)", "Counter (N)", "Zähler (N)", "Compteur (N)", "Contador (N)", "カウンター (N)", "计数器 (N)", "Счётчик (N)"}},
        {"toolEraser",     {"Silgi (X)", "Eraser (X)", "Radierer (X)", "Gomme (X)", "Borrador (X)", "消しゴム (X)", "橡皮 (X)", "Ластик (X)"}},
        {"toolLine",       {"Çizgi (L)", "Line (L)", "Linie (L)", "Ligne (L)", "Línea (L)", "線 (L)", "线条 (L)", "Линия (L)"}},
        {"toolColor",      {"Renk Seç", "Pick Color", "Farbe wählen", "Couleur", "Elegir color", "色を選択", "选择颜色", "Выбрать цвет"}},
        {"toolFont",       {"Yazi Tipi", "Font", "Schriftart", "Police", "Fuente", "フォント", "字体", "Шрифт"}},
        {"toolMove",       {"Tasi", "Move", "Verschieben", "Deplacer", "Mover", "移動", "移动", "Переместить"}},
        {"toolUndo",       {"Geri Al (Ctrl+Z)", "Undo (Ctrl+Z)", "Rückgängig (Strg+Z)", "Annuler (Ctrl+Z)", "Deshacer (Ctrl+Z)", "元に戻す (Ctrl+Z)", "撤销 (Ctrl+Z)", "Отменить (Ctrl+Z)"}},
        {"toolRedo",       {"İleri Al (Ctrl+Y)", "Redo (Ctrl+Y)", "Wiederholen (Strg+Y)", "Rétablir (Ctrl+Y)", "Rehacer (Ctrl+Y)", "やり直し (Ctrl+Y)", "重做 (Ctrl+Y)", "Повторить (Ctrl+Y)"}},
        {"actionPin",      {"Ekrana Sabitle", "Pin to Screen", "Heften", "Épingler", "Fijar", "ピン留め", "固定", "Закрепить"}},
        {"actionCopy",     {"Kopyala (Ctrl+C)", "Copy (Ctrl+C)", "Kopieren (Strg+C)", "Copier (Ctrl+C)", "Copiar (Ctrl+C)", "コピー (Ctrl+C)", "复制 (Ctrl+C)", "Копировать (Ctrl+C)"}},
        {"actionSave",     {"Kaydet (Ctrl+S)", "Save (Ctrl+S)", "Speichern (Strg+S)", "Enregistrer (Ctrl+S)", "Guardar (Ctrl+S)", "保存 (Ctrl+S)", "保存 (Ctrl+S)", "Сохранить (Ctrl+S)"}},
        {"actionClose",    {"Kapat (Esc)", "Close (Esc)", "Schließen (Esc)", "Fermer (Esc)", "Cerrar (Esc)", "閉じる (Esc)", "关闭 (Esc)", "Закрыть (Esc)"}},
        {"actionLock",     {"Seçimi Kilitle", "Lock Selection", "Auswahl sperren", "Verrouiller sélection", "Blocar selección", "選択をロック", "锁定选区", "Заблокировать выделение"}},
        {"actionOcr",      {"Metin Tanıma (OCR)", "Recognize Text (OCR)", "Texterkennung (OCR)", "Reconnaître le texte (OCR)", "Reconocer texto (OCR)", "テキスト認識 (OCR)", "识别文字 (OCR)", "Распознать текст (OCR)"}},
        {"visualSearchTitle", {"Görsel Arama", "Visual Search", "Visuelle Suche", "Recherche visuelle", "Búsqueda visual", "画像検索", "视觉搜索", "Поиск по изображению"}},
        {"visualSearchProvider", {"Sağlayıcı:", "Provider:", "Anbieter:", "Fournisseur :", "Proveedor:", "プロバイダー:", "提供方：", "Сервис:"}},
        {"visualSearchGoogleLens", {"Google Lens", "Google Lens", "Google Lens", "Google Lens", "Google Lens", "Google Lens", "Google Lens", "Google Lens"}},
        {"visualSearchYandexImages", {"Yandex Görseller", "Yandex Images", "Yandex Bilder", "Yandex Images", "Yandex Imágenes", "Yandex Images", "Yandex 图片", "Yandex Картинки"}},
        {"visualSearchAction", {"Görselle Ara", "Search by Image", "Mit Bild suchen", "Rechercher par image", "Buscar por imagen", "画像で検索", "以图搜索", "Поиск по картинке"}},
        {"visualSearchGoogleTooltip", {"Google Lens ile ara", "Search with Google Lens", "Mit Google Lens suchen", "Rechercher avec Google Lens", "Buscar con Google Lens", "Google Lens で検索", "使用 Google Lens 搜索", "Искать через Google Lens"}},
        {"visualSearchYandexTooltip", {"Yandex Görseller ile ara", "Search with Yandex Images", "Mit Yandex Bilder suchen", "Rechercher avec Yandex Images", "Buscar con Yandex Imágenes", "Yandex Images で検索", "使用 Yandex 图片搜索", "Искать через Yandex Картинки"}},
        {"toolEyedropper", {"Renk Seçici", "Eyedropper", "Pipette", "Pipette", "Cuentagotas", "Eyedropper", "取色器", "Пипетка"}},
        {"toolSemiRect",   {"Saydam Kare", "Semi-Transparent", "Halbtransparent", "Semi-transparent", "Semi-transparente", "半透明", "半透明矩形", "Полупрозрачный"}},

        // ─── Araç listesi ───
        {"toolListPen",    {"✏️ Kalem", "✏️ Pen", "✏️ Stift", "✏️ Stylo", "✏️ Lápiz", "✏️ ペン", "✏️ 画笔", "✏️ Карандаш"}},
        {"toolListArrow",  {"➡️ Ok", "➡️ Arrow", "➡️ Pfeil", "➡️ Flèche", "➡️ Flecha", "➡️ 矢印", "➡️ 箭头", "➡️ Стрелка"}},
        {"toolListRect",   {"⬜ Kare", "⬜ Rectangle", "⬜ Rechteck", "⬜ Rectangle", "⬜ Rectángulo", "⬜ 長方形", "⬜ 矩形", "⬜ Прямоугольник"}},
        {"toolListCircle", {"⭕ Çember", "⭕ Circle", "⭕ Kreis", "⭕ Cercle", "⭕ Círculo", "⭕ 円", "⭕ 圆形", "⭕ Круг"}},
        {"toolListText",   {"🔤 Metin", "🔤 Text", "🔤 Text", "🔤 Texte", "🔤 Texto", "🔤 テキスト", "🔤 文本", "🔤 Текст"}},
        {"toolListHighlight",{"🖍️ Vurgulayıcı", "🖍️ Highlighter", "🖍️ Textmarker", "🖍️ Surligneur", "🖍️ Resaltador", "🖍️ マーカー", "🖍️ 荧光笔", "🖍️ Маркер"}},
        {"toolListBlur",   {"💧 Bulanıklaştır", "💧 Blur", "💧 Unschärfe", "💧 Flou", "💧 Desenfocar", "💧 ぼかし", "💧 模糊", "💧 Размытие"}},
        {"toolListPixelate", {"🔲 Pikselleştir", "🔲 Pixelate", "🔲 Verpixeln", "🔲 Pixeliser", "🔲 Pixelar", "🔲 モザイク", "🔲 像素化", "🔲 Пикселизация"}},
        {"toolListCounter",{"🔢 Numara", "🔢 Counter", "🔢 Zähler", "🔢 Compteur", "🔢 Contador", "🔢 カウンター", "🔢 计数器", "🔢 Счётчик"}},
        {"toolListEraser", {"🧹 Silgi", "🧹 Eraser", "🧹 Radierer", "🧹 Gomme", "🧹 Borrador", "🧹 消しゴム", "🧹 橡皮", "🧹 Ластик"}},
        {"toolListLine",   {"📏 Çizgi", "📏 Line", "📏 Linie", "📏 Ligne", "📏 Línea", "📏 線", "📏 线条", "📏 Линия"}},
        {"toolListSemiRect",{"🔳 Saydam Kare", "🔳 Semi-Rect", "🔳 Halbtransparent", "🔳 Semi-rect", "🔳 Semi-rect", "🔳 半透明", "🔳 半透明矩形", "🔳 Полупрозрачный"}},

        // ─── Pinned ───
        {"pinnedCopy",     {"Kopyala", "Copy", "Kopieren", "Copier", "Copiar", "コピー", "复制", "Копировать"}},
        {"pinnedSave",     {"Farklı Kaydet...", "Save As...", "Speichern unter...", "Enregistrer sous...", "Guardar como...", "名前を付けて保存...", "另存为...", "Сохранить как..."}},
        {"pinnedClose",    {"Kapat", "Close", "Schließen", "Fermer", "Cerrar", "閉じる", "关闭", "Закрыть"}},

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
        {"version",        {"Sürüm", "Version", "Version", "Version", "Versión", "バージョン", "版本", "Версия"}},

        // ─── Bildirim ───
        {"notifCaptureTitle",{"EShot", "EShot", "EShot", "EShot", "EShot", "EShot", "EShot", "EShot"}},
        {"notifCaptureMsg",{"Ekran görüntüsü alındı (%1x%2)", "Screenshot taken (%1x%2)", "Bildschirmfoto (%1x%2)", "Capture (%1x%2)", "Captura (%1x%2)", "スクリーンショット (%1x%2)", "截图 (%1x%2)", "Скриншот (%1x%2)"}},
        {"captureSaved",   {"Ekran görüntüsü kaydedildi:", "Screenshot saved:", "Screenshot gespeichert:", "Capture enregistrée :", "Captura guardada:", "スクリーンショット保存:", "截图已保存:", "Скриншот сохранён:"}},

        // ─── Güncelleme ───
        {"updateTitle",    {"Güncelleme Mevcut", "Update Available", "Update verfügbar", "Mise à jour", "Actualización", "アップデート可能", "有可用更新", "Доступно обновление"}},
        {"updateNow",      {"Şimdi Güncelle", "Update Now", "Jetzt aktualisieren", "Mettre à jour", "Actualizar ahora", "今すぐ更新", "立即更新", "Обновить сейчас"}},
        {"updateStatusIdle",{"Güncelleme otomatik kontrol edilir.", "Updates are checked automatically.", "Updates werden automatisch geprüft.", "Les mises à jour sont vérifiées automatiquement.", "Las actualizaciones se comprueban automáticamente.", "アップデートは自動的に確認されます。", "会自动检查更新。", "Обновления проверяются автоматически."}},
        {"updateStatusChecking",{"Güncellemeler kontrol ediliyor...", "Checking for updates...", "Updates werden gesucht...", "Recherche de mises à jour...", "Buscando actualizaciones...", "アップデートを確認中...", "正在检查更新...", "Проверка обновлений..."}},
        {"updateStatusUpToDate",{"EShot güncel.", "EShot is up to date.", "EShot ist aktuell.", "EShot est à jour.", "EShot está actualizado.", "EShot は最新です。", "EShot 已是最新。", "EShot обновлён."}},
        {"updateStatusAvailable",{"Yeni sürüm mevcut: %1", "New version available: %1", "Neue Version verfügbar: %1", "Nouvelle version disponible : %1", "Nueva versión disponible: %1", "新しいバージョンがあります: %1", "有新版本：%1", "Доступна новая версия: %1"}},
        {"updateStatusDownloading",{"Güncelleme indiriliyor...", "Downloading update...", "Update wird heruntergeladen...", "Téléchargement de la mise à jour...", "Descargando actualización...", "アップデートをダウンロード中...", "正在下载更新...", "Загрузка обновления..."}},
        {"updateStatusInstalling",{"Güncelleme kuruluyor...", "Installing update...", "Update wird installiert...", "Installation de la mise à jour...", "Instalando actualización...", "アップデートをインストール中...", "正在安装更新...", "Установка обновления..."}},
        {"updateStatusRestarting",{"EShot güncelleniyor ve yeniden açılacak.", "EShot is updating and will reopen.", "EShot wird aktualisiert und neu gestartet.", "EShot se met à jour et va se rouvrir.", "EShot se está actualizando y se volverá a abrir.", "EShot を更新中です。再起動します。", "EShot 正在更新并将重新打开。", "EShot обновляется и откроется снова."}},
        {"updateStatusFailed",{"Güncelleme başarısız: %1", "Update failed: %1", "Update fehlgeschlagen: %1", "Échec de la mise à jour : %1", "Error al actualizar: %1", "アップデートに失敗しました: %1", "更新失败：%1", "Не удалось обновить: %1"}},
        {"updateNoInstaller",{"Bu cihaz için uygun installer bulunamadı.", "No suitable installer was found for this device.", "Für dieses Gerät wurde kein passender Installer gefunden.", "Aucun installateur adapté n'a été trouvé pour cet appareil.", "No se encontró un instalador adecuado para este dispositivo.", "このデバイスに適したインストーラーが見つかりませんでした。", "未找到适用于此设备的安装程序。", "Не найден подходящий установщик для этого устройства."}},
        {"updateInvalidResponse",{"Güncelleme yanıtı okunamadı.", "Could not read the update response.", "Update-Antwort konnte nicht gelesen werden.", "Impossible de lire la réponse de mise à jour.", "No se pudo leer la respuesta de actualización.", "アップデート応答を読み取れませんでした。", "无法读取更新响应。", "Не удалось прочитать ответ обновления."}},
        {"updateInvalidDownload",{"İndirilen güncelleme dosyası doğrulanamadı.", "The downloaded update file could not be verified.", "Die heruntergeladene Update-Datei konnte nicht geprüft werden.", "Le fichier de mise à jour téléchargé n'a pas pu être vérifié.", "No se pudo verificar el archivo de actualización descargado.", "ダウンロードしたアップデートファイルを確認できませんでした。", "无法验证下载的更新文件。", "Не удалось проверить загруженный файл обновления."}},
        {"updateStatusAur",{"EShot %1 yayınlandı. Bu kopya AUR paketi eshot-bin ile yönetiliyor; AUR yardımcınızla güncelleyin (örneğin yay -Syu).", "EShot %1 is available. This copy is managed by the AUR package eshot-bin; update it with your AUR helper (for example yay -Syu).", "EShot %1 ist verfügbar. Diese Kopie wird vom AUR-Paket eshot-bin verwaltet; aktualisieren Sie sie mit Ihrem AUR-Helfer (z. B. yay -Syu).", "EShot %1 est disponible. Cette copie est gérée par le paquet AUR eshot-bin ; mettez-la à jour avec votre assistant AUR (par exemple yay -Syu).", "EShot %1 está disponible. Esta copia la gestiona el paquete AUR eshot-bin; actualícela con su asistente de AUR (por ejemplo yay -Syu).", "EShot %1 が利用可能です。このコピーは AUR パッケージ eshot-bin で管理されています。AUR ヘルパーで更新してください（例: yay -Syu）。", "EShot %1 已发布。此副本由 AUR 软件包 eshot-bin 管理，请使用 AUR 助手更新（例如 yay -Syu）。", "Доступна EShot %1. Эта копия управляется пакетом AUR eshot-bin; обновите её через AUR-помощник (например, yay -Syu)."}},
        {"updateStatusPackageManager",{"EShot %1 yayınlandı. Bu kopya kendini güncelleyemez; paket yöneticinizle güncelleyin veya sürümler sayfasından indirin.", "EShot %1 is available. This copy cannot update itself; update it with your package manager or download it from the releases page.", "EShot %1 ist verfügbar. Diese Kopie kann sich nicht selbst aktualisieren; aktualisieren Sie sie mit Ihrer Paketverwaltung oder laden Sie sie von der Release-Seite herunter.", "EShot %1 est disponible. Cette copie ne peut pas se mettre à jour elle-même ; utilisez votre gestionnaire de paquets ou téléchargez-la depuis la page des versions.", "EShot %1 está disponible. Esta copia no puede actualizarse sola; actualícela con su gestor de paquetes o descárguela desde la página de versiones.", "EShot %1 が利用可能です。このコピーは自動更新できません。パッケージマネージャーで更新するか、リリースページからダウンロードしてください。", "EShot %1 已发布。此副本无法自动更新，请使用包管理器更新或从发布页面下载。", "Доступна EShot %1. Эта копия не может обновиться сама; обновите её через менеджер пакетов или скачайте со страницы релизов."}},
        {"updateCannotLaunchInstaller",{"Installer başlatılamadı.", "Could not launch the installer.", "Installer konnte nicht gestartet werden.", "Impossible de lancer l'installateur.", "No se pudo iniciar el instalador.", "インストーラーを起動できませんでした。", "无法启动安装程序。", "Не удалось запустить установщик."}},

        // ─── Sihirbaz ───
        {"wizardTitle",    {"EShot'a Hoş Geldiniz!", "Welcome to EShot!", "Willkommen bei EShot!", "Bienvenue !", "¡Bienvenido!", "ようこそ！", "欢迎使用EShot！", "Добро пожаловать в EShot!"}},
        {"wizardDesc",     {"Ayarları yapılandırın.", "Configure your settings.", "Einstellungen konfigurieren.", "Configurez vos paramètres.", "Configure sus ajustes.", "設定を構成。", "配置设置。", "Настройте параметры."}},
        {"wizardFinish",   {"Tamamla", "Finish", "Fertig", "Terminer", "Finalizar", "完了", "完成", "Готово"}},
        {"wizardHotkeyDesc",{"Kısayol tuşunu ayarlayın.\nVarsayılan: Print Screen", "Set the hotkey.\nDefault: Print Screen", "Tastenkürzel festlegen.\nStandard: Druck", "Définir le raccourci.\nDéfaut: Impr. écran", "Configure el atajo.\nPredeterminado: Imp Pant", "ホットキーを設定。\nデフォルト: Print Screen", "设置快捷键。\n默认: Print Screen", "Установите клавишу.\nПо умолчанию: Print Screen"}},
        {"printScreenConflictTitle",{"Print Screen çakışması", "Print Screen conflict", "Print Screen Konflikt", "Conflit Print Screen", "Conflicto de Print Screen", "Print Screen の競合", "Print Screen 冲突", "Конфликт Print Screen"}},
        {"printScreenConflictMessage",{"Windows, Print Screen tuşunu Snipping Tool için kullanıyor. EShot'un Print Screen ile çalışması için bu Windows ayarını kapatın.", "Windows is using Print Screen for Snipping Tool. Disable this Windows setting to let EShot use Print Screen.", "Windows verwendet Print Screen für das Snipping Tool. Deaktivieren Sie diese Windows-Einstellung, damit EShot Print Screen verwenden kann.", "Windows utilise Print Screen pour Snipping Tool. Désactivez ce réglage Windows pour permettre à EShot d'utiliser Print Screen.", "Windows usa Print Screen para Snipping Tool. Desactive este ajuste de Windows para que EShot pueda usar Print Screen.", "Windows が Print Screen を Snipping Tool に使用しています。EShot で Print Screen を使うには、この Windows 設定を無効にしてください。", "Windows 正在将 Print Screen 用于截图工具。请关闭此 Windows 设置，以便 EShot 使用 Print Screen。", "Windows использует Print Screen для Snipping Tool. Отключите этот параметр Windows, чтобы EShot мог использовать Print Screen."}},
        {"printScreenConflictFix",{"Windows Print Screen ayarını kapat", "Disable Windows Print Screen shortcut", "Windows Print Screen Kurzbefehl deaktivieren", "Désactiver le raccourci Print Screen de Windows", "Desactivar el atajo Print Screen de Windows", "Windows の Print Screen ショートカットを無効にする", "禁用 Windows Print Screen 快捷键", "Отключить сочетание Windows Print Screen"}},
        {"printScreenConflictDisabled",{"Windows Print Screen Snipping Tool ayarı kapatıldı. Print Screen artık EShot tarafından kullanılabilir.", "Windows Print Screen Snipping Tool shortcut has been disabled. Print Screen can now be used by EShot.", "Windows Print Screen für Snipping Tool wurde deaktiviert. Print Screen kann jetzt von EShot verwendet werden.", "Le raccourci Windows Print Screen pour Snipping Tool a été désactivé. Print Screen peut maintenant être utilisé par EShot.", "El atajo Windows Print Screen para Snipping Tool se ha desactivado. EShot ya puede usar Print Screen.", "Windows Print Screen の Snipping Tool ショートカットを無効にしました。Print Screen を EShot で使用できます。", "Windows Print Screen 截图工具快捷键已禁用。EShot 现在可以使用 Print Screen。", "Сочетание Windows Print Screen для Snipping Tool отключено. Теперь EShot может использовать Print Screen."}},
        {"hotkeyMayBeInUse",{"Bu kısayol başka bir uygulama tarafından kullanılıyor olabilir. Lütfen farklı bir kombinasyon seçin.", "This hotkey may already be used by another app. Please choose a different combination.", "Dieses Kürzel wird möglicherweise bereits von einer anderen App verwendet. Bitte wählen Sie eine andere Kombination.", "Ce raccourci est peut-être déjà utilisé par une autre application. Choisissez une autre combinaison.", "Este atajo ya puede estar usado por otra aplicación. Elija otra combinación.", "このホットキーは他のアプリで既に使用されている可能性があります。別の組み合わせを選んでください。", "此快捷键可能已被其他应用使用。请选择其他组合。", "Эта горячая клавиша уже может использоваться другим приложением. Выберите другую комбинацию."}},
        {"recordingHotkeyMayBeInUse",{"Bu kayıt kısayollarından biri başka bir uygulama tarafından kullanılıyor olabilir.", "One of these recording hotkeys may already be used by another app.", "Eines dieser Aufnahme-Kürzel wird möglicherweise bereits von einer anderen App verwendet.", "Un de ces raccourcis d'enregistrement est peut-être déjà utilisé par une autre application.", "Uno de estos atajos de grabación ya puede estar usado por otra aplicación.", "録画用ホットキーのいずれかが他のアプリで使用されている可能性があります。", "某个录制快捷键可能已被其他应用使用。", "Одна из горячих клавиш записи уже может использоваться другим приложением."}},
        {"directCaptureHotkeyMayBeInUse",{"Doğrudan yakalama kısayollarından biri başka bir uygulama tarafından kullanılıyor olabilir.", "One of the direct capture hotkeys may already be used by another app.", "Eines der Direktaufnahme-Kürzel wird möglicherweise bereits von einer anderen App verwendet.", "Un des raccourcis de capture directe est peut-être déjà utilisé par une autre application.", "Uno de los atajos de captura directa ya puede estar usado por otra aplicación.", "直接キャプチャのホットキーのいずれかが他のアプリで使用されている可能性があります。", "某个直接捕获快捷键可能已被其他应用使用。", "Одна из горячих клавиш прямого захвата уже может использоваться другим приложением."}},
        {"hotkeyNotActiveTitle",{"Kısayol etkin değil", "Hotkey not active", "Tastenkürzel nicht aktiv", "Raccourci inactif", "Atajo no activo", "ホットキーが無効です", "快捷键未生效", "Горячая клавиша не активна"}},
        {"hotkeyNotActiveBody",{"%1 kaydedilemedi. Başka bir uygulama (ör. ShareX, Spectacle veya Lightshot) bu kısayolu kullanıyor olabilir. Ayarlar > %2 bölümünden farklı bir kısayol seçin.", "%1 could not be registered. Another app (such as ShareX, Spectacle or Lightshot) may already be using it. Choose a different key in Settings > %2.", "%1 konnte nicht registriert werden. Eine andere App (z. B. ShareX, Spectacle oder Lightshot) verwendet es möglicherweise bereits. Wählen Sie unter Einstellungen > %2 eine andere Taste.", "%1 n'a pas pu être enregistré. Une autre application (comme ShareX, Spectacle ou Lightshot) l'utilise peut-être déjà. Choisissez une autre touche dans Paramètres > %2.", "No se pudo registrar %1. Otra aplicación (como ShareX, Spectacle o Lightshot) puede estar usándolo. Elija otra tecla en Ajustes > %2.", "%1 を登録できませんでした。他のアプリ（ShareX、Spectacle、Lightshot など）が使用している可能性があります。設定 > %2 で別のキーを選んでください。", "无法注册 %1。其他应用（如 ShareX、Spectacle 或 Lightshot）可能已在使用。请在 设置 > %2 中选择其他按键。", "Не удалось зарегистрировать %1. Возможно, его уже использует другое приложение (например, ShareX, Spectacle или Lightshot). Выберите другую клавишу в разделе Настройки > %2."}},
        {"hotkeyCaptureFallback",{"Ekran görüntüsü şimdilik %1 ile alınır.", "Screenshots use %1 for now.", "Screenshots verwenden vorerst %1.", "Les captures utilisent %1 pour le moment.", "Por ahora, las capturas usan %1.", "現在、スクリーンショットには %1 を使用します。", "目前截图使用 %1。", "Пока для снимков используется %1."}},
        {"hotkeyNoneActive",{"etkin kısayol yok", "no hotkey active", "kein Kürzel aktiv", "aucun raccourci actif", "ningún atajo activo", "有効なホットキーなし", "无可用快捷键", "нет активной клавиши"}},
        {"hotkeyConflictWith",{"⚠️ Bu kısayol zaten kullanılıyor: %1", "⚠️ Already used by: %1", "⚠️ Bereits verwendet von: %1", "⚠️ Déjà utilisé par : %1", "⚠️ Ya lo usa: %1", "⚠️ 既に使用中: %1", "⚠️ 已被占用：%1", "⚠️ Уже используется: %1"}},
        {"hotkeyConflictSave",{"Birden fazla EShot kısayolu aynı tuşu kullanıyor. Kaydetmeden önce birini değiştirin.", "Several EShot hotkeys use the same key. Change one of them before saving.", "Mehrere EShot-Kürzel verwenden dieselbe Taste. Ändern Sie eines davon vor dem Speichern.", "Plusieurs raccourcis EShot utilisent la même touche. Modifiez-en un avant d'enregistrer.", "Varios atajos de EShot usan la misma tecla. Cambie uno antes de guardar.", "複数の EShot ホットキーが同じキーを使用しています。保存する前にいずれかを変更してください。", "多个 EShot 快捷键使用了相同的按键。请在保存前修改其中一个。", "Несколько горячих клавиш EShot используют одну и ту же клавишу. Измените одну из них перед сохранением."}},
        {"autoStartSaveFailed",{"Sistemle başlat ayarı kaydedilemedi.", "Could not save the startup setting.", "Die Startoption konnte nicht gespeichert werden.", "Impossible d'enregistrer le réglage de démarrage.", "No se pudo guardar la opción de inicio.", "起動時設定を保存できませんでした。", "无法保存启动设置。", "Не удалось сохранить настройку автозапуска."}},
        {"removeFromSystemTitle",{"EShot'u kaldır", "Remove EShot", "EShot entfernen", "Supprimer EShot", "Quitar EShot", "EShot を削除", "移除 EShot", "Удалить EShot"}},
        {"removeFromSystem",{"EShot'u bu sistemden kaldır", "Remove EShot from this system", "EShot von diesem System entfernen", "Supprimer EShot de ce système", "Quitar EShot de este sistema", "このシステムから EShot を削除", "从此系统中移除 EShot", "Удалить EShot из этой системы"}},
        {"removeFromSystemConfirm",{"Print Screen kısayolu masaüstünüze geri verilecek; EShot'un uygulama menüsü girdisi, simgeleri, otomatik başlatma girdisi ve kurulu kopyası (~/.local/opt/EShot) silinecek ve EShot kapanacak. Ekran görüntüleriniz ve ayarlarınız korunur. Devam edilsin mi?", "The Print Screen shortcut will be given back to your desktop, EShot's application menu entry, icons, autostart entry and installed copy (~/.local/opt/EShot) will be deleted, and EShot will quit. Your screenshots and settings are kept. Continue?", "Die Druck-Taste wird wieder dem Desktop zugewiesen, EShots Menüeintrag, Symbole, Autostart-Eintrag und installierte Kopie (~/.local/opt/EShot) werden gelöscht und EShot wird beendet. Screenshots und Einstellungen bleiben erhalten. Fortfahren?", "Le raccourci Impr. écran sera rendu au bureau, l'entrée de menu, les icônes, l'entrée de démarrage automatique et la copie installée d'EShot (~/.local/opt/EShot) seront supprimées, puis EShot se fermera. Vos captures et réglages sont conservés. Continuer ?", "El atajo Imprimir pantalla se devolverá al escritorio, se eliminarán la entrada de menú, los iconos, la entrada de inicio automático y la copia instalada de EShot (~/.local/opt/EShot), y EShot se cerrará. Sus capturas y ajustes se conservan. ¿Continuar?", "Print Screen ショートカットをデスクトップに戻し、EShot のアプリメニュー項目、アイコン、自動起動項目、インストール済みコピー (~/.local/opt/EShot) を削除して EShot を終了します。スクリーンショットと設定は残ります。続行しますか？", "Print Screen 快捷键将归还给桌面，EShot 的应用菜单项、图标、自启动项和已安装副本 (~/.local/opt/EShot) 将被删除，随后 EShot 将退出。您的截图和设置会保留。是否继续？", "Клавиша Print Screen будет возвращена рабочему столу, пункт меню, значки, запись автозапуска и установленная копия EShot (~/.local/opt/EShot) будут удалены, после чего EShot закроется. Снимки экрана и настройки сохранятся. Продолжить?"}},
        {"removeFromSystemConfirmPackage",{"Print Screen kısayolu masaüstünüze geri verilecek, EShot'un otomatik başlatma ve KWin izin girdileri silinecek ve EShot kapanacak. EShot paketinin kendisini ardından paket yöneticinizle kaldırın (örneğin: sudo pacman -R eshot-bin). Devam edilsin mi?", "The Print Screen shortcut will be given back to your desktop, EShot's autostart and KWin permission entries will be deleted, and EShot will quit. Afterwards remove the EShot package itself with your package manager (for example: sudo pacman -R eshot-bin). Continue?", "Die Druck-Taste wird wieder dem Desktop zugewiesen, EShots Autostart- und KWin-Berechtigungseinträge werden gelöscht und EShot wird beendet. Entfernen Sie danach das EShot-Paket mit Ihrer Paketverwaltung (zum Beispiel: sudo pacman -R eshot-bin). Fortfahren?", "Le raccourci Impr. écran sera rendu au bureau, les entrées de démarrage automatique et d'autorisation KWin d'EShot seront supprimées, puis EShot se fermera. Supprimez ensuite le paquet EShot avec votre gestionnaire de paquets (par exemple : sudo pacman -R eshot-bin). Continuer ?", "El atajo Imprimir pantalla se devolverá al escritorio, se eliminarán las entradas de inicio automático y de permiso de KWin de EShot, y EShot se cerrará. Después, quite el paquete de EShot con su gestor de paquetes (por ejemplo: sudo pacman -R eshot-bin). ¿Continuar?", "Print Screen ショートカットをデスクトップに戻し、EShot の自動起動項目と KWin 許可項目を削除して EShot を終了します。その後、パッケージマネージャーで EShot パッケージ自体を削除してください (例: sudo pacman -R eshot-bin)。続行しますか？", "Print Screen 快捷键将归还给桌面，EShot 的自启动项和 KWin 权限项将被删除，随后 EShot 将退出。之后请用包管理器移除 EShot 软件包本身（例如：sudo pacman -R eshot-bin）。是否继续？", "Клавиша Print Screen будет возвращена рабочему столу, записи автозапуска и разрешения KWin для EShot будут удалены, после чего EShot закроется. Затем удалите сам пакет EShot менеджером пакетов (например: sudo pacman -R eshot-bin). Продолжить?"}},
        {"removeFromSystemDone",{"EShot bu sistemden kaldırıldı ve şimdi kapanacak.", "EShot was removed from this system and will now quit.", "EShot wurde von diesem System entfernt und wird jetzt beendet.", "EShot a été supprimé de ce système et va se fermer.", "EShot se ha quitado de este sistema y se cerrará ahora.", "EShot をこのシステムから削除しました。EShot を終了します。", "EShot 已从此系统中移除，现在将退出。", "EShot удалён из этой системы и сейчас закроется."}},
        {"removeFromSystemDonePackage",{"EShot'un masaüstü entegrasyonu kaldırıldı ve EShot şimdi kapanacak. Paketi paket yöneticinizle kaldırmayı unutmayın.", "EShot's desktop integration was removed and EShot will now quit. Remember to remove the package with your package manager.", "EShots Desktop-Integration wurde entfernt und EShot wird jetzt beendet. Entfernen Sie das Paket noch mit Ihrer Paketverwaltung.", "L'intégration d'EShot au bureau a été supprimée et EShot va se fermer. Pensez à supprimer le paquet avec votre gestionnaire de paquets.", "Se quitó la integración de EShot con el escritorio y EShot se cerrará ahora. Recuerde quitar el paquete con su gestor de paquetes.", "EShot のデスクトップ統合を削除しました。EShot を終了します。パッケージマネージャーでパッケージも削除してください。", "EShot 的桌面集成已移除，现在将退出。请记得用包管理器移除软件包。", "Интеграция EShot с рабочим столом удалена, EShot сейчас закроется. Не забудьте удалить пакет менеджером пакетов."}},
        {"removeFromSystemErrors",{"Bazı öğeler kaldırılamadı:", "Some items could not be removed:", "Einige Elemente konnten nicht entfernt werden:", "Certains éléments n'ont pas pu être supprimés :", "Algunos elementos no se pudieron quitar:", "一部の項目を削除できませんでした:", "部分项目无法移除：", "Некоторые элементы не удалось удалить:"}},

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
        {"tipAutoStart",   {"Sistemle başlat.", "Start with system.", "Mit dem System starten.", "Démarrer avec le système.", "Iniciar con el sistema.", "システムと同時に開始。", "随系统启动。", "Запускать вместе с системой."}},
        {"tipNotifications",{"Bildirim göster.", "Show notifications.", "Benachrichtigungen.", "Notifications.", "Notificaciones.", "通知を表示。", "显示通知。", "Показывать уведомления."}},
        {"tipPlaySound",   {"Ses çal.", "Play sound.", "Ton abspielen.", "Jouer le son.", "Reproducir sonido.", "サウンド再生。", "播放声音。", "Звук."}},
        {"tipCopyPath",    {"Yolu kopyala.", "Copy path.", "Pfad kopieren.", "Copier le chemin.", "Copiar ruta.", "パスをコピー。", "复制路径。", "Копировать путь."}},
        {"tipHighContrast",{"Yüksek kontrast.", "High contrast.", "Hoher Kontrast.", "Contraste élevé.", "Alto contraste.", "ハイコントラスト。", "高对比度。", "Высокий контраст."}},
        {"tipTrayIcon",    {"Açık temalarda daha görünür olması için siyah tepsi simgesi kullanır.", "Use a black tray icon for better visibility on light themes.", "Verwendet ein schwarzes Taskbar-Symbol für bessere Sichtbarkeit bei hellen Designs.", "Utilise une icône de zone noire pour mieux se voir avec les thèmes clairs.", "Usa un icono de bandeja negro para verlo mejor con temas claros.", "明るいテーマで見やすい黒いトレイアイコンを使います。", "在浅色主题下使用更清晰的黑色托盘图标。", "Использует чёрный значок в трее для лучшей видимости в светлых темах."}},

        // ─── OCR ───
        {"ocrTitle",       {"Metin Tanıma (OCR)", "Text Recognition (OCR)", "Texterkennung (OCR)", "Reconnaissance de texte (OCR)", "Reconocimiento de texto (OCR)", "テキスト認識 (OCR)", "文字识别 (OCR)", "Распознавание текста (OCR)"}},
        {"ocrCopy",        {"Panoya Kopyala", "Copy to Clipboard", "In die Zwischenablage", "Copier dans le presse-papiers", "Copiar al portapapeles", "クリップボードにコピー", "复制到剪贴板", "Копировать в буфер"}},
        {"ocrCopied",      {"Metin panoya kopyalandı", "Text copied to clipboard", "Text in die Zwischenablage kopiert", "Texte copié dans le presse-papiers", "Texto copiado al portapapeles", "テキストをコピーしました", "文本已复制", "Текст скопирован"}},
        {"ocrEmpty",       {"Görüntüde metin bulunamadı", "No text found in image", "Kein Text im Bild gefunden", "Aucun texte trouvé dans l'image", "No se encontró texto en la imagen", "画像にテキストが見つかりません", "图像中未找到文字", "Текст не найден"}},
        {"ocrFailed",      {"OCR başarısız oldu", "OCR failed", "OCR fehlgeschlagen", "Échec de l'OCR", "OCR falló", "OCRに失敗しました", "OCR 失败", "Ошибка OCR"}},
        {"ocrClose",       {"Kapat", "Close", "Schließen", "Fermer", "Cerrar", "閉じる", "关闭", "Закрыть"}},
        {"ocrRetry",       {"Yeniden Dene", "Retry", "Erneut", "Réessayer", "Reintentar", "再試行", "重试", "Повторить"}},
        {"ocrTranslate",   {"Çevir", "Translate", "Übersetzen", "Traduire", "Traducir", "翻訳", "翻译", "Перевести"}},
        {"ocrTranslateBrowserError", {"Google Translate varsayılan tarayıcınızda açılamadı.", "Google Translate could not be opened in your default browser.", "Google Translate konnte nicht im Standardbrowser geöffnet werden.", "Google Translate n’a pas pu être ouvert dans votre navigateur par défaut.", "No se pudo abrir Google Translate en el navegador predeterminado.", "Google Translate を既定のブラウザーで開けませんでした。", "无法在默认浏览器中打开 Google 翻译。", "Не удалось открыть Google Translate в браузере по умолчанию."}},
        {"ocrProcessing",  {"Tanınıyor...", "Recognizing...", "Erkennung...", "Reconnaissance...", "Reconociendo...", "認識中...", "识别中...", "Распознавание..."}},
        {"ocrAutomatic",   {"Otomatik", "Automatic", "Automatisch", "Automatique", "Automático", "自動", "自动", "Автоматически"}},

        // ─── Kayıt (Recording) ───
        {"recordingStart", {"Kaydı Başlat", "Start Recording", "Aufnahme starten", "Démarrer l'enregistrement", "Iniciar grabación", "録画開始", "开始录制", "Начать запись"}},
        {"recordingStop",  {"Kaydı Durdur", "Stop Recording", "Aufnahme stoppen", "Arrêter l'enregistrement", "Detener grabación", "録画停止", "停止录制", "Остановить запись"}},
        {"recordingStopShort", {"Durdur", "Stop", "Stopp", "Arrêter", "Detener", "停止", "停止", "Стоп"}},
        {"recordingPauseResume",{"Duraklat / Sürdür", "Pause / Resume", "Pausieren / Fortsetzen", "Pause / Reprendre", "Pausar / Reanudar", "一時停止 / 再開", "暂停 / 继续", "Пауза / Продолжить"}},
        {"recordingDetails", {"Ayrıntılar", "Details", "Einzelheiten", "Détails", "Detalles", "詳細", "详细", "Подробности"}},
        {"recordingDrag", {"Taşımak için sürükleyin", "Drag to move", "Zum Verschieben ziehen", "Faites glisser pour déplacer", "Arrastra para mover", "ドラッグして移動", "拖动以移动", "Перетащите для перемещения"}},
        {"recordingStartTitle",{"GIF Kaydı", "GIF Recording", "GIF-Aufnahme", "Enregistrement GIF", "Grabación GIF", "GIF録画", "GIF 录制", "Запись GIF"}},
        {"recordingSaved", {"GIF kaydedildi:", "GIF saved:", "GIF gespeichert:", "GIF enregistré :", "GIF guardado:", "GIF保存:", "GIF 已保存:", "GIF сохранён:"}},
        {"recordingFailed",{"Kayıt başarısız oldu", "Recording failed", "Aufnahme fehlgeschlagen", "Échec de l'enregistrement", "Grabación falló", "録画に失敗しました", "录制失败", "Ошибка записи"}},
        {"recordingMaxTime",{"Maks. süre (sn):", "Max time (sec):", "Max. Zeit (Sek.):", "Temps max (sec) :", "Tiempo máx. (seg):", "最大時間 (秒):", "最大时长 (秒):", "Макс. время (сек):"}},
        {"quickSettings",{"Hızlı Ayarlar", "Quick Settings", "Schnelleinstellungen", "Réglages rapides", "Ajustes rápidos", "クイック設定", "快速设置", "Быстрые настройки"}},
        {"quickPenWidth",{"Kalem / şekil kalınlığı", "Pen / shape width", "Stift-/Formbreite", "Épaisseur stylo/forme", "Grosor de lápiz/forma", "ペン/図形の太さ", "画笔/形状粗细", "Толщина пера/фигуры"}},
        {"quickBlurIntensity",{"Bulanıklık şiddeti", "Blur intensity", "Unschärfeintensität", "Intensité du flou", "Intensidad de desenfoque", "ぼかし強度", "模糊强度", "Сила размытия"}},
        {"quickGifRecording",{"GIF kaydı", "GIF recording", "GIF-Aufnahme", "Enregistrement GIF", "Grabación GIF", "GIF録画", "GIF 录制", "Запись GIF"}},
        {"quickMaxSeconds",{"Maks. saniye", "Max seconds", "Max. Sekunden", "Secondes max", "Segundos máx.", "最大秒数", "最大秒数", "Макс. секунд"}},
        {"videoRecordingTitle",{"Video kaydı", "Video recording", "Videoaufnahme", "Enregistrement vidéo", "Grabación de video", "動画録画", "视频录制", "Запись видео"}},
        {"videoSaved",{"Video kaydedildi:", "Video saved:", "Video gespeichert:", "Vidéo enregistrée :", "Video guardado:", "動画保存:", "视频已保存:", "Видео сохранено:"}},
        {"videoFailed",{"Video kaydı başarısız oldu", "Video recording failed", "Videoaufnahme fehlgeschlagen", "Échec de l'enregistrement vidéo", "La grabación de video falló", "動画録画に失敗しました", "视频录制失败", "Ошибка записи видео"}},
        {"videoFfmpegMissing",{"FFmpeg bulunamadı. FFmpeg'i sistemden kurun veya uygulamanın ffmpeg klasörüne koyun.", "FFmpeg was not found. Install FFmpeg system-wide or put it in the app's ffmpeg folder.", "FFmpeg wurde nicht gefunden. Installieren Sie FFmpeg systemweit oder legen Sie es in den ffmpeg-Ordner der App.", "FFmpeg est introuvable. Installez FFmpeg sur le système ou placez-le dans le dossier ffmpeg de l'application.", "No se encontró FFmpeg. Instale FFmpeg en el sistema o colóquelo en la carpeta ffmpeg de la app.", "FFmpeg が見つかりません。システムにインストールするか、アプリの ffmpeg フォルダーに入れてください。", "未找到 FFmpeg。请在系统中安装 FFmpeg，或将其放入应用的 ffmpeg 文件夹。", "FFmpeg не найден. Установите FFmpeg в системе или поместите его в папку ffmpeg приложения."}},
        {"videoGstreamerMissing",{"GStreamer bulunamadı. Linux'ta gst-launch-1.0 ve gerekli GStreamer eklentilerini kurun.", "GStreamer was not found. Install gst-launch-1.0 and the required GStreamer plugins on Linux.", "GStreamer wurde nicht gefunden. Installieren Sie gst-launch-1.0 und die benötigten GStreamer-Plugins unter Linux.", "GStreamer est introuvable. Installez gst-launch-1.0 et les modules GStreamer requis sous Linux.", "No se encontró GStreamer. Instale gst-launch-1.0 y los plugins de GStreamer necesarios en Linux.", "GStreamer が見つかりません。Linux に gst-launch-1.0 と必要な GStreamer プラグインをインストールしてください。", "未找到 GStreamer。请在 Linux 上安装 gst-launch-1.0 和所需的 GStreamer 插件。", "GStreamer не найден. Установите gst-launch-1.0 и нужные плагины GStreamer в Linux."}},
        {"videoWaylandPortalMissing",{"Wayland ekran kaydı portalı bulunamadı. xdg-desktop-portal ve masaüstü portal arka ucunu kurun.", "The Wayland screen recording portal is not available. Install xdg-desktop-portal and your desktop portal backend.", "Das Wayland-Portal für Bildschirmaufnahmen ist nicht verfügbar. Installieren Sie xdg-desktop-portal und das passende Desktop-Portal-Backend.", "Le portail d'enregistrement d'écran Wayland n'est pas disponible. Installez xdg-desktop-portal et le backend de portail de votre bureau.", "El portal de grabación de pantalla de Wayland no está disponible. Instale xdg-desktop-portal y el backend de portal de su escritorio.", "Wayland の画面録画ポータルを利用できません。xdg-desktop-portal とデスクトップ用ポータルバックエンドをインストールしてください。", "Wayland 屏幕录制门户不可用。请安装 xdg-desktop-portal 和桌面门户后端。", "Портал записи экрана Wayland недоступен. Установите xdg-desktop-portal и backend портала для вашей среды."}},
        {"videoWaylandPermissionDenied",{"Wayland ekran kaydı izni verilmedi.", "Wayland screen recording permission was not granted.", "Die Wayland-Bildschirmaufnahme wurde nicht erlaubt.", "L'autorisation d'enregistrement d'écran Wayland n'a pas été accordée.", "No se concedió permiso para grabar la pantalla en Wayland.", "Wayland の画面録画権限が許可されませんでした。", "未授予 Wayland 屏幕录制权限。", "Разрешение на запись экрана Wayland не предоставлено."}},
        {"videoWaylandWrongSource",{"Seçilen alan, izin verilen Wayland kayıt ekranında değil. Doğru monitörü seçip tekrar deneyin.", "The selected area is not on the permitted Wayland recording screen. Choose the correct monitor and try again.", "Der ausgewählte Bereich liegt nicht auf dem freigegebenen Wayland-Aufnahmebildschirm. Wählen Sie den richtigen Monitor und versuchen Sie es erneut.", "La zone sélectionnée ne se trouve pas sur l'écran Wayland autorisé. Choisissez le bon moniteur et réessayez.", "El área seleccionada no está en la pantalla de grabación Wayland permitida. Elija el monitor correcto e inténtelo de nuevo.", "選択範囲が許可された Wayland 録画画面にありません。正しいモニターを選んでもう一度お試しください。", "所选区域不在已授权的 Wayland 录制屏幕上。请选择正确的显示器后重试。", "Выбранная область находится не на разрешённом экране записи Wayland. Выберите правильный монитор и повторите попытку."}},
        {"videoGstreamerStartFailed",{"GStreamer kaydı başlatılamadı.", "Could not start GStreamer recording.", "GStreamer-Aufnahme konnte nicht gestartet werden.", "Impossible de démarrer l'enregistrement GStreamer.", "No se pudo iniciar la grabación con GStreamer.", "GStreamer 録画を開始できませんでした。", "无法启动 GStreamer 录制。", "Не удалось запустить запись через GStreamer."}},
        {"videoPipeWireRemoteFailed",{"Wayland PipeWire bağlantısı açılamadı. Portal iznini tekrar verin ve PipeWire/xdg-desktop-portal servislerini kontrol edin.", "Could not open the Wayland PipeWire connection. Grant the portal permission again and check PipeWire/xdg-desktop-portal services.", "Die Wayland-PipeWire-Verbindung konnte nicht geöffnet werden. Erteilen Sie die Portalberechtigung erneut und prüfen Sie PipeWire/xdg-desktop-portal.", "Impossible d'ouvrir la connexion PipeWire Wayland. Accordez de nouveau l'autorisation du portail et vérifiez PipeWire/xdg-desktop-portal.", "No se pudo abrir la conexión PipeWire de Wayland. Vuelva a conceder el permiso del portal y revise PipeWire/xdg-desktop-portal.", "Wayland PipeWire 接続を開けませんでした。ポータル権限を再度許可し、PipeWire/xdg-desktop-portal を確認してください。", "无法打开 Wayland PipeWire 连接。请重新授予门户权限，并检查 PipeWire/xdg-desktop-portal 服务。", "Не удалось открыть соединение Wayland PipeWire. Повторно разрешите доступ через портал и проверьте PipeWire/xdg-desktop-portal."}},
        {"gifSettings",{"GIF Ayarları", "GIF Settings", "GIF-Einstellungen", "Réglages GIF", "Ajustes GIF", "GIF設定", "GIF 设置", "Настройки GIF"}},
        {"recordingCancel",{"İptal", "Cancel", "Abbrechen", "Annuler", "Cancelar", "キャンセル", "取消", "Отмена"}},
        {"recordingStartsIn",{"Kayıt başlıyor", "Recording starts in", "Aufnahme beginnt in", "L'enregistrement commence dans", "La grabación empieza en", "録画開始まで", "录制即将开始", "Запись начнётся через"}},
        {"videoQualityCrf",{"Video kalitesi (CRF)", "Video quality (CRF)", "Videoqualität (CRF)", "Qualité vidéo (CRF)", "Calidad de video (CRF)", "動画品質 (CRF)", "视频质量 (CRF)", "Качество видео (CRF)"}},
        {"videoCrfHint",{"Düşük değer daha yüksek kalite ve daha büyük dosya demektir. 24 dengeli varsayılandır.", "Lower values mean higher quality and larger files. 24 is the balanced default.", "Niedrigere Werte bedeuten höhere Qualität und größere Dateien. 24 ist der ausgewogene Standard.", "Une valeur plus basse donne une meilleure qualité et des fichiers plus volumineux. 24 est le réglage équilibré par défaut.", "Los valores más bajos dan más calidad y archivos más grandes. 24 es el valor equilibrado predeterminado.", "値が低いほど高品質でファイルサイズが大きくなります。24 がバランスの取れた既定値です。", "数值越低质量越高，文件越大。24 是均衡默认值。", "Чем ниже значение, тем выше качество и больше файл. 24 — сбалансированное значение по умолчанию."}},
        {"audioMode",{"Ses", "Audio", "Audio", "Audio", "Audio", "音声", "音频", "Аудио"}},
        {"audioNone",{"Ses yok", "No audio", "Kein Audio", "Sans audio", "Sin audio", "音声なし", "无音频", "Без аудио"}},
        {"audioDesktop",{"Masaüstü sesi", "Desktop audio", "Desktop-Audio", "Audio du bureau", "Audio del escritorio", "デスクトップ音声", "桌面音频", "Звук рабочего стола"}},
        {"audioMicrophone",{"Mikrofon", "Microphone", "Mikrofon", "Microphone", "Micrófono", "マイク", "麦克风", "Микрофон"}},
        {"audioDesktopMic",{"Masaüstü + mikrofon", "Desktop + microphone", "Desktop + Mikrofon", "Bureau + microphone", "Escritorio + micrófono", "デスクトップ + マイク", "桌面 + 麦克风", "Рабочий стол + микрофон"}},
        {"audioSource",{"Kaynak", "Source", "Quelle", "Source", "Fuente", "ソース", "来源", "Источник"}},
        {"audioMicrophoneDevice",{"Mikrofon", "Microphone", "Mikrofon", "Microphone", "Micrófono", "マイク", "麦克风", "Микрофон"}},
        {"audioSystemLoopback",{"Sistem sesi", "System audio", "Systemaudio", "Audio système", "Audio del sistema", "システム音声", "系统音频", "Системный звук"}},

        // ─── Yükleme (Upload) ───
        {"uploadToService",{"Servise Yükle", "Upload to Service", "Zu Dienst hochladen", "Téléverser vers un service", "Subir al servicio", "サービスにアップロード", "上传到服务", "Загрузить в сервис"}},
        {"uploadTitle",    {"Görsel Yükle", "Upload Image", "Bild hochladen", "Téléverser l'image", "Subir imagen", "画像をアップロード", "上传图片", "Загрузить изображение"}},
        {"uploadProvider", {"Servis:", "Service:", "Dienst:", "Service :", "Servicio:", "サービス:", "服务:", "Сервис:"}},
        {"upload",         {"Yükle", "Upload", "Hochladen", "Téléverser", "Subir", "アップロード", "上传", "Загрузить"}},
        {"uploadUploading",{"Yükleniyor...", "Uploading...", "Wird hochgeladen...", "Téléversement...", "Subiendo...", "アップロード中...", "上传中...", "Загрузка..."}},
        {"uploadSuccess",  {"Yüklendi!", "Uploaded!", "Hochgeladen!", "Téléversé !", "¡Subido!", "アップロード完了！", "上传成功！", "Загружено!"}},
        {"uploadFailed",   {"Yükleme başarısız", "Upload failed", "Hochladen fehlgeschlagen", "Échec du téléversement", "Error al subir", "アップロード失敗", "上传失败", "Ошибка загрузки"}},
        {"visualSearchBrowserLaunchTitle", {"Tarayıcı açılamadı", "Browser launch failed", "Browser konnte nicht geöffnet werden", "Échec de l'ouverture du navigateur", "No se pudo abrir el navegador", "ブラウザーを開けませんでした", "无法打开浏览器", "Не удалось открыть браузер"}},
        {"visualSearchBrowserLaunchError", {"Görsel arama sonucu varsayılan tarayıcınızda açılamadı.", "The visual-search result could not be opened in your default browser.", "Das Ergebnis der visuellen Suche konnte nicht im Standardbrowser geöffnet werden.", "Le résultat de recherche visuelle n'a pas pu être ouvert dans votre navigateur par défaut.", "El resultado de búsqueda visual no pudo abrirse en el navegador predeterminado.", "画像検索結果を既定のブラウザーで開けませんでした。", "无法在默认浏览器中打开视觉搜索结果。", "Не удалось открыть результат визуального поиска в браузере по умолчанию."}},
        {"uploadLinkCopied",{"Bağlantı panoya kopyalandı", "Link copied to clipboard", "Link in die Zwischenablage kopiert", "Lien copié", "Enlace copiado", "リンクをコピーしました", "链接已复制", "Ссылка скопирована"}},
        {"uploadOpen",     {"Bağlantıyı Aç", "Open Link", "Link öffnen", "Ouvrir le lien", "Abrir enlace", "リンクを開く", "打开链接", "Открыть ссылку"}},
        {"uploadLinkPlaceholder",{"Görsel bağlantısı burada görünecek", "Image link will appear here", "Bildlink erscheint hier", "Le lien apparaîtra ici", "El enlace aparecerá aquí", "リンクがここに表示されます", "链接将显示在此处", "Ссылка появится здесь"}},
        {"uploadDeletePlaceholder",{"Silme bağlantısı (opsiyonel)", "Delete link (optional)", "Löschlink (optional)", "Lien de suppression (optionnel)", "Enlace de borrado (opcional)", "削除リンク（任意）", "删除链接（可选）", "Ссылка на удаление (необязательно)"}},
        {"yandexAuthPlaceholder",{"Yandex OAuth tokeni veya yönlendirme URL'si", "Yandex OAuth token or redirect URL", "Yandex-OAuth-Token oder Weiterleitungs-URL", "Jeton OAuth Yandex ou URL de redirection", "Token OAuth de Yandex o URL de redirección", "Yandex OAuth トークンまたはリダイレクト URL", "Yandex OAuth 令牌或重定向 URL", "OAuth-токен Yandex или URL перенаправления"}},
        {"googleDriveAuthPlaceholder",{"Google Drive OAuth tokeni veya yönlendirme URL'si", "Google Drive OAuth token or redirect URL", "Google-Drive-OAuth-Token oder Weiterleitungs-URL", "Jeton OAuth Google Drive ou URL de redirection", "Token OAuth de Google Drive o URL de redirección", "Google Drive OAuth トークンまたはリダイレクト URL", "Google Drive OAuth 令牌或重定向 URL", "OAuth-токен Google Drive или URL перенаправления"}},
        {"catboxUserHashPlaceholder",{"İsteğe bağlı Catbox user hash", "Optional Catbox user hash", "Optionaler Catbox-User-Hash", "Hash utilisateur Catbox facultatif", "Hash de usuario de Catbox opcional", "任意の Catbox ユーザーハッシュ", "可选 Catbox 用户哈希", "Необязательный user hash Catbox"}},
        {"uploadAuthHelpCatbox",{"Catbox user hash isteğe bağlıdır. Boş bırakırsanız anonim yüklenir; user hash girerseniz dosya hesabınızda görünür.", "Catbox user hash is optional. Leave it empty for anonymous upload, or paste your user hash to attach uploads to your account.", "Der Catbox-User-Hash ist optional. Leer lassen für anonymes Hochladen oder einfügen, damit Uploads Ihrem Konto zugeordnet werden.", "Le hash utilisateur Catbox est facultatif. Laissez vide pour un envoi anonyme ou collez votre hash pour lier les envois à votre compte.", "El hash de usuario de Catbox es opcional. Déjalo vacío para subir de forma anónima o pégalo para vincular las subidas a tu cuenta.", "Catbox ユーザーハッシュは任意です。匿名アップロードなら空のまま、アカウントに紐付ける場合は貼り付けてください。", "Catbox 用户哈希是可选的。留空即匿名上传；粘贴用户哈希可将上传关联到你的账户。", "User hash Catbox необязателен. Оставьте пустым для анонимной загрузки или вставьте hash, чтобы привязать файлы к аккаунту."}},
        {"uploadAuthSaveFailed", {"Kimlik bilgisi güvenli depoya kaydedilemedi; yalnızca bu oturumda kullanılacak.", "Could not store the credentials securely; they will only be used for this session.", "Anmeldedaten konnten nicht sicher gespeichert werden; sie gelten nur für diese Sitzung.", "Impossible d'enregistrer les identifiants de façon sécurisée ; ils ne serviront que pour cette session.", "No se pudieron guardar las credenciales de forma segura; solo se usarán en esta sesión.", "認証情報を安全に保存できませんでした。このセッションでのみ使用されます。", "无法安全保存凭据；仅在本次会话中使用。", "Не удалось надёжно сохранить учётные данные; они будут использоваться только в этом сеансе."}},
        {"uploadAuthSaved", {"Kimlik bilgisi kaydedildi.", "Credentials saved.", "Anmeldedaten gespeichert.", "Identifiants enregistrés.", "Credenciales guardadas.", "認証情報を保存しました。", "凭据已保存。", "Учётные данные сохранены."}},
        {"uploadErrorInProgress",{"Yükleme zaten devam ediyor", "Upload already in progress", "Upload läuft bereits", "Téléversement déjà en cours", "La subida ya está en curso", "アップロードは既に進行中です", "上传已在进行中", "Загрузка уже выполняется"}},
        {"uploadErrorImageMissing",{"Yüklenecek görsel yok", "Image missing", "Bild fehlt", "Image manquante", "Falta la imagen", "画像がありません", "缺少图片", "Нет изображения"}},
        {"uploadErrorCannotReadImage",{"Görsel dosyası okunamadı", "Cannot read image", "Bild kann nicht gelesen werden", "Impossible de lire l'image", "No se puede leer la imagen", "画像を読み取れません", "无法读取图片", "Не удалось прочитать изображение"}},
        {"uploadErrorServerRejected",{"Sunucu yüklemeyi reddetti", "Server rejected the upload", "Server hat den Upload abgelehnt", "Le serveur a refusé le téléversement", "El servidor rechazó la subida", "サーバーがアップロードを拒否しました", "服务器拒绝上传", "Сервер отклонил загрузку"}},
        {"uploadErrorNetwork",{"Ağ hatası: %1", "Network error: %1", "Netzwerkfehler: %1", "Erreur réseau : %1", "Error de red: %1", "ネットワークエラー: %1", "网络错误：%1", "Ошибка сети: %1"}},
        {"uploadErrorHttp",{"HTTP hatası %1", "HTTP error %1", "HTTP-Fehler %1", "Erreur HTTP %1", "Error HTTP %1", "HTTP エラー %1", "HTTP 错误 %1", "Ошибка HTTP %1"}},
        {"uploadErrorUnexpectedResponse",{"Beklenmeyen yanıt: %1", "Unexpected response: %1", "Unerwartete Antwort: %1", "Réponse inattendue : %1", "Respuesta inesperada: %1", "予期しない応答: %1", "意外响应：%1", "Неожиданный ответ: %1"}},
        {"uploadErrorYandexTokenMissing",{"Yandex Disk tokeni eksik", "Yandex Disk token missing", "Yandex-Disk-Token fehlt", "Jeton Yandex Disk manquant", "Falta el token de Yandex Disk", "Yandex Disk トークンがありません", "缺少 Yandex Disk 令牌", "Отсутствует токен Yandex Disk"}},
        {"uploadErrorYandexUnsupportedToken",{"Bu Yandex değeri access_token değil. Client ID, Client secret, authorization code veya verification_code URL'si yerine access_token içeren yönlendirme URL'sini yapıştırın.", "This Yandex value is not an access_token. Instead of a Client ID, Client secret, authorization code, or verification_code URL, paste the redirect URL that contains access_token.", "Dieser Yandex-Wert ist kein access_token. Fügen Sie statt Client-ID, Client Secret, Autorisierungscode oder verification_code-URL die Weiterleitungs-URL mit access_token ein.", "Cette valeur Yandex n'est pas un access_token. Au lieu du Client ID, Client secret, code d'autorisation ou URL verification_code, collez l'URL de redirection contenant access_token.", "Este valor de Yandex no es un access_token. En lugar de Client ID, Client secret, código de autorización o URL verification_code, pega la URL de redirección que contiene access_token.", "この Yandex の値は access_token ではありません。Client ID、Client secret、認可コード、verification_code URL ではなく、access_token を含むリダイレクト URL を貼り付けてください。", "此 Yandex 值不是 access_token。不要使用 Client ID、Client secret、授权 code 或 verification_code URL；请粘贴包含 access_token 的重定向 URL。", "Это значение Yandex не является access_token. Вместо Client ID, Client secret, authorization code или URL verification_code вставьте URL перенаправления, содержащий access_token."}},
        {"uploadErrorYandexAuthFailed",{"Yandex yetkilendirmesi başarısız (HTTP 401). Token süresi dolmuş veya yanlış değer kaydedilmiş olabilir. Client ID/secret/code değil, access_token veya içinde access_token geçen tam yönlendirme URL'sini kaydedin.", "Yandex authorization failed (HTTP 401). The token may be expired or the wrong value may be saved. Save an access_token or the full redirect URL containing access_token, not the Client ID/secret/code.", "Yandex-Autorisierung fehlgeschlagen (HTTP 401). Das Token ist möglicherweise abgelaufen oder ein falscher Wert wurde gespeichert. Speichern Sie ein access_token oder die vollständige Weiterleitungs-URL mit access_token, nicht Client-ID/Secret/code.", "Échec de l'autorisation Yandex (HTTP 401). Le jeton a peut-être expiré ou une mauvaise valeur est enregistrée. Enregistrez un access_token ou l'URL de redirection complète contenant access_token, pas le Client ID/secret/code.", "Falló la autorización de Yandex (HTTP 401). El token puede haber caducado o puede haberse guardado un valor incorrecto. Guarda un access_token o la URL de redirección completa que contenga access_token, no Client ID/secret/code.", "Yandex 認証に失敗しました (HTTP 401)。トークンの期限切れ、または誤った値が保存されている可能性があります。Client ID/secret/code ではなく、access_token または access_token を含む完全なリダイレクト URL を保存してください。", "Yandex 授权失败 (HTTP 401)。令牌可能已过期，或保存了错误的值。请保存 access_token 或包含 access_token 的完整重定向 URL，不要保存 Client ID/secret/code。", "Авторизация Yandex не удалась (HTTP 401). Возможно, токен истек или сохранено неверное значение. Сохраните access_token или полный URL перенаправления с access_token, а не Client ID/secret/code."}},
        {"uploadErrorYandexStep",{"Yandex %1 hatası (HTTP %2)", "Yandex %1 error (HTTP %2)", "Yandex-%1-Fehler (HTTP %2)", "Erreur Yandex %1 (HTTP %2)", "Error de Yandex %1 (HTTP %2)", "Yandex %1 エラー (HTTP %2)", "Yandex %1 错误 (HTTP %2)", "Ошибка Yandex %1 (HTTP %2)"}},
        {"uploadErrorYandexUploadUrlMissing",{"Yandex yükleme URL'si bulunamadı", "Yandex upload URL missing", "Yandex-Upload-URL fehlt", "URL de téléversement Yandex manquante", "Falta la URL de subida de Yandex", "Yandex アップロード URL がありません", "缺少 Yandex 上传 URL", "Отсутствует URL загрузки Yandex"}},
        {"uploadErrorYandexPublicLinkMissing",{"Yandex herkese açık bağlantı döndürmedi", "Yandex public link missing", "Öffentlicher Yandex-Link fehlt", "Lien public Yandex manquant", "Falta el enlace público de Yandex", "Yandex 公開リンクがありません", "缺少 Yandex 公开链接", "Отсутствует публичная ссылка Yandex"}},
        {"uploadErrorGoogleTokenMissing",{"Google Drive tokeni eksik", "Google Drive token missing", "Google-Drive-Token fehlt", "Jeton Google Drive manquant", "Falta el token de Google Drive", "Google Drive トークンがありません", "缺少 Google Drive 令牌", "Отсутствует токен Google Drive"}},
        {"uploadErrorGoogleFileIdMissing",{"Google Drive dosya kimliği döndürmedi", "Google Drive file ID missing", "Google-Drive-Datei-ID fehlt", "ID de fichier Google Drive manquant", "Falta el ID del archivo de Google Drive", "Google Drive ファイル ID がありません", "缺少 Google Drive 文件 ID", "Отсутствует ID файла Google Drive"}},
        {"uploadApiKeyPlaceholder",{"%1 API key", "%1 API key", "%1 API key", "%1 API key", "%1 API key", "%1 API key", "%1 API key", "%1 API key"}},
        // Audited overrides for entries that previously fell back to English.
        {"tabPackages", {"Paketler", "Packages", "Pakete", "Paquets", "Paquetes", "パッケージ", "软件包", "Пакеты"}},
        {"mediaFolders", {"Medya klasörleri", "Media folders", "Medienordner", "Dossiers multimédias", "Carpetas multimedia", "メディアフォルダー", "媒体文件夹", "Папки мультимедиа"}},
        {"screenshotsLabel", {"Ekran görüntüleri:", "Screenshots:", "Screenshots:", "Captures d’écran :", "Capturas de pantalla:", "スクリーンショット：", "屏幕截图：", "Снимки экрана:"}},
        {"videosLabel", {"Videolar:", "Videos:", "Videos:", "Vidéos :", "Vídeos:", "動画：", "视频：", "Видео:"}},
        {"defaultFolderHint", {"Boş bırakılırsa varsayılan klasör kullanılır", "Leave empty to use the default folder", "Leer lassen, um den Standardordner zu verwenden", "Laissez vide pour utiliser le dossier par défaut", "Déjalo vacío para usar la carpeta predeterminada", "空欄の場合は既定のフォルダーを使用します", "留空则使用默认文件夹", "Оставьте пустым, чтобы использовать папку по умолчанию"}},
        {"componentStatus", {"Bileşen durumu", "Component status", "Komponentenstatus", "État des composants", "Estado de los componentes", "コンポーネントの状態", "组件状态", "Состояние компонентов"}},
        {"packageInstalled", {"Kurulu", "Installed", "Installiert", "Installé", "Instalado", "インストール済み", "已安装", "Установлено"}},
        {"packageMissing", {"Eksik", "Missing", "Fehlt", "Manquant", "Falta", "未インストール", "缺失", "Отсутствует"}},
        {"packageMissingDownloadable", {"Eksik - indirilebilir", "Missing - downloadable", "Fehlt - herunterladbar", "Manquant - téléchargeable", "Falta - descargable", "未インストール - ダウンロード可能", "缺失 - 可下载", "Отсутствует - можно скачать"}},
        {"packageDelete", {"Sil", "Delete", "Löschen", "Supprimer", "Eliminar", "削除", "删除", "Удалить"}},
        {"packageDownload", {"İndir", "Download", "Herunterladen", "Télécharger", "Descargar", "ダウンロード", "下载", "Скачать"}},
        {"packageDownloading", {"İndiriliyor...", "Downloading...", "Wird heruntergeladen...", "Téléchargement...", "Descargando...", "ダウンロード中...", "正在下载...", "Загрузка..."}},
        {"packageChecksumUnavailable", {"İndirilen paket doğrulanamadı: SHA-256 özeti yayınlanmamış. Doğrulanmamış dosyalar kurulmaz.", "The download could not be verified: no SHA-256 checksum is published for it. Unverified files are not installed.", "Der Download konnte nicht geprüft werden: Es ist keine SHA-256-Prüfsumme veröffentlicht. Ungeprüfte Dateien werden nicht installiert.", "Le téléchargement n'a pas pu être vérifié : aucune somme SHA-256 n'est publiée. Les fichiers non vérifiés ne sont pas installés.", "No se pudo verificar la descarga: no hay una suma SHA-256 publicada. Los archivos sin verificar no se instalan.", "ダウンロードを検証できませんでした: SHA-256 チェックサムが公開されていません。検証されていないファイルはインストールされません。", "无法验证下载内容：未发布 SHA-256 校验和。未经验证的文件不会被安装。", "Не удалось проверить загрузку: контрольная сумма SHA-256 не опубликована. Непроверенные файлы не устанавливаются."}},
        {"packageChecksumMismatch", {"İndirilen paketin SHA-256 özeti eşleşmedi; dosya silindi ve kurulmadı.", "The downloaded file failed its SHA-256 check; it was discarded and not installed.", "Die heruntergeladene Datei hat die SHA-256-Prüfung nicht bestanden; sie wurde verworfen und nicht installiert.", "Le fichier téléchargé a échoué à la vérification SHA-256 ; il a été supprimé et n'a pas été installé.", "El archivo descargado no superó la comprobación SHA-256; se descartó y no se instaló.", "ダウンロードしたファイルが SHA-256 検証に失敗したため、破棄されインストールされませんでした。", "下载的文件未通过 SHA-256 校验，已被丢弃且未安装。", "Загруженный файл не прошёл проверку SHA-256; он удалён и не установлен."}},
        {"ocrLanguagePacks", {"OCR dil paketleri", "OCR language packs", "OCR-Sprachpakete", "Modules linguistiques OCR", "Paquetes de idioma OCR", "OCR 言語パック", "OCR 语言包", "Языковые пакеты OCR"}},
        {"ocrPackagesHint", {"Eksik dilleri buradan indirebilir, kullanmadıklarını silebilirsin.", "Download missing languages here, or remove the ones you do not use.", "Laden Sie hier fehlende Sprachen herunter oder entfernen Sie nicht benötigte.", "Téléchargez ici les langues manquantes ou supprimez celles inutilisées.", "Descarga aquí los idiomas que faltan o elimina los que no uses.", "不足している言語をここからダウンロードし、不要な言語を削除できます。", "可在此下载缺少的语言，或删除不使用的语言。", "Здесь можно скачать недостающие языки или удалить ненужные."}},
        {"downloadEssentials", {"Temel paketleri indir", "Download essentials", "Grundpakete herunterladen", "Télécharger les essentiels", "Descargar esenciales", "基本パックをダウンロード", "下载基础包", "Скачать основные пакеты"}},
        {"deleteSelected", {"Seçili paketleri sil", "Delete selected", "Ausgewählte löschen", "Supprimer la sélection", "Eliminar seleccionados", "選択項目を削除", "删除所选项", "Удалить выбранные"}},
        {"ocrPackageInstallHint", {"Paketler tesseract/tessdata klasörüne yazılır. Kurulum klasörü korumalıysa yönetici izni gerekebilir.", "Packages are written to the tesseract/tessdata folder. Administrator permission may be required if the install folder is protected.", "Pakete werden in den Ordner tesseract/tessdata geschrieben. Bei einem geschützten Installationsordner können Administratorrechte erforderlich sein.", "Les modules sont écrits dans le dossier tesseract/tessdata. Des droits administrateur peuvent être requis si le dossier d’installation est protégé.", "Los paquetes se guardan en la carpeta tesseract/tessdata. Puede requerirse permiso de administrador si la carpeta de instalación está protegida.", "パックは tesseract/tessdata フォルダーに保存されます。インストール先が保護されている場合は管理者権限が必要です。", "语言包将写入 tesseract/tessdata 文件夹。如果安装文件夹受保护，可能需要管理员权限。", "Пакеты записываются в папку tesseract/tessdata. Если папка установки защищена, могут потребоваться права администратора."}},
        {"ocrEngineMissingHint", {"Tesseract OCR motoru eksik. Dil paketleri indirilebilir ama OCR için motorun da kurulu olması gerekir.", "The Tesseract OCR engine is missing. Language packs can be downloaded, but the engine must also be installed for OCR.", "Die Tesseract-OCR-Engine fehlt. Sprachpakete können geladen werden, für OCR muss jedoch auch die Engine installiert sein.", "Le moteur OCR Tesseract est absent. Les modules linguistiques peuvent être téléchargés, mais le moteur doit aussi être installé.", "Falta el motor OCR Tesseract. Se pueden descargar idiomas, pero el motor también debe estar instalado.", "Tesseract OCR エンジンがありません。言語パックはダウンロードできますが、OCR にはエンジンも必要です。", "缺少 Tesseract OCR 引擎。可以下载语言包，但 OCR 还需要安装引擎。", "Отсутствует движок Tesseract OCR. Языковые пакеты можно скачать, но для OCR также нужен сам движок."}},
        {"formatLabel", {"Format:", "Format:", "Format:", "Format :", "Formato:", "形式：", "格式：", "Формат:"}},
        {"instantCopyAfterSelection", {"Alan seçimi bitince hemen kopyala", "Copy immediately after selecting a region", "Nach der Bereichsauswahl sofort kopieren", "Copier immédiatement après la sélection d’une zone", "Copiar inmediatamente después de seleccionar una región", "範囲選択後すぐにコピー", "选择区域后立即复制", "Копировать сразу после выбора области"}},
        {"gifSizeSmallest", {"En düşük boyut", "Smallest size", "Kleinste Größe", "Taille minimale", "Tamaño mínimo", "最小サイズ", "最小尺寸", "Минимальный размер"}},
        {"gifSizeBalanced", {"Dengeli", "Balanced", "Ausgewogen", "Équilibré", "Equilibrado", "バランス", "均衡", "Сбалансированный"}},
        {"gifSizeBest", {"En iyi kalite", "Best quality", "Beste Qualität", "Meilleure qualité", "Mejor calidad", "最高品質", "最佳质量", "Лучшее качество"}},
        {"gifSizePresetLabel", {"GIF boyut ön ayarı:", "GIF size preset:", "GIF-Größenvorgabe:", "Préréglage de taille GIF :", "Preajuste de tamaño GIF:", "GIF サイズプリセット：", "GIF 尺寸预设：", "Предустановка размера GIF:"}},
        {"recordingStartDelayLabel", {"Başlangıç gecikmesi:", "Start delay:", "Startverzögerung:", "Délai de démarrage :", "Retraso de inicio:", "開始遅延：", "开始延迟：", "Задержка запуска:"}},
        {"desktopVolumeLabel", {"Masaüstü ses düzeyi", "Desktop volume", "Desktop-Lautstärke", "Volume du bureau", "Volumen del escritorio", "デスクトップ音量", "桌面音量", "Громкость системного звука"}},
        {"microphoneVolumeLabel", {"Mikrofon ses düzeyi", "Microphone volume", "Mikrofonlautstärke", "Volume du microphone", "Volumen del micrófono", "マイク音量", "麦克风音量", "Громкость микрофона"}},
        {"defaultAudioDevice", {"Varsayılan", "Default", "Standard", "Par défaut", "Predeterminado", "既定", "默认", "По умолчанию"}},
        {"visualSearchPrivacy", {"Gizlilik: Görsel arama, seçilen görüntüyü geçici ve herkese açık bir görsel barındırma hizmetine yükler; ardından bu bağlantıyı Google Lens veya Yandex Görseller’e gönderir.", "Privacy: visual search uploads the selected image to a temporary public image host, then sends that public URL to Google Lens or Yandex Images.", "Datenschutz: Die visuelle Suche lädt das ausgewählte Bild auf einen temporären öffentlichen Bildhoster hoch und sendet die öffentliche URL an Google Lens oder Yandex Bilder.", "Confidentialité : la recherche visuelle envoie l’image sélectionnée vers un hébergeur public temporaire, puis transmet son URL à Google Lens ou Yandex Images.", "Privacidad: la búsqueda visual sube la imagen seleccionada a un alojamiento público temporal y envía esa URL a Google Lens o Yandex Imágenes.", "プライバシー：画像検索では、選択した画像を一時的な公開画像ホストにアップロードし、その URL を Google Lens または Yandex Images に送信します。", "隐私：视觉搜索会将所选图片上传到临时公开图片托管服务，然后把公开链接发送给 Google Lens 或 Yandex 图片。", "Конфиденциальность: поиск по изображению загружает выбранную картинку на временный общедоступный хостинг, а затем передаёт ссылку Google Lens или Яндекс Картинкам."}},
        {"visualSearchFailed", {"Görsel arama başarısız", "Visual search failed", "Visuelle Suche fehlgeschlagen", "Échec de la recherche visuelle", "Error en la búsqueda visual", "画像検索に失敗しました", "视觉搜索失败", "Не удалось выполнить поиск по изображению"}},
        {"visualSearchTempCreateError", {"Görsel arama için geçici dosya oluşturulamadı.", "Could not create the temporary image for visual search.", "Das temporäre Bild für die visuelle Suche konnte nicht erstellt werden.", "Impossible de créer l’image temporaire pour la recherche visuelle.", "No se pudo crear la imagen temporal para la búsqueda visual.", "画像検索用の一時画像を作成できませんでした。", "无法创建视觉搜索所需的临时图片。", "Не удалось создать временное изображение для поиска."}},
        {"visualSearchTempPrepareError", {"Seçilen görsel arama için hazırlanamadı.", "Could not prepare the selected image for visual search.", "Das ausgewählte Bild konnte nicht für die visuelle Suche vorbereitet werden.", "Impossible de préparer l’image sélectionnée pour la recherche visuelle.", "No se pudo preparar la imagen seleccionada para la búsqueda visual.", "選択した画像を画像検索用に準備できませんでした。", "无法为视觉搜索准备所选图片。", "Не удалось подготовить выбранное изображение для поиска."}},
        {"visualSearchUploaderCreateError", {"Geçici görsel yükleyici oluşturulamadı.", "Could not create the temporary image uploader.", "Der temporäre Bild-Uploader konnte nicht erstellt werden.", "Impossible de créer le service d’envoi temporaire.", "No se pudo crear el cargador temporal de imágenes.", "一時画像アップローダーを作成できませんでした。", "无法创建临时图片上传器。", "Не удалось создать временный загрузчик изображения."}},
        {"visualSearchUploadUnavailable", {"Geçici görsel yükleme hizmetlerinin hiçbirine ulaşılamadı. Lütfen bağlantınızı kontrol edip tekrar deneyin.\n\n%1", "None of the temporary image services could be reached. Check your connection and try again.\n\n%1", "Keiner der temporären Bilddienste war erreichbar. Prüfen Sie Ihre Verbindung und versuchen Sie es erneut.\n\n%1", "Aucun service d’images temporaire n’est accessible. Vérifiez votre connexion et réessayez.\n\n%1", "No se pudo acceder a ningún servicio temporal de imágenes. Comprueba tu conexión e inténtalo de nuevo.\n\n%1", "一時画像サービスに接続できませんでした。接続を確認してもう一度お試しください。\n\n%1", "无法连接任何临时图片服务。请检查网络连接后重试。\n\n%1", "Не удалось подключиться ни к одному временному сервису изображений. Проверьте соединение и повторите попытку.\n\n%1"}},
        {"toolFontSize", {"Yazı Boyutu", "Font Size", "Schriftgröße", "Taille de police", "Tamaño de fuente", "フォントサイズ", "字体大小", "Размер шрифта"}},
        {"ocrLanguagePackMissing", {"Dil paketi yüklü değil", "Language pack is not installed", "Sprachpaket ist nicht installiert", "Le module linguistique n’est pas installé", "El paquete de idioma no está instalado", "言語パックがインストールされていません", "未安装语言包", "Языковой пакет не установлен"}},
        {"uploadAuthHelpYandex", {"Yandex Disk için okuma ve yazma izinli bir OAuth access_token gerekir. Tokeni buraya yapıştırın.", "Yandex Disk needs an OAuth access_token with read and write permissions. Paste the token here.", "Yandex Disk benötigt ein OAuth-access_token mit Lese- und Schreibrechten. Fügen Sie das Token hier ein.", "Yandex Disk nécessite un access_token OAuth avec des droits de lecture et d’écriture. Collez le jeton ici.", "Yandex Disk necesita un access_token OAuth con permisos de lectura y escritura. Pega el token aquí.", "Yandex Disk には読み取り・書き込み権限を持つ OAuth access_token が必要です。ここに貼り付けてください。", "Yandex Disk 需要具有读写权限的 OAuth access_token。请在此粘贴令牌。", "Для Yandex Disk нужен OAuth access_token с правами чтения и записи. Вставьте токен сюда."}},
        {"uploadAuthHelpGoogleDrive", {"Google Drive için OAuth Playground access_token gerekir. Tokeni buraya yapıştırın.", "Google Drive needs an OAuth Playground access_token. Paste the token here.", "Google Drive benötigt ein OAuth-Playground-access_token. Fügen Sie das Token hier ein.", "Google Drive nécessite un access_token OAuth Playground. Collez le jeton ici.", "Google Drive necesita un access_token de OAuth Playground. Pega el token aquí.", "Google Drive には OAuth Playground の access_token が必要です。ここに貼り付けてください。", "Google Drive 需要 OAuth Playground access_token。请在此粘贴令牌。", "Для Google Drive нужен access_token из OAuth Playground. Вставьте токен сюда."}},
        {"uploadAuthHelpApiKey", {"%1 için bir API anahtarı gerekir. Anahtarı hizmetin hesap sayfasından alıp buraya yapıştırın.", "%1 needs an API key. Get it from the service account page and paste it here.", "%1 benötigt einen API-Schlüssel. Kopieren Sie ihn aus der Kontoseite des Dienstes hierher.", "%1 nécessite une clé API. Copiez-la depuis la page du compte du service.", "%1 necesita una clave API. Cópiala desde la página de cuenta del servicio.", "%1 には API キーが必要です。サービスのアカウントページから取得して貼り付けてください。", "%1 需要 API 密钥。请从服务账户页面获取并粘贴到此处。", "Для %1 нужен ключ API. Скопируйте его со страницы аккаунта сервиса."}},
        {"uploadErrorYandexScopeMissing", {"Yandex Disk izni eksik (HTTP 403). OAuth uygulamasında okuma ve yazma izinlerini açıp yeni token alın.", "Yandex Disk permission is missing (HTTP 403). Enable read and write permissions and issue a new token.", "Yandex-Disk-Berechtigung fehlt (HTTP 403). Aktivieren Sie Lese- und Schreibrechte und erstellen Sie ein neues Token.", "Autorisation Yandex Disk manquante (HTTP 403). Activez la lecture et l’écriture puis créez un nouveau jeton.", "Falta el permiso de Yandex Disk (HTTP 403). Activa lectura y escritura y genera un token nuevo.", "Yandex Disk の権限がありません (HTTP 403)。読み取り・書き込み権限を有効にして新しいトークンを発行してください。", "缺少 Yandex Disk 权限 (HTTP 403)。请启用读写权限并生成新令牌。", "Нет разрешений Yandex Disk (HTTP 403). Включите чтение и запись и выпустите новый токен."}},
        {"uploadErrorGoogleAuthFailed", {"Google Drive yetkilendirmesi başarısız (HTTP 401). Yeni bir access_token alın.", "Google Drive authorization failed (HTTP 401). Issue a new access_token.", "Google-Drive-Autorisierung fehlgeschlagen (HTTP 401). Erstellen Sie ein neues access_token.", "Échec de l’autorisation Google Drive (HTTP 401). Créez un nouvel access_token.", "Falló la autorización de Google Drive (HTTP 401). Genera un access_token nuevo.", "Google Drive の認証に失敗しました (HTTP 401)。新しい access_token を発行してください。", "Google Drive 授权失败 (HTTP 401)。请生成新的 access_token。", "Ошибка авторизации Google Drive (HTTP 401). Выпустите новый access_token."}},
        {"uploadErrorApiKeyMissing", {"%1 API anahtarı eksik", "%1 API key missing", "API-Schlüssel für %1 fehlt", "Clé API %1 manquante", "Falta la clave API de %1", "%1 の API キーがありません", "缺少 %1 API 密钥", "Отсутствует ключ API для %1"}},
        {"copy",           {"Kopyala", "Copy", "Kopieren", "Copier", "Copiar", "コピー", "复制", "Копировать"}},

        // ─── Localized settings, setup and OCR messages (formerly TR/EN-only or English-only) ───
        {"tipFilenamePattern", {"Dosya adında tarih/saat ve pencere başlığı değişkenlerini kullanır.", "Use date/time and window title variables in saved filenames.", "Verwendet Datum/Uhrzeit und Fenstertitel als Variablen im Dateinamen.", "Utilise la date, l’heure et le titre de la fenêtre dans le nom du fichier.", "Usa variables de fecha/hora y título de ventana en el nombre del archivo.", "保存するファイル名に日時やウィンドウタイトルの変数を使用します。", "在保存的文件名中使用日期/时间和窗口标题变量。", "Использует в имени файла переменные даты, времени и заголовка окна."}},
        {"tipNotifyCopy", {"Görsel panoya kopyalanınca bildirim gösterir.", "Show a notification when an image is copied.", "Zeigt eine Benachrichtigung, wenn ein Bild kopiert wird.", "Affiche une notification lorsqu’une image est copiée.", "Muestra una notificación al copiar una imagen.", "画像をコピーしたときに通知を表示します。", "复制图片时显示通知。", "Показывает уведомление при копировании изображения."}},
        {"tipNotifySave", {"Görsel dosyaya kaydedilince klasörü açabilen bir bildirim gösterir.", "Show a folder-opening notification when an image is saved.", "Zeigt beim Speichern eines Bildes eine Benachrichtigung, über die sich der Ordner öffnen lässt.", "Affiche une notification permettant d’ouvrir le dossier lorsqu’une image est enregistrée.", "Muestra una notificación que permite abrir la carpeta al guardar una imagen.", "画像を保存したときに、フォルダーを開ける通知を表示します。", "保存图片时显示可打开文件夹的通知。", "Показывает уведомление с возможностью открыть папку после сохранения изображения."}},
        {"tipNotifyGif", {"GIF kaydı bitince klasörü açabilen bir bildirim gösterir.", "Show a folder-opening notification when a GIF recording finishes.", "Zeigt nach einer GIF-Aufnahme eine Benachrichtigung, über die sich der Ordner öffnen lässt.", "Affiche une notification permettant d’ouvrir le dossier à la fin d’un enregistrement GIF.", "Muestra una notificación que permite abrir la carpeta al terminar una grabación GIF.", "GIF 録画が終わったときに、フォルダーを開ける通知を表示します。", "GIF 录制完成时显示可打开文件夹的通知。", "Показывает уведомление с возможностью открыть папку после записи GIF."}},
        {"tipNotifyVideo", {"Video kaydı bitince klasörü açabilen bir bildirim gösterir.", "Show a folder-opening notification when a video recording finishes.", "Zeigt nach einer Videoaufnahme eine Benachrichtigung, über die sich der Ordner öffnen lässt.", "Affiche une notification permettant d’ouvrir le dossier à la fin d’un enregistrement vidéo.", "Muestra una notificación que permite abrir la carpeta al terminar una grabación de video.", "動画の録画が終わったときに、フォルダーを開ける通知を表示します。", "视频录制完成时显示可打开文件夹的通知。", "Показывает уведомление с возможностью открыть папку после записи видео."}},
        {"tipNotificationOpenFolder", {"Kaydedilen dosyaların bildirimlerinde Klasörü Aç eylemini gösterir.", "Show the Open Folder action in notifications for saved files.", "Zeigt in Benachrichtigungen zu gespeicherten Dateien die Aktion „Ordner öffnen“.", "Affiche l’action « Ouvrir le dossier » dans les notifications des fichiers enregistrés.", "Muestra la acción Abrir carpeta en las notificaciones de archivos guardados.", "保存したファイルの通知に「フォルダーを開く」操作を表示します。", "在已保存文件的通知中显示“打开文件夹”操作。", "Показывает действие «Открыть папку» в уведомлениях о сохранённых файлах."}},
        {"openLinuxDependencySetup", {"Linux bağımlılık kurulumunu aç", "Open Linux dependency setup", "Linux-Abhängigkeiten einrichten", "Ouvrir l’installation des dépendances Linux", "Abrir la instalación de dependencias de Linux", "Linux 依存関係のセットアップを開く", "打开 Linux 依赖项安装", "Открыть установку зависимостей Linux"}},
        {"tipFileFormat", {"Kaydedilen ekran görüntülerinin dosya biçimi.", "File format for saved screenshots.", "Dateiformat für gespeicherte Bildschirmfotos.", "Format des captures d’écran enregistrées.", "Formato de archivo de las capturas guardadas.", "保存するスクリーンショットのファイル形式。", "保存截图时使用的文件格式。", "Формат файла для сохраняемых скриншотов."}},
        {"tipJpegQuality", {"Yalnızca JPEG için kalite ayarı.", "Quality setting for JPEG only.", "Qualitätseinstellung, nur für JPEG.", "Réglage de qualité, uniquement pour JPEG.", "Ajuste de calidad, solo para JPEG.", "JPEG のみの品質設定です。", "仅适用于 JPEG 的质量设置。", "Настройка качества только для JPEG."}},
        {"tipCaptureDelay", {"Kısayola bastıktan sonra yakalama ekranının açılması için bekleme süresi.", "Delay before opening the capture overlay after the shortcut is pressed.", "Wartezeit nach dem Tastenkürzel, bis die Aufnahmeansicht geöffnet wird.", "Délai avant l’ouverture de l’écran de capture après le raccourci.", "Espera antes de abrir la pantalla de captura tras pulsar el atajo.", "ショートカットを押してからキャプチャ画面を開くまでの待ち時間。", "按下快捷键后打开截图界面前的等待时间。", "Задержка перед открытием экрана захвата после нажатия сочетания клавиш."}},
        {"tipCloseAfterCopy", {"Kopyaladıktan sonra seçim ekranını otomatik kapatır.", "Automatically closes the capture overlay after copying.", "Schließt die Aufnahmeansicht nach dem Kopieren automatisch.", "Ferme automatiquement l’écran de capture après la copie.", "Cierra automáticamente la pantalla de captura después de copiar.", "コピー後にキャプチャ画面を自動で閉じます。", "复制后自动关闭截图界面。", "Автоматически закрывает экран захвата после копирования."}},
        {"tipInstantCopyAfterSelection", {"Varsayılan olarak kapalıdır. Açıksa alan seçimini bitirdiğiniz anda görüntü panoya kopyalanır.", "Off by default. When enabled, the image is copied as soon as you finish selecting a region.", "Standardmäßig aus. Wenn aktiviert, wird das Bild sofort nach der Bereichsauswahl kopiert.", "Désactivé par défaut. Si activé, l’image est copiée dès la fin de la sélection.", "Desactivado por defecto. Si se activa, la imagen se copia en cuanto terminas de seleccionar una región.", "既定ではオフです。有効にすると、範囲の選択を終えた時点で画像がコピーされます。", "默认关闭。启用后，完成区域选择时立即复制图片。", "По умолчанию выключено. Если включено, изображение копируется сразу после выбора области."}},
        {"tipDarkMode", {"Ayarlar ve yardımcı pencereler için koyu tema.", "Dark theme for settings and helper windows.", "Dunkles Design für Einstellungen und Hilfsfenster.", "Thème sombre pour les paramètres et les fenêtres auxiliaires.", "Tema oscuro para la configuración y las ventanas auxiliares.", "設定と補助ウィンドウのダークテーマ。", "设置窗口和辅助窗口的深色主题。", "Тёмная тема для настроек и вспомогательных окон."}},
        {"tipBgOpacity", {"Seçilmeyen ekran alanının karartma miktarı.", "Dim amount for the non-selected screen area.", "Abdunklung des nicht ausgewählten Bildschirmbereichs.", "Assombrissement de la zone non sélectionnée.", "Oscurecimiento del área de pantalla no seleccionada.", "選択されていない画面領域の暗さ。", "未选中屏幕区域的变暗程度。", "Степень затемнения невыделенной области экрана."}},
        {"tipCrosshair", {"Alan seçmeden önce imleç kılavuz çizgisinin stili.", "Cursor guide line style before selecting an area.", "Stil der Cursor-Hilfslinien vor der Bereichsauswahl.", "Style des lignes de guidage du curseur avant la sélection.", "Estilo de las guías del cursor antes de seleccionar un área.", "範囲選択前のカーソルガイド線のスタイル。", "选择区域前光标辅助线的样式。", "Стиль направляющих линий курсора до выбора области."}},
        {"tipGifFps", {"GIF için saniyedeki kare sayısı. Yüksek değerler daha akıcı ama daha büyük dosyalar üretir.", "Frames per second for GIF. Higher values are smoother but create larger files.", "Bilder pro Sekunde für GIFs. Höhere Werte sind flüssiger, erzeugen aber größere Dateien.", "Images par seconde du GIF. Une valeur élevée est plus fluide mais produit des fichiers plus lourds.", "Fotogramas por segundo del GIF. Valores más altos son más fluidos pero generan archivos más grandes.", "GIF の毎秒フレーム数。値が高いほど滑らかになりますが、ファイルは大きくなります。", "GIF 每秒帧数。数值越高越流畅，但文件越大。", "Кадров в секунду для GIF. Чем выше значение, тем плавнее анимация, но больше файл."}},
        {"tipGifMaxSeconds", {"GIF kaydının otomatik olarak duracağı süre. 0 sınırsız demektir.", "Time limit for GIF recording. 0 means unlimited.", "Zeitlimit für GIF-Aufnahmen. 0 bedeutet unbegrenzt.", "Durée maximale d’un enregistrement GIF. 0 = illimité.", "Límite de tiempo de la grabación GIF. 0 significa ilimitado.", "GIF 録画の制限時間。0 は無制限です。", "GIF 录制的时长上限。0 表示不限制。", "Ограничение длительности записи GIF. 0 — без ограничений."}},
        {"tipGifLoop", {"GIF'in kaç kez tekrar oynatılacağı.", "How many times the GIF should loop.", "Wie oft das GIF wiederholt wird.", "Nombre de répétitions du GIF.", "Cuántas veces se repite el GIF.", "GIF をループ再生する回数。", "GIF 循环播放的次数。", "Сколько раз повторяется GIF."}},
        {"tipGifSizePreset", {"GIF'in uzun kenar sınırını belirler. Düşük değerler dosya boyutunu ciddi ölçüde azaltır.", "Controls the GIF max side length. Lower values can greatly reduce file size.", "Legt die maximale Kantenlänge des GIFs fest. Niedrigere Werte verkleinern die Datei deutlich.", "Définit la longueur maximale du grand côté du GIF. Une valeur plus basse réduit nettement la taille du fichier.", "Define el lado máximo del GIF. Valores más bajos reducen mucho el tamaño del archivo.", "GIF の長辺の上限を設定します。値を下げるとファイルサイズを大きく減らせます。", "设置 GIF 长边的最大尺寸。数值越低，文件越小。", "Задаёт максимальную длину длинной стороны GIF. Меньшие значения сильно уменьшают размер файла."}},
        {"tipRecordingStartDelay", {"Alan seçildikten sonra GIF/video kaydı başlamadan önceki bekleme süresi.", "Delay after selecting the area before GIF/video recording starts.", "Wartezeit nach der Bereichsauswahl, bevor die GIF-/Videoaufnahme startet.", "Délai entre la sélection de la zone et le début de l’enregistrement GIF/vidéo.", "Espera entre la selección del área y el inicio de la grabación GIF/video.", "範囲を選択してから GIF/動画の録画が始まるまでの待ち時間。", "选择区域后开始录制 GIF/视频前的等待时间。", "Задержка между выбором области и началом записи GIF/видео."}},
        {"tipVideoFps", {"Video için saniyedeki kare sayısı.", "Frames per second for video recording.", "Bilder pro Sekunde für Videoaufnahmen.", "Images par seconde pour l’enregistrement vidéo.", "Fotogramas por segundo de la grabación de video.", "動画録画の毎秒フレーム数。", "视频录制的每秒帧数。", "Кадров в секунду для записи видео."}},
        {"tipVideoMaxSeconds", {"Video kaydının otomatik olarak duracağı süre. 0 sınırsız demektir.", "Time limit for video recording. 0 means unlimited.", "Zeitlimit für Videoaufnahmen. 0 bedeutet unbegrenzt.", "Durée maximale d’un enregistrement vidéo. 0 = illimité.", "Límite de tiempo de la grabación de video. 0 significa ilimitado.", "動画録画の制限時間。0 は無制限です。", "视频录制的时长上限。0 表示不限制。", "Ограничение длительности записи видео. 0 — без ограничений."}},
        {"tipVideoDesktopAudio", {"Video kaydına sistem/masaüstü sesini ekler.", "Include system/desktop audio in video recordings.", "Nimmt den System-/Desktopton im Video mit auf.", "Inclut le son du système dans les vidéos.", "Incluye el audio del sistema en las grabaciones de video.", "動画にシステム/デスクトップの音声を含めます。", "在视频中录制系统/桌面声音。", "Добавляет системный звук в запись видео."}},
        {"tipVideoDesktopVolume", {"Kaydedilen masaüstü sesinin düzeyi.", "Recorded desktop audio volume.", "Lautstärke des aufgenommenen Desktoptons.", "Volume du son système enregistré.", "Volumen del audio del sistema grabado.", "録音するデスクトップ音声の音量。", "录制的桌面声音音量。", "Громкость записываемого системного звука."}},
        {"tipVideoMicrophone", {"Video kaydına mikrofon sesini ekler.", "Include microphone audio in video recordings.", "Nimmt das Mikrofon im Video mit auf.", "Inclut le microphone dans les vidéos.", "Incluye el audio del micrófono en las grabaciones de video.", "動画にマイク音声を含めます。", "在视频中录制麦克风声音。", "Добавляет звук микрофона в запись видео."}},
        {"tipVideoMicrophoneDevice", {"Video kaydında kullanılacak mikrofon kaynağı.", "Microphone source used for video recording.", "Mikrofonquelle für Videoaufnahmen.", "Source du microphone pour l’enregistrement vidéo.", "Fuente de micrófono para la grabación de video.", "動画録画に使うマイク。", "视频录制使用的麦克风来源。", "Источник микрофона для записи видео."}},
        {"tipVideoMicrophoneVolume", {"Kaydedilen mikrofon sesinin düzeyi.", "Recorded microphone volume.", "Lautstärke des aufgenommenen Mikrofons.", "Volume du microphone enregistré.", "Volumen del micrófono grabado.", "録音するマイクの音量。", "录制的麦克风音量。", "Громкость записываемого микрофона."}},
        {"linuxPrintScreenGnome", {"PrintScreen kısayolunu GNOME ile ayarla", "Configure PrintScreen shortcut in GNOME", "PrintScreen-Kürzel in GNOME einrichten", "Configurer le raccourci Impr. écran dans GNOME", "Configurar el atajo Imprimir pantalla en GNOME", "GNOME で PrintScreen ショートカットを設定", "在 GNOME 中设置 PrintScreen 快捷键", "Настроить PrintScreen в GNOME"}},
        {"linuxPrintScreenKde", {"PrintScreen kısayolunu KDE ile yeniden ayarla", "Reconfigure PrintScreen shortcut in KDE", "PrintScreen-Kürzel in KDE neu einrichten", "Reconfigurer le raccourci Impr. écran dans KDE", "Volver a configurar el atajo Imprimir pantalla en KDE", "KDE で PrintScreen ショートカットを再設定", "在 KDE 中重新设置 PrintScreen 快捷键", "Перенастроить PrintScreen в KDE"}},
        {"linuxPrintScreenDesktop", {"PrintScreen kısayolunu masaüstüyle ayarla", "Configure PrintScreen with the desktop", "PrintScreen über die Arbeitsumgebung einrichten", "Configurer Impr. écran avec le bureau", "Configurar Imprimir pantalla con el escritorio", "デスクトップで PrintScreen を設定", "通过桌面环境设置 PrintScreen", "Настроить PrintScreen через окружение рабочего стола"}},
        {"directCaptureHotkeys", {"Doğrudan yakalama kısayolları", "Direct capture hotkeys", "Direkte Aufnahmekürzel", "Raccourcis de capture directe", "Atajos de captura directa", "ダイレクトキャプチャのショートカット", "直接截图快捷键", "Горячие клавиши прямого захвата"}},
        {"tipInstantCaptureHotkey", {"Boş bırakılırsa kapalı kalır. Alan seçimi bitince otomatik kopyalar.", "Leave empty to disable. Copies automatically when region selection finishes.", "Leer lassen zum Deaktivieren. Kopiert automatisch, sobald der Bereich ausgewählt ist.", "Laisser vide pour désactiver. Copie automatiquement à la fin de la sélection.", "Déjalo vacío para desactivarlo. Copia automáticamente al terminar la selección.", "空欄にすると無効になります。範囲選択が終わると自動でコピーします。", "留空则禁用。完成区域选择后自动复制。", "Оставьте пустым, чтобы отключить. Копирует автоматически после выбора области."}},
        {"tipGifCaptureHotkey", {"Boş bırakılırsa kapalı kalır. GIF alan seçimini doğrudan açar.", "Leave empty to disable. Opens GIF area selection directly.", "Leer lassen zum Deaktivieren. Öffnet direkt die GIF-Bereichsauswahl.", "Laisser vide pour désactiver. Ouvre directement la sélection de zone GIF.", "Déjalo vacío para desactivarlo. Abre directamente la selección de área para GIF.", "空欄にすると無効になります。GIF の範囲選択を直接開きます。", "留空则禁用。直接打开 GIF 区域选择。", "Оставьте пустым, чтобы отключить. Сразу открывает выбор области для GIF."}},
        {"tipVideoCaptureHotkey", {"Boş bırakılırsa kapalı kalır. Video alan seçimini doğrudan açar.", "Leave empty to disable. Opens video area selection directly.", "Leer lassen zum Deaktivieren. Öffnet direkt die Video-Bereichsauswahl.", "Laisser vide pour désactiver. Ouvre directement la sélection de zone vidéo.", "Déjalo vacío para desactivarlo. Abre directamente la selección de área para video.", "空欄にすると無効になります。動画の範囲選択を直接開きます。", "留空则禁用。直接打开视频区域选择。", "Оставьте пустым, чтобы отключить. Сразу открывает выбор области для видео."}},
        {"tipWindowCaptureHotkey", {"Boş bırakılırsa kapalı kalır. Fareyle pencere seçme modunu açar.", "Leave empty to disable. Opens window selection mode.", "Leer lassen zum Deaktivieren. Öffnet den Fensterauswahlmodus.", "Laisser vide pour désactiver. Ouvre le mode de sélection de fenêtre.", "Déjalo vacío para desactivarlo. Abre el modo de selección de ventana.", "空欄にすると無効になります。ウィンドウ選択モードを開きます。", "留空则禁用。打开窗口选择模式。", "Оставьте пустым, чтобы отключить. Открывает режим выбора окна."}},
        {"hotkeyWindowLabel", {"Pencere:", "Window:", "Fenster:", "Fenêtre :", "Ventana:", "ウィンドウ:", "窗口：", "Окно:"}},
        {"hotkeyInstantRegionLabel", {"Anında bölge:", "Instant region:", "Sofortbereich:", "Zone instantanée :", "Región instantánea:", "即時範囲:", "即时区域：", "Мгновенная область:"}},
        {"hotkeyVideoLabel", {"Video:", "Video:", "Video:", "Vidéo :", "Video:", "動画:", "视频：", "Видео:"}},
        {"overlayShortcutsGroup", {"Ekran görüntüsü ekranı kısayolları", "Screenshot screen shortcuts", "Kürzel im Aufnahmebildschirm", "Raccourcis de l’écran de capture", "Atajos de la pantalla de captura", "スクリーンショット画面のショートカット", "截图界面快捷键", "Горячие клавиши экрана скриншота"}},
        {"tipOverlayShortcut", {"Bu kısayol yalnızca ekran görüntüsü seçme/düzenleme ekranında çalışır.", "This shortcut works only on the screenshot selection/annotation screen.", "Dieses Kürzel funktioniert nur im Auswahl- und Bearbeitungsbildschirm.", "Ce raccourci ne fonctionne que sur l’écran de sélection/annotation.", "Este atajo solo funciona en la pantalla de selección/anotación.", "このショートカットはスクリーンショットの選択・編集画面でのみ有効です。", "此快捷键仅在截图选择/标注界面中有效。", "Это сочетание работает только на экране выбора и редактирования скриншота."}},
        {"packageFolderCreateFailed", {"Paket klasörü oluşturulamadı.", "Could not create the package folder.", "Der Paketordner konnte nicht erstellt werden.", "Impossible de créer le dossier des paquets.", "No se pudo crear la carpeta de paquetes.", "パッケージフォルダーを作成できませんでした。", "无法创建包文件夹。", "Не удалось создать папку пакетов."}},
        {"unknownNetworkError", {"Bilinmeyen ağ hatası", "Unknown network error", "Unbekannter Netzwerkfehler", "Erreur réseau inconnue", "Error de red desconocido", "不明なネットワークエラー", "未知网络错误", "Неизвестная сетевая ошибка"}},
        {"ocrPackageDownloadFailed", {"OCR paketi indirilemedi: ", "Could not download OCR package: ", "OCR-Paket konnte nicht heruntergeladen werden: ", "Impossible de télécharger le paquet OCR : ", "No se pudo descargar el paquete OCR: ", "OCR パッケージをダウンロードできませんでした: ", "无法下载 OCR 包：", "Не удалось скачать пакет OCR: "}},
        {"ocrPackageWriteFailed", {"OCR paketi yazılamadı.", "Could not write the OCR package.", "Das OCR-Paket konnte nicht gespeichert werden.", "Impossible d’écrire le paquet OCR.", "No se pudo escribir el paquete OCR.", "OCR パッケージを書き込めませんでした。", "无法写入 OCR 包。", "Не удалось записать пакет OCR."}},
        {"ocrPackageMoveFailed", {"OCR paketi yerine taşınamadı.", "Could not move the OCR package into place.", "Das OCR-Paket konnte nicht an seinen Zielort verschoben werden.", "Impossible de mettre en place le paquet OCR.", "No se pudo mover el paquete OCR a su ubicación.", "OCR パッケージを所定の場所に移動できませんでした。", "无法将 OCR 包移动到目标位置。", "Не удалось переместить пакет OCR на место."}},
        {"ocrPackageDeleteFailed", {"OCR paketi silinemedi.", "Could not delete the OCR package.", "Das OCR-Paket konnte nicht gelöscht werden.", "Impossible de supprimer le paquet OCR.", "No se pudo eliminar el paquete OCR.", "OCR パッケージを削除できませんでした。", "无法删除 OCR 包。", "Не удалось удалить пакет OCR."}},
        {"deleteOcrComponentTitle", {"OCR bileşenini sil", "Delete OCR component", "OCR-Komponente löschen", "Supprimer le composant OCR", "Eliminar componente OCR", "OCR コンポーネントを削除", "删除 OCR 组件", "Удалить компонент OCR"}},
        {"deleteOcrComponentConfirm", {"Tesseract OCR ve tüm OCR dil paketleri silinsin mi?", "Delete Tesseract OCR and all OCR language packs?", "Tesseract OCR und alle OCR-Sprachpakete löschen?", "Supprimer Tesseract OCR et tous les modules linguistiques OCR ?", "¿Eliminar Tesseract OCR y todos los paquetes de idioma OCR?", "Tesseract OCR とすべての OCR 言語パックを削除しますか？", "删除 Tesseract OCR 及所有 OCR 语言包？", "Удалить Tesseract OCR и все языковые пакеты OCR?"}},
        {"ocrComponentDeleteFailed", {"Tesseract OCR bileşeni silinemedi.", "Could not delete the Tesseract OCR component.", "Die Tesseract-OCR-Komponente konnte nicht gelöscht werden.", "Impossible de supprimer le composant Tesseract OCR.", "No se pudo eliminar el componente Tesseract OCR.", "Tesseract OCR コンポーネントを削除できませんでした。", "无法删除 Tesseract OCR 组件。", "Не удалось удалить компонент Tesseract OCR."}},
        {"ocrComponentName", {"OCR bileşeni", "OCR component", "OCR-Komponente", "Composant OCR", "Componente OCR", "OCR コンポーネント", "OCR 组件", "Компонент OCR"}},
        {"linuxComponentsViaPackageManager", {"Linux'ta bileşenler sistem paket yöneticisinden veya ileride eklenecek Linux paketinden kurulur.", "On Linux, components should be installed through the system package manager or a future Linux package.", "Unter Linux werden Komponenten über den Paketmanager des Systems oder ein künftiges Linux-Paket installiert.", "Sous Linux, les composants s’installent via le gestionnaire de paquets du système ou un futur paquet Linux.", "En Linux, los componentes se instalan con el gestor de paquetes del sistema o con un futuro paquete para Linux.", "Linux では、コンポーネントはシステムのパッケージマネージャーまたは今後提供される Linux パッケージでインストールします。", "在 Linux 上，组件应通过系统包管理器或今后提供的 Linux 软件包安装。", "В Linux компоненты устанавливаются через системный менеджер пакетов или будущий пакет для Linux."}},
        {"releasePackageSearching", {"sürüm paketi aranıyor...", "looking for release package...", "Release-Paket wird gesucht...", "recherche du paquet de version...", "buscando el paquete de la versión...", "リリースパッケージを検索中...", "正在查找发布包...", "поиск пакета выпуска..."}},
        {"releaseInfoFailed", {"Sürüm bilgisi alınamadı: ", "Could not read release info: ", "Release-Informationen konnten nicht gelesen werden: ", "Impossible de lire les informations de version : ", "No se pudo leer la información de la versión: ", "リリース情報を取得できませんでした: ", "无法读取发布信息：", "Не удалось получить сведения о выпуске: "}},
        {"portableReleaseNotFound", {"Bu cihaz için taşınabilir sürüm paketi bulunamadı.", "No portable release package was found for this device.", "Für dieses Gerät wurde kein portables Release-Paket gefunden.", "Aucun paquet portable n’a été trouvé pour cet appareil.", "No se encontró un paquete portable para este dispositivo.", "このデバイス向けのポータブル版パッケージが見つかりませんでした。", "未找到适用于此设备的便携版发布包。", "Для этого устройства не найден портативный пакет."}},
        {"componentDownloadFileOpenFailed", {"OCR bileşeni için indirme dosyası açılamadı.", "Could not open the OCR component download file.", "Die Downloaddatei der OCR-Komponente konnte nicht geöffnet werden.", "Impossible d’ouvrir le fichier de téléchargement du composant OCR.", "No se pudo abrir el archivo de descarga del componente OCR.", "OCR コンポーネントのダウンロードファイルを開けませんでした。", "无法打开 OCR 组件的下载文件。", "Не удалось открыть файл загрузки компонента OCR."}},
        {"statusDownloading", {"indiriliyor...", "downloading...", "wird heruntergeladen...", "téléchargement...", "descargando...", "ダウンロード中...", "正在下载...", "загрузка..."}},
        {"componentDownloadFailed", {"OCR bileşeni indirilemedi: ", "Could not download OCR component: ", "OCR-Komponente konnte nicht heruntergeladen werden: ", "Impossible de télécharger le composant OCR : ", "No se pudo descargar el componente OCR: ", "OCR コンポーネントをダウンロードできませんでした: ", "无法下载 OCR 组件：", "Не удалось скачать компонент OCR: "}},
        {"statusInstalling", {"kuruluyor...", "installing...", "wird installiert...", "installation...", "instalando...", "インストール中...", "正在安装...", "установка..."}},
        {"ocrComponentInstallFailed", {"OCR bileşeni kurulamadı. Kurulum klasörü için yönetici izni gerekebilir.", "Could not install the OCR component. Administrator permission may be required for the install folder.", "Die OCR-Komponente konnte nicht installiert werden. Für den Installationsordner sind eventuell Administratorrechte nötig.", "Impossible d’installer le composant OCR. Des droits administrateur peuvent être nécessaires pour le dossier d’installation.", "No se pudo instalar el componente OCR. Puede que se necesiten permisos de administrador para la carpeta de instalación.", "OCR コンポーネントをインストールできませんでした。インストール先フォルダーには管理者権限が必要な場合があります。", "无法安装 OCR 组件。安装文件夹可能需要管理员权限。", "Не удалось установить компонент OCR. Для папки установки могут потребоваться права администратора."}},
        {"deleteFfmpegComponentTitle", {"FFmpeg bileşenini sil", "Delete FFmpeg component", "FFmpeg-Komponente löschen", "Supprimer le composant FFmpeg", "Eliminar componente FFmpeg", "FFmpeg コンポーネントを削除", "删除 FFmpeg 组件", "Удалить компонент FFmpeg"}},
        {"deleteFfmpegComponentConfirm", {"Paketle gelen FFmpeg video bileşeni silinsin mi?", "Delete the bundled FFmpeg video component?", "Mitgelieferte FFmpeg-Videokomponente löschen?", "Supprimer le composant vidéo FFmpeg intégré ?", "¿Eliminar el componente de video FFmpeg incluido?", "同梱の FFmpeg 動画コンポーネントを削除しますか？", "删除内置的 FFmpeg 视频组件？", "Удалить встроенный видеокомпонент FFmpeg?"}},
        {"ffmpegComponentDeleteFailed", {"FFmpeg bileşeni silinemedi.", "Could not delete the FFmpeg component.", "Die FFmpeg-Komponente konnte nicht gelöscht werden.", "Impossible de supprimer le composant FFmpeg.", "No se pudo eliminar el componente FFmpeg.", "FFmpeg コンポーネントを削除できませんでした。", "无法删除 FFmpeg 组件。", "Не удалось удалить компонент FFmpeg."}},
        {"gnomePrintScreenAssigned", {"PrintScreen, GNOME'da EShot'a atandı.", "PrintScreen was assigned to EShot in GNOME.", "PrintScreen wurde in GNOME EShot zugewiesen.", "Impr. écran a été attribué à EShot dans GNOME.", "Imprimir pantalla se asignó a EShot en GNOME.", "GNOME で PrintScreen を EShot に割り当てました。", "已在 GNOME 中将 PrintScreen 分配给 EShot。", "PrintScreen назначена EShot в GNOME."}},
        {"gnomeShortcutFailed", {"GNOME kısayolu ayarlanamadı: ", "Could not configure the GNOME shortcut: ", "GNOME-Kürzel konnte nicht eingerichtet werden: ", "Impossible de configurer le raccourci GNOME : ", "No se pudo configurar el atajo de GNOME: ", "GNOME のショートカットを設定できませんでした: ", "无法设置 GNOME 快捷键：", "Не удалось настроить сочетание клавиш GNOME: "}},
        {"kdeShortcutSettingsOpenFailed", {"KDE kısayol ayarları açılamadı.", "KDE shortcut settings could not be opened.", "Die KDE-Kürzeleinstellungen konnten nicht geöffnet werden.", "Impossible d’ouvrir les réglages des raccourcis KDE.", "No se pudo abrir la configuración de atajos de KDE.", "KDE のショートカット設定を開けませんでした。", "无法打开 KDE 快捷键设置。", "Не удалось открыть настройки горячих клавиш KDE."}},
        {"gnomeCaptureShortcutFailed", {"GNOME yakalama kısayolu ayarlanamadı: ", "Could not configure the GNOME capture shortcut: ", "GNOME-Aufnahmekürzel konnte nicht eingerichtet werden: ", "Impossible de configurer le raccourci de capture GNOME : ", "No se pudo configurar el atajo de captura de GNOME: ", "GNOME のキャプチャショートカットを設定できませんでした: ", "无法设置 GNOME 截图快捷键：", "Не удалось настроить сочетание клавиш захвата GNOME: "}},
        // ─── Recording warnings ───
        {"recWarnMicMissing", {"Mikrofon cihazı bulunamadığı için mikrofon kaydedilmedi.", "The microphone was not recorded because no microphone device could be found.", "Das Mikrofon wurde nicht aufgenommen, weil kein Mikrofongerät gefunden wurde.", "Le micro n'a pas été enregistré car aucun périphérique micro n'a été trouvé.", "No se grabó el micrófono porque no se encontró ningún dispositivo.", "マイクデバイスが見つからなかったため、マイクは録音されませんでした。", "未找到麦克风设备，因此未录制麦克风。", "Микрофон не записан: устройство микрофона не найдено."}},
        {"recWarnResumeFailed", {"Kayıt devam ettirilemedi: %1", "The recording could not be resumed: %1", "Die Aufnahme konnte nicht fortgesetzt werden: %1", "Impossible de reprendre l'enregistrement : %1", "No se pudo reanudar la grabación: %1", "録画を再開できませんでした: %1", "无法继续录制：%1", "Не удалось продолжить запись: %1"}},
        {"recWarnEndNotSaved", {"Kaydın son kısmı kaydedilemedi.", "The end of the recording could not be saved.", "Das Ende der Aufnahme konnte nicht gespeichert werden.", "La fin de l'enregistrement n'a pas pu être sauvegardée.", "No se pudo guardar el final de la grabación.", "録画の最後の部分を保存できませんでした。", "无法保存录制的结尾部分。", "Не удалось сохранить конец записи."}},
        {"recWarnSystemAudioCapture", {"Sistem sesi alınamadı; video sessiz kaydedildi.", "System audio could not be captured; the video was saved without it.", "Systemton konnte nicht aufgenommen werden; das Video wurde ohne ihn gespeichert.", "Le son système n'a pas pu être capturé ; la vidéo a été enregistrée sans.", "No se pudo capturar el audio del sistema; el vídeo se guardó sin él.", "システム音声を取得できなかったため、音声なしで保存しました。", "无法捕获系统音频；视频已在无系统音频的情况下保存。", "Не удалось записать системный звук; видео сохранено без него."}},
        {"recWarnSystemAudioAdd", {"Sistem sesi videoya eklenemedi; video sessiz kaydedildi.", "System audio could not be added; the video was saved without it.", "Systemton konnte nicht hinzugefügt werden; das Video wurde ohne ihn gespeichert.", "Le son système n'a pas pu être ajouté ; la vidéo a été enregistrée sans.", "No se pudo añadir el audio del sistema; el vídeo se guardó sin él.", "システム音声を追加できなかったため、音声なしで保存しました。", "无法添加系统音频；视频已在无系统音频的情况下保存。", "Не удалось добавить системный звук; видео сохранено без него."}},
        {"recWarnSystemAudioTimeout", {"Sistem sesini eklemek zaman aşımına uğradı; video sessiz kaydedildi.", "Adding system audio timed out; the video was saved without it.", "Das Hinzufügen des Systemtons hat zu lange gedauert; das Video wurde ohne ihn gespeichert.", "L'ajout du son système a expiré ; la vidéo a été enregistrée sans.", "Se agotó el tiempo al añadir el audio del sistema; el vídeo se guardó sin él.", "システム音声の追加がタイムアウトしたため、音声なしで保存しました。", "添加系统音频超时；视频已在无系统音频的情况下保存。", "Истекло время добавления системного звука; видео сохранено без него."}},
        {"recVideoKeptAt", {"Kaydedilen video şurada saklandı: %1", "The recorded video was kept at: %1", "Das aufgenommene Video wurde hier behalten: %1", "La vidéo enregistrée a été conservée ici : %1", "El vídeo grabado se conservó en: %1", "録画した動画はここに保存されています: %1", "录制的视频已保留在：%1", "Записанное видео сохранено здесь: %1"}},
        {"recPartsNotJoined", {"Duraklatılan kayıt parçaları birleştirilemedi (%1)", "The paused recording parts could not be joined (%1)", "Die Teile der pausierten Aufnahme konnten nicht zusammengefügt werden (%1)", "Les parties de l'enregistrement mis en pause n'ont pas pu être assemblées (%1)", "No se pudieron unir las partes de la grabación pausada (%1)", "一時停止した録画の各部分を結合できませんでした (%1)", "无法合并暂停录制的各个片段 (%1)", "Не удалось объединить части приостановленной записи (%1)"}},
        {"gifConversionTimedOut", {"GIF dönüştürme zaman aşımına uğradı", "GIF conversion timed out", "GIF-Umwandlung hat zu lange gedauert", "La conversion en GIF a expiré", "Se agotó el tiempo de conversión a GIF", "GIF 変換がタイムアウトしました", "GIF 转换超时", "Истекло время преобразования в GIF"}},
        {"appImageManagerWarning", {
            "<b>Bu AppImage bir AppImage yöneticisi tarafından yönetiliyor gibi görünüyor</b><br>(Gear Lever, AppImageLauncher vb.). EShot kendi kurulumunu yapar: aşağıda <i>EShot'ı uygulama menüsüne ekle</i> seçili kalsın, sonra EShot'ı o yöneticiden kaldırın. Yoksa menüde iki kayıt olur ve diğer kopya EShot güncellemelerini almaz.",
            "<b>This AppImage seems to be managed by an AppImage manager</b><br>(Gear Lever, AppImageLauncher or similar). EShot sets itself up: keep <i>Add EShot to the application menu</i> selected below, then remove EShot from that manager. Otherwise you get two menu entries and the other copy does not receive EShot updates.",
            "<b>Dieses AppImage scheint von einem AppImage-Manager verwaltet zu werden</b><br>(Gear Lever, AppImageLauncher o. Ä.). EShot richtet sich selbst ein: Lass unten <i>EShot zum Anwendungsmenü hinzufügen</i> aktiviert und entferne EShot danach aus diesem Manager. Sonst gibt es zwei Menüeinträge und die andere Kopie erhält keine EShot-Updates.",
            "<b>Cet AppImage semble géré par un gestionnaire d'AppImage</b><br>(Gear Lever, AppImageLauncher ou similaire). EShot s'installe lui-même : laissez <i>Ajouter EShot au menu des applications</i> coché ci-dessous, puis retirez EShot de ce gestionnaire. Sinon, vous aurez deux entrées de menu et l'autre copie ne recevra pas les mises à jour d'EShot.",
            "<b>Este AppImage parece estar gestionado por un gestor de AppImage</b><br>(Gear Lever, AppImageLauncher o similar). EShot se instala solo: deja marcada <i>Añadir EShot al menú de aplicaciones</i> abajo y luego quita EShot de ese gestor. Si no, tendrás dos entradas en el menú y la otra copia no recibirá las actualizaciones de EShot.",
            "<b>この AppImage は AppImage マネージャーで管理されているようです</b><br>(Gear Lever、AppImageLauncher など)。EShot は自身でセットアップします。下の<i>EShot をアプリケーションメニューに追加</i>をオンのままにし、そのマネージャーから EShot を削除してください。そうしないとメニュー項目が 2 つになり、もう一方のコピーは EShot の更新を受け取れません。",
            "<b>此 AppImage 似乎由 AppImage 管理器管理</b><br>(Gear Lever、AppImageLauncher 等)。EShot 会自行完成安装：请保持下方的<i>将 EShot 添加到应用程序菜单</i>处于选中状态，然后从该管理器中移除 EShot。否则菜单中会出现两个条目，另一个副本也收不到 EShot 更新。",
            "<b>Похоже, этим AppImage управляет менеджер AppImage</b><br>(Gear Lever, AppImageLauncher и т. п.). EShot устанавливается сам: оставьте ниже включённым <i>Добавить EShot в меню приложений</i>, затем удалите EShot из этого менеджера. Иначе в меню будет две записи, а другая копия не будет получать обновления EShot."}},
        {"installedVersion", {"Yüklü sürüm: v%1", "Installed version: v%1", "Installierte Version: v%1", "Version installée : v%1", "Versión instalada: v%1", "インストール済みバージョン: v%1", "已安装版本：v%1", "Установленная версия: v%1"}},
        {"ocrErrorEmptyImage", {"Görüntü boş", "Image is empty", "Das Bild ist leer", "L’image est vide", "La imagen está vacía", "画像が空です", "图片为空", "Изображение пустое"}},
        {"ocrErrorAlreadyRunning", {"OCR zaten çalışıyor", "OCR is already running", "OCR läuft bereits", "L’OCR est déjà en cours", "El OCR ya está en ejecución", "OCR はすでに実行中です", "OCR 正在运行", "OCR уже выполняется"}},
        {"ocrErrorEngineMissing", {"Tesseract OCR motoru bulunamadı. Tesseract-OCR'ı kurun veya Tesseract'ı uygulama klasörüne yerleştirin.", "Tesseract OCR engine not found. Please install Tesseract-OCR or place Tesseract in the app folder.", "Tesseract-OCR-Engine nicht gefunden. Installieren Sie Tesseract-OCR oder legen Sie Tesseract in den Programmordner.", "Moteur Tesseract OCR introuvable. Installez Tesseract-OCR ou placez Tesseract dans le dossier de l’application.", "No se encontró el motor Tesseract OCR. Instala Tesseract-OCR o colócalo en la carpeta de la aplicación.", "Tesseract OCR エンジンが見つかりません。Tesseract-OCR をインストールするか、アプリのフォルダーに配置してください。", "未找到 Tesseract OCR 引擎。请安装 Tesseract-OCR，或将 Tesseract 放入应用文件夹。", "Движок Tesseract OCR не найден. Установите Tesseract-OCR или поместите его в папку приложения."}},
        {"ocrErrorTempImage", {"Geçici görüntü kaydedilemedi", "Could not save the temporary image", "Temporäres Bild konnte nicht gespeichert werden", "Impossible d’enregistrer l’image temporaire", "No se pudo guardar la imagen temporal", "一時画像を保存できませんでした", "无法保存临时图片", "Не удалось сохранить временное изображение"}},
        {"ocrErrorNoLanguagePacks", {"Yüklü OCR dil paketi yok", "No OCR language packs are installed", "Keine OCR-Sprachpakete installiert", "Aucun module linguistique OCR n’est installé", "No hay paquetes de idioma OCR instalados", "OCR 言語パックがインストールされていません", "未安装任何 OCR 语言包", "Языковые пакеты OCR не установлены"}},
        {"ocrErrorNoUsableLanguagePack", {"Kullanılabilir OCR dil paketi yok", "No usable OCR language pack is installed", "Kein nutzbares OCR-Sprachpaket installiert", "Aucun module linguistique OCR utilisable n’est installé", "No hay ningún paquete de idioma OCR utilizable", "使用可能な OCR 言語パックがありません", "没有可用的 OCR 语言包", "Нет подходящего языкового пакета OCR"}},
        {"ocrErrorCannotStart", {"Tesseract başlatılamadı: ", "Cannot start Tesseract: ", "Tesseract konnte nicht gestartet werden: ", "Impossible de démarrer Tesseract : ", "No se pudo iniciar Tesseract: ", "Tesseract を起動できませんでした: ", "无法启动 Tesseract：", "Не удалось запустить Tesseract: "}},
        {"desktopUnknown", {"Bilinmeyen masaüstü", "Unknown desktop", "Unbekannte Arbeitsumgebung", "Bureau inconnu", "Escritorio desconocido", "不明なデスクトップ", "未知桌面", "Неизвестное окружение"}},
        {"desktopUnsupportedWarning", {"<b>Desteklenmeyen masaüstü ortamı: %1</b><br>EShot resmi olarak Wayland üzerinde KDE Plasma 6 ve GNOME'u destekler. Yakalama, genel kısayollar, kayıt veya sistem tepsisi düzgün çalışmayabilir.", "<b>Unsupported desktop environment: %1</b><br>EShot officially supports KDE Plasma 6 and GNOME on Wayland. Capture, global shortcuts, recording, or tray integration may not work correctly.", "<b>Nicht unterstützte Arbeitsumgebung: %1</b><br>EShot unterstützt offiziell KDE Plasma 6 und GNOME unter Wayland. Aufnahme, globale Kürzel, Bildschirmaufzeichnung oder Infobereich funktionieren möglicherweise nicht richtig.", "<b>Environnement de bureau non pris en charge : %1</b><br>EShot prend officiellement en charge KDE Plasma 6 et GNOME sous Wayland. La capture, les raccourcis globaux, l’enregistrement ou l’icône de la barre système peuvent mal fonctionner.", "<b>Entorno de escritorio no compatible: %1</b><br>EShot es compatible oficialmente con KDE Plasma 6 y GNOME en Wayland. La captura, los atajos globales, la grabación o el icono de la bandeja podrían no funcionar bien.", "<b>サポート対象外のデスクトップ環境: %1</b><br>EShot が公式にサポートしているのは Wayland 上の KDE Plasma 6 と GNOME です。キャプチャ、グローバルショートカット、録画、トレイが正しく動作しない可能性があります。", "<b>不受支持的桌面环境：%1</b><br>EShot 官方支持 Wayland 下的 KDE Plasma 6 和 GNOME。截图、全局快捷键、录制或托盘功能可能无法正常工作。", "<b>Неподдерживаемое окружение рабочего стола: %1</b><br>EShot официально поддерживает KDE Plasma 6 и GNOME на Wayland. Захват, глобальные сочетания клавиш, запись или значок в трее могут работать некорректно."}},
        {"desktopLimitedWarning", {"<b>Sınırlı masaüstü oturumu desteği</b><br>EShot öncelikle KDE Plasma 6 ve GNOME Wayland üzerinde test edilir. Bu oturumda yakalama, kısayol, kayıt veya sistem tepsisi davranışı farklı olabilir.", "<b>Limited desktop session support</b><br>EShot is primarily tested on KDE Plasma 6 and GNOME Wayland. Some capture, shortcut, recording, or tray behavior may differ in this session.", "<b>Eingeschränkte Unterstützung für diese Sitzung</b><br>EShot wird vor allem unter KDE Plasma 6 und GNOME Wayland getestet. Aufnahme, Kürzel, Aufzeichnung oder Infobereich können sich in dieser Sitzung anders verhalten.", "<b>Prise en charge limitée de cette session</b><br>EShot est principalement testé sur KDE Plasma 6 et GNOME Wayland. La capture, les raccourcis, l’enregistrement ou la barre système peuvent se comporter différemment ici.", "<b>Compatibilidad limitada con esta sesión</b><br>EShot se prueba principalmente en KDE Plasma 6 y GNOME Wayland. La captura, los atajos, la grabación o la bandeja pueden comportarse de otra forma en esta sesión.", "<b>このデスクトップセッションのサポートは限定的です</b><br>EShot は主に KDE Plasma 6 と GNOME Wayland でテストされています。このセッションでは、キャプチャ、ショートカット、録画、トレイの動作が異なる場合があります。", "<b>此桌面会话的支持有限</b><br>EShot 主要在 KDE Plasma 6 和 GNOME Wayland 上测试。在此会话中，截图、快捷键、录制或托盘行为可能有所不同。", "<b>Ограниченная поддержка этого сеанса</b><br>EShot в основном тестируется в KDE Plasma 6 и GNOME Wayland. В этом сеансе захват, сочетания клавиш, запись или трей могут работать иначе."}},
        {"wizardLanguageTip", {"Kurulumdan sonra EShot'ın kullanacağı dil. Kurulum ekranı İngilizce başlar.", "Language used by EShot after setup. The setup screen starts in English.", "Sprache, die EShot nach der Einrichtung verwendet. Die Einrichtung selbst beginnt auf Englisch.", "Langue utilisée par EShot après la configuration. L’écran de configuration démarre en anglais.", "Idioma que usará EShot tras la configuración. La pantalla de configuración empieza en inglés.", "セットアップ後に EShot で使う言語です。セットアップ画面は英語で始まります。", "设置完成后 EShot 使用的语言。设置界面默认以英语显示。", "Язык EShot после настройки. Экран настройки открывается на английском."}},
        {"usePrintScreenForEshot", {"Print Screen'i EShot için kullan", "Use Print Screen for EShot", "Druck-Taste für EShot verwenden", "Utiliser Impr. écran pour EShot", "Usar Imprimir pantalla para EShot", "Print Screen を EShot で使う", "将 Print Screen 用于 EShot", "Использовать Print Screen для EShot"}},
        {"usePrintScreenKdeTip", {"Spectacle'dan yalnızca düz Print Screen kısayolunu kaldırır ve EShot'a atar. Spectacle'ın diğer kısayolları korunur.", "Removes only the plain Print Screen shortcut from Spectacle and assigns it to EShot. Spectacle keeps its other shortcuts.", "Entfernt nur die einfache Druck-Taste aus Spectacle und weist sie EShot zu. Andere Spectacle-Kürzel bleiben erhalten.", "Retire uniquement le raccourci Impr. écran simple de Spectacle et l’attribue à EShot. Les autres raccourcis de Spectacle sont conservés.", "Quita solo el atajo simple Imprimir pantalla de Spectacle y lo asigna a EShot. Spectacle conserva sus demás atajos.", "Spectacle から単独の Print Screen ショートカットだけを外し、EShot に割り当てます。Spectacle の他のショートカットはそのままです。", "仅从 Spectacle 移除单独的 Print Screen 快捷键并分配给 EShot。Spectacle 的其他快捷键保持不变。", "Снимает с Spectacle только одиночную клавишу Print Screen и назначает её EShot. Остальные сочетания Spectacle сохраняются."}},
        {"usePrintScreenTip", {"Masaüstünden Print Screen'i EShot'a atamasını ister. Diğer kısayollar değişmez.", "Asks the desktop to assign Print Screen to EShot. Other shortcuts are left unchanged.", "Bittet die Arbeitsumgebung, die Druck-Taste EShot zuzuweisen. Andere Kürzel bleiben unverändert.", "Demande au bureau d’attribuer Impr. écran à EShot. Les autres raccourcis ne changent pas.", "Pide al escritorio que asigne Imprimir pantalla a EShot. Los demás atajos no cambian.", "デスクトップに Print Screen を EShot へ割り当てるよう要求します。他のショートカットは変更されません。", "请求桌面环境将 Print Screen 分配给 EShot。其他快捷键保持不变。", "Просит окружение рабочего стола назначить Print Screen для EShot. Другие сочетания не меняются."}},
        {"linuxOptionalFeatures", {"İsteğe bağlı Linux özellikleri", "Optional Linux features", "Optionale Linux-Funktionen", "Fonctionnalités Linux facultatives", "Funciones opcionales de Linux", "オプションの Linux 機能", "可选 Linux 功能", "Дополнительные функции Linux"}},
        {"linuxOptionalFeaturesHint", {"Sistem paket yöneticisiyle kurulacak isteğe bağlı özellikleri seçin. Bu adımı atlayıp daha sonra Ayarlar'dan tekrar deneyebilirsiniz.", "Select optional features to install with the system package manager. You can skip and retry from Settings.", "Wählen Sie optionale Funktionen, die über den Paketmanager installiert werden. Sie können diesen Schritt überspringen und später in den Einstellungen wiederholen.", "Choisissez les fonctionnalités facultatives à installer avec le gestionnaire de paquets. Vous pouvez passer cette étape et réessayer depuis les Paramètres.", "Elige las funciones opcionales que se instalarán con el gestor de paquetes. Puedes omitir este paso y reintentarlo desde Configuración.", "システムのパッケージマネージャーでインストールするオプション機能を選んでください。スキップして、後で設定から再試行することもできます。", "选择要通过系统包管理器安装的可选功能。可以跳过，之后在设置中重试。", "Выберите дополнительные функции для установки через системный менеджер пакетов. Можно пропустить и повторить позже в настройках."}},
        {"linuxFeatureFfmpeg", {"FFmpeg (video ve GIF kaydı)", "FFmpeg (video and GIF recording)", "FFmpeg (Video- und GIF-Aufnahme)", "FFmpeg (enregistrement vidéo et GIF)", "FFmpeg (grabación de video y GIF)", "FFmpeg（動画・GIF 録画）", "FFmpeg（视频和 GIF 录制）", "FFmpeg (запись видео и GIF)"}},
        {"linuxFeaturePortal", {"Wayland kayıt ve masaüstü portalı paketleri", "Wayland recording and desktop portal packages", "Pakete für Wayland-Aufnahme und Desktop-Portal", "Paquets d’enregistrement Wayland et de portail de bureau", "Paquetes de grabación en Wayland y portal de escritorio", "Wayland 録画とデスクトップポータルのパッケージ", "Wayland 录制和桌面门户软件包", "Пакеты записи Wayland и портала рабочего стола"}},
        {"linuxFeatureAppMenu", {"EShot'ı uygulama menüsüne ekle ve kısayolları kur", "Add EShot to the application menu and install shortcuts", "EShot zum Anwendungsmenü hinzufügen und Kürzel einrichten", "Ajouter EShot au menu des applications et installer les raccourcis", "Añadir EShot al menú de aplicaciones e instalar atajos", "EShot をアプリケーションメニューに追加し、ショートカットを設定", "将 EShot 添加到应用菜单并安装快捷键", "Добавить EShot в меню приложений и установить сочетания клавиш"}},
        {"linuxFeatureFfmpegTip", {"MP4 video ve GIF kayıtlarını kaydetmek için kullanılan medya kodlayıcısını kurar. Ekran görüntüleri onsuz da çalışır.", "Installs the media encoder used to save MP4 videos and GIF recordings. Screenshots work without it.", "Installiert den Medien-Encoder zum Speichern von MP4-Videos und GIF-Aufnahmen. Bildschirmfotos funktionieren auch ohne.", "Installe l’encodeur utilisé pour enregistrer les vidéos MP4 et les GIF. Les captures d’écran fonctionnent sans lui.", "Instala el codificador usado para guardar videos MP4 y grabaciones GIF. Las capturas funcionan sin él.", "MP4 動画と GIF 録画の保存に使うエンコーダーをインストールします。スクリーンショットはなくても使えます。", "安装用于保存 MP4 视频和 GIF 录制的媒体编码器。截图无需此组件。", "Устанавливает кодировщик для сохранения видео MP4 и GIF. Скриншоты работают и без него."}},
        {"linuxFeatureOcrTip", {"EShot'ın ekran görüntülerindeki metni okuyup kopyalayabilmesi için metin tanımayı kurar. OCR dillerini aşağıdan seçin.", "Installs text recognition so EShot can read and copy text from screenshots. Select OCR languages below.", "Installiert die Texterkennung, damit EShot Text aus Bildschirmfotos lesen und kopieren kann. OCR-Sprachen unten auswählen.", "Installe la reconnaissance de texte pour lire et copier le texte des captures. Choisissez les langues OCR ci-dessous.", "Instala el reconocimiento de texto para que EShot pueda leer y copiar texto de las capturas. Elige los idiomas OCR abajo.", "スクリーンショットの文字を読み取ってコピーできるよう、文字認識をインストールします。OCR 言語は下で選択してください。", "安装文字识别，让 EShot 能读取并复制截图中的文字。请在下方选择 OCR 语言。", "Устанавливает распознавание текста, чтобы EShot мог читать и копировать текст со скриншотов. Языки OCR выберите ниже."}},
        {"linuxFeaturePortalTip", {"KDE Plasma ve GNOME gibi Wayland masaüstlerinde güvenli ekran paylaşımı ve kaydı için kullanılan PipeWire ve masaüstü portalı bileşenlerini kurar.", "Installs PipeWire and desktop portal components used for secure screen sharing and recording on Wayland desktops such as KDE Plasma and GNOME.", "Installiert PipeWire- und Desktop-Portal-Komponenten für sichere Bildschirmfreigabe und -aufnahme unter Wayland, etwa in KDE Plasma und GNOME.", "Installe PipeWire et les composants du portail de bureau, utilisés pour partager et enregistrer l’écran de façon sécurisée sous Wayland (KDE Plasma, GNOME…).", "Instala PipeWire y los componentes del portal de escritorio para compartir y grabar la pantalla de forma segura en Wayland, como KDE Plasma y GNOME.", "KDE Plasma や GNOME などの Wayland デスクトップで、安全な画面共有と録画に使う PipeWire とデスクトップポータルをインストールします。", "安装 PipeWire 和桌面门户组件，用于在 KDE Plasma、GNOME 等 Wayland 桌面上安全地共享和录制屏幕。", "Устанавливает PipeWire и компоненты портала рабочего стола для безопасной трансляции и записи экрана в Wayland (KDE Plasma, GNOME и др.)."}},
        {"linuxFeatureAppMenuTip", {"Bu AppImage'ı kullanıcı uygulamaları klasörünüze kopyalar ve EShot'ı uygulama menüsüne ekler. Sistem geneline kurulum yapılmaz. Kurulumdan sonra uygulama menüsündeki girişi kullanın; indirdiğiniz AppImage ayrı bir taşınabilir kopya olarak kalır.", "Copies this AppImage to your user applications folder and adds EShot to the application menu. No system-wide installation is performed. After setup, use the application-menu entry; the downloaded AppImage remains a separate portable copy.", "Kopiert dieses AppImage in Ihren Benutzer-Anwendungsordner und fügt EShot dem Anwendungsmenü hinzu. Es erfolgt keine systemweite Installation. Verwenden Sie danach den Eintrag im Anwendungsmenü; das heruntergeladene AppImage bleibt eine separate portable Kopie.", "Copie cet AppImage dans votre dossier d’applications utilisateur et ajoute EShot au menu des applications. Aucune installation système n’est effectuée. Ensuite, utilisez l’entrée du menu ; l’AppImage téléchargé reste une copie portable distincte.", "Copia este AppImage a tu carpeta de aplicaciones de usuario y añade EShot al menú de aplicaciones. No se instala nada a nivel de sistema. Después, usa la entrada del menú; el AppImage descargado queda como una copia portátil aparte.", "この AppImage をユーザーのアプリケーションフォルダーにコピーし、EShot をアプリケーションメニューに追加します。システム全体へのインストールは行いません。セットアップ後はメニューの項目から起動してください。ダウンロードした AppImage は別のポータブル版として残ります。", "将此 AppImage 复制到用户应用文件夹，并把 EShot 添加到应用菜单。不会进行系统级安装。设置完成后请使用应用菜单中的入口；下载的 AppImage 仍是一个独立的便携副本。", "Копирует этот AppImage в папку приложений пользователя и добавляет EShot в меню приложений. Системная установка не выполняется. После настройки запускайте EShot из меню; скачанный AppImage остаётся отдельной портативной копией."}},
        {"ocrLanguageDataTip", {"%1 dilindeki metinleri tanımak için OCR dil verisi. EShot arayüz dilini değiştirmez.", "OCR language data for recognizing text written in %1. This does not change the EShot interface language.", "OCR-Sprachdaten zur Erkennung von Text in der Sprache %1. Die Sprache der EShot-Oberfläche ändert sich dadurch nicht.", "Données OCR pour reconnaître le texte écrit en %1. Cela ne change pas la langue de l’interface d’EShot.", "Datos OCR para reconocer texto escrito en %1. No cambia el idioma de la interfaz de EShot.", "%1 の文字を認識するための OCR 言語データです。EShot の表示言語は変わりません。", "用于识别%1文字的 OCR 语言数据。不会更改 EShot 的界面语言。", "Языковые данные OCR для распознавания текста на языке %1. Язык интерфейса EShot не меняется."}},
        {"ocrLanguageDataShortTip", {"%1 dilindeki metinleri tanımak için OCR dil verisi.", "OCR language data for recognizing text written in %1.", "OCR-Sprachdaten zur Erkennung von Text in der Sprache %1.", "Données OCR pour reconnaître le texte écrit en %1.", "Datos OCR para reconocer texto escrito en %1.", "%1 の文字を認識するための OCR 言語データです。", "用于识别%1文字的 OCR 语言数据。", "Языковые данные OCR для распознавания текста на языке %1."}},
        {"linuxSkipOptionalSetup", {"İsteğe bağlı bağımlılık kurulumunu atla", "Skip optional dependency setup", "Optionale Abhängigkeiten überspringen", "Ignorer l’installation des dépendances facultatives", "Omitir la instalación de dependencias opcionales", "オプションの依存関係のセットアップをスキップ", "跳过可选依赖项安装", "Пропустить установку дополнительных зависимостей"}},
        {"linuxSkipOptionalSetupTip", {"EShot'ı isteğe bağlı kayıt veya OCR bileşenlerini kurmadan başlatır. Bu kurulumu daha sonra Ayarlar'dan yeniden açabilirsiniz.", "Starts EShot without installing optional recording or OCR components. You can reopen this setup from Settings later.", "Startet EShot ohne optionale Aufnahme- oder OCR-Komponenten. Sie können diese Einrichtung später in den Einstellungen erneut öffnen.", "Démarre EShot sans installer les composants facultatifs d’enregistrement ou d’OCR. Vous pourrez rouvrir cette configuration depuis les Paramètres.", "Inicia EShot sin instalar los componentes opcionales de grabación u OCR. Puedes volver a abrir esta configuración desde Configuración.", "オプションの録画・OCR コンポーネントをインストールせずに EShot を起動します。このセットアップは後で設定から再度開けます。", "不安装可选的录制或 OCR 组件直接启动 EShot。之后可在设置中重新打开此安装。", "Запускает EShot без установки дополнительных компонентов записи и OCR. Позже эту настройку можно открыть из параметров."}},
        {"linuxSetupWillBeSkipped", {"İsteğe bağlı kurulum atlanacak. Devam etmek için Tamamla'ya tıklayın.", "Optional setup will be skipped. Click Finish to continue.", "Die optionale Einrichtung wird übersprungen. Klicken Sie auf „Fertig“, um fortzufahren.", "La configuration facultative sera ignorée. Cliquez sur « Terminer » pour continuer.", "Se omitirá la configuración opcional. Pulsa «Finalizar» para continuar.", "オプションのセットアップはスキップされます。「完了」をクリックして続行してください。", "将跳过可选设置。点击“完成”继续。", "Дополнительная настройка будет пропущена. Нажмите «Готово», чтобы продолжить."}},
        {"printScreenDesktopsOnly", {"Print Screen'in otomatik etkinleştirilmesi şu anda yalnızca KDE Plasma ve GNOME'da kullanılabilir.", "Automatic Print Screen activation is currently available on KDE Plasma and GNOME.", "Die automatische Aktivierung der Druck-Taste ist derzeit nur unter KDE Plasma und GNOME verfügbar.", "L’activation automatique d’Impr. écran n’est disponible que sous KDE Plasma et GNOME pour le moment.", "La activación automática de Imprimir pantalla solo está disponible por ahora en KDE Plasma y GNOME.", "Print Screen の自動設定は、現在 KDE Plasma と GNOME でのみ利用できます。", "自动启用 Print Screen 目前仅支持 KDE Plasma 和 GNOME。", "Автоматическая настройка Print Screen пока доступна только в KDE Plasma и GNOME."}},
        {"gnomePrintScreenDenied", {"GNOME, Print Screen kısayoluna izin vermedi. Ayarlar'dan tekrar deneyin.", "GNOME did not allow the Print Screen shortcut. Try again from Settings.", "GNOME hat das Druck-Kürzel nicht zugelassen. Versuchen Sie es in den Einstellungen erneut.", "GNOME n’a pas autorisé le raccourci Impr. écran. Réessayez depuis les Paramètres.", "GNOME no permitió el atajo Imprimir pantalla. Inténtalo de nuevo desde Configuración.", "GNOME が Print Screen ショートカットを許可しませんでした。設定からもう一度お試しください。", "GNOME 未允许 Print Screen 快捷键。请在设置中重试。", "GNOME не разрешил сочетание Print Screen. Повторите попытку в настройках."}},
        {"gnomePrintScreenChoose", {"GNOME kısayol izni penceresinde Print Screen'i seçin.", "Choose Print Screen in the GNOME shortcut permission window.", "Wählen Sie im GNOME-Berechtigungsfenster für Kürzel die Druck-Taste.", "Choisissez Impr. écran dans la fenêtre d’autorisation des raccourcis GNOME.", "Elige Imprimir pantalla en la ventana de permisos de atajos de GNOME.", "GNOME のショートカット許可ウィンドウで Print Screen を選択してください。", "请在 GNOME 快捷键权限窗口中选择 Print Screen。", "Выберите Print Screen в окне разрешений сочетаний клавиш GNOME."}},
        {"gnomePrintScreenFailed", {"GNOME'da Print Screen ayarlanamadı: %1", "Could not configure GNOME Print Screen: %1", "Druck-Taste in GNOME konnte nicht eingerichtet werden: %1", "Impossible de configurer Impr. écran dans GNOME : %1", "No se pudo configurar Imprimir pantalla en GNOME: %1", "GNOME で Print Screen を設定できませんでした: %1", "无法在 GNOME 中设置 Print Screen：%1", "Не удалось настроить Print Screen в GNOME: %1"}},
        {"gnomePrintScreenActivated", {"Print Screen, GNOME'da EShot için etkinleştirildi.", "Print Screen activated for EShot in GNOME.", "Druck-Taste in GNOME für EShot aktiviert.", "Impr. écran est activé pour EShot dans GNOME.", "Imprimir pantalla activado para EShot en GNOME.", "GNOME で Print Screen を EShot に設定しました。", "已在 GNOME 中为 EShot 启用 Print Screen。", "Print Screen назначена EShot в GNOME."}},
        {"kdeShortcutsReadFailed", {"KDE kısayolları okunamadı. Sistem Ayarları > Kısayollar bölümünü açıp Print tuşunu Spectacle'dan kaldırın.", "Could not read KDE shortcuts. Open System Settings > Shortcuts and remove Print from Spectacle.", "KDE-Kürzel konnten nicht gelesen werden. Öffnen Sie Systemeinstellungen > Kurzbefehle und entfernen Sie Print bei Spectacle.", "Impossible de lire les raccourcis KDE. Ouvrez Configuration du système > Raccourcis et retirez Impr. de Spectacle.", "No se pudieron leer los atajos de KDE. Abre Preferencias del sistema > Atajos y quita Imprimir de Spectacle.", "KDE のショートカットを読み取れませんでした。システム設定 > ショートカット を開き、Spectacle から Print を外してください。", "无法读取 KDE 快捷键。请打开 系统设置 > 快捷键，并从 Spectacle 中移除 Print。", "Не удалось прочитать сочетания клавиш KDE. Откройте Параметры системы > Комбинации клавиш и уберите Print у Spectacle."}},
        {"kdeShortcutChangeDenied", {"KDE kısayol değişikliğine izin vermedi. Bunun yerine Sistem Ayarları > Kısayollar bölümünü kullanın.", "KDE did not allow the shortcut change. Use System Settings > Shortcuts instead.", "KDE hat die Änderung nicht zugelassen. Verwenden Sie stattdessen Systemeinstellungen > Kurzbefehle.", "KDE n’a pas autorisé la modification. Passez plutôt par Configuration du système > Raccourcis.", "KDE no permitió el cambio de atajo. Usa Preferencias del sistema > Atajos.", "KDE がショートカットの変更を許可しませんでした。代わりに システム設定 > ショートカット を使用してください。", "KDE 未允许更改快捷键。请改用 系统设置 > 快捷键。", "KDE не разрешил изменить сочетание. Используйте Параметры системы > Комбинации клавиш."}},
        {"kdeSpectacleRestored", {"Spectacle'ın Print Screen kısayolu geri yüklendi.", "Spectacle's Print Screen shortcut was restored.", "Das Druck-Kürzel von Spectacle wurde wiederhergestellt.", "Le raccourci Impr. écran de Spectacle a été rétabli.", "Se restauró el atajo Imprimir pantalla de Spectacle.", "Spectacle の Print Screen ショートカットを元に戻しました。", "已恢复 Spectacle 的 Print Screen 快捷键。", "Сочетание Print Screen для Spectacle восстановлено."}},
        {"kdeSpectacleRestoreFailed", {"KDE, Spectacle'ın Print Screen kısayolunu geri yükleyemedi. Sistem Ayarları > Kısayollar bölümünden geri yükleyin.", "KDE could not restore Spectacle's Print Screen shortcut. Restore it from System Settings > Shortcuts.", "KDE konnte das Druck-Kürzel von Spectacle nicht wiederherstellen. Stellen Sie es unter Systemeinstellungen > Kurzbefehle wieder her.", "KDE n’a pas pu rétablir le raccourci Impr. écran de Spectacle. Rétablissez-le dans Configuration du système > Raccourcis.", "KDE no pudo restaurar el atajo Imprimir pantalla de Spectacle. Restáuralo desde Preferencias del sistema > Atajos.", "KDE は Spectacle の Print Screen ショートカットを元に戻せませんでした。システム設定 > ショートカット から戻してください。", "KDE 无法恢复 Spectacle 的 Print Screen 快捷键。请在 系统设置 > 快捷键 中恢复。", "KDE не удалось восстановить Print Screen для Spectacle. Восстановите его в Параметры системы > Комбинации клавиш."}},
        {"kdePrintScreenRegisterFailed", {"EShot, Print Screen'i KDE'ye kaydedemedi. %1", "EShot could not register Print Screen with KDE. %1", "EShot konnte die Druck-Taste nicht bei KDE registrieren. %1", "EShot n’a pas pu enregistrer Impr. écran auprès de KDE. %1", "EShot no pudo registrar Imprimir pantalla en KDE. %1", "EShot は KDE に Print Screen を登録できませんでした。%1", "EShot 无法在 KDE 中注册 Print Screen。%1", "EShot не удалось зарегистрировать Print Screen в KDE. %1"}},
        {"kdePrintScreenActivated", {"Print Screen EShot için etkinleştirildi. Spectacle'ın diğer kısayolları korundu.", "Print Screen activated for EShot. Spectacle's other shortcuts were kept.", "Druck-Taste für EShot aktiviert. Die übrigen Spectacle-Kürzel bleiben erhalten.", "Impr. écran est activé pour EShot. Les autres raccourcis de Spectacle sont conservés.", "Imprimir pantalla activado para EShot. Se conservaron los demás atajos de Spectacle.", "Print Screen を EShot に設定しました。Spectacle の他のショートカットはそのままです。", "已为 EShot 启用 Print Screen。Spectacle 的其他快捷键已保留。", "Print Screen назначена EShot. Остальные сочетания Spectacle сохранены."}},
        {"kdePrintScreenTakenBySpectacle", {"Print Screen şu anda Spectacle'a atanmış. Yukarıdaki \"%1\" düğmesine tıklayın veya başka bir kısayol seçin.", "Print Screen is currently assigned to Spectacle. Click \"%1\" above, or choose another shortcut.", "Die Druck-Taste ist derzeit Spectacle zugewiesen. Klicken Sie oben auf „%1“ oder wählen Sie ein anderes Kürzel.", "Impr. écran est actuellement attribué à Spectacle. Cliquez sur « %1 » ci-dessus ou choisissez un autre raccourci.", "Imprimir pantalla está asignado a Spectacle. Pulsa «%1» arriba o elige otro atajo.", "Print Screen は現在 Spectacle に割り当てられています。上の「%1」をクリックするか、別のショートカットを選んでください。", "Print Screen 当前已分配给 Spectacle。请点击上方的“%1”，或选择其他快捷键。", "Print Screen сейчас назначена Spectacle. Нажмите «%1» выше или выберите другое сочетание."}},
        {"installerScriptMissingWizard", {"Kurulum betiği bulunamadı. EShot'ı yeniden kurduktan sonra tekrar deneyin veya isteğe bağlı kurulumu atlayın.", "Installer script was not found. Retry after reinstalling EShot, or skip optional setup.", "Installationsskript nicht gefunden. Installieren Sie EShot neu und versuchen Sie es erneut, oder überspringen Sie die optionale Einrichtung.", "Script d’installation introuvable. Réinstallez EShot puis réessayez, ou ignorez la configuration facultative.", "No se encontró el script de instalación. Reinstala EShot y vuelve a intentarlo, u omite la configuración opcional.", "インストールスクリプトが見つかりません。EShot を再インストールしてから再試行するか、オプションのセットアップをスキップしてください。", "未找到安装脚本。请重新安装 EShot 后重试，或跳过可选设置。", "Сценарий установки не найден. Переустановите EShot и повторите попытку или пропустите дополнительную настройку."}},
        {"installingSelectedDependencies", {"Seçilen bağımlılıklar kuruluyor…", "Installing selected dependencies…", "Ausgewählte Abhängigkeiten werden installiert…", "Installation des dépendances sélectionnées…", "Instalando las dependencias seleccionadas…", "選択した依存関係をインストールしています…", "正在安装所选依赖项…", "Установка выбранных зависимостей…"}},
        {"installingOptionalComponents", {"Seçilen isteğe bağlı bileşenler kuruluyor…", "Installing selected optional components…", "Ausgewählte optionale Komponenten werden installiert…", "Installation des composants facultatifs sélectionnés…", "Instalando los componentes opcionales seleccionados…", "選択したオプションコンポーネントをインストールしています…", "正在安装所选可选组件…", "Установка выбранных дополнительных компонентов…"}},
        {"optionalSetupStartFailed", {"İsteğe bağlı kurulum başlatılamadı. Tekrar denemek için Tamamla'ya tıklayın veya kurulumu atlayın.", "Could not start optional setup. Click Finish to retry, or choose Skip.", "Die optionale Einrichtung konnte nicht gestartet werden. Klicken Sie auf „Fertig“, um es erneut zu versuchen, oder überspringen Sie sie.", "Impossible de lancer la configuration facultative. Cliquez sur « Terminer » pour réessayer, ou ignorez-la.", "No se pudo iniciar la configuración opcional. Pulsa «Finalizar» para reintentarlo u omítela.", "オプションのセットアップを開始できませんでした。「完了」をクリックして再試行するか、スキップしてください。", "无法启动可选设置。点击“完成”重试，或选择跳过。", "Не удалось запустить дополнительную настройку. Нажмите «Готово», чтобы повторить, или пропустите её."}},
        {"optionalSetupIncomplete", {"Kurulum tamamlandı ancak seçilen özelliklerden biri veya birkaçı hâlâ kullanılamıyor. Tekrar denemek için Tamamla'ya tıklayın veya kurulumu atlayın.", "Setup finished, but one or more selected capabilities are still unavailable. Click Finish to retry, or choose Skip.", "Die Einrichtung ist abgeschlossen, aber mindestens eine ausgewählte Funktion ist noch nicht verfügbar. Klicken Sie auf „Fertig“, um es erneut zu versuchen, oder überspringen Sie sie.", "Configuration terminée, mais au moins une fonctionnalité sélectionnée reste indisponible. Cliquez sur « Terminer » pour réessayer, ou ignorez-la.", "La configuración terminó, pero una o más funciones seleccionadas siguen sin estar disponibles. Pulsa «Finalizar» para reintentarlo u omítela.", "セットアップは終了しましたが、選択した機能の一部がまだ使えません。「完了」をクリックして再試行するか、スキップしてください。", "设置已结束，但仍有一项或多项所选功能不可用。点击“完成”重试，或选择跳过。", "Настройка завершена, но некоторые выбранные функции всё ещё недоступны. Нажмите «Готово», чтобы повторить, или пропустите."}},
        {"optionalSetupSucceeded", {"Seçilen isteğe bağlı bileşenler başarıyla kuruldu.", "Selected optional components installed successfully.", "Die ausgewählten optionalen Komponenten wurden installiert.", "Les composants facultatifs sélectionnés ont été installés.", "Los componentes opcionales seleccionados se instalaron correctamente.", "選択したオプションコンポーネントをインストールしました。", "所选可选组件已成功安装。", "Выбранные дополнительные компоненты установлены."}},
        {"optionalSetupFailed", {"Kurulum başarısız oldu veya yetkilendirme iptal edildi. Paket yöneticinizi kontrol edin, ardından tekrar denemek için Tamamla'ya tıklayın veya kurulumu atlayın. %1", "Installation failed or authorization was cancelled. Check your package manager, then click Finish to retry, or choose Skip. %1", "Installation fehlgeschlagen oder Autorisierung abgebrochen. Prüfen Sie Ihren Paketmanager und klicken Sie dann auf „Fertig“, um es erneut zu versuchen, oder überspringen Sie die Einrichtung. %1", "L’installation a échoué ou l’autorisation a été annulée. Vérifiez votre gestionnaire de paquets, puis cliquez sur « Terminer » pour réessayer, ou ignorez la configuration. %1", "La instalación falló o se canceló la autorización. Revisa tu gestor de paquetes y pulsa «Finalizar» para reintentarlo, u omite la configuración. %1", "インストールに失敗したか、認証がキャンセルされました。パッケージマネージャーを確認してから「完了」をクリックして再試行するか、スキップしてください。%1", "安装失败或授权已取消。请检查包管理器，然后点击“完成”重试，或选择跳过。%1", "Установка не удалась или авторизация отменена. Проверьте менеджер пакетов, затем нажмите «Готово», чтобы повторить, или пропустите настройку. %1"}},
        {"linuxDependencySetupTitle", {"Linux bağımlılık kurulumu", "Linux dependency setup", "Linux-Abhängigkeiten einrichten", "Installation des dépendances Linux", "Instalación de dependencias de Linux", "Linux 依存関係のセットアップ", "Linux 依赖项安装", "Установка зависимостей Linux"}},
        {"linuxFeatureFfmpegShortTip", {"MP4 video ve GIF kayıtlarını kaydetmek için kullanılan medya kodlayıcısını kurar.", "Installs the media encoder used to save MP4 videos and GIF recordings.", "Installiert den Medien-Encoder zum Speichern von MP4-Videos und GIF-Aufnahmen.", "Installe l’encodeur utilisé pour enregistrer les vidéos MP4 et les GIF.", "Instala el codificador usado para guardar videos MP4 y grabaciones GIF.", "MP4 動画と GIF 録画の保存に使うエンコーダーをインストールします。", "安装用于保存 MP4 视频和 GIF 录制的媒体编码器。", "Устанавливает кодировщик для сохранения видео MP4 и GIF."}},
        {"linuxFeatureOcrShortTip", {"Metin tanımayı ve seçilen OCR dil verilerini kurar.", "Installs text recognition and the selected OCR language data.", "Installiert die Texterkennung und die ausgewählten OCR-Sprachdaten.", "Installe la reconnaissance de texte et les langues OCR sélectionnées.", "Instala el reconocimiento de texto y los datos de idioma OCR seleccionados.", "文字認識と、選択した OCR 言語データをインストールします。", "安装文字识别和所选的 OCR 语言数据。", "Устанавливает распознавание текста и выбранные языковые данные OCR."}},
        {"linuxFeaturePortalShortTip", {"Wayland'de güvenli ekran kaydı için PipeWire ve masaüstü portalı bileşenlerini kurar.", "Installs PipeWire and desktop portal components for secure screen recording on Wayland.", "Installiert PipeWire- und Desktop-Portal-Komponenten für sichere Bildschirmaufnahmen unter Wayland.", "Installe PipeWire et les composants du portail de bureau pour enregistrer l’écran de façon sécurisée sous Wayland.", "Instala PipeWire y los componentes del portal de escritorio para grabar la pantalla de forma segura en Wayland.", "Wayland で安全に画面を録画するための PipeWire とデスクトップポータルをインストールします。", "安装 PipeWire 和桌面门户组件，用于在 Wayland 上安全录屏。", "Устанавливает PipeWire и компоненты портала для безопасной записи экрана в Wayland."}},
        {"installSelected", {"Seçilenleri kur", "Install selected", "Auswahl installieren", "Installer la sélection", "Instalar selección", "選択項目をインストール", "安装所选项", "Установить выбранное"}},
        {"installerScriptMissingDialog", {"Kurulum betiği bulunamadı. EShot'ı yeniden kurup tekrar deneyin veya iptal edin.", "Installer script not found. Reinstall EShot and retry, or cancel.", "Installationsskript nicht gefunden. Installieren Sie EShot neu und versuchen Sie es erneut, oder brechen Sie ab.", "Script d’installation introuvable. Réinstallez EShot puis réessayez, ou annulez.", "No se encontró el script de instalación. Reinstala EShot y vuelve a intentarlo, o cancela.", "インストールスクリプトが見つかりません。EShot を再インストールして再試行するか、キャンセルしてください。", "未找到安装脚本。请重新安装 EShot 后重试，或取消。", "Сценарий установки не найден. Переустановите EShot и повторите попытку или отмените."}},
        {"dependenciesInstalled", {"Seçilen bağımlılıklar başarıyla kuruldu.", "Selected dependencies installed successfully.", "Die ausgewählten Abhängigkeiten wurden installiert.", "Les dépendances sélectionnées ont été installées.", "Las dependencias seleccionadas se instalaron correctamente.", "選択した依存関係をインストールしました。", "所选依赖项已成功安装。", "Выбранные зависимости установлены."}},
        {"dependencyInstallFailed", {"Kurulum başarısız oldu veya yetkilendirme iptal edildi. Paket yöneticinizi kontrol edip tekrar deneyin ya da iptal edin. %1", "Installation failed or authorization was cancelled. Check your package manager, then retry or cancel. %1", "Installation fehlgeschlagen oder Autorisierung abgebrochen. Prüfen Sie Ihren Paketmanager und versuchen Sie es erneut oder brechen Sie ab. %1", "L’installation a échoué ou l’autorisation a été annulée. Vérifiez votre gestionnaire de paquets, puis réessayez ou annulez. %1", "La instalación falló o se canceló la autorización. Revisa tu gestor de paquetes y vuelve a intentarlo, o cancela. %1", "インストールに失敗したか、認証がキャンセルされました。パッケージマネージャーを確認して再試行するか、キャンセルしてください。%1", "安装失败或授权已取消。请检查包管理器后重试，或取消。%1", "Установка не удалась или авторизация отменена. Проверьте менеджер пакетов и повторите попытку или отмените. %1"}},
        {"installerStartFailed", {"Kurulum programı başlatılamadı. Tekrar deneyin veya iptal edin.", "Could not start the installer. Retry or cancel.", "Das Installationsprogramm konnte nicht gestartet werden. Versuchen Sie es erneut oder brechen Sie ab.", "Impossible de lancer l’installation. Réessayez ou annulez.", "No se pudo iniciar el instalador. Reinténtalo o cancela.", "インストーラーを起動できませんでした。再試行するか、キャンセルしてください。", "无法启动安装程序。请重试或取消。", "Не удалось запустить установщик. Повторите попытку или отмените."}},
    };

    static constexpr int s_transCount = sizeof(s_trans) / sizeof(s_trans[0]);
};

#endif
