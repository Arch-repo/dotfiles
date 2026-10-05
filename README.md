# Anto Desktop

Desktop Hyprland con menu nativi C17/C++20, GTK4 e gtk4-layer-shell. Menu e galleria sono centrati sullo schermo sotto il puntatore, con dimensioni comuni fino a 1080 × 740 pixel logici. La galleria è integrata nel menu, con ricerca, filtri immagini/video e anteprima temporanea a schermo intero dietro il pannello. L'applicazione dalla galleria riguarda sempre tutti gli schermi.

## Struttura

| Percorso | Responsabilità |
| --- | --- |
| `native/menu` | Navigazione, componenti comuni e pagine GTK4 in C |
| `native/wallpaper` | Galleria C++20, catalogo, cache e applicazione degli sfondi |
| `native/osd` | Indicatori GTK4 per volume, microfono e luminosità |
| `native/services` | Operazioni di sistema per dominio, backend indipendente dalla UI |
| `design/tokens.json` | Spaziatura, raggi, tipografia, dimensioni e materiale comuni |
| `native/common` | Primitive GTK, palette, geometria, persistenza e protocollo del vetro |
| `native/shell/assets` | Stili Waybar e SwayNC generati dagli stessi token |
| `native/widgets`, `native/calendar`, `native/status` | Componenti nativi separati |
| `support` | Adattatori di compatibilità, sincronizzazione e integrazioni dei temi |
| `.config` | Configurazione statica delle applicazioni |
| `scripts` | Installazione, risorse verificate e diagnostica |

Le preferenze modificabili stanno in `~/.config/anto426-local`, i dati in `~/.local/share/anto426`, i file derivati nella cache. I binari si compilano con CMake, prima dell'installazione. Nessun menu compila codice durante l'apertura.

Ogni pagina del menu ha una directory in `native/menu/src/modules`, con vista, stato, azioni e binding separati quando necessari e un contratto privato `internal.h`. La galleria ha builder distinti per finestra, intestazione, ricerca, raccolta, anteprima e footer; OSD separa vista, comandi e ciclo di vita. Le primitive riutilizzabili sono in `native/common/src/ui`; i componenti specifici del menu restano in `native/menu/src/ui`.

`scripts/design.py` genera costanti C/C++, stili GTK3/GTK4 e configurazione del terminale dai token. `native/common/assets/material.css.in` definisce un unico materiale di vetro, colorato dalla palette, per menu, galleria, OSD, Waybar e notifiche. `design/surface-controls.json` adatta gli stessi corpi delle regole ai diversi alberi di widget. I file in `build/design` e gli stili installati sono derivati: si modificano i token o i template `.css.in`. La palette degli sfondi rimane dinamica e viene osservata una volta per processo; un aggiornamento incompleto conserva l'ultima palette valida.

## Installazione

Richiede GTK4 >= 4.22, gtk4-layer-shell >= 1.3, GLib/GIO, json-c, Wayland, Noto Color Emoji, Python3, CMake, Ninja e compilatori C17/C++20. Login e test richiedono Qt6 >= 6.5 (Quick, QML, GUI e Test); il lock usa Hyprlock >= 0.9 e fprintd per l’impronta. Il renderer esterno richiede anche un compilatore C++23 e gli header della stessa revisione di Hyprland in esecuzione. Le dipendenze delle singole funzioni sono documentate nel [report](docs/reconstruction.md).

```sh
python3 scripts/resources.py
python3 scripts/install.py
```

Per importare il backup storico già esistente senza sovrascrivere preferenze nuove:

```sh
python3 scripts/install.py --restore /percorso/dotfiles-backup-20261002-154626
```

L'installazione non crea backup. Non copia credenziali o vecchi eseguibili. Non modifica il sistema come amministratore. Le impostazioni dei temi delle applicazioni vengono installate in directory utente reali, per evitare che la generazione dei colori modifichi i sorgenti.

## Uso

La galleria è una pagina del menu principale, con la stessa finestra e la stessa navigazione. La larghezza del pannello e della barra laterale rimane fissa passando tra le pagine. `anto-menu system` apre il centro di controllo; `anto-menu --list-pages` elenca tutte le pagine. `anto-wallpaper` apre la pagina Sfondi, `anto-wallpaper --random` applica uno sfondo della raccolta. `anto-menu settings` gestisce le destinazioni della palette e le preferenze. `Esc` chiude sempre il menu, anche durante la ricerca; `Ctrl+Backspace` torna alla pagina precedente.

Applicare uno sfondo chiude subito il menu e avvia un processo indipendente in background. Gli errori del job vengono registrati in `~/.local/state/anto426/wallpaper-jobs.log` e notificati senza trattenere la finestra.

