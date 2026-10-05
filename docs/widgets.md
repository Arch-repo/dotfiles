# Widget nativi

I widget sono superfici GTK4 del desktop, gestite da un unico processo `anto-widgets`. Il catalogo disponibile in **Impostazioni → Widget** consente di aggiungere o rimuovere ogni scheda dal desktop. La rimozione conserva il widget nel catalogo e la sua disposizione salvata, per poterlo riattivare.

Il catalogo comprende Orologio, Il tuo computer, Agenda e Musica. La scheda Musica unisce player e spettro audio e usa MPRIS: play, pausa, traccia precedente e successiva rispettano le capacità del player. Per i player remoti di KDE Connect lo spettro continua a rappresentare l’audio del PC: i dati MPRIS non trasportano il segnale audio del dispositivo remoto. La vista lo indica esplicitamente. Le copertine locali sono supportate; quelle disponibili soltanto attraverso URL remoti non vengono scaricate. L’agenda legge gli stessi eventi locali e Google del menu; la sincronizzazione del feed resta responsabilità del servizio calendario.

## Confini del sistema

| Percorso | Responsabilità |
| --- | --- |
| `native/widgets/widgets.h` | Contratto di una scheda, stato condiviso e API del runtime |
| `native/widgets/views/<scheda>.c` | Dichiarazione, composizione e binding di una sola scheda |
| `native/widgets/ui/spectrum.c` | Disegno e smoothing dello spettro dentro la scheda Musica |
| `native/widgets/views/view.c` | Pannello, intestazione e apertura della pagina correlata nel menu |
| `native/widgets/core/runtime.c` | Riconciliazione delle superfici per monitor, singleton e ciclo di vita |
| `native/widgets/core/window.c` | Layer desktop, trascinamento, ridimensionamento e limiti geometrici |
| `native/widgets/core/settings.c` | Importazione dei dati legacy e persistenza atomica |
| `native/widgets/core/main.c` | CLI e azioni inviate al proprietario attraverso D-Bus |
| `native/widgets/data/*.c` | Clock, sistema, agenda, MPRIS e flusso audio condivisi |
| `scripts/widgets_registry.py` | Registro generato dalle dichiarazioni dei file C |
| `native/menu/src/modules/widgets/view.c` | Catalogo e preferenze nel menu |
| `native/widgets/assets/widgets.css.in` | Layout specifico delle schede, con token condivisi |

Le viste riusano `native/common` per testi, icone, pulsanti, metriche, campi, liste, scorrimento, palette e vetro. Il materiale del pannello è la stessa primitiva `ui-glass` del menu: non contiene una copia separata dei colori o dell’effetto. Ogni processo osserva la palette una volta. Il modello eventi è condiviso in `native/common/src/calendar_data.c`.

Le superfici usano il layer **BOTTOM**, namespace `anto426-widget`, tastiera **NONE** e nessuna zona esclusiva. I clic raggiungono i controlli senza assegnare il focus della tastiera ai widget. Le normali finestre restano davanti alle schede. Il blocco riguarda la disposizione: i controlli musicali rimangono sempre interattivi. In modalità modifica si trascina l’intestazione e si ridimensiona dall’angolo inferiore destro.

Il motore segue gli eventi GDK dei monitor, senza interrogare `hyprctl` in un ciclo. Le schede sullo stesso monitor vengono riconciliate; rimuovere un output distrugge le sue superfici. Cambi di geometria riapplicano i limiti del monitor. Non vengono avviati terminali, shell di rendering o processi per ciascun monitor.

I dati sono letti una volta e condivisi: hardware ogni due secondi da `/proc` e sysfs; agenda con file monitor e task cancellabili; musica con segnali D-Bus. Lo spettro usa un solo processo **CAVA come analizzatore audio**, con output numerico PipeWire; il disegno e lo smoothing sono nativi GTK. CAVA è avviato soltanto se serve lo spettro. La disattivazione delle schede ricostruisce le sottoscrizioni necessarie; modificare il blocco delle posizioni conserva i servizi attivi.

## Aggiungere una scheda

Si aggiunge un file C in `native/widgets/views`, con una factory che riceve `WidgetStore *` e restituisce il corpo GTK. La factory costruisce le primitive comuni, ascolta soltanto i campi necessari del segnale `changed` e aggiorna le proprietà dei controlli senza ricostruire il pannello. Usare `g_signal_connect_object` con la vista come destinatario evita callback dopo la sua distruzione.

Nello stesso file si aggiunge **una sola dichiarazione**:

```c
WIDGET_DECLARE(nome_stabile, "Nome nel catalogo", "icona-symbolic",
               "Descrizione", "pagina_menu", "alias_legacy",
               420, 240, TRUE, FALSE, 380,
               W_CHANGED_MEDIA, crea_la_scheda);
```

Le dimensioni sono in pixel logici; `TRUE, FALSE` allinea a destra e conserva una coordinata verticale predefinita. Il bitmask dichiara i dati richiesti. L’identificatore stabile è la chiave delle preferenze: cambiare il nome visualizzato non perde la disposizione.

CMake rileva il nuovo file, genera il registro e aggiorna anche il catalogo delle impostazioni. Non serve modificare una lista centrale di widget o aggiungere rami condizionali al motore. Per un nuovo tipo di dati, aggiungere un provider in `data`, il relativo campo/signal bit nel contratto e il suo avvio/arresto nel ciclo di vita del modello; evitare letture o subprocessi dentro una singola vista.

```sh
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build
```

Il renderer supporta fino a 64 schede dichiarate e 16 output. Le fixture verificano il rilevamento di una nuova dichiarazione, duplicati, importazione delle posizioni, visibilità persistente, cambi simultanei, singleton, avvio/arresto, agenda e controlli MPRIS. `scripts/verify_widgets.py` aggiunge una prova con un puntatore Wayland reale e una finestra che mantiene il focus.

## Persistenza e comandi

La configurazione è `~/.config/anto426-local/widgets/state.json`, versione 2, con geometria separata per connettore. Al primo avvio vengono importati `settings.env`, `custom.env` e `layout.env` come dati: i vecchi comandi non sono eseguiti e i file legacy non sono sovrascritti. Gli identificatori `scheda_macchina` e `calendario` vengono associati alle rispettive schede native; `spettro_audio`, `spectrum` e `spotify` confluiscono in Musica. La versione 1 viene aggiornata conservando le posizioni: Musica rimane attiva se era attivo il player oppure lo spettro. Il runtime attivo è l’unico autore delle modifiche, serializzate nel thread GTK; a motore fermo un lock protegge l’intera lettura/modifica/scrittura.

L’adattatore `~/.config/anto426/widgets.sh` inoltra gli argomenti al nuovo binario. Comandi principali: `start`, `stop`, `toggle`, `autostart`, `enable`, `disable`, `status`, `lock`, `unlock`, `set-visible <id> 0|1`, `set-autostart 0|1`, `reset-layout` e `list-widgets`.

`scripts/verify_widget_audio.py` verifica CAVA e PipeWire con un segnale sintetico su un sink privato, senza uscita sugli altoparlanti e senza cambiare il dispositivo predefinito. Il risultato è in `widgets-audio-verification.json`.

Fonti delle API: [gtk4-layer-shell](https://wmww.github.io/gtk4-layer-shell/), [monitor GDK](https://docs.gtk.org/gdk4/class.Monitor.html), [MPRIS Player](https://specifications.freedesktop.org/mpris/latest/Player_Interface.html) e [configurazione raw di CAVA](https://github.com/karlstav/cava/blob/master/example_files/config).
