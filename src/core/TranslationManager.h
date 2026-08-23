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
    static QString gifFpsLabel()     { return tr("gifFpsLabel"); }
    static QString videoFpsLabel()   { return tr("videoFpsLabel"); }
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
    static QString trayWindowCapture() { return tr("trayWindowCapture"); }
    static QString trayScrollingCapture() { return tr("trayScrollingCapture"); }
    static QString trayRecordGif()    { return tr("trayRecordGif"); }
    static QString trayStopRecording() { return tr("trayStopRecording"); }
    static QString trayCancelRecording() { return tr("trayCancelRecording"); }
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
    static QString toolFont()         { return tr("toolFont"); }
    static QString toolFontSize()     { return tr("toolFontSize"); }
    static QString toolMove()         { return tr("toolMove"); }
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

    // ─── Sihirbaz ───
    static QString wizardTitle()      { return tr("wizardTitle"); }
    static QString wizardDesc()       { return tr("wizardDesc"); }
    static QString wizardNext()       { return tr("wizardNext"); }
    static QString wizardBack()       { return tr("wizardBack"); }
    static QString wizardFinish()     { return tr("wizardFinish"); }
    static QString wizardHotkeyDesc() { return tr("wizardHotkeyDesc"); }
    static QString printScreenConflictTitle() { return tr("printScreenConflictTitle"); }
    static QString printScreenConflictMessage() { return tr("printScreenConflictMessage"); }
    static QString printScreenConflictFix() { return tr("printScreenConflictFix"); }
    static QString printScreenConflictDisabled() { return tr("printScreenConflictDisabled"); }
    static QString hotkeyMayBeInUse() { return tr("hotkeyMayBeInUse"); }
    static QString recordingHotkeyMayBeInUse() { return tr("recordingHotkeyMayBeInUse"); }
    static QString directCaptureHotkeyMayBeInUse() { return tr("directCaptureHotkeyMayBeInUse"); }
    static QString autoStartSaveFailed() { return tr("autoStartSaveFailed"); }

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
    static QString quickSettings() { return tr("quickSettings"); }
    static QString quickPenWidth() { return tr("quickPenWidth"); }
    static QString quickBlurIntensity() { return tr("quickBlurIntensity"); }
    static QString quickGifRecording() { return tr("quickGifRecording"); }
    static QString quickMaxSeconds() { return tr("quickMaxSeconds"); }
    static QString quickVideoComingSoon() { return tr("quickVideoComingSoon"); }
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
    static QString videoQuality() { return tr("videoQuality"); }
    static QString gifSettings() { return tr("gifSettings"); }
    static QString recordingCancel() { return tr("recordingCancel"); }
    static QString videoQualityCrf() { return tr("videoQualityCrf"); }
    static QString videoCrfHint() { return tr("videoCrfHint"); }
    static QString videoBitrate() { return tr("videoBitrate"); }
    static QString audioMode() { return tr("audioMode"); }
    static QString audioNone() { return tr("audioNone"); }
    static QString audioDesktop() { return tr("audioDesktop"); }
    static QString audioMicrophone() { return tr("audioMicrophone"); }
    static QString audioDesktopMic() { return tr("audioDesktopMic"); }
    static QString audioSource() { return tr("audioSource"); }
    static QString audioMicrophoneDevice() { return tr("audioMicrophoneDevice"); }
    static QString audioNoDevice() { return tr("audioNoDevice"); }
    static QString audioSystemLoopback() { return tr("audioSystemLoopback"); }

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
        {"gifFpsLabel",    {"GIF FPS:", "GIF FPS:", "GIF-FPS:", "FPS GIF :", "FPS de GIF:", "GIF フレームレート:", "GIF 帧率:", "FPS GIF:"}},
        {"videoFpsLabel",  {"Video FPS:", "Video FPS:", "Video-FPS:", "FPS vidéo :", "FPS de video:", "動画フレームレート:", "视频帧率:", "FPS видео:"}},
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
        {"trayWindowCapture",{"Pencere Yakala", "Capture Window", "Fenster erfassen", "Capturer une fenêtre", "Capturar ventana", "ウィンドウをキャプチャ", "捕获窗口", "Захват окна"}},
        {"trayScrollingCapture",{"Kaydırmalı Yakala", "Scrolling Capture", "Scrollaufnahme", "Capture défilante", "Captura con desplazamiento", "スクロールキャプチャ", "滚动截图", "Захват с прокруткой"}},
        {"trayRecordGif",  {"GIF Kaydı Başlat", "Start GIF Recording", "GIF-Aufnahme starten", "Démarrer enregistrement GIF", "Iniciar grabación GIF", "GIF録画開始", "开始 GIF 录制", "Начать запись GIF"}},
        {"trayStopRecording",{"Kaydı Durdur", "Stop Recording", "Aufnahme stoppen", "Arrêter l'enregistrement", "Detener grabación", "録画停止", "停止录制", "Остановить запись"}},
        {"trayCancelRecording",{"Kaydı İptal Et", "Cancel Recording", "Aufnahme abbrechen", "Annuler l'enregistrement", "Cancelar grabación", "録画をキャンセル", "取消录制", "Отменить запись"}},
        {"traySettings",   {"Ayarlar", "Settings", "Einstellungen", "Paramètres", "Configuración", "設定", "设置", "Настройки"}},
        {"trayAbout",      {"Hakkında", "About", "Über", "À propos", "Acerca de", "概要", "关于", "О программе"}},
        {"trayQuit",       {"Çıkış", "Quit", "Beenden", "Quitter", "Salir", "終了", "退出", "Выход"}},

        // ─── Erişilebilirlik ───
        {"accessibility",  {"Erişilebilirlik", "Accessibility", "Barrierefreiheit", "Accessibilité", "Accesibilidad", "アクセシビリティ", "无障碍", "Доступность"}},
        {"highContrast",   {"Yüksek kontrast", "High contrast", "Hoher Kontrast", "Contraste élevé", "Alto contraste", "ハイコントラスト", "高对比度", "Высокий контраст"}},
        {"trayIcon",       {"Tepsi Simgesi", "Tray Icon", "Taskbar-Symbol", "Icône de zone", "Icono de bandeja", "トレイアイコン", "托盘图标", "Значок в трее"}},
        {"trayIconDark",   {"Siyah tepsi simgesi", "Black tray icon", "Schwarzes Taskbar-Symbol", "Icône de zone noire", "Icono de bandeja negro", "黒いトレイアイコン", "黑色托盘图标", "Чёрный значок в трее"}},
        {"trayIconLight",  {"Açık", "Light", "Hell", "Clair", "Claro", "ライト", "浅色", "Светлый"}},

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
        {"toolCounter",    {"Numara (N)", "Counter (N)", "Zähler (N)", "Compteur (N)", "Contador (N)", "カウンター (N)", "计数器 (N)", "Счётчик (N)"}},
        {"toolEraser",     {"Silgi (X)", "Eraser (X)", "Radierer (X)", "Gomme (X)", "Borrador (X)", "消しゴム (X)", "橡皮 (X)", "Ластик (X)"}},
        {"toolLine",       {"Çizgi (L)", "Line (L)", "Linie (L)", "Ligne (L)", "Línea (L)", "線 (L)", "线条 (L)", "Линия (L)"}},
        {"toolColor",      {"Renk Seç", "Pick Color", "Farbe wählen", "Couleur", "Elegir color", "色を選択", "选择颜色", "Выбрать цвет"}},
        {"toolWidth",      {"Kalınlık", "Width", "Breite", "Épaisseur", "Grosor", "太さ", "粗细", "Толщина"}},
        {"toolFont",       {"Yazi Tipi", "Font", "Schriftart", "Police", "Fuente", "フォント", "字体", "Шрифт"}},
        {"toolFontSize",   {"Yazi Boyutu", "Font Size", "Schriftgroesse", "Taille de police", "Tamano de fuente", "Font Size", "Font Size", "Font Size"}},
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
        {"updateMessage",  {"Yeni sürüm: %1", "New version: %1", "Neue Version: %1", "Nouvelle version : %1", "Nueva versión: %1", "新しいバージョン: %1", "新版本: %1", "Новая версия: %1"}},
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
        {"updateCannotLaunchInstaller",{"Installer başlatılamadı.", "Could not launch the installer.", "Installer konnte nicht gestartet werden.", "Impossible de lancer l'installateur.", "No se pudo iniciar el instalador.", "インストーラーを起動できませんでした。", "无法启动安装程序。", "Не удалось запустить установщик."}},

        // ─── Sihirbaz ───
        {"wizardTitle",    {"EShot'a Hoş Geldiniz!", "Welcome to EShot!", "Willkommen bei EShot!", "Bienvenue !", "¡Bienvenido!", "ようこそ！", "欢迎使用EShot！", "Добро пожаловать в EShot!"}},
        {"wizardDesc",     {"Ayarları yapılandırın.", "Configure your settings.", "Einstellungen konfigurieren.", "Configurez vos paramètres.", "Configure sus ajustes.", "設定を構成。", "配置设置。", "Настройте параметры."}},
        {"wizardNext",     {"İleri", "Next", "Weiter", "Suivant", "Siguiente", "次へ", "下一步", "Далее"}},
        {"wizardBack",     {"Geri", "Back", "Zurück", "Retour", "Atrás", "戻る", "返回", "Назад"}},
        {"wizardFinish",   {"Tamamla", "Finish", "Fertig", "Terminer", "Finalizar", "完了", "完成", "Готово"}},
        {"wizardHotkeyDesc",{"Kısayol tuşunu ayarlayın.\nVarsayılan: Print Screen", "Set the hotkey.\nDefault: Print Screen", "Tastenkürzel festlegen.\nStandard: Druck", "Définir le raccourci.\nDéfaut: Impr. écran", "Configure el atajo.\nPredeterminado: Imp Pant", "ホットキーを設定。\nデフォルト: Print Screen", "设置快捷键。\n默认: Print Screen", "Установите клавишу.\nПо умолчанию: Print Screen"}},
        {"printScreenConflictTitle",{"Print Screen çakışması", "Print Screen conflict", "Print Screen Konflikt", "Conflit Print Screen", "Conflicto de Print Screen", "Print Screen の競合", "Print Screen 冲突", "Конфликт Print Screen"}},
        {"printScreenConflictMessage",{"Windows, Print Screen tuşunu Snipping Tool için kullanıyor. EShot'un Print Screen ile çalışması için bu Windows ayarını kapatın.", "Windows is using Print Screen for Snipping Tool. Disable this Windows setting to let EShot use Print Screen.", "Windows verwendet Print Screen für das Snipping Tool. Deaktivieren Sie diese Windows-Einstellung, damit EShot Print Screen verwenden kann.", "Windows utilise Print Screen pour Snipping Tool. Désactivez ce réglage Windows pour permettre à EShot d'utiliser Print Screen.", "Windows usa Print Screen para Snipping Tool. Desactive este ajuste de Windows para que EShot pueda usar Print Screen.", "Windows が Print Screen を Snipping Tool に使用しています。EShot で Print Screen を使うには、この Windows 設定を無効にしてください。", "Windows 正在将 Print Screen 用于截图工具。请关闭此 Windows 设置，以便 EShot 使用 Print Screen。", "Windows использует Print Screen для Snipping Tool. Отключите этот параметр Windows, чтобы EShot мог использовать Print Screen."}},
        {"printScreenConflictFix",{"Windows Print Screen ayarını kapat", "Disable Windows Print Screen shortcut", "Windows Print Screen Kurzbefehl deaktivieren", "Désactiver le raccourci Print Screen de Windows", "Desactivar el atajo Print Screen de Windows", "Windows の Print Screen ショートカットを無効にする", "禁用 Windows Print Screen 快捷键", "Отключить сочетание Windows Print Screen"}},
        {"printScreenConflictDisabled",{"Windows Print Screen Snipping Tool ayarı kapatıldı. Print Screen artık EShot tarafından kullanılabilir.", "Windows Print Screen Snipping Tool shortcut has been disabled. Print Screen can now be used by EShot.", "Windows Print Screen für Snipping Tool wurde deaktiviert. Print Screen kann jetzt von EShot verwendet werden.", "Le raccourci Windows Print Screen pour Snipping Tool a été désactivé. Print Screen peut maintenant être utilisé par EShot.", "El atajo Windows Print Screen para Snipping Tool se ha desactivado. EShot ya puede usar Print Screen.", "Windows Print Screen の Snipping Tool ショートカットを無効にしました。Print Screen を EShot で使用できます。", "Windows Print Screen 截图工具快捷键已禁用。EShot 现在可以使用 Print Screen。", "Сочетание Windows Print Screen для Snipping Tool отключено. Теперь EShot может использовать Print Screen."}},
        {"hotkeyMayBeInUse",{"Bu kısayol başka bir uygulama tarafından kullanılıyor olabilir. Lütfen farklı bir kombinasyon seçin.", "This hotkey may already be used by another app. Please choose a different combination.", "Dieses Kürzel wird möglicherweise bereits von einer anderen App verwendet. Bitte wählen Sie eine andere Kombination.", "Ce raccourci est peut-être déjà utilisé par une autre application. Choisissez une autre combinaison.", "Este atajo ya puede estar usado por otra aplicación. Elija otra combinación.", "このホットキーは他のアプリで既に使用されている可能性があります。別の組み合わせを選んでください。", "此快捷键可能已被其他应用使用。请选择其他组合。", "Эта горячая клавиша уже может использоваться другим приложением. Выберите другую комбинацию."}},
        {"recordingHotkeyMayBeInUse",{"Bu kayıt kısayollarından biri başka bir uygulama tarafından kullanılıyor olabilir.", "One of these recording hotkeys may already be used by another app.", "Eines dieser Aufnahme-Kürzel wird möglicherweise bereits von einer anderen App verwendet.", "Un de ces raccourcis d'enregistrement est peut-être déjà utilisé par une autre application.", "Uno de estos atajos de grabación ya puede estar usado por otra aplicación.", "録画用ホットキーのいずれかが他のアプリで使用されている可能性があります。", "某个录制快捷键可能已被其他应用使用。", "Одна из горячих клавиш записи уже может использоваться другим приложением."}},
        {"directCaptureHotkeyMayBeInUse",{"Doğrudan yakalama kısayollarından biri başka bir uygulama tarafından kullanılıyor olabilir.", "One of the direct capture hotkeys may already be used by another app.", "Eines der Direktaufnahme-Kürzel wird möglicherweise bereits von einer anderen App verwendet.", "Un des raccourcis de capture directe est peut-être déjà utilisé par une autre application.", "Uno de los atajos de captura directa ya puede estar usado por otra aplicación.", "直接キャプチャのホットキーのいずれかが他のアプリで使用されている可能性があります。", "某个直接捕获快捷键可能已被其他应用使用。", "Одна из горячих клавиш прямого захвата уже может использоваться другим приложением."}},
        {"autoStartSaveFailed",{"Sistemle başlat ayarı kaydedilemedi.", "Could not save the startup setting.", "Die Startoption konnte nicht gespeichert werden.", "Impossible d'enregistrer le réglage de démarrage.", "No se pudo guardar la opción de inicio.", "起動時設定を保存できませんでした。", "无法保存启动设置。", "Не удалось сохранить настройку автозапуска."}},

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
        {"ocrNoText",      {"Metin algılanmadı", "No text detected", "Kein Text erkannt", "Aucun texte détecté", "No se detectó texto", "テキストが検出されませんでした", "未检测到文字", "Текст не обнаружен"}},
        {"ocrAutomatic",   {"Otomatik", "Automatic", "Automatisch", "Automatique", "Automático", "自動", "自动", "Автоматически"}},
        {"ocrLanguagePackMissing",{"Dil paketi yuklu degil", "Language pack is not installed", "Sprachpaket ist nicht installiert", "Le module de langue n'est pas installe", "El paquete de idioma no esta instalado", "Language pack is not installed", "Language pack is not installed", "Language pack is not installed"}},

        // ─── Kayıt (Recording) ───
        {"recordingStart", {"Kaydı Başlat", "Start Recording", "Aufnahme starten", "Démarrer l'enregistrement", "Iniciar grabación", "録画開始", "开始录制", "Начать запись"}},
        {"recordingStop",  {"Kaydı Durdur", "Stop Recording", "Aufnahme stoppen", "Arrêter l'enregistrement", "Detener grabación", "録画停止", "停止录制", "Остановить запись"}},
        {"recordingStopShort", {"Durdur", "Stop", "Stopp", "Arrêter", "Detener", "停止", "停止", "Стоп"}},
        {"recordingPauseResume",{"Duraklat / Sürdür", "Pause / Resume", "Pausieren / Fortsetzen", "Pause / Reprendre", "Pausar / Reanudar", "一時停止 / 再開", "暂停 / 继续", "Пауза / Продолжить"}},
        {"recordingDetails", {"Ayrıntılar", "Details", "Einzelheiten", "Détails", "Detalles", "詳細", "详细", "Подробности"}},
        {"recordingDrag", {"Taşımak için sürükleyin", "Drag to move", "Zum Verschieben ziehen", "Faites glisser pour déplacer", "Arrastra para mover", "ドラッグして移動", "拖动以移动", "Перетащите для перемещения"}},
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
        {"quickSettings",{"Hızlı Ayarlar", "Quick Settings", "Schnelleinstellungen", "Réglages rapides", "Ajustes rápidos", "クイック設定", "快速设置", "Быстрые настройки"}},
        {"quickPenWidth",{"Kalem / şekil kalınlığı", "Pen / shape width", "Stift-/Formbreite", "Épaisseur stylo/forme", "Grosor de lápiz/forma", "ペン/図形の太さ", "画笔/形状粗细", "Толщина пера/фигуры"}},
        {"quickBlurIntensity",{"Bulanıklık şiddeti", "Blur intensity", "Unschärfeintensität", "Intensité du flou", "Intensidad de desenfoque", "ぼかし強度", "模糊强度", "Сила размытия"}},
        {"quickGifRecording",{"GIF kaydı", "GIF recording", "GIF-Aufnahme", "Enregistrement GIF", "Grabación GIF", "GIF録画", "GIF 录制", "Запись GIF"}},
        {"quickMaxSeconds",{"Maks. saniye", "Max seconds", "Max. Sekunden", "Secondes max", "Segundos máx.", "最大秒数", "最大秒数", "Макс. секунд"}},
        {"quickVideoComingSoon",{"Video ayarları: yakında", "Video settings: coming soon", "Videoeinstellungen: demnächst", "Réglages vidéo : bientôt", "Ajustes de video: pronto", "動画設定: 近日対応", "视频设置：即将支持", "Настройки видео: скоро"}},
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
        {"videoQuality",{"Video kalitesi", "Video quality", "Videoqualität", "Qualité vidéo", "Calidad de video", "動画品質", "视频质量", "Качество видео"}},
        {"gifSettings",{"GIF Ayarları", "GIF Settings", "GIF-Einstellungen", "Réglages GIF", "Ajustes GIF", "GIF設定", "GIF 设置", "Настройки GIF"}},
        {"recordingCancel",{"İptal", "Cancel", "Abbrechen", "Annuler", "Cancelar", "キャンセル", "取消", "Отмена"}},
        {"videoQualityCrf",{"Video kalitesi (CRF)", "Video quality (CRF)", "Videoqualität (CRF)", "Qualité vidéo (CRF)", "Calidad de video (CRF)", "動画品質 (CRF)", "视频质量 (CRF)", "Качество видео (CRF)"}},
        {"videoCrfHint",{"Düşük değer daha yüksek kalite ve daha büyük dosya demektir. 24 dengeli varsayılandır.", "Lower values mean higher quality and larger files. 24 is the balanced default.", "Niedrigere Werte bedeuten höhere Qualität und größere Dateien. 24 ist der ausgewogene Standard.", "Une valeur plus basse donne une meilleure qualité et des fichiers plus volumineux. 24 est le réglage équilibré par défaut.", "Los valores más bajos dan más calidad y archivos más grandes. 24 es el valor equilibrado predeterminado.", "値が低いほど高品質でファイルサイズが大きくなります。24 がバランスの取れた既定値です。", "数值越低质量越高，文件越大。24 是均衡默认值。", "Чем ниже значение, тем выше качество и больше файл. 24 — сбалансированное значение по умолчанию."}},
        {"videoBitrate",{"Bitrate", "Bitrate", "Bitrate", "Débit", "Bitrate", "ビットレート", "比特率", "Битрейт"}},
        {"audioMode",{"Ses", "Audio", "Audio", "Audio", "Audio", "音声", "音频", "Аудио"}},
        {"audioNone",{"Ses yok", "No audio", "Kein Audio", "Sans audio", "Sin audio", "音声なし", "无音频", "Без аудио"}},
        {"audioDesktop",{"Masaüstü sesi", "Desktop audio", "Desktop-Audio", "Audio du bureau", "Audio del escritorio", "デスクトップ音声", "桌面音频", "Звук рабочего стола"}},
        {"audioMicrophone",{"Mikrofon", "Microphone", "Mikrofon", "Microphone", "Micrófono", "マイク", "麦克风", "Микрофон"}},
        {"audioDesktopMic",{"Masaüstü + mikrofon", "Desktop + microphone", "Desktop + Mikrofon", "Bureau + microphone", "Escritorio + micrófono", "デスクトップ + マイク", "桌面 + 麦克风", "Рабочий стол + микрофон"}},
        {"audioSource",{"Kaynak", "Source", "Quelle", "Source", "Fuente", "ソース", "来源", "Источник"}},
        {"audioMicrophoneDevice",{"Mikrofon", "Microphone", "Mikrofon", "Microphone", "Micrófono", "マイク", "麦克风", "Микрофон"}},
        {"audioNoDevice",{"Uygun cihaz yok", "No compatible device", "Kein kompatibles Gerät", "Aucun appareil compatible", "No hay dispositivo compatible", "対応デバイスなし", "没有兼容设备", "Нет совместимого устройства"}},
        {"audioSystemLoopback",{"Sistem sesi", "System audio", "Systemaudio", "Audio système", "Audio del sistema", "システム音声", "系统音频", "Системный звук"}},

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
        {"visualSearchBrowserLaunchTitle", {"Tarayıcı açılamadı", "Browser launch failed", "Browser konnte nicht geöffnet werden", "Échec de l'ouverture du navigateur", "No se pudo abrir el navegador", "ブラウザーを開けませんでした", "无法打开浏览器", "Не удалось открыть браузер"}},
        {"visualSearchBrowserLaunchError", {"Görsel arama sonucu varsayılan tarayıcınızda açılamadı.", "The visual-search result could not be opened in your default browser.", "Das Ergebnis der visuellen Suche konnte nicht im Standardbrowser geöffnet werden.", "Le résultat de recherche visuelle n'a pas pu être ouvert dans votre navigateur par défaut.", "El resultado de búsqueda visual no pudo abrirse en el navegador predeterminado.", "画像検索結果を既定のブラウザーで開けませんでした。", "无法在默认浏览器中打开视觉搜索结果。", "Не удалось открыть результат визуального поиска в браузере по умолчанию."}},
        {"uploadLinkCopied",{"Bağlantı panoya kopyalandı", "Link copied to clipboard", "Link in die Zwischenablage kopiert", "Lien copié", "Enlace copiado", "リンクをコピーしました", "链接已复制", "Ссылка скопирована"}},
        {"uploadOpen",     {"Bağlantıyı Aç", "Open Link", "Link öffnen", "Ouvrir le lien", "Abrir enlace", "リンクを開く", "打开链接", "Открыть ссылку"}},
        {"uploadLinkPlaceholder",{"Görsel bağlantısı burada görünecek", "Image link will appear here", "Bildlink erscheint hier", "Le lien apparaîtra ici", "El enlace aparecerá aquí", "リンクがここに表示されます", "链接将显示在此处", "Ссылка появится здесь"}},
        {"uploadDeletePlaceholder",{"Silme bağlantısı (opsiyonel)", "Delete link (optional)", "Löschlink (optional)", "Lien de suppression (optionnel)", "Enlace de borrado (opcional)", "削除リンク（任意）", "删除链接（可选）", "Ссылка на удаление (необязательно)"}},
        {"yandexAuthPlaceholder",{"Yandex OAuth tokeni veya yönlendirme URL'si", "Yandex OAuth token or redirect URL", "Yandex-OAuth-Token oder Weiterleitungs-URL", "Jeton OAuth Yandex ou URL de redirection", "Token OAuth de Yandex o URL de redirección", "Yandex OAuth トークンまたはリダイレクト URL", "Yandex OAuth 令牌或重定向 URL", "OAuth-токен Yandex или URL перенаправления"}},
        {"googleDriveAuthPlaceholder",{"Google Drive OAuth tokeni veya yönlendirme URL'si", "Google Drive OAuth token or redirect URL", "Google-Drive-OAuth-Token oder Weiterleitungs-URL", "Jeton OAuth Google Drive ou URL de redirection", "Token OAuth de Google Drive o URL de redirección", "Google Drive OAuth トークンまたはリダイレクト URL", "Google Drive OAuth 令牌或重定向 URL", "OAuth-токен Google Drive или URL перенаправления"}},
        {"catboxUserHashPlaceholder",{"İsteğe bağlı Catbox user hash", "Optional Catbox user hash", "Optionaler Catbox-User-Hash", "Hash utilisateur Catbox facultatif", "Hash de usuario de Catbox opcional", "任意の Catbox ユーザーハッシュ", "可选 Catbox 用户哈希", "Необязательный user hash Catbox"}},
        {"uploadAuthHelpYandex",{"Yandex Disk icin Data access altında cloud_api:disk.write ve cloud_api:disk.read izinleri olan OAuth access_token gerekir. Client ID, Client secret veya code degil; tokeni ya da access_token iceren tam yonlendirme URL'sini yapistirin. <a href=\"https://yandex.com/dev/disk-api/doc/en/concepts/quickstart\">Disk API</a> | <a href=\"https://yandex.com/dev/id/doc/en/tokens/debug-token\">Token alma</a>", "Yandex Disk needs an OAuth access_token with cloud_api:disk.write and cloud_api:disk.read enabled under Data access. Do not paste the Client ID, Client secret, or code; paste the token or the full redirect URL that contains access_token. <a href=\"https://yandex.com/dev/disk-api/doc/en/concepts/quickstart\">Disk API</a> | <a href=\"https://yandex.com/dev/id/doc/en/tokens/debug-token\">Get token</a>", "Yandex Disk needs an OAuth access_token with cloud_api:disk.write and cloud_api:disk.read enabled under Data access. Do not paste the Client ID, Client secret, or code; paste the token or the full redirect URL that contains access_token. <a href=\"https://yandex.com/dev/disk-api/doc/en/concepts/quickstart\">Disk API</a> | <a href=\"https://yandex.com/dev/id/doc/en/tokens/debug-token\">Get token</a>", "Yandex Disk needs an OAuth access_token with cloud_api:disk.write and cloud_api:disk.read enabled under Data access. Do not paste the Client ID, Client secret, or code; paste the token or the full redirect URL that contains access_token. <a href=\"https://yandex.com/dev/disk-api/doc/en/concepts/quickstart\">Disk API</a> | <a href=\"https://yandex.com/dev/id/doc/en/tokens/debug-token\">Get token</a>", "Yandex Disk needs an OAuth access_token with cloud_api:disk.write and cloud_api:disk.read enabled under Data access. Do not paste the Client ID, Client secret, or code; paste the token or the full redirect URL that contains access_token. <a href=\"https://yandex.com/dev/disk-api/doc/en/concepts/quickstart\">Disk API</a> | <a href=\"https://yandex.com/dev/id/doc/en/tokens/debug-token\">Get token</a>", "Yandex Disk needs an OAuth access_token with cloud_api:disk.write and cloud_api:disk.read enabled under Data access. Do not paste the Client ID, Client secret, or code; paste the token or the full redirect URL that contains access_token. <a href=\"https://yandex.com/dev/disk-api/doc/en/concepts/quickstart\">Disk API</a> | <a href=\"https://yandex.com/dev/id/doc/en/tokens/debug-token\">Get token</a>", "Yandex Disk needs an OAuth access_token with cloud_api:disk.write and cloud_api:disk.read enabled under Data access. Do not paste the Client ID, Client secret, or code; paste the token or the full redirect URL that contains access_token. <a href=\"https://yandex.com/dev/disk-api/doc/en/concepts/quickstart\">Disk API</a> | <a href=\"https://yandex.com/dev/id/doc/en/tokens/debug-token\">Get token</a>", "Yandex Disk needs an OAuth access_token with cloud_api:disk.write and cloud_api:disk.read enabled under Data access. Do not paste the Client ID, Client secret, or code; paste the token or the full redirect URL that contains access_token. <a href=\"https://yandex.com/dev/disk-api/doc/en/concepts/quickstart\">Disk API</a> | <a href=\"https://yandex.com/dev/id/doc/en/tokens/debug-token\">Get token</a>"}},
        {"uploadAuthHelpGoogleDrive",{"Google Drive icin OAuth Playground access_token gerekir. Tokeni ya da Playground JSON yanitini yapistirin. <a href=\"https://github.com/Benoks/EShot#google-drive-token-setup\">README adimlari</a>", "Google Drive needs an OAuth Playground access_token. Paste the token or the full Playground JSON response. <a href=\"https://github.com/Benoks/EShot#google-drive-token-setup\">README steps</a>", "Google Drive needs an OAuth Playground access_token. Paste the token or the full Playground JSON response. <a href=\"https://github.com/Benoks/EShot#google-drive-token-setup\">README steps</a>", "Google Drive needs an OAuth Playground access_token. Paste the token or the full Playground JSON response. <a href=\"https://github.com/Benoks/EShot#google-drive-token-setup\">README steps</a>", "Google Drive needs an OAuth Playground access_token. Paste the token or the full Playground JSON response. <a href=\"https://github.com/Benoks/EShot#google-drive-token-setup\">README steps</a>", "Google Drive needs an OAuth Playground access_token. Paste the token or the full Playground JSON response. <a href=\"https://github.com/Benoks/EShot#google-drive-token-setup\">README steps</a>", "Google Drive needs an OAuth Playground access_token. Paste the token or the full Playground JSON response. <a href=\"https://github.com/Benoks/EShot#google-drive-token-setup\">README steps</a>", "Google Drive needs an OAuth Playground access_token. Paste the token or the full Playground JSON response. <a href=\"https://github.com/Benoks/EShot#google-drive-token-setup\">README steps</a>"}},
        {"uploadAuthHelpCatbox",{"Catbox user hash isteğe bağlıdır. Boş bırakırsanız anonim yüklenir; user hash girerseniz dosya hesabınızda görünür.", "Catbox user hash is optional. Leave it empty for anonymous upload, or paste your user hash to attach uploads to your account.", "Der Catbox-User-Hash ist optional. Leer lassen für anonymes Hochladen oder einfügen, damit Uploads Ihrem Konto zugeordnet werden.", "Le hash utilisateur Catbox est facultatif. Laissez vide pour un envoi anonyme ou collez votre hash pour lier les envois à votre compte.", "El hash de usuario de Catbox es opcional. Déjalo vacío para subir de forma anónima o pégalo para vincular las subidas a tu cuenta.", "Catbox ユーザーハッシュは任意です。匿名アップロードなら空のまま、アカウントに紐付ける場合は貼り付けてください。", "Catbox 用户哈希是可选的。留空即匿名上传；粘贴用户哈希可将上传关联到你的账户。", "User hash Catbox необязателен. Оставьте пустым для анонимной загрузки или вставьте hash, чтобы привязать файлы к аккаунту."}},
        {"uploadAuthSaved", {"Kimlik bilgisi kaydedildi.", "Credentials saved.", "Anmeldedaten gespeichert.", "Identifiants enregistrés.", "Credenciales guardadas.", "認証情報を保存しました。", "凭据已保存。", "Учётные данные сохранены."}},
        {"uploadAuthHelpApiKey",{"%1 API key gerekir. API key'i servisin API/account sayfasından alın ve buraya yapıştırın.", "%1 needs an API key. Get it from the service API/account page and paste it here.", "%1 needs an API key. Get it from the service API/account page and paste it here.", "%1 needs an API key. Get it from the service API/account page and paste it here.", "%1 needs an API key. Get it from the service API/account page and paste it here.", "%1 needs an API key. Get it from the service API/account page and paste it here.", "%1 needs an API key. Get it from the service API/account page and paste it here.", "%1 needs an API key. Get it from the service API/account page and paste it here."}},
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
        {"uploadErrorYandexScopeMissing",{"Yandex Disk izni eksik (HTTP 403). Yandex OAuth uygulamasında Data access altında cloud_api:disk.write ve cloud_api:disk.read izinlerini açın, sonra yeni access_token alın.", "Yandex Disk permission is missing (HTTP 403). In the Yandex OAuth app, enable cloud_api:disk.write and cloud_api:disk.read under Data access, then issue a new access_token.", "Yandex Disk permission is missing (HTTP 403). In the Yandex OAuth app, enable cloud_api:disk.write and cloud_api:disk.read under Data access, then issue a new access_token.", "Yandex Disk permission is missing (HTTP 403). In the Yandex OAuth app, enable cloud_api:disk.write and cloud_api:disk.read under Data access, then issue a new access_token.", "Yandex Disk permission is missing (HTTP 403). In the Yandex OAuth app, enable cloud_api:disk.write and cloud_api:disk.read under Data access, then issue a new access_token.", "Yandex Disk permission is missing (HTTP 403). In the Yandex OAuth app, enable cloud_api:disk.write and cloud_api:disk.read under Data access, then issue a new access_token.", "Yandex Disk permission is missing (HTTP 403). In the Yandex OAuth app, enable cloud_api:disk.write and cloud_api:disk.read under Data access, then issue a new access_token.", "Yandex Disk permission is missing (HTTP 403). In the Yandex OAuth app, enable cloud_api:disk.write and cloud_api:disk.read under Data access, then issue a new access_token."}},
        {"uploadErrorYandexStep",{"Yandex %1 hatası (HTTP %2)", "Yandex %1 error (HTTP %2)", "Yandex-%1-Fehler (HTTP %2)", "Erreur Yandex %1 (HTTP %2)", "Error de Yandex %1 (HTTP %2)", "Yandex %1 エラー (HTTP %2)", "Yandex %1 错误 (HTTP %2)", "Ошибка Yandex %1 (HTTP %2)"}},
        {"uploadErrorYandexUploadUrlMissing",{"Yandex yükleme URL'si bulunamadı", "Yandex upload URL missing", "Yandex-Upload-URL fehlt", "URL de téléversement Yandex manquante", "Falta la URL de subida de Yandex", "Yandex アップロード URL がありません", "缺少 Yandex 上传 URL", "Отсутствует URL загрузки Yandex"}},
        {"uploadErrorYandexPublicLinkMissing",{"Yandex herkese açık bağlantı döndürmedi", "Yandex public link missing", "Öffentlicher Yandex-Link fehlt", "Lien public Yandex manquant", "Falta el enlace público de Yandex", "Yandex 公開リンクがありません", "缺少 Yandex 公开链接", "Отсутствует публичная ссылка Yandex"}},
        {"uploadErrorGoogleTokenMissing",{"Google Drive tokeni eksik", "Google Drive token missing", "Google-Drive-Token fehlt", "Jeton Google Drive manquant", "Falta el token de Google Drive", "Google Drive トークンがありません", "缺少 Google Drive 令牌", "Отсутствует токен Google Drive"}},
        {"uploadErrorGoogleAuthFailed",{"Google Drive yetkilendirmesi başarısız (HTTP 401). Sadece access_token değerini yapıştırın veya OAuth Playground'dan yeni token alın; token süresi dolmuş olabilir.", "Google Drive authorization failed (HTTP 401). Paste only the access_token value or issue a new token in OAuth Playground; the token may have expired.", "Google Drive authorization failed (HTTP 401). Paste only the access_token value or issue a new token in OAuth Playground; the token may have expired.", "Google Drive authorization failed (HTTP 401). Paste only the access_token value or issue a new token in OAuth Playground; the token may have expired.", "Google Drive authorization failed (HTTP 401). Paste only the access_token value or issue a new token in OAuth Playground; the token may have expired.", "Google Drive authorization failed (HTTP 401). Paste only the access_token value or issue a new token in OAuth Playground; the token may have expired.", "Google Drive authorization failed (HTTP 401). Paste only the access_token value or issue a new token in OAuth Playground; the token may have expired.", "Google Drive authorization failed (HTTP 401). Paste only the access_token value or issue a new token in OAuth Playground; the token may have expired."}},
        {"uploadErrorGoogleFileIdMissing",{"Google Drive dosya kimliği döndürmedi", "Google Drive file ID missing", "Google-Drive-Datei-ID fehlt", "ID de fichier Google Drive manquant", "Falta el ID del archivo de Google Drive", "Google Drive ファイル ID がありません", "缺少 Google Drive 文件 ID", "Отсутствует ID файла Google Drive"}},
        {"uploadApiKeyPlaceholder",{"%1 API key", "%1 API key", "%1 API key", "%1 API key", "%1 API key", "%1 API key", "%1 API key", "%1 API key"}},
        {"uploadErrorApiKeyMissing",{"%1 API key eksik", "%1 API key missing", "%1 API key missing", "%1 API key missing", "%1 API key missing", "%1 API key missing", "%1 API key missing", "%1 API key missing"}},
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
        {"catboxUserHash", {"Catbox Kullanıcı Hash:", "Catbox User Hash:", "Catbox Benutzer-Hash:", "Hash utilisateur Catbox :", "Hash de usuario Catbox:", "Catbox ユーザーハッシュ:", "Catbox 用户哈希:", "Хэш пользователя Catbox:"}},
        {"catboxUserHashDesc",{"İsteğe bağlı. Hesabınızla yükleme/düzenleme/silme için catbox.moe → Hesap.", "Optional. For upload/edit/delete with your account: catbox.moe → Account.", "Optional. Für Hochladen/Bearbeiten/Löschen mit Konto: catbox.moe → Konto.", "Optionnel. Pour téléverser/modifier/supprimer avec votre compte : catbox.moe → Compte.", "Opcional. Para subir/editar/borrar con su cuenta: catbox.moe → Cuenta.", "任意。アカウントでアップロード/編集/削除する場合: catbox.moe → アカウント", "可选。使用您的账户上传/编辑/删除：catbox.moe → 账户", "Необязательно. Для загрузки/правки/удаления в своём аккаунте: catbox.moe → Аккаунт."}},
    };

    static constexpr int s_transCount = sizeof(s_trans) / sizeof(s_trans[0]);
};

#endif