L’anteprima di Sfondi riprende il legacy: la selezione viene mostrata temporaneamente a tutto schermo dietro il menu, senza modificare il desktop. La pagina contiene la raccolta, senza riquadro di anteprima, nome file o fascia delle azioni. Invio dalla ricerca sposta il focus nella raccolta; Invio o Spazio sulla selezione applica.

**GRUB e login** è attivo per impostazione predefinita, insieme alle altre destinazioni della palette in Impostazioni. Ogni cambio sfondo aggiorna anche i temi di avvio e login in background. Il renderer prepara una generazione privata prima di sostituire gli asset nei temi predisposti. Gli errori vengono notificati.

**Impostazioni → Widget** gestisce le schede native del desktop: orologio, sistema, agenda e Musica, con player e spettro audio nella stessa scheda. Si possono aggiungere o rimuovere dal desktop, disporre per monitor e bloccare le posizioni conservando i controlli interattivi. Il nuovo motore GTK4 usa un solo processo e dati condivisi, con tastiera `NONE`. Ogni scheda è dichiarata nel proprio file C e compare automaticamente nel catalogo: architettura e procedura di estensione sono in [Widget nativi](docs/widgets.md).

## Vetro della shell

Il renderer [HyprGlass](https://github.com/hyprnux/hyprglass), fissato in `resources.lock.json` e adattato dalla patch in `support/patches`, campiona il framebuffer del compositore. Menu, galleria, widget e OSD comunicano forma e regione del pannello; Waybar e notifiche usano le sagome alpha dei singoli elementi. Ghostty usa lo stesso preset del vetro. Il testo viene composto sopra il materiale.

Il campionamento segue i frame danneggiati del compositore, senza un limite fisso di frequenza; le scene statiche usano la cache. Non vengono catturati screenshot nel percorso di rendering. L'installer controlla revisione degli header, sorgenti e binario, quindi scrive il percorso assoluto del plugin nella configurazione locale. Dopo un aggiornamento di Hyprland occorre aggiornare il lock e ricompilare il plugin compatibile.

`python3 scripts/doctor.py` confronta gli artefatti installati con la build. `python3 scripts/verify_glass.py` verifica i campioni del compositore e i pixel delle superfici in una sessione Wayland annidata, con uno sfondo sintetico variabile. Richiede Pillow e ripristina il workspace iniziale; le notifiche di prova appartengono soltanto alla sessione di test.

Scorciatoie: Alt+Spazio applicazioni, Super+W sfondi, Super+P impostazioni, Super+V appunti, Super+period emoji. Le altre scorciatoie del backup sono conservate nella pagina Tastiera.

Per verificare una modifica:

```sh
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build
```

Le integrazioni esterne comprendono Waybar, SwayNC, mpvpaper/awww, NetworkManager, BlueZ, PipeWire, Ghostty e strumenti di cattura. Il backend espone contratti verificabili per 16 servizi e un runtime comune per comandi, protocollo e persistenza; conserva gli algoritmi e le integrazioni dei provider già collaudati. Gli adattatori shell rimanenti sono espliciti in `support`.

## Login, lock e avvio

Il login Qt6 è in `native/login`, suddiviso in pannello, controlli, selettori e
form di autenticazione. I token sono compilati anche in `Tokens.qml`.
Il layout Hyprlock proviene da `native/lock/hyprlock.conf.in`, con palette
completa di fallback, password PAM e impronta parallela tramite fprintd.
Il lettore deve avere almeno un’impronta già registrata.

Il renderer `support/compat/boot_render.py` produce il pannello GRUB, le nove
parti della selezione e una composizione centrata proporzionata alla risoluzione.
GRUB usa il tema di [grub2-themes](https://github.com/Arch-repo/grub2-themes) per
font e icone; lo sfondo e la palette vengono aggiornati dal worker dei dotfiles.
Il vetro di avvio è statico; il login sfoca lo sfondo con Qt6 MultiEffect e il
lock usa il renderer di Hyprlock.

Per predisporre il tema SDDM e i permessi dei temi di sistema dopo la build:

```sh
sudo python3 scripts/install_system_themes.py --user "$USER" --activate
```

Questo comando non riavvia SDDM e non cambia PAM o le voci di avvio.
La verifica grafica è documentata in [Login e lock](docs/login-lock.md).

Rofi e hyprquickpaper non fanno parte della nuova shell. Il catalogo emoji
proviene da dati Unicode versionati e verificati da `scripts/resources.py`,
con licenza inclusa; viene letto direttamente dal picker GTK4.
