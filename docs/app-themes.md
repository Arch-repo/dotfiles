# Temi delle applicazioni

GTK, Qt, VS Code e Obsidian ricevono i ruoli della palette prodotta dal worker nativo degli
sfondi. L'accento non viene rimescolato dai singoli temi: editor, menu e shell
usano gli stessi colori di origine.

## Responsabilità

| Componente | Responsabilità |
| --- | --- |
| [vscode-theme](https://github.com/Arch-repo/vscode-theme) | Generatore canonico `palette/generate.cjs`, colori del workbench, token semantici e TextMate, estensione desktop e browser |
| [obsidian-theme](https://github.com/Arch-repo/obsidian-theme) | Fork di Minimal 9.1.1, CSS di base e mapping canonico `palette/template.css` |
| [gtk-theme](https://github.com/Arch-repo/gtk-theme) | Base Orchis mantenuta, renderer GTK3, ruoli GTK4/libadwaita e fallback named colors |
| [qt-theme](https://github.com/Arch-repo/qt-theme) | Renderer Qt5/Qt6, asset Kvantum, primitive Widgets e stile Qt Quick |
| `resources.lock.json` | Revisioni immutabili e SHA-256 delle risorse dei quattro temi, incluse le licenze |
| `scripts/resources.py` | Download e verifica delle sole risorse necessarie alla generazione |
| `support/wallpaper_effects.d/toolkit_theme.sh` | Bridge GTK/Qt senza mapping duplicati nei dotfiles |
| `support/wallpaper_effects.d/editor_theme.sh` | Passaggio dei ruoli nativi ai renderer e all'installer condiviso |
| `support/compat/app_themes.py` | Installazione atomica, aggiornamento delle preferenze JSONC e selezione dei temi e aggiornamento degli INI senza perdere font/commenti |

Non occorre un clone Git durante un cambio sfondo. Le risorse installate sono in
`$XDG_DATA_HOME/anto-desktop/{gtk-theme,qt-theme,vscode-theme,obsidian-theme}`, con il normale fallback
`~/.local/share`. Ogni job riceve uno snapshot privato della palette e si interrompe
se appartiene a uno sfondo ormai sostituito. La generazione rimane in background.

## Installazione e preferenze

```sh
python3 scripts/resources.py
python3 scripts/install.py
```

L'estensione completa di VS Code si installa dal suo repository con
`./install-anto426.sh`: il comando compila, verifica, crea il VSIX e lo installa
con la CLI di Code. L'installer `arch-hyprland` richiama questo stesso comando.
La versione corrente è 0.2.0, con minimo dichiarato VS Code 1.100. L'ID
`Anto426.anto426-vscode-theme`, l'etichetta `Anto426 Rofi Dynamic` e i comandi
esistenti rimangono compatibili. Rofi non viene eseguito dal desktop.

Le impostazioni dell'editor ricevono override limitati al tema selezionato;
commenti JSONC, profili e preferenze estranee sono conservati. La palette corrente
è salvata in `anto-desktop/vscode-palette.json`, così grassetto, corsivo e colori
personalizzati dell'estensione continuano a funzionare dopo il cambio sfondo.
Un aggiornamento del codice dell'estensione richiede la normale ricarica della
finestra; gli aggiornamenti successivi della palette si applicano mentre è aperta.

Obsidian installa **Anto426 Monet** 1.0.0 nelle raccolte registrate dall'app, con
minimo dichiarato 1.13.0. Installa il tema nella directory `.obsidian/themes` e
abilita lo snippet `anto426-palette.css`. Note, plugin e altri snippet sono
conservati. Lo snippet separato permette l'aggiornamento della palette mentre
Obsidian è aperto. Per raccolte gestite esternamente si può impostare
`ANTO426_OBSIDIAN_VAULTS` con percorsi separati da `:`.

**Impostazioni → Editor** e **Impostazioni → Obsidian** controllano separatamente
gli aggiornamenti automatici. Disattivare una destinazione conserva i colori già
installati; dopo averla disattivata si può scegliere un altro tema nell'app.

## GTK, libadwaita e Qt

La generazione è stata spostata nelle repository dei temi. Il worker nativo
continua a produrre i ruoli della palette; `application-material.json`, generato
da `design/tokens.json`, passa opacità e raggi condivisi agli stessi renderer.
I dotfiles installano gli artefatti senza compilare SCSS durante il cambio sfondo.

Il fork GTK è stato confrontato con Orchis al commit
`a4c48a425e63808345e824495964bd9c2f462e04` del 14 settembre 2026: i selettori
Nautilus aggiornati sono integrati. GTK3 conserva il fallback di controlli
compilato; libadwaita conserva la geometria nativa aggiornata e riceve i ruoli
semantici. Sidebar, seconda sidebar, headerbar, card, dialoghi, popover, toggle,
overview e thumbnail hanno mapping espliciti. Font e helper di accessibilità
rimangono gestiti dal toolkit. GTK4 prima della 4.16 riceve automaticamente il
percorso con named colors, senza variabili CSS o `color-mix` incompatibili.
La base completa dei controlli precedenti rimane in GTK3 e nelle app GTK4 senza
libadwaita. Un overlay condiviso copre testo delle azioni e delle selezioni,
placeholder, stati semantici e slider; anche i colori storici di errore, avviso
e successo nel CSS compilato ricevono la palette corrente.

Qt5 riceve 21 ruoli, Qt6.6+ 22 con Accent; Qt6 precedente conserva il fallback
con 21 ruoli. PlaceholderText usa il colore muted opaco. Kvantum si occupa dello
sfondo dei Widgets; `QT_STYLE_OVERRIDE` viene svuotata perché non scavalchi la
palette di qt5ct/qt6ct. La stessa chiave `QT_QPA_PLATFORMTHEME=qt5ct` è supportata
dal Qt5ct installato e dal Qt6ct corrente. I controlli Widgets ricevono anche il foglio di stile condiviso: campi, pulsanti,
indicatori, spin/combo con popup e frecce, slider, progress, scroll, viste/header,
tab, menu, toolbar e gruppi. I colori fissi della base Kvantum sono tutti mappati,
compresi i focus blu che rimanevano fuori dalla palette. I placeholder conservano
il ruolo nativo anche in Qt5, senza proprietà QSS introdotte solo in Qt6.5.

Qt Quick usa ora lo stile **Anto426**: 47 tipi nativi Templates, in file separati,
con primitive condivise per superficie, testo, indicatori e thumb. Palette,
raggi, spaziature e alpha provengono dai token della shell. Fusion rimane il
fallback per tipi nuovi non implementati; i controlli mantengono comportamento,
accessibilità e modelli di Qt. La ricerca dello stile comprende sia i percorsi
QML di Qt6 sia `QT_QUICK_CONTROLS_STYLE_PATH`, necessario per Qt5 e rimosso da
Qt6. Lo stile Quick richiede Qt5.15+ o Qt6; i Widgets conservano Qt5.12+.

È **un solo tema con percorsi di compatibilità**, senza creare un tema Legacy
separato. Le risorse originali GPL sono incluse. L'installer conserva font,
commenti, CSS personali e fogli di stile Qt esterni; sostituisce soltanto le
chiavi gestite e la precedente sezione CSS generata. Il menu mantiene una base GTK privata per la shell; il launcher delle
applicazioni e quello dei comandi rimuovono `GTK_THEME` dall'ambiente dei figli.
Altrimenti `Adwaita:dark` passa anche al file manager e scavalca il tema scelto
nelle GtkSettings, come rilevato nella sessione reale. Un test lancia una vera
voce desktop e controlla l'ambiente del processo figlio, conservando quello del
menu. Nemo è stato riaperto e verificato senza questo override.

GTK/libadwaita e molte app
Qt possono richiedere la riapertura per ricaricare uno stile già in memoria.

### LibreOffice

LibreOffice 26.8 usa GTK3 per i controlli, ma la vista dei documenti recenti è
disegnata da VCL: `RecentDocsView::UpdateColors` legge una preferenza propria,
il cui valore predefinito è `#666666`. Il CSS GTK non raggiunge questa superficie.
Il renderer GTK emette anche `libreoffice.json`, con il fondo del desktop e il
testo condivisi. `support/compat/libreoffice_theme.py` aggiorna i due colori dei
documenti recenti e `AppBackground` dello schema attualmente selezionato.

L'aggiornamento passa attraverso la Configuration API di LibreOffice, quindi
funziona mentre l'app è aperta senza modificare il file delle preferenze dietro
al processo. Una pipe UNO locale temporanea viene chiusa dopo il lavoro; se
l'app non è aperta, un'istanza senza finestre iniziali aggiorna il profilo e viene
terminata. Mantiene il backend grafico, così un documento aperto dall'utente
durante il job può apparire normalmente e conserva la propria sessione.
Lo schema selezionato, i colori dei documenti e le altre preferenze rimangono
invariati. La stessa fase GTK lo aggiorna ai cambi di sfondo.

La verifica `python3 scripts/verify_libreoffice_theme.py` usa un profilo isolato
e l'app reale: conserva uno schema personale e i colori di pagina/testo,
verifica aggiornamenti live, idempotenza e persistenza dopo il riavvio.
Il [rapporto](libreoffice-theme-verification.json) distingue questa verifica
dalla copertura generica del toolkit. LibreOffice rende la superficie VCL dei
documenti recenti come RGB opaco: il colore è dinamico, ma non viene dichiarata
una trasparenza per quel canvas e non si sbiadisce l'intera finestra.

Riferimenti: [Appearance](https://help.libreoffice.org/latest/en-US/text/shared/optionen/01012000.html),
[parametri UNO](https://help.libreoffice.org/latest/en-US/text/shared/guide/start_parameters.html),
[RecentDocsView 26.8](https://github.com/LibreOffice/core/blob/libreoffice-26.8.0.3/sfx2/source/control/recentdocsview.cxx),
[ThumbnailView 26.8](https://github.com/LibreOffice/core/blob/libreoffice-26.8.0.3/sfx2/source/control/thumbnailview.cxx).

## Trasparenza

GTK e Kvantum usano l’alpha del fondo (opacità condivisa 0.46); il compositore
non sbiadisce testo e icone. Le regole `$antoToolkitClasses` assegnano il preset
HyprGlass a file manager e applicazioni native supportate. Nemo non riceve più
opacità diverse tra finestra attiva e inattiva; i pannelli hanno separatori
contenuti e chrome coerente con il materiale.

Le grandi superfici usano colori alpha; testo, selezioni e dialoghi conservano
colori leggibili. Una finestra Electron conserva comunque il proprio fondo
opaco: la traslucenza della finestra viene applicata da Hyprland, senza modificare
i binari delle applicazioni. L'opacità del compositore è 0.90 sia a finestra attiva
sia inattiva e coinvolge anche il testo della finestra.

Le regole condividono `$antoEditorClasses`, dichiarato in `glass.conf`, e il preset
HyprGlass `anto-desktop`. Il matcher include la classe Wayland effettivamente
osservata **`com.microsoft.VSCode`**, oltre alle varianti Code e Obsidian. Il vetro
campiona la scena del compositore come per le altre superfici della shell.

## Verifica

`scripts/verify_app_themes.py` apre applicazioni reali in una sessione Hyprland
privata con dati sintetici. Verifica attivazione dell'estensione, selezione del
tema, PTY nativo, conservazione dello stile dopo il cambio palette e assegnazione
del materiale alla finestra Wayland. Per Obsidian verifica il caricamento di una
nota e l'aggiornamento live dello snippet. Ripristina il workspace precedente.

Il [rapporto](app-themes-verification.json) registra VS Code **1.140.0** e Obsidian
**1.13.7**. I test dei renderer e dell'installer controllano valori validi,
superfici nuove, conservazione dei dati e idempotenza delle scritture. La build
nativa supera **34 test**; `scripts/doctor.py` verifica anche le risorse fissate.

Riferimenti primari: [colori VS Code](https://code.visualstudio.com/api/references/theme-color),
[token semantici](https://code.visualstudio.com/api/language-extensions/semantic-highlight-guide),
[test dell'Extension Host](https://code.visualstudio.com/api/working-with-extensions/testing-extension),
[temi Obsidian](https://github.com/obsidianmd/obsidian-developer-docs/blob/main/en/Themes/App%20themes/Build%20a%20theme.md).

Il [rapporto toolkit](toolkit-themes-verification.json) registra GTK3 **3.24.52**,
GTK4 **4.22.5**, libadwaita **1.9.4**, Qt5 **5.15.19**, Qt6/Quick **6.11.2**.
`scripts/verify_toolkit_themes.py` avvia finestre sintetiche e Nemo in una sessione
Wayland privata. Verifica parsing CSS, ruoli di colore, testo opaco, classe/tag
materiale e variazione dei pixel del fondo mentre cambia la scena sottostante.
Include GTK4 sia con libadwaita sia con la base completa del tema e verifica
anche il contrasto del testo sui pulsanti accentati. Qt5 e Qt6 verificano gli
stessi controlli Widgets e Quick: geometria condivisa, colore del focus dipinto,
placeholder, azioni da tastiera e mouse, popup e selezione delle combo. La prova
Quick crea anche i tipi secondari e richiede zero avvisi QML. GTK3 e GTK4 senza libadwaita
caricano il tema dalle normali GtkSettings, senza provider forzati dalla prova.
Le schermate condivise contengono solo dati sintetici.

I percorsi per GTK4/Qt6 storici hanno verifiche degli artefatti, ma non sono stati
eseguiti su vecchi runtime in questa macchina. Applicazioni con rendering,
palette o stili propri possono ignorare il tema; le app Flatpak richiedono
accesso ai CSS/temi dell'utente. La verifica non equivale alla copertura universale
di ogni applicazione GTK o Qt.

Riferimenti toolkit: [Orchis upstream](https://github.com/vinceliuice/Orchis-theme),
[proprietà CSS GTK](https://docs.gtk.org/gtk4/css-properties.html),
[variabili libadwaita](https://gnome.pages.gitlab.gnome.org/libadwaita/doc/1-latest/css-variables.html),
[ruoli QPalette](https://doc.qt.io/qt-6/qpalette.html),
[subcontrol Widgets](https://doc.qt.io/qt-6/stylesheet-syntax.html),
[stili Quick](https://doc.qt.io/qt-6/qtquickcontrols-customize.html),
[migrazione Qt6](https://doc.qt.io/qt-6/qtquickcontrols-changes-qt6.html),
[configurazione Qt Quick](https://doc.qt.io/qt-6/qtquickcontrols-configuration.html),
[Kvantum](https://github.com/tsujan/Kvantum/blob/master/Kvantum/doc/Theme-Config).

## Nomi delle repository

I nomi pubblici dell'ecosistema desktop sono normalizzati in minuscolo con
trattini. Le directory dei checkout già esistenti non vengono spostate.

| Nome precedente | Nome attuale |
| --- | --- |
| `Arch-Hyprland` | `arch-hyprland` |
| `Anto426-theme` | `gtk-theme` |
| `Anto426-material-icons` | `material-icons` |
| `Wallpaper-Collection` | `wallpaper-collection` |
| `vscodetheme` | `vscode-theme` |
| `obsidian-monet` | `obsidian-theme` |
| `auto-setup-LT` | `auto-setup-lt` |

`dotfiles`, `grub2-themes` e `zen-browser` erano già coerenti. I remoti dei
checkout coinvolti e i riferimenti degli installer sono stati aggiornati. GitHub
mantiene i reindirizzamenti dei vecchi URL, come documentato nella
[guida alla rinomina](https://docs.github.com/en/repositories/creating-and-managing-repositories/renaming-a-repository).
