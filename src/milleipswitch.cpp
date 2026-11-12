// MilleIpSwitch – fast network profile switcher for Windows
// SPDX-License-Identifier: MIT

#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS

#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef UNICODE
#define UNICODE
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <shellapi.h>
#include <commctrl.h>
#include <windowsx.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <knownfolders.h>
#include <icmpapi.h>
#include <cwctype>
#include <cstring>
#include <map>
#include <commdlg.h>
#include <winhttp.h>
#include <taskschd.h>
#include <setupapi.h>
#include <devguid.h>
#include <cfgmgr32.h>
#include <algorithm>
#include <cstdarg>
#include <set>
#include <cwchar>
#include <string>
#include <utility>
#include <vector>

#include "resource.h"

using std::string;
using std::vector;
using std::wstring;

static HINSTANCE g_hInst;
static wstring g_exePath, g_iniPath;

#define NS_VERSION L"1.1.1"
static int g_lang = 0;
enum { LANG_COUNT = 4 };
static const wchar_t* trLookup(const wchar_t* en);

static inline const wchar_t* TR(const wchar_t* de, const wchar_t* en) {
    return g_lang == 0 ? de : g_lang == 1 ? en : trLookup(en);
}

// Spanish / French strings, keyed by the English text
struct TrEntry { const wchar_t *en, *es, *fr; };
static const TrEntry kTr[] = {
    {L"New profile created – choose an adapter and click “Save”.", L"Perfil nuevo creado: elige un adaptador y pulsa «Guardar».", L"Nouveau profil créé – choisissez une carte puis cliquez sur « Enregistrer »."},
    {L"MilleIpSwitch is located in a folder that programs without administrator rights can modify:\n%ls\n\nAutostart runs with administrator rights. Another program could replace the exe and gain administrator rights that way. It is safer to move the exe to “C:\\Program Files\\MilleIpSwitch” and enable autostart there.\n\nEnable autostart here anyway?", L"MilleIpSwitch está en una carpeta que los programas sin derechos de administrador pueden modificar:\n%ls\n\nEl inicio automático se ejecuta con derechos de administrador. Otro programa podría sustituir el exe y obtener así derechos de administrador. Es más seguro mover el exe a «C:\\Program Files\\MilleIpSwitch» y activar allí el inicio automático.\n\n¿Activar el inicio automático aquí de todos modos?", L"MilleIpSwitch se trouve dans un dossier que des programmes sans droits d’administrateur peuvent modifier :\n%ls\n\nLe démarrage automatique s’exécute avec les droits d’administrateur. Un autre programme pourrait remplacer l’exe et obtenir ainsi ces droits. Il est plus sûr de déplacer l’exe vers « C:\\Program Files\\MilleIpSwitch » et d’y activer le démarrage automatique.\n\nActiver quand même le démarrage automatique ici ?"},
    {L"paused %d min", L"en pausa %d min", L"en pause %d min"},
    {L"A device answers ARP (also works from a different subnet on the same switch):", L"Un dispositivo responde a ARP (también desde otra subred en el mismo switch):", L"Un appareil répond à ARP (fonctionne aussi depuis un autre sous-réseau sur le même switch) :"},
    {L"Test", L"Probar", L"Tester"},
    {L"Device answers: MAC %ls", L"El dispositivo responde: MAC %ls", L"L’appareil répond : MAC %ls"},
    {L"No answer – is the device on the same switch and powered on?", L"Sin respuesta: ¿está el dispositivo en el mismo switch y encendido?", L"Aucune réponse – l’appareil est-il sur le même switch et allumé ?"},
    {L"Please enter a valid IPv4 address.", L"Introduce una dirección IPv4 válida.", L"Veuillez saisir une adresse IPv4 valide."},
    {L"Please enter at least one IPv6 address.", L"Introduce al menos una dirección IPv6.", L"Veuillez saisir au moins une adresse IPv6."},
    {L"Invalid IPv6 gateway.", L"Puerta de enlace IPv6 no válida.", L"Passerelle IPv6 invalide."},
    {L"Invalid IPv6 DNS server.", L"Servidor DNS IPv6 no válido.", L"Serveur DNS IPv6 invalide."},
    {L"Please enter a valid MAC address (e.g. AA-BB-CC-DD-EE-FF).", L"Introduce una dirección MAC válida (p. ej. AA-BB-CC-DD-EE-FF).", L"Veuillez saisir une adresse MAC valide (p. ex. AA-BB-CC-DD-EE-FF)."},
    {L"Please enter a host to ping.", L"Introduce un host para el ping.", L"Veuillez indiquer un hôte à tester par ping."},
    {L"Gateway MAC captured: %ls", L"MAC de la puerta de enlace capturada: %ls", L"MAC de la passerelle récupérée : %ls"},
    {L"No MAC found – is the adapter connected and the gateway reachable?", L"No se encontró ninguna MAC: ¿está conectado el adaptador y es accesible la puerta de enlace?", L"Aucune MAC trouvée – la carte est-elle connectée et la passerelle joignable ?"},
    {L"IPv6 …", L"IPv6…", L"IPv6…"},
    {L"IPv6: automatic", L"IPv6: automática", L"IPv6 : automatique"},
    {L"IPv6: static", L"IPv6: estática", L"IPv6 : statique"},
    {L"AutoSwitch …", L"AutoSwitch…", L"AutoSwitch…"},
    {L"AutoSwitch: on", L"AutoSwitch: activo", L"AutoSwitch : actif"},
    {L"IPv6 settings", L"Configuración IPv6", L"Paramètres IPv6"},
    {L"Invalid IPv6 address: ", L"Dirección IPv6 no válida: ", L"Adresse IPv6 invalide : "},
    {L"Invalid IPv6 prefix length: ", L"Longitud de prefijo IPv6 no válida: ", L"Longueur de préfixe IPv6 invalide : "},
    {L"Line %d: invalid IPv6 gateway: ", L"Línea %d: puerta de enlace IPv6 no válida: ", L"Ligne %d : passerelle IPv6 invalide : "},
    {L"Line %d: invalid IPv6 DNS server: ", L"Línea %d: servidor DNS IPv6 no válido: ", L"Ligne %d : serveur DNS IPv6 invalide : "},
    {L"]: static IPv6, but no 'ip6' given", L"]: IPv6 estática, pero sin 'ip6'", L"] : IPv6 statique, mais aucune 'ip6' indiquée"},
    {L"IPv6 gateway", L"Gateway IPv6", L"Passerelle IPv6"},
    {L"AutoSwitch: profile switched", L"AutoSwitch: perfil cambiado", L"AutoSwitch : profil changé"},
    {L"%d profiles", L"%d perfiles", L"%d profils"},
    {L"Leave IPv6 unchanged", L"No cambiar IPv6", L"Ne pas modifier IPv6"},
    {L"Obtain IPv6 automatically (router advertisement / DHCPv6)", L"Obtener IPv6 automáticamente (anuncio de router / DHCPv6)", L"Obtenir IPv6 automatiquement (annonce de routeur / DHCPv6)"},
    {L"Use the following IPv6 addresses:", L"Usar las siguientes direcciones IPv6:", L"Utiliser les adresses IPv6 suivantes :"},
    {L"Addresses (one per line, e.g. 2001:db8::10/64):", L"Direcciones (una por línea, p. ej. 2001:db8::10/64):", L"Adresses (une par ligne, p. ex. 2001:db8::10/64) :"},
    {L"Leave DNS empty for automatic.", L"DNS vacío = automático.", L"DNS vide = automatique."},
    {L"OK", L"Aceptar", L"OK"},
    {L"Cancel", L"Cancelar", L"Annuler"},
    {L"Activate this profile automatically", L"Activar este perfil automáticamente", L"Activer ce profil automatiquement"},
    {L"The profile is activated as soon as the adapter is connected and all checked conditions are met. If several profiles of the same adapter match, the topmost in the list wins. After a manual switch, AutoSwitch pauses for that adapter for 10 minutes. Gateway MAC and ping apply to the currently connected network; the ARP check also finds devices in a different subnet on the same switch.", L"El perfil se activa en cuanto el adaptador está conectado y se cumplen todas las condiciones marcadas. Si coinciden varios perfiles del mismo adaptador, gana el primero de la lista. Tras un cambio manual, AutoSwitch se detiene 10 minutos para ese adaptador. La MAC de la puerta de enlace y el ping se refieren a la red conectada; la prueba ARP también encuentra dispositivos de otra subred en el mismo switch.", L"Le profil est activé dès que la carte est connectée et que toutes les conditions cochées sont remplies. Si plusieurs profils d’une même carte correspondent, le premier de la liste l’emporte. Après un changement manuel, AutoSwitch se met en pause 10 minutes pour cette carte. La MAC de la passerelle et le ping concernent le réseau connecté ; le test ARP trouve aussi les appareils d’un autre sous-réseau sur le même switch."},
    {L"Conditions (in addition to “adapter connected”)", L"Condiciones (además de «adaptador conectado»)", L"Conditions (en plus de « carte connectée »)"},
    {L"The gateway has the MAC address:", L"La puerta de enlace tiene la dirección MAC:", L"La passerelle a l’adresse MAC :"},
    {L"Use current", L"Usar la actual", L"Utiliser l’actuelle"},
    {L"This host answers ping:", L"Este host responde al ping:", L"Cet hôte répond au ping :"},
    {L"Import …", L"Importar…", L"Importer…"},
    {L"Export …", L"Exportar…", L"Exporter…"},
    {L"Profile files (*.ini)", L"Archivos de perfiles (*.ini)", L"Fichiers de profils (*.ini)"},
    {L"All files (*.*)", L"Todos los archivos (*.*)", L"Tous les fichiers (*.*)"},
    {L"%u profiles exported.", L"%u perfiles exportados.", L"%u profils exportés."},
    {L"No profiles found in this file.", L"No se encontraron perfiles en este archivo.", L"Aucun profil trouvé dans ce fichier."},
    {L"%u profiles imported – click “Save” to keep them.", L"%u perfiles importados: pulsa «Guardar» para conservarlos.", L"%u profils importés – cliquez sur « Enregistrer » pour les conserver."},
    {L"Updates", L"Actualizaciones", L"Mises à jour"},
    {L"Check for updates now", L"Buscar actualizaciones ahora", L"Rechercher maintenant"},
    {L"Check automatically", L"Buscar automáticamente", L"Rechercher automatiquement"},
    {L"Download %ls …", L"Descargar %ls…", L"Télécharger %ls…"},
    {L"Update available", L"Actualización disponible", L"Mise à jour disponible"},
    {L"Version %ls is available – click to open the download page.", L"La versión %ls está disponible: haz clic para abrir la página de descarga.", L"La version %ls est disponible – cliquez pour ouvrir la page de téléchargement."},
    {L"You have the latest version (%ls).", L"Tienes la versión más reciente (%ls).", L"Vous avez la dernière version (%ls)."},
    {L"No published release found yet.", L"Aún no hay ninguna versión publicada.", L"Aucune version publiée pour l’instant."},
    {L"Update check failed – could not reach GitHub.", L"Error al buscar actualizaciones: no se pudo conectar con GitHub.", L"Échec de la recherche de mises à jour – GitHub injoignable."},
    {L"Sort by metric", L"Ordenar por métrica", L"Trier par métrique"},
    {L"Sorted by metric", L"Ordenado por métrica", L"Trié par métrique"},
    {L"Interface metric", L"Métrica de interfaz", L"Métrique d’interface"},
    {L"Metric", L"Métrica", L"Métrique"},
    {L"automatic", L"automática", L"automatique"},
    {L"manual", L"manual", L"manuelle"},
    {L"Flush DNS cache", L"Vaciar caché DNS", L"Vider le cache DNS"},
    {L"The Windows DNS cache will be flushed.\nCached name resolutions are discarded and will be looked up again on next access.\n\nContinue?",
     L"Se vaciará la caché DNS de Windows.\nLas resoluciones de nombres guardadas se descartan y se volverán a consultar en el próximo acceso.\n\n¿Continuar?",
     L"Le cache DNS de Windows va être vidé.\nLes résolutions de noms enregistrées sont supprimées et seront de nouveau demandées au prochain accès.\n\nContinuer ?"},
    {L"✓ DNS cache flushed", L"✓ Caché DNS vaciada", L"✓ Cache DNS vidé"},
    {L"Could not flush the DNS cache", L"No se pudo vaciar la caché DNS", L"Impossible de vider le cache DNS"},
    {L"Renew DHCP", L"Renovar DHCP", L"Renouveler DHCP"},
    {L"✓ DHCP lease renewed", L"✓ Concesión DHCP renovada", L"✓ Bail DHCP renouvelé"},
    {L"DHCP renewal failed", L"Error al renovar DHCP", L"Échec du renouvellement DHCP"},
    {L"Show disabled", L"Mostrar desactivados", L"Afficher désactivées"},
    {L"Disabled", L"Desactivado", L"Désactivé"},
    {L"Enabled", L"Activado", L"Activé"},
    {L"Settings", L"Ajustes", L"Réglages"},
    {L"Disable “%ls”?", L"¿Desactivar «%ls»?", L"Désactiver « %ls » ?"},
    {L"All connections over this adapter will be dropped.", L"Se cortarán todas las conexiones de este adaptador.", L"Toutes les connexions de cette carte seront coupées."},
    {L"The adapter could not be switched.", L"No se pudo cambiar el estado del adaptador.", L"Impossible de changer l’état de la carte."},
    {L"DHCP server", L"Servidor DHCP", L"Serveur DHCP"},
    {L"Profiles", L"Perfiles", L"Profils"},
    {L"%d active", L"%d activos", L"%d actifs"},
    {L"No connection", L"Sin conexión", L"Aucune connexion"},
    {L"%d virtual", L"%d virtuales", L"%d virtuelles"},
    {L"Start with Windows\nStarts in the notification area at sign-in, without a UAC prompt.",
     L"Iniciar con Windows\nSe inicia en el área de notificación al iniciar sesión, sin aviso de UAC.",
     L"Démarrer avec Windows\nDémarre dans la zone de notification à l’ouverture de session, sans invite UAC."},
    {L"Open taskbar settings\nTurn on MilleIpSwitch under “Other system tray icons” to keep the icon visible.",
     L"Abrir la configuración de la barra de tareas\nActiva MilleIpSwitch en «Otros iconos de la bandeja del sistema» para que el icono esté siempre visible.",
     L"Ouvrir les paramètres de la barre des tâches\nActivez MilleIpSwitch sous « Autres icônes de la barre d’état système » pour garder l’icône visible."},
    {L"Task Scheduler not available.", L"Programador de tareas no disponible.", L"Planificateur de tâches indisponible."},
    {L"You'll find the icon at the bottom right next to the clock – possibly behind the small arrow ˄.\nClicking it shows your profiles; active ones are marked green.\n\nYou can open this profile manager at any time via “Manage profiles …” in the icon menu or by starting MilleIpSwitch again.",
     L"Encontrarás el icono abajo a la derecha, junto al reloj; quizá detrás de la pequeña flecha ˄.\nAl hacer clic se muestran tus perfiles; los activos aparecen en verde.\n\nPuedes abrir este gestor de perfiles en cualquier momento con «Administrar perfiles…» en el menú del icono o iniciando MilleIpSwitch de nuevo.",
     L"L’icône se trouve en bas à droite, à côté de l’horloge – éventuellement derrière la petite flèche ˄.\nUn clic affiche vos profils ; les profils actifs sont marqués en vert.\n\nVous pouvez ouvrir ce gestionnaire à tout moment via « Gérer les profils… » dans le menu de l’icône ou en relançant MilleIpSwitch."},
    {L"Prefix missing (e.g. 192.168.1.10/24): ", L"Falta el prefijo (p. ej. 192.168.1.10/24): ", L"Préfixe manquant (p. ex. 192.168.1.10/24) : "},
    {L"Invalid IP: ", L"IP no válida: ", L"IP invalide : "},
    {L"Invalid subnet mask: ", L"Máscara de subred no válida: ", L"Masque de sous-réseau invalide : "},
    {L"Invalid prefix length: ", L"Longitud de prefijo no válida: ", L"Longueur de préfixe invalide : "},
    {L"Cannot open profile file: ", L"No se puede abrir el archivo de perfiles: ", L"Impossible d’ouvrir le fichier de profils : "},
    {L"Line %d: missing ']'", L"Línea %d: falta ']'", L"Ligne %d : ']' manquant"},
    {L"Line %d: expected 'key = value' inside a [profile]", L"Línea %d: se esperaba 'clave = valor' dentro de un [perfil]", L"Ligne %d : 'clé = valeur' attendu dans un [profil]"},
    {L"\n\nLine content: “", L"\n\nContenido de la línea: «", L"\n\nContenu de la ligne : « "},
    {L"”\nFile: ", L"»\nArchivo: ", L" »\nFichier : "},
    {L"Line %d: ", L"Línea %d: ", L"Ligne %d : "},
    {L"Line %d: invalid gateway: ", L"Línea %d: puerta de enlace no válida: ", L"Ligne %d : passerelle invalide : "},
    {L"Line %d: invalid DNS server: ", L"Línea %d: servidor DNS no válido: ", L"Ligne %d : serveur DNS invalide : "},
    {L"Line %d: unknown key '", L"Línea %d: clave desconocida '", L"Ligne %d : clé inconnue '"},
    {L"Profile [", L"Perfil [", L"Profil ["},
    {L"]: 'adapter' missing", L"]: falta 'adapter'", L"] : 'adapter' manquant"},
    {L"]: static, but no 'ip' given", L"]: estático, pero sin 'ip'", L"] : statique, mais aucune 'ip' indiquée"},
    {L"# MilleIpSwitch – profiles\r\n# Written by the profile manager, but may also be edited by hand.\r\n\r\n", L"# MilleIpSwitch – perfiles\r\n# Escrito por el gestor de perfiles, pero también se puede editar a mano.\r\n\r\n", L"# MilleIpSwitch – profils\r\n# Écrit par le gestionnaire de profils, mais modifiable aussi à la main.\r\n\r\n"},
    {L"Cannot write (error %lu): ", L"No se puede escribir (error %lu): ", L"Écriture impossible (erreur %lu) : "},
    {L"Saving failed (error %lu): ", L"Error al guardar (error %lu): ", L"Échec de l’enregistrement (erreur %lu) : "},
    {L"connected", L"conectado", L"connecté"},
    {L"disconnected", L"desconectado", L"déconnecté"},
    {L"static", L"estático", L"statique"},
    {L"Delete %ls: error %lu\n", L"Eliminar %ls: error %lu\n", L"Supprimer %ls : erreur %lu\n"},
    {L"Set %ls: error %lu\n", L"Establecer %ls: error %lu\n", L"Définir %ls : erreur %lu\n"},
    {L"Delete gateway: error %lu\n", L"Eliminar puerta de enlace: error %lu\n", L"Supprimer la passerelle : erreur %lu\n"},
    {L"Set gateway: error %lu\n", L"Establecer puerta de enlace: error %lu\n", L"Définir la passerelle : erreur %lu\n"},
    {L"Registry: error %ld\n", L"Registro: error %ld\n", L"Registre : erreur %ld\n"},
    {L"DNS via netsh failed\n", L"DNS mediante netsh falló\n", L"DNS via netsh a échoué\n"},
    {L"Adapter not found: ", L"Adaptador no encontrado: ", L"Carte introuvable : "},
    {L"with errors", L"con errores", L"avec erreurs"},
    {L", incl. DHCP mode change", L", incl. cambio de modo DHCP", L", changement de mode DHCP inclus"},
    {L"Could not create temporary file.", L"No se pudo crear el archivo temporal.", L"Impossible de créer le fichier temporaire."},
    {L"Wi-Fi", L"Wi-Fi", L"Wi-Fi"},
    {L"Virtual adapters", L"Adaptadores virtuales", L"Cartes virtuelles"},
    {L"Bluetooth & other", L"Bluetooth y otros", L"Bluetooth et autres"},
    {L"MilleIpSwitch – profile file", L"MilleIpSwitch – archivo de perfiles", L"MilleIpSwitch – fichier de profils"},
    {L"Link speed", L"Velocidad", L"Débit"},
    {L"Connected", L"Conectado", L"Connecté"},
    {L"Disconnected", L"Desconectado", L"Déconnecté"},
    {L"Static", L"Estático", L"Statique"},
    {L"No connected adapters", L"No hay adaptadores conectados", L"Aucune carte connectée"},
    {L"Network status", L"Estado de la red", L"État du réseau"},
    {L"%d of %d adapters connected", L"%d de %d adaptadores conectados", L"%d cartes connectées sur %d"},
    {L"Show disconnected", L"Mostrar desconectados", L"Afficher déconnectées"},
    {L"MilleIpSwitch – Network status", L"MilleIpSwitch – Estado de la red", L"MilleIpSwitch – État du réseau"},
    {L" – Profiles", L" – Perfiles", L" – Profils"},
    {L"  (not saved)", L"  (sin guardar)", L"  (non enregistré)"},
    {L"MilleIpSwitch – check input", L"MilleIpSwitch – revisar entrada", L"MilleIpSwitch – vérifier la saisie"},
    {L"Please enter a profile name.", L"Introduce un nombre de perfil.", L"Veuillez saisir un nom de profil."},
    {L"The profile name must not contain square brackets.", L"El nombre del perfil no puede contener corchetes.", L"Le nom du profil ne doit pas contenir de crochets."},
    {L"A profile with this name already exists.", L"Ya existe un perfil con este nombre.", L"Un profil portant ce nom existe déjà."},
    {L"Please select an adapter.", L"Selecciona un adaptador.", L"Veuillez choisir une carte."},
    {L"Please enter a complete IP address.", L"Introduce una dirección IP completa.", L"Veuillez saisir une adresse IP complète."},
    {L"The subnet mask is invalid.", L"La máscara de subred no es válida.", L"Le masque de sous-réseau est invalide."},
    {L"Additional IP addresses: ", L"Direcciones IP adicionales: ", L"Adresses IP supplémentaires : "},
    {L"The default gateway is incomplete.", L"La puerta de enlace predeterminada está incompleta.", L"La passerelle par défaut est incomplète."},
    {L"The DNS server address is incomplete.", L"La dirección del servidor DNS está incompleta.", L"L’adresse du serveur DNS est incomplète."},
    {L"Saving failed", L"Error al guardar", L"Échec de l’enregistrement"},
    {L"No changes – everything is already saved.", L"Sin cambios: todo está guardado.", L"Aucune modification – tout est déjà enregistré."},
    {L"Changes saved.", L"Cambios guardados.", L"Modifications enregistrées."},
    {L"1 change saved.", L"1 cambio guardado.", L"1 modification enregistrée."},
    {L"%u changes saved.", L"%u cambios guardados.", L"%u modifications enregistrées."},
    {L"  “Activate” applies the profile to the adapter.", L"  «Activar» aplica el perfil al adaptador.", L"  « Activer » applique le profil à la carte."},
    {L"Settings of “", L"Configuración de «", L"Paramètres de « "},
    {L"” loaded – click “Save” to keep them.", L"» cargada: pulsa «Guardar» para conservarla.", L" » chargés – cliquez sur « Enregistrer » pour les conserver."},
    {L"Save changes to the profiles?", L"¿Guardar los cambios en los perfiles?", L"Enregistrer les modifications des profils ?"},
    {L" – DHCP", L" – DHCP", L" – DHCP"},
    {L" – static", L" – estático", L" – statique"},
    {L"%u profiles prepared from the current adapter settings – click “Save” to keep them.", L"%u perfiles preparados a partir de la configuración actual: pulsa «Guardar» para conservarlos.", L"%u profils préparés à partir des paramètres actuels – cliquez sur « Enregistrer » pour les conserver."},
    {L"No profiles yet – create the first one with “New”.", L"Aún no hay perfiles: crea el primero con «Nuevo».", L"Aucun profil – créez le premier avec « Nouveau »."},
    {L" (copy)", L" (copia)", L" (copie)"},
    {L"New profile", L"Perfil nuevo", L"Nouveau profil"},
    {L"Copy created – adjust and click “Save”.", L"Copia creada: ajústala y pulsa «Guardar».", L"Copie créée – ajustez-la puis cliquez sur « Enregistrer »."},
    {L"New profile with the current values of “", L"Perfil nuevo con los valores actuales de «", L"Nouveau profil avec les valeurs actuelles de « "},
    {L"” created – adjust and click “Save”.", L"» creado: ajústalo y pulsa «Guardar».", L" » créé – ajustez-le puis cliquez sur « Enregistrer »."},
    {L"Delete profile “", L"¿Eliminar el perfil «", L"Supprimer le profil « "},
    {L"”?", L"»?", L" » ?"},
    {L"” is being activated …", L"» se está activando…", L" » est en cours d’activation…"},
    {L"A switch is already in progress.", L"Ya hay un cambio en curso.", L"Un changement est déjà en cours."},
    {L"MilleIpSwitch keeps running", L"MilleIpSwitch sigue en ejecución", L"MilleIpSwitch reste actif"},
    {L"Switch profiles via the icon in the notification area.", L"Cambia de perfil con el icono del área de notificación.", L"Changez de profil via l’icône de la zone de notification."},
    {L"Autostart could not be set up.\n", L"No se pudo configurar el inicio automático.\n", L"Impossible de configurer le démarrage automatique.\n"},
    {L"Autostart enabled", L"Inicio automático activado", L"Démarrage automatique activé"},
    {L"MilleIpSwitch now starts at sign-in – without a UAC prompt.", L"MilleIpSwitch se inicia ahora al iniciar sesión, sin aviso de UAC.", L"MilleIpSwitch démarre désormais à l’ouverture de session – sans invite UAC."},
    {L"Start with Windows and always show the icon\nRecommended: starts at sign-in without a UAC prompt, the icon stays visible in the taskbar.", L"Iniciar con Windows y mostrar siempre el icono\nRecomendado: se inicia al iniciar sesión sin aviso de UAC y el icono permanece visible en la barra de tareas.", L"Démarrer avec Windows et toujours afficher l’icône\nRecommandé : démarre à l’ouverture de session sans invite UAC, l’icône reste visible dans la barre des tâches."},
    {L"Only open the taskbar settings\nTurn on MilleIpSwitch there under “Other system tray icons”.", L"Abrir solo la configuración de la barra de tareas\nActiva allí MilleIpSwitch en «Otros iconos de la bandeja del sistema».", L"Ouvrir seulement les paramètres de la barre des tâches\nActivez-y MilleIpSwitch sous « Autres icônes de la barre d’état système »."},
    {L"Welcome to MilleIpSwitch", L"Bienvenido a MilleIpSwitch", L"Bienvenue dans MilleIpSwitch"},
    {L"MilleIpSwitch runs in the notification area", L"MilleIpSwitch se ejecuta en el área de notificación", L"MilleIpSwitch s’exécute dans la zone de notification"},
    {L"switching …", L"cambiando…", L"changement…"},
    {L"No profiles yet – create one …", L"Aún no hay perfiles: crear uno…", L"Aucun profil – en créer un…"},
    {L"Manage profiles …", L"Administrar perfiles…", L"Gérer les profils…"},
    {L"Start with Windows", L"Iniciar con Windows", L"Démarrer avec Windows"},
    {L"no UAC", L"sin UAC", L"sans UAC"},
    {L"Language", L"Idioma", L"Langue"},
    {L"Exit", L"Salir", L"Quitter"},
    {L"Profile switched", L"Perfil cambiado", L"Profil changé"},
    {L"Switching failed", L"Error al cambiar", L"Échec du changement"},
    {L" – fast network profile switcher\n\n  milleipswitch                 Start (tray + profile manager)\n  milleipswitch --tray          Start to tray only (for autostart)\n  milleipswitch \"<profile>\"     Apply profile\n  milleipswitch --list          List profiles\n  milleipswitch --status        Show adapter status\n  milleipswitch --edit          Open profile manager\n  milleipswitch --version       Show version\n\n", L" – cambio rápido de perfiles de red\n\n  milleipswitch                 Iniciar (bandeja + gestor de perfiles)\n  milleipswitch --tray          Iniciar solo en la bandeja (inicio automático)\n  milleipswitch \"<perfil>\"      Aplicar perfil\n  milleipswitch --list          Listar perfiles\n  milleipswitch --status        Mostrar estado de los adaptadores\n  milleipswitch --edit          Abrir gestor de perfiles\n  milleipswitch --version       Mostrar versión\n\n", L" – changement rapide de profils réseau\n\n  milleipswitch                 Démarrer (zone de notification + gestionnaire)\n  milleipswitch --tray          Démarrer uniquement dans la zone de notification\n  milleipswitch \"<profil>\"      Appliquer un profil\n  milleipswitch --list          Lister les profils\n  milleipswitch --status        Afficher l’état des cartes\n  milleipswitch --edit          Ouvrir le gestionnaire de profils\n  milleipswitch --version       Afficher la version\n\n"},
    {L"Profiles: ", L"Perfiles: ", L"Profils : "},
    {L"Unknown profile: ", L"Perfil desconocido: ", L"Profil inconnu : "},
    {L": OK (via tray)", L": OK (mediante la bandeja)", L" : OK (via la zone de notification)"},
    {L": error (details in the tray notification)", L": error (detalles en la notificación)", L" : erreur (détails dans la notification)"},
    {L"Administrator rights were not granted.", L"No se concedieron derechos de administrador.", L"Les droits d’administrateur n’ont pas été accordés."},
    {L": error", L": error", L" : erreur"},
    {L"Profiles:", L"Perfiles:", L"Profils :"},
    {L"New", L"Nuevo", L"Nouveau"},
    {L"Copy", L"Copiar", L"Copier"},
    {L"Delete", L"Eliminar", L"Supprimer"},
    {L"Move up", L"Subir", L"Monter"},
    {L"Move down", L"Bajar", L"Descendre"},
    {L"Profile name:", L"Nombre del perfil:", L"Nom du profil :"},
    {L"Adapter:", L"Adaptador:", L"Carte :"},
    {L"Load the adapter's current settings", L"Cargar la configuración actual del adaptador", L"Charger les paramètres actuels de la carte"},
    {L"IP address", L"Dirección IP", L"Adresse IP"},
    {L"Obtain an IP address automatically (DHCP)", L"Obtener una dirección IP automáticamente (DHCP)", L"Obtenir une adresse IP automatiquement (DHCP)"},
    {L"Use the following IP address:", L"Usar la siguiente dirección IP:", L"Utiliser l’adresse IP suivante :"},
    {L"IP address:", L"Dirección IP:", L"Adresse IP :"},
    {L"Subnet mask:", L"Máscara de subred:", L"Masque de sous-réseau :"},
    {L"Default gateway:", L"Puerta de enlace predet.:", L"Passerelle par défaut :"},
    {L"Additional IP addresses (one per line, e.g. 10.0.0.5/8):", L"Direcciones IP adicionales (una por línea, p. ej. 10.0.0.5/8):", L"Adresses IP supplémentaires (une par ligne, p. ex. 10.0.0.5/8) :"},
    {L"DNS servers", L"Servidores DNS", L"Serveurs DNS"},
    {L"Obtain DNS server address automatically", L"Obtener la dirección del servidor DNS automáticamente", L"Obtenir les adresses des serveurs DNS automatiquement"},
    {L"Use the following DNS server addresses:", L"Usar las siguientes direcciones de servidor DNS:", L"Utiliser l’adresse de serveur DNS suivante :"},
    {L"Preferred DNS server:", L"Servidor DNS preferido:", L"Serveur DNS préféré :"},
    {L"Alternate DNS server:", L"Servidor DNS alternativo:", L"Serveur DNS auxiliaire :"},
    {L"Activate", L"Activar", L"Activer"},
    {L"Save", L"Guardar", L"Enregistrer"},
    {L"Close", L"Cerrar", L"Fermer"},
};

static const wchar_t* trLookup(const wchar_t* en) {
    for (const auto& e : kTr)
        if (wcscmp(e.en, en) == 0) return g_lang == 2 ? e.es : e.fr;
    return en;
}
static bool g_console = false;
static HANDLE g_conOut = INVALID_HANDLE_VALUE;

static const wchar_t* kWndClass = L"MilleIpSwitchTrayWnd";
static const ULONG_PTR kCopyDataId = 0x4E53;

enum { RC_OK = 0, RC_FAIL = 1, RC_UNKNOWN = 2, RC_CONFIG = 3, RC_BUSY = 4 };

static void out(const wstring& s) {
    if (!g_console) return;
    DWORD w;
    WriteConsoleW(g_conOut, s.c_str(), (DWORD)s.size(), &w, nullptr);
}

static wstring fmt(const wchar_t* f, ...) {
    wchar_t buf[2048];
    va_list ap;
    va_start(ap, f);
    _vsnwprintf(buf, 2047, f, ap);
    va_end(ap);
    buf[2047] = 0;
    return buf;
}

static wstring trim(const wstring& s) {
    static const wchar_t* ws = L" \t\r\n\x00A0\xFEFF\x200B";
    size_t a = s.find_first_not_of(ws);
    if (a == wstring::npos) return L"";
    size_t b = s.find_last_not_of(ws);
    return s.substr(a, b - a + 1);
}

static vector<wstring> splitList(const wstring& s) {
    vector<wstring> r;
    wstring cur;
    for (wchar_t c : s) {
        if (c == L',' || c == L';') {
            cur = trim(cur);
            if (!cur.empty()) r.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    cur = trim(cur);
    if (!cur.empty()) r.push_back(cur);
    return r;
}

static wstring lower(wstring s) {
    if (!s.empty()) CharLowerBuffW(&s[0], (DWORD)s.size());
    return s;
}

static bool ieq(const wstring& a, const wstring& b) {
    return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
}

static wstring ipStr(const IN_ADDR& a) {
    wchar_t b[32] = {};
    InetNtopW(AF_INET, (void*)&a, b, 32);
    return b;
}

static bool parseIp(const wstring& s, IN_ADDR& a) {
    return InetPtonW(AF_INET, s.c_str(), &a) == 1;
}

static bool maskToPrefix(ULONG maskNet, UINT8& len) {
    ULONG m = ntohl(maskNet);
    UINT8 n = 0;
    while (n < 32 && (m & 0x80000000u)) {
        m <<= 1;
        ++n;
    }
    if (m != 0) return false;
    len = n;
    return true;
}

static ULONG prefixToMask(UINT8 len) { return len == 0 ? 0 : htonl(0xFFFFFFFFu << (32 - (len > 32 ? 32 : len))); }

static wstring maskStr(UINT8 len) {
    ULONG m = 0;
    m = prefixToMask(len);
    IN_ADDR a;
    a.s_addr = m;
    return ipStr(a);
}

static bool sameAddr(const IN_ADDR& a, const IN_ADDR& b) { return a.s_addr == b.s_addr; }

static double nowMs() {
    static LARGE_INTEGER f = {};
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart * 1000.0 / (double)f.QuadPart;
}

// Profiles
struct Ip4 {
    IN_ADDR addr{};
    UINT8 prefix = 0;
};

struct Ip6 {
    IN6_ADDR addr{};
    UINT8 prefix = 64;
    bool manual = false, linkLocal = false, temp = false;
};

static wstring ip6Str(const IN6_ADDR& a) {
    wchar_t b[64] = {};
    InetNtopW(AF_INET6, (void*)&a, b, 64);
    return b;
}

static bool parseIp6(const wstring& s, IN6_ADDR& a) { return InetPtonW(AF_INET6, s.c_str(), &a) == 1; }

static bool sameAddr6(const IN6_ADDR& a, const IN6_ADDR& b) { return memcmp(&a, &b, sizeof(a)) == 0; }

static wstring cidr6Str(const Ip6& w) { return ip6Str(w.addr) + fmt(L"/%u", (unsigned)w.prefix); }

static wstring normMac(const wstring& s) {
    wstring h;
    for (wchar_t c : s)
        if (iswxdigit(c)) h += (wchar_t)towupper(c);
    if (h.size() != 12) return L"";
    wstring r;
    for (size_t i = 0; i < 12; i += 2) {
        if (i) r += L'-';
        r += h.substr(i, 2);
    }
    return r;
}

static bool parseCidr6(const wstring& s, Ip6& r, wstring& err) {
    size_t p = s.find(L'/');
    wstring a = trim(p == wstring::npos ? s : s.substr(0, p));
    if (!parseIp6(a, r.addr)) {
        err = TR(L"Ungültige IPv6-Adresse: ", L"Invalid IPv6 address: ") + s;
        return false;
    }
    r.prefix = 64;
    if (p != wstring::npos) {
        int n = _wtoi(trim(s.substr(p + 1)).c_str());
        if (n < 1 || n > 128) {
            err = TR(L"Ungültige IPv6-Präfixlänge: ", L"Invalid IPv6 prefix length: ") + s;
            return false;
        }
        r.prefix = (UINT8)n;
    }
    return true;
}

struct Profile {
    wstring name, adapter;
    bool dhcp = false;
    vector<Ip4> ips;
    bool hasGw = false;
    IN_ADDR gw{};
    vector<IN_ADDR> dns;
    int ip6mode = 0;
    vector<Ip6> ips6;
    bool hasGw6 = false;
    IN6_ADDR gw6{};
    vector<IN6_ADDR> dns6;
    bool autoOn = false;
    wstring autoMac, autoPing, autoArpIp, autoArpMac;
};

static bool parseCidr(const wstring& s, Ip4& r, wstring& err) {
    size_t p = s.find(L'/');
    if (p == wstring::npos) {
        err = TR(L"Präfix fehlt (z. B. 192.168.1.10/24): ", L"Prefix missing (e.g. 192.168.1.10/24): ") + s;
        return false;
    }
    if (!parseIp(trim(s.substr(0, p)), r.addr)) {
        err = TR(L"Ungültige IP: ", L"Invalid IP: ") + s;
        return false;
    }
    wstring pre = trim(s.substr(p + 1));
    if (pre.find(L'.') != wstring::npos) {
        IN_ADDR m;
        UINT8 len = 0;
        if (!parseIp(pre, m) || !maskToPrefix(m.s_addr, len)) {
            err = TR(L"Ungültige Subnetzmaske: ", L"Invalid subnet mask: ") + s;
            return false;
        }
        r.prefix = len;
    } else {
        int n = pre.empty() ? 0 : _wtoi(pre.c_str());
        if (n < 1 || n > 32) {
            err = TR(L"Ungültige Präfixlänge: ", L"Invalid prefix length: ") + s;
            return false;
        }
        r.prefix = (UINT8)n;
    }
    return true;
}

static wstring decodeText(const string& data) {
    const unsigned char* b = (const unsigned char*)data.data();
    size_t n = data.size();
    wstring t;
    bool le = n >= 2 && ((b[0] == 0xFF && b[1] == 0xFE) || (b[0] != 0 && b[1] == 0));
    bool be = n >= 2 && ((b[0] == 0xFE && b[1] == 0xFF) || (b[0] == 0 && b[1] != 0));
    if (le || be) {
        size_t start = (b[0] == 0xFF || b[0] == 0xFE) ? 2 : 0;
        for (size_t i = start; i + 1 < n; i += 2)
            t.push_back(le ? (wchar_t)(b[i] | (b[i + 1] << 8)) : (wchar_t)((b[i] << 8) | b[i + 1]));
    } else {
        size_t off = (n >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF) ? 3 : 0;
        UINT cp = CP_UTF8;
        int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data.data() + off, (int)(n - off), nullptr, 0);
        if (len == 0 && n > off) {
            cp = CP_ACP;
            len = MultiByteToWideChar(CP_ACP, 0, data.data() + off, (int)(n - off), nullptr, 0);
        }
        t.assign(len, L'\0');
        if (len) MultiByteToWideChar(cp, 0, data.data() + off, (int)(n - off), &t[0], len);
    }
    while (!t.empty() && (t[0] == 0xFEFF || t[0] == 0xFFFE)) t.erase(0, 1);
    return t;
}

static bool loadProfilesFrom(const wstring& path, vector<Profile>& ps, wstring& err) {
    ps.clear();
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) return true;
        err = TR(L"Kann Profildatei nicht öffnen: ", L"Cannot open profile file: ") + path;
        return false;
    }
    LARGE_INTEGER sz{};
    GetFileSizeEx(f, &sz);
    string data((size_t)sz.QuadPart, '\0');
    DWORD rd = 0;
    if (sz.QuadPart) ReadFile(f, &data[0], (DWORD)sz.QuadPart, &rd, nullptr);
    CloseHandle(f);
    data.resize(rd);
    wstring text = decodeText(data);
    int cur = -1, lineNo = 0;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t e = text.find(L'\n', pos);
        if (e == wstring::npos) e = text.size();
        wstring line = trim(text.substr(pos, e - pos));
        pos = e + 1;
        ++lineNo;
        if (line.empty() || line[0] == L'#' || line[0] == L';') continue;

        if (line[0] == L'[') {
            size_t c = line.find(L']');
            if (c == wstring::npos) {
                err = fmt(TR(L"Zeile %d: fehlende ']'", L"Line %d: missing ']'"), lineNo);
                return false;
            }
            Profile p;
            p.name = trim(line.substr(1, c - 1));
            ps.push_back(p);
            cur = (int)ps.size() - 1;
            continue;
        }

        size_t eq = line.find(L'=');
        if (eq == wstring::npos || cur < 0) {
            err = fmt(TR(L"Zeile %d: Erwartet 'schlüssel = wert' innerhalb eines [Profils]", L"Line %d: expected 'key = value' inside a [profile]"), lineNo) +
                  TR(L"\n\nInhalt der Zeile: „", L"\n\nLine content: “") + line.substr(0, 80) + TR(L"“\nDatei: ", L"”\nFile: ") + path;
            return false;
        }
        Profile& p = ps[cur];
        wstring k = lower(trim(line.substr(0, eq)));
        wstring v = trim(line.substr(eq + 1));
        wstring e2;

        if (k == L"adapter") {
            p.adapter = v;
        } else if (k == L"dhcp") {
            wstring lv = lower(v);
            p.dhcp = (lv == L"yes" || lv == L"ja" || lv == L"1" || lv == L"true" || lv == L"on");
        } else if (k == L"ip") {
            for (auto& t : splitList(v)) {
                Ip4 r;
                if (!parseCidr(t, r, e2)) {
                    err = fmt(TR(L"Zeile %d: ", L"Line %d: "), lineNo) + e2;
                    return false;
                }
                p.ips.push_back(r);
            }
        } else if (k == L"gateway") {
            if (!v.empty()) {
                if (!parseIp(v, p.gw)) {
                    err = fmt(TR(L"Zeile %d: Ungültiges Gateway: ", L"Line %d: invalid gateway: "), lineNo) + v;
                    return false;
                }
                p.hasGw = true;
            }
        } else if (k == L"dns") {
            for (auto& t : splitList(v)) {
                IN_ADDR a;
                if (!parseIp(t, a)) {
                    err = fmt(TR(L"Zeile %d: Ungültiger DNS-Server: ", L"Line %d: invalid DNS server: "), lineNo) + t;
                    return false;
                }
                p.dns.push_back(a);
            }
        } else if (k == L"ipv6") {
            wstring lv = lower(v);
            p.ip6mode = (lv == L"auto" || lv == L"automatic") ? 1 : (lv == L"static" || lv == L"statisch") ? 2 : 0;
        } else if (k == L"ip6") {
            for (auto& t : splitList(v)) {
                Ip6 r;
                if (!parseCidr6(t, r, e2)) {
                    err = fmt(TR(L"Zeile %d: ", L"Line %d: "), lineNo) + e2;
                    return false;
                }
                p.ips6.push_back(r);
            }
        } else if (k == L"gateway6") {
            if (!v.empty()) {
                if (!parseIp6(v, p.gw6)) {
                    err = fmt(TR(L"Zeile %d: Ungültiges IPv6-Gateway: ", L"Line %d: invalid IPv6 gateway: "), lineNo) + v;
                    return false;
                }
                p.hasGw6 = true;
            }
        } else if (k == L"dns6") {
            for (auto& t : splitList(v)) {
                IN6_ADDR a;
                if (!parseIp6(t, a)) {
                    err = fmt(TR(L"Zeile %d: Ungültiger IPv6-DNS-Server: ", L"Line %d: invalid IPv6 DNS server: "), lineNo) + t;
                    return false;
                }
                p.dns6.push_back(a);
            }
        } else if (k == L"auto") {
            wstring lv = lower(v);
            p.autoOn = (lv == L"yes" || lv == L"ja" || lv == L"1" || lv == L"true" || lv == L"on");
        } else if (k == L"auto_gateway_mac") {
            p.autoMac = normMac(v);
        } else if (k == L"auto_ping") {
            p.autoPing = v;
        } else if (k == L"auto_arp_ip") {
            p.autoArpIp = v;
        } else if (k == L"auto_arp_mac") {
            p.autoArpMac = normMac(v);
        } else if (k == L"auto_probe") {
        } else {
            err = fmt(TR(L"Zeile %d: Unbekannter Schlüssel '", L"Line %d: unknown key '"), lineNo) + k + L"'";
            return false;
        }
    }

    for (auto& p : ps) {
        if (p.adapter.empty()) {
            err = TR(L"Profil [", L"Profile [") + p.name + TR(L"]: 'adapter' fehlt", L"]: 'adapter' missing");
            return false;
        }
        if (!p.dhcp && p.ips.empty()) {
            err = TR(L"Profil [", L"Profile [") + p.name + TR(L"]: statisch, aber keine 'ip' angegeben", L"]: static, but no 'ip' given");
            return false;
        }
        if (p.ip6mode == 2 && p.ips6.empty()) {
            err = TR(L"Profil [", L"Profile [") + p.name + TR(L"]: IPv6 statisch, aber keine 'ip6' angegeben", L"]: static IPv6, but no 'ip6' given");
            return false;
        }
    }
    return true;
}

static bool loadProfiles(vector<Profile>& ps, wstring& err) { return loadProfilesFrom(g_iniPath, ps, err); }

static const Profile* findProfile(const vector<Profile>& ps, const wstring& name) {
    for (auto& p : ps)
        if (ieq(p.name, name)) return &p;
    return nullptr;
}

static wstring cidrStr(const Ip4& w) { return ipStr(w.addr) + fmt(L"/%u", (unsigned)w.prefix); }

static bool writeProfilesTo(const wstring& path, const vector<Profile>& ps, wstring& err) {
    wstring t = TR(L"# MilleIpSwitch – Profile\r\n"
                   L"# Wird von der Profilverwaltung geschrieben, kann aber auch von Hand bearbeitet werden.\r\n\r\n",
                   L"# MilleIpSwitch – profiles\r\n"
                   L"# Written by the profile manager, but may also be edited by hand.\r\n\r\n");
    for (auto& p : ps) {
        t += L"[" + p.name + L"]\r\n";
        t += L"adapter = " + p.adapter + L"\r\n";
        t += wstring(L"dhcp    = ") + (p.dhcp ? L"ja" : L"nein") + L"\r\n";
        if (!p.dhcp) {
            t += L"ip      = ";
            for (size_t i = 0; i < p.ips.size(); ++i) t += (i ? L", " : L"") + cidrStr(p.ips[i]);
            t += L"\r\n";
            if (p.hasGw) t += L"gateway = " + ipStr(p.gw) + L"\r\n";
        }
        if (!p.dns.empty()) {
            t += L"dns     = ";
            for (size_t i = 0; i < p.dns.size(); ++i) t += (i ? L", " : L"") + ipStr(p.dns[i]);
            t += L"\r\n";
        }
        if (p.ip6mode) {
            t += wstring(L"ipv6    = ") + (p.ip6mode == 1 ? L"auto" : L"static") + L"\r\n";
            if (p.ip6mode == 2) {
                t += L"ip6     = ";
                for (size_t i = 0; i < p.ips6.size(); ++i) t += (i ? L", " : L"") + cidr6Str(p.ips6[i]);
                t += L"\r\n";
                if (p.hasGw6) t += L"gateway6 = " + ip6Str(p.gw6) + L"\r\n";
            }
            if (!p.dns6.empty()) {
                t += L"dns6    = ";
                for (size_t i = 0; i < p.dns6.size(); ++i) t += (i ? L", " : L"") + ip6Str(p.dns6[i]);
                t += L"\r\n";
            }
        }
        if (p.autoOn) {
            t += L"auto    = yes\r\n";
            if (!p.autoMac.empty()) t += L"auto_gateway_mac = " + p.autoMac + L"\r\n";
            if (!p.autoPing.empty()) t += L"auto_ping = " + p.autoPing + L"\r\n";
            if (!p.autoArpIp.empty()) {
                t += L"auto_arp_ip = " + p.autoArpIp + L"\r\n";
                if (!p.autoArpMac.empty()) t += L"auto_arp_mac = " + p.autoArpMac + L"\r\n";
            }
        }
        t += L"\r\n";
    }
    int n = WideCharToMultiByte(CP_UTF8, 0, t.c_str(), (int)t.size(), nullptr, 0, nullptr, nullptr);
    string data = "\xEF\xBB\xBF" + string(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, t.c_str(), (int)t.size(), &data[3], n, nullptr, nullptr);

    wstring tmp = path + L".tmp";
    HANDLE f = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        err = fmt(TR(L"Kann nicht schreiben (Fehler %lu): ", L"Cannot write (error %lu): "), GetLastError()) + tmp;
        return false;
    }
    DWORD wr = 0;
    BOOL ok = WriteFile(f, data.data(), (DWORD)data.size(), &wr, nullptr);
    CloseHandle(f);
    if (!ok || wr != data.size() ||
        !MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        err = fmt(TR(L"Speichern fehlgeschlagen (Fehler %lu): ", L"Saving failed (error %lu): "), GetLastError()) + path;
        DeleteFileW(tmp.c_str());
        return false;
    }
    return true;
}

static void backupProfiles() {
    if (GetFileAttributesW(g_iniPath.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    wstring dir = g_iniPath.substr(0, g_iniPath.find_last_of(L'\\')) + L"\\backups";
    CreateDirectoryW(dir.c_str(), nullptr);
    SYSTEMTIME st;
    GetLocalTime(&st);
    wstring name = dir + fmt(L"\\profiles_%04d%02d%02d_%02d%02d%02d.ini", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    CopyFileW(g_iniPath.c_str(), name.c_str(), FALSE);
    vector<wstring> files;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\profiles_*.ini").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do files.push_back(fd.cFileName);
    while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(files.begin(), files.end());
    for (size_t i = 0; i + 10 < files.size(); ++i) DeleteFileW((dir + L"\\" + files[i]).c_str());
}

static bool saveProfiles(const vector<Profile>& ps, wstring& err) {
    backupProfiles();
    return writeProfilesTo(g_iniPath, ps, err);
}

// Adapters
struct Adapter {
    wstring friendly, desc;
    string guidStr;
    NET_LUID luid{};
    GUID guid{};
    bool dhcp = false, up = false;
    vector<Ip4> ips;
    vector<IN_ADDR> gws, dns;
    vector<Ip6> ips6;
    vector<IN6_ADDR> gws6, dns6;
    int kind = 0;
    bool bluetooth = false;
    ULONG64 speed = 0;
    bool enabled = true;
    bool hasMetric = false, autoMetric = true;
    ULONG metric = 0;
    bool hasDhcpSrv = false;
    IN_ADDR dhcpSrv{};
    wstring mac;
};

enum { K_ETH, K_WLAN, K_VPN, K_VIRT, K_OTHER, K_COUNT };

static int classify(const wstring& friendly, const wstring& desc, DWORD ifType, bool hardware, bool& bt) {
    wstring d = lower(desc + L" " + friendly);
    auto has = [&](const wchar_t* t) { return d.find(t) != wstring::npos; };
    bt = has(L"bluetooth");
    if (bt) return K_OTHER;
    if (has(L"vpn") || has(L"tap-windows") || has(L"openvpn") || has(L"wireguard") || has(L"wintun") ||
        has(L"anyconnect") || has(L"fortinet") || has(L"forticlient") || has(L"pangp") || has(L"globalprotect") ||
        has(L"tailscale") || has(L"zerotier") || has(L"juniper") || has(L"sonicwall") || has(L"nordlynx") ||
        ifType == IF_TYPE_PPP || ifType == IF_TYPE_TUNNEL)
        return K_VPN;
    if (has(L"virtual") || has(L"hyper-v") || has(L"vmware") || has(L"virtualbox") || has(L"vethernet") ||
        has(L"loopback") || !hardware)
        return K_VIRT;
    if (ifType == IF_TYPE_IEEE80211) return K_WLAN;
    if (ifType == IF_TYPE_ETHERNET_CSMACD) return K_ETH;
    return K_OTHER;
}

static wstring regStr(HKEY root, const wstring& path, const wchar_t* name) {
    wchar_t buf[512] = L"";
    DWORD sz = sizeof(buf);
    if (RegGetValueW(root, path.c_str(), name, RRF_RT_REG_SZ, nullptr, buf, &sz) != ERROR_SUCCESS) return L"";
    return buf;
}

static wstring netCfgId(HDEVINFO hdi, SP_DEVINFO_DATA& dd) {
    HKEY k = SetupDiOpenDevRegKey(hdi, &dd, DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_READ);
    if (k == INVALID_HANDLE_VALUE) return L"";
    wchar_t buf[64] = L"";
    DWORD sz = sizeof(buf), type = 0;
    if (RegQueryValueExW(k, L"NetCfgInstanceId", nullptr, &type, (BYTE*)buf, &sz) != ERROR_SUCCESS || type != REG_SZ)
        buf[0] = 0;
    RegCloseKey(k);
    return buf;
}

static void appendDisabledAdapters(vector<Adapter>& r) {
    HDEVINFO hdi = SetupDiGetClassDevsW(&GUID_DEVCLASS_NET, nullptr, nullptr, DIGCF_PRESENT);
    if (hdi == INVALID_HANDLE_VALUE) return;
    SP_DEVINFO_DATA dd{};
    dd.cbSize = sizeof(dd);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(hdi, i, &dd); ++i) {
        ULONG st = 0, prob = 0;
        if (CM_Get_DevNode_Status(&st, &prob, dd.DevInst, 0) != CR_SUCCESS || prob != CM_PROB_DISABLED) continue;
        wstring id = netCfgId(hdi, dd);
        if (id.empty()) continue;
        wstring name = regStr(HKEY_LOCAL_MACHINE,
                              L"SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}\\" + id +
                                  L"\\Connection",
                              L"Name");
        if (name.empty()) continue;
        wchar_t desc[256] = L"";
        if (!SetupDiGetDeviceRegistryPropertyW(hdi, &dd, SPDRP_FRIENDLYNAME, nullptr, (BYTE*)desc, sizeof(desc), nullptr))
            SetupDiGetDeviceRegistryPropertyW(hdi, &dd, SPDRP_DEVICEDESC, nullptr, (BYTE*)desc, sizeof(desc), nullptr);
        Adapter a;
        a.friendly = name;
        a.desc = desc;
        a.guidStr = string(id.begin(), id.end());
        a.enabled = false;
        a.up = false;
        HKEY k;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, (L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces\\" + id).c_str(),
                          0, KEY_READ, &k) == ERROR_SUCCESS) {
            DWORD v = 1, sz = sizeof(v);
            if (RegQueryValueExW(k, L"EnableDHCP", nullptr, nullptr, (BYTE*)&v, &sz) == ERROR_SUCCESS) a.dhcp = v != 0;
            RegCloseKey(k);
        }
        wstring dl = lower(a.desc);
        bool wifi = dl.find(L"wi-fi") != wstring::npos || dl.find(L"wireless") != wstring::npos ||
                    dl.find(L"wlan") != wstring::npos || dl.find(L"802.11") != wstring::npos;
        a.kind = classify(a.friendly, a.desc, wifi ? IF_TYPE_IEEE80211 : IF_TYPE_ETHERNET_CSMACD, true, a.bluetooth);
        r.push_back(a);
    }
    SetupDiDestroyDeviceInfoList(hdi);
}

static bool setAdapterEnabled(const string& guidStr, bool enable) {
    wstring want(guidStr.begin(), guidStr.end());
    HDEVINFO hdi = SetupDiGetClassDevsW(&GUID_DEVCLASS_NET, nullptr, nullptr, DIGCF_PRESENT);
    if (hdi == INVALID_HANDLE_VALUE) return false;
    bool ok = false;
    SP_DEVINFO_DATA dd{};
    dd.cbSize = sizeof(dd);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(hdi, i, &dd); ++i) {
        if (!ieq(netCfgId(hdi, dd), want)) continue;
        SP_PROPCHANGE_PARAMS pc{};
        pc.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
        pc.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
        pc.StateChange = enable ? DICS_ENABLE : DICS_DISABLE;
        pc.Scope = DICS_FLAG_GLOBAL;
        ok = SetupDiSetClassInstallParamsW(hdi, &dd, &pc.ClassInstallHeader, sizeof(pc)) &&
             SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, hdi, &dd);
        break;
    }
    SetupDiDestroyDeviceInfoList(hdi);
    return ok;
}

static vector<Adapter> g_disabledCache;
static DWORD g_disabledStamp = 0;

static vector<Adapter> getAdapters(bool withDisabled);
static vector<Adapter> getAdapters() { return getAdapters(false); }

static vector<Adapter> getAdapters(bool withDisabled) {
    vector<Adapter> r;
    ULONG len = 32 * 1024;
    vector<BYTE> buf;
    ULONG ret = ERROR_BUFFER_OVERFLOW;
    for (int i = 0; i < 3 && ret == ERROR_BUFFER_OVERFLOW; ++i) {
        buf.resize(len);
        ret = GetAdaptersAddresses(AF_UNSPEC,
                                   GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST,
                                   nullptr, (PIP_ADAPTER_ADDRESSES)buf.data(), &len);
    }
    if (ret != NO_ERROR) return r;

    for (auto a = (PIP_ADAPTER_ADDRESSES)buf.data(); a; a = a->Next) {
        if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        Adapter d;
        d.friendly = a->FriendlyName ? a->FriendlyName : L"";
        d.desc = a->Description ? a->Description : L"";
        d.guidStr = a->AdapterName ? a->AdapterName : "";
        d.luid = a->Luid;
        ConvertInterfaceLuidToGuid(&a->Luid, &d.guid);
        d.dhcp = (a->Flags & IP_ADAPTER_DHCP_ENABLED) != 0;
        d.up = a->OperStatus == IfOperStatusUp;
        d.speed = a->TransmitLinkSpeed;
        {
            MIB_IPINTERFACE_ROW ir;
            InitializeIpInterfaceEntry(&ir);
            ir.Family = AF_INET;
            ir.InterfaceLuid = a->Luid;
            if (GetIpInterfaceEntry(&ir) == NO_ERROR) {
                d.hasMetric = true;
                d.metric = ir.Metric;
                d.autoMetric = ir.UseAutomaticMetric != 0;
            }
        }
        if (a->Dhcpv4Server.lpSockaddr && a->Dhcpv4Server.lpSockaddr->sa_family == AF_INET) {
            d.dhcpSrv = ((sockaddr_in*)a->Dhcpv4Server.lpSockaddr)->sin_addr;
            d.hasDhcpSrv = d.dhcpSrv.s_addr != 0;
        }
        for (ULONG i = 0; i < a->PhysicalAddressLength && i < 8; ++i)
            d.mac += fmt(i ? L"-%02X" : L"%02X", a->PhysicalAddress[i]);
        bool hw = true;
        MIB_IF_ROW2 row{};
        row.InterfaceLuid = a->Luid;
        if (GetIfEntry2(&row) == NO_ERROR) hw = row.InterfaceAndOperStatusFlags.HardwareInterface != 0;
        d.kind = classify(d.friendly, d.desc, a->IfType, hw, d.bluetooth);
        for (auto u = a->FirstUnicastAddress; u; u = u->Next) {
            int fam = u->Address.lpSockaddr->sa_family;
            if (fam == AF_INET) {
                Ip4 x;
                x.addr = ((sockaddr_in*)u->Address.lpSockaddr)->sin_addr;
                x.prefix = u->OnLinkPrefixLength;
                d.ips.push_back(x);
            } else if (fam == AF_INET6) {
                Ip6 x;
                x.addr = ((sockaddr_in6*)u->Address.lpSockaddr)->sin6_addr;
                x.prefix = u->OnLinkPrefixLength;
                x.linkLocal = IN6_IS_ADDR_LINKLOCAL(&x.addr) != 0;
                x.manual = u->PrefixOrigin == IpPrefixOriginManual;
                x.temp = u->SuffixOrigin == IpSuffixOriginRandom;
                d.ips6.push_back(x);
            }
        }
        for (auto g = a->FirstGatewayAddress; g; g = g->Next) {
            int fam = g->Address.lpSockaddr->sa_family;
            if (fam == AF_INET) d.gws.push_back(((sockaddr_in*)g->Address.lpSockaddr)->sin_addr);
            else if (fam == AF_INET6) d.gws6.push_back(((sockaddr_in6*)g->Address.lpSockaddr)->sin6_addr);
        }
        for (auto sv = a->FirstDnsServerAddress; sv; sv = sv->Next) {
            int fam = sv->Address.lpSockaddr->sa_family;
            if (fam == AF_INET) {
                d.dns.push_back(((sockaddr_in*)sv->Address.lpSockaddr)->sin_addr);
            } else if (fam == AF_INET6) {
                const IN6_ADDR& x = ((sockaddr_in6*)sv->Address.lpSockaddr)->sin6_addr;
                if (!IN6_IS_ADDR_SITELOCAL(&x)) d.dns6.push_back(x);
            }
        }
        r.push_back(d);
    }
    if (withDisabled) {
        DWORD now = GetTickCount();
        if (!g_disabledStamp || now - g_disabledStamp > 10000) {
            g_disabledCache.clear();
            appendDisabledAdapters(g_disabledCache);
            g_disabledStamp = now ? now : 1;
        }
        for (auto& dis : g_disabledCache) {
            bool dup = false;
            for (auto& x : r)
                if (x.guidStr == dis.guidStr) dup = true;
            if (!dup) r.push_back(dis);
        }
    }
    return r;
}

static const Adapter* findAdapter(const vector<Adapter>& ads, const wstring& name) {
    for (auto& a : ads)
        if (ieq(a.friendly, name)) return &a;
    for (auto& a : ads)
        if (ieq(a.desc, name)) return &a;
    return nullptr;
}

static bool arpMac(IPAddr dst, IPAddr src, wstring& mac) {
    ULONG phys[2] = {0, 0}, len = 6;
    if (SendARP(dst, src, phys, &len) != NO_ERROR || len < 6) return false;
    const BYTE* b = (const BYTE*)phys;
    mac.clear();
    for (int i = 0; i < 6; ++i) mac += fmt(i ? L"-%02X" : L"%02X", (unsigned)b[i]);
    return true;
}

static bool arpFresh(const Adapter& a, IPAddr dst, wstring& mac) {
    MIB_IPNET_ROW2 row;
    memset(&row, 0, sizeof(row));
    row.Address.si_family = AF_INET;
    row.Address.Ipv4.sin_family = AF_INET;
    row.Address.Ipv4.sin_addr.s_addr = dst;
    row.InterfaceLuid = a.luid;
    if (GetIpNetEntry2(&row) == NO_ERROR && row.State != NlnsPermanent) DeleteIpNetEntry2(&row);
    return arpMac(dst, a.ips.empty() ? 0 : a.ips[0].addr.s_addr, mac);
}

static bool gatewayMac(const Adapter& a, wstring& mac) {
    if (a.gws.empty()) return false;
    return arpMac(a.gws[0].s_addr, a.ips.empty() ? 0 : a.ips[0].addr.s_addr, mac);
}

static bool pingHost(const wstring& host) {
    IN_ADDR a{};
    if (!parseIp(host, a)) {
        ADDRINFOW hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        if (GetAddrInfoW(host.c_str(), nullptr, &hints, &res) != 0 || !res) return false;
        a = ((sockaddr_in*)res->ai_addr)->sin_addr;
        FreeAddrInfoW(res);
    }
    HANDLE h = IcmpCreateFile();
    if (h == INVALID_HANDLE_VALUE) return false;
    char data[8] = "MilleIp";
    BYTE reply[256];
    DWORD n = IcmpSendEcho(h, a.s_addr, data, sizeof(data), nullptr, reply, sizeof(reply), 1000);
    bool ok = n > 0 && ((ICMP_ECHO_REPLY*)reply)->Status == IP_SUCCESS;
    IcmpCloseHandle(h);
    return ok;
}

static bool isActive(const Profile& p, const vector<Adapter>& ads) {
    const Adapter* a = findAdapter(ads, p.adapter);
    if (!a || a->dhcp != p.dhcp) return false;
    if (!p.dhcp) {
        if (a->ips.size() != p.ips.size()) return false;
        for (auto& w : p.ips) {
            bool found = false;
            for (auto& x : a->ips)
                if (sameAddr(w.addr, x.addr) && w.prefix == x.prefix) found = true;
            if (!found) return false;
        }
    }
    if (!p.dns.empty()) {
        if (a->dns.size() != p.dns.size()) return false;
        for (size_t i = 0; i < p.dns.size(); ++i)
            if (!sameAddr(p.dns[i], a->dns[i])) return false;
    }
    if (p.ip6mode) {
        size_t manual = 0;
        for (auto& x : a->ips6)
            if (x.manual && !x.linkLocal) ++manual;
        if (p.ip6mode == 1 && manual) return false;
        if (p.ip6mode == 2) {
            if (manual != p.ips6.size()) return false;
            for (auto& w : p.ips6) {
                bool found = false;
                for (auto& x : a->ips6)
                    if (x.manual && sameAddr6(w.addr, x.addr) && w.prefix == x.prefix) found = true;
                if (!found) return false;
            }
        }
        if (!p.dns6.empty()) {
            if (a->dns6.size() != p.dns6.size()) return false;
            for (size_t i = 0; i < p.dns6.size(); ++i)
                if (!sameAddr6(p.dns6[i], a->dns6[i])) return false;
        }
    }
    return true;
}

static wstring statusText() {
    wstring s;
    for (auto& a : getAdapters()) {
        s += a.friendly + L"  (" + a.desc + L")\n";
        s += wstring(L"   ") + (a.up ? TR(L"verbunden", L"connected") : TR(L"getrennt", L"disconnected")) + L", " + (a.dhcp ? L"DHCP" : TR(L"statisch", L"static")) + L"\n";
        s += L"   IP:      ";
        for (size_t i = 0; i < a.ips.size(); ++i) s += (i ? L", " : L"") + ipStr(a.ips[i].addr) + fmt(L"/%u", a.ips[i].prefix);
        s += L"\n   Gateway: ";
        for (size_t i = 0; i < a.gws.size(); ++i) s += (i ? L", " : L"") + ipStr(a.gws[i]);
        s += L"\n   DNS:     ";
        for (size_t i = 0; i < a.dns.size(); ++i) s += (i ? L", " : L"") + ipStr(a.dns[i]);
        s += L"\n\n";
    }
    return s;
}

// Applying profiles: IP Helper API, netsh only for switching DHCP <-> static
static DWORD runHidden(wstring cmdline) {
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, &cmdline[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        return GetLastError();
    WaitForSingleObject(pi.hProcess, 30000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code;
}

static wstring netsh() {
    wchar_t d[MAX_PATH];
    GetSystemDirectoryW(d, MAX_PATH);
    return wstring(L"\"") + d + L"\\netsh.exe\" ";
}

static bool applyAddresses(const Adapter& a, const Profile& p, wstring& log) {
    bool ok = true;
    PMIB_UNICASTIPADDRESS_TABLE t = nullptr;
    if (GetUnicastIpAddressTable(AF_INET, &t) == NO_ERROR) {
        for (ULONG i = 0; i < t->NumEntries; ++i) {
            MIB_UNICASTIPADDRESS_ROW& row = t->Table[i];
            if (row.InterfaceLuid.Value != a.luid.Value) continue;
            IN_ADDR x = row.Address.Ipv4.sin_addr;
            bool keep = false;
            for (auto& w : p.ips)
                if (sameAddr(w.addr, x) && w.prefix == row.OnLinkPrefixLength) keep = true;
            if (!keep) {
                DWORD e = DeleteUnicastIpAddressEntry(&row);
                if (e != NO_ERROR && e != ERROR_NOT_FOUND) log += fmt(TR(L"Löschen %ls: Fehler %lu\n", L"Delete %ls: error %lu\n"), ipStr(x).c_str(), e);
            }
        }
        FreeMibTable(t);
    }
    for (auto& w : p.ips) {
        MIB_UNICASTIPADDRESS_ROW r;
        InitializeUnicastIpAddressEntry(&r);
        r.InterfaceLuid = a.luid;
        r.Address.si_family = AF_INET;
        r.Address.Ipv4.sin_family = AF_INET;
        r.Address.Ipv4.sin_addr = w.addr;
        r.OnLinkPrefixLength = w.prefix;
        r.PrefixOrigin = IpPrefixOriginManual;
        r.SuffixOrigin = IpSuffixOriginManual;
        r.DadState = IpDadStatePreferred;
        DWORD e = CreateUnicastIpAddressEntry(&r);
        if (e != NO_ERROR && e != ERROR_OBJECT_ALREADY_EXISTS) {
            log += fmt(TR(L"Setzen %ls: Fehler %lu\n", L"Set %ls: error %lu\n"), ipStr(w.addr).c_str(), e);
            ok = false;
        }
    }
    return ok;
}

static bool applyGateway(const Adapter& a, const Profile& p, wstring& log) {
    bool ok = true, present = false;
    PMIB_IPFORWARD_TABLE2 t = nullptr;
    if (GetIpForwardTable2(AF_INET, &t) == NO_ERROR) {
        for (ULONG i = 0; i < t->NumEntries; ++i) {
            MIB_IPFORWARD_ROW2& row = t->Table[i];
            if (row.InterfaceLuid.Value != a.luid.Value || row.DestinationPrefix.PrefixLength != 0) continue;
            if (p.hasGw && sameAddr(row.NextHop.Ipv4.sin_addr, p.gw)) {
                present = true;
                continue;
            }
            DWORD e = DeleteIpForwardEntry2(&row);
            if (e != NO_ERROR && e != ERROR_NOT_FOUND)
                log += fmt(TR(L"Gateway löschen: Fehler %lu\n", L"Delete gateway: error %lu\n"), e);
        }
        FreeMibTable(t);
    }
    if (p.hasGw && !present) {
        MIB_IPFORWARD_ROW2 r;
        InitializeIpForwardEntry(&r);
        r.InterfaceLuid = a.luid;
        r.DestinationPrefix.Prefix.si_family = AF_INET;
        r.DestinationPrefix.Prefix.Ipv4.sin_family = AF_INET;
        r.DestinationPrefix.PrefixLength = 0;
        r.NextHop.si_family = AF_INET;
        r.NextHop.Ipv4.sin_family = AF_INET;
        r.NextHop.Ipv4.sin_addr = p.gw;
        r.Protocol = (NL_ROUTE_PROTOCOL)MIB_IPPROTO_NETMGMT;
        r.Origin = NlroManual;
        r.Metric = 0;
        DWORD e = CreateIpForwardEntry2(&r);
        if (e != NO_ERROR && e != ERROR_OBJECT_ALREADY_EXISTS) {
            log += fmt(TR(L"Gateway setzen: Fehler %lu\n", L"Set gateway: error %lu\n"), e);
            ok = false;
        }
    }
    return ok;
}

static void setMultiSz(HKEY k, const wchar_t* name, const vector<wstring>& vals) {
    wstring data;
    for (auto& v : vals) {
        data += v;
        data.push_back(L'\0');
    }
    data.push_back(L'\0');
    if (vals.empty()) data.push_back(L'\0');
    RegSetValueExW(k, name, 0, REG_MULTI_SZ, (const BYTE*)data.c_str(), (DWORD)(data.size() * sizeof(wchar_t)));
}

static bool persistStatic(const Adapter& a, const Profile& p, wstring& log) {
    wstring guid(a.guidStr.begin(), a.guidStr.end());
    wstring path = L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces\\" + guid;
    HKEY k;
    LONG e = RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_SET_VALUE, &k);
    if (e != ERROR_SUCCESS) {
        log += fmt(TR(L"Registry: Fehler %ld\n", L"Registry: error %ld\n"), e);
        return false;
    }
    DWORD zero = 0;
    RegSetValueExW(k, L"EnableDHCP", 0, REG_DWORD, (const BYTE*)&zero, sizeof(zero));
    vector<wstring> ips, masks, gws, metrics;
    for (auto& w : p.ips) {
        ips.push_back(ipStr(w.addr));
        masks.push_back(maskStr(w.prefix));
    }
    if (p.hasGw) {
        gws.push_back(ipStr(p.gw));
        metrics.push_back(L"0");
    }
    setMultiSz(k, L"IPAddress", ips);
    setMultiSz(k, L"SubnetMask", masks);
    setMultiSz(k, L"DefaultGateway", gws);
    setMultiSz(k, L"DefaultGatewayMetric", metrics);
    RegCloseKey(k);
    return true;
}

struct DnsIfSettings {
    ULONG Version;
    ULONG64 Flags;
    PWSTR Domain;
    PWSTR NameServer;
    PWSTR SearchList;
    ULONG RegistrationEnabled;
    ULONG RegisterAdapterName;
    ULONG EnableLLMNR;
    ULONG QueryAdapterName;
    PWSTR ProfileNameServer;
};
typedef DWORD(WINAPI* PFN_SetInterfaceDnsSettings)(GUID, const DnsIfSettings*);

static bool applyDns(const Adapter& a, const Profile& p, wstring& log) {
    wstring list;
    for (size_t i = 0; i < p.dns.size(); ++i) list += (i ? L"," : L"") + ipStr(p.dns[i]);

    static PFN_SetInterfaceDnsSettings fn =
        (PFN_SetInterfaceDnsSettings)(void*)GetProcAddress(LoadLibraryW(L"iphlpapi.dll"), "SetInterfaceDnsSettings");
    if (fn) {
        DnsIfSettings s{};
        s.Version = 1;
        s.Flags = 0x0002;
        s.NameServer = list.data();
        DWORD e = fn(a.guid, &s);
        if (e == NO_ERROR) return true;
        log += fmt(L"SetInterfaceDnsSettings: Fehler %lu, versuche netsh\n", e);
    }

    wstring nm = L"name=\"" + a.friendly + L"\"";
    if (p.dns.empty()) return runHidden(netsh() + L"interface ipv4 set dnsservers " + nm + L" source=dhcp") == 0;
    bool ok = runHidden(netsh() + L"interface ipv4 set dnsservers " + nm + L" source=static address=" +
                        ipStr(p.dns[0]) + L" register=primary validate=no") == 0;
    for (size_t i = 1; i < p.dns.size(); ++i)
        ok = runHidden(netsh() + L"interface ipv4 add dnsservers " + nm + L" address=" + ipStr(p.dns[i]) +
                       fmt(L" index=%u validate=no", (unsigned)(i + 1))) == 0 && ok;
    if (!ok) log += TR(L"DNS über netsh fehlgeschlagen\n", L"DNS via netsh failed\n");
    return ok;
}

static void rememberLast(const Profile& p) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\MilleIpSwitch\\Last", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS)
        return;
    RegSetValueExW(k, p.adapter.c_str(), 0, REG_SZ, (const BYTE*)p.name.c_str(), (DWORD)((p.name.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
}

static bool applyIpv6(const Adapter& a, const Profile& p, wstring& log) {
    if (p.ip6mode == 0) return true;
    bool ok = true;
    bool stat = p.ip6mode == 2;

    MIB_IPINTERFACE_ROW ir;
    InitializeIpInterfaceEntry(&ir);
    ir.Family = AF_INET6;
    ir.InterfaceLuid = a.luid;
    if (GetIpInterfaceEntry(&ir) == NO_ERROR) {
        ir.RouterDiscoveryBehavior = stat ? RouterDiscoveryDisabled : RouterDiscoveryDhcp;
        DWORD e = SetIpInterfaceEntry(&ir);
        if (e != NO_ERROR) log += fmt(L"IPv6 interface: error %lu\n", e);
    }

    PMIB_UNICASTIPADDRESS_TABLE t = nullptr;
    if (GetUnicastIpAddressTable(AF_INET6, &t) == NO_ERROR) {
        for (ULONG i = 0; i < t->NumEntries; ++i) {
            MIB_UNICASTIPADDRESS_ROW& row = t->Table[i];
            if (row.InterfaceLuid.Value != a.luid.Value) continue;
            IN6_ADDR x = row.Address.Ipv6.sin6_addr;
            if (IN6_IS_ADDR_LINKLOCAL(&x)) continue;
            bool wanted = false;
            if (stat)
                for (auto& w : p.ips6)
                    if (sameAddr6(w.addr, x) && w.prefix == row.OnLinkPrefixLength) wanted = true;
            bool manual = row.PrefixOrigin == IpPrefixOriginManual;
            if (!wanted && (stat || manual)) {
                DWORD e = DeleteUnicastIpAddressEntry(&row);
                if (e != NO_ERROR && e != ERROR_NOT_FOUND) log += fmt(L"IPv6 delete: error %lu\n", e);
            }
        }
        FreeMibTable(t);
    }
    if (stat)
        for (auto& w : p.ips6) {
            MIB_UNICASTIPADDRESS_ROW r;
            InitializeUnicastIpAddressEntry(&r);
            r.InterfaceLuid = a.luid;
            r.Address.si_family = AF_INET6;
            r.Address.Ipv6.sin6_family = AF_INET6;
            r.Address.Ipv6.sin6_addr = w.addr;
            r.OnLinkPrefixLength = w.prefix;
            r.PrefixOrigin = IpPrefixOriginManual;
            r.SuffixOrigin = IpSuffixOriginManual;
            r.DadState = IpDadStatePreferred;
            DWORD e = CreateUnicastIpAddressEntry(&r);
            if (e != NO_ERROR && e != ERROR_OBJECT_ALREADY_EXISTS) {
                log += fmt(L"IPv6 set: error %lu\n", e);
                ok = false;
            }
        }

    bool present = false;
    PMIB_IPFORWARD_TABLE2 ft = nullptr;
    if (GetIpForwardTable2(AF_INET6, &ft) == NO_ERROR) {
        for (ULONG i = 0; i < ft->NumEntries; ++i) {
            MIB_IPFORWARD_ROW2& row = ft->Table[i];
            if (row.InterfaceLuid.Value != a.luid.Value || row.DestinationPrefix.PrefixLength != 0) continue;
            if (stat && p.hasGw6 && sameAddr6(row.NextHop.Ipv6.sin6_addr, p.gw6)) {
                present = true;
                continue;
            }
            if (stat || row.Origin == NlroManual) {
                DWORD e = DeleteIpForwardEntry2(&row);
                if (e != NO_ERROR && e != ERROR_NOT_FOUND) log += fmt(L"IPv6 gateway delete: error %lu\n", e);
            }
        }
        FreeMibTable(ft);
    }
    if (stat && p.hasGw6 && !present) {
        MIB_IPFORWARD_ROW2 r;
        InitializeIpForwardEntry(&r);
        r.InterfaceLuid = a.luid;
        r.DestinationPrefix.Prefix.si_family = AF_INET6;
        r.DestinationPrefix.Prefix.Ipv6.sin6_family = AF_INET6;
        r.DestinationPrefix.PrefixLength = 0;
        r.NextHop.si_family = AF_INET6;
        r.NextHop.Ipv6.sin6_family = AF_INET6;
        r.NextHop.Ipv6.sin6_addr = p.gw6;
        r.Protocol = (NL_ROUTE_PROTOCOL)MIB_IPPROTO_NETMGMT;
        r.Origin = NlroManual;
        r.Metric = 0;
        DWORD e = CreateIpForwardEntry2(&r);
        if (e != NO_ERROR && e != ERROR_OBJECT_ALREADY_EXISTS) {
            log += fmt(L"IPv6 gateway set: error %lu\n", e);
            ok = false;
        }
    }

    static PFN_SetInterfaceDnsSettings fn =
        (PFN_SetInterfaceDnsSettings)(void*)GetProcAddress(LoadLibraryW(L"iphlpapi.dll"), "SetInterfaceDnsSettings");
    if (fn) {
        wstring list;
        for (size_t i = 0; i < p.dns6.size(); ++i) list += (i ? L"," : L"") + ip6Str(p.dns6[i]);
        DnsIfSettings sd{};
        sd.Version = 1;
        sd.Flags = 0x0003;
        sd.NameServer = list.data();
        DWORD e = fn(a.guid, &sd);
        if (e != NO_ERROR) {
            log += fmt(L"IPv6 DNS: error %lu\n", e);
            ok = false;
        }
    }
    return ok;
}

static bool applyProfile(const Profile& p, wstring& msg) {
    double t0 = nowMs();
    vector<Adapter> ads = getAdapters();
    const Adapter* a = findAdapter(ads, p.adapter);
    if (!a) {
        msg = TR(L"Adapter nicht gefunden: ", L"Adapter not found: ") + p.adapter;
        return false;
    }

    wstring log;
    bool ok = true, viaNetsh = false;
    wstring nm = L"name=\"" + a->friendly + L"\"";

    if (p.dhcp) {
        if (!a->dhcp) {
            viaNetsh = true;
            DWORD c = runHidden(netsh() + L"interface ipv4 set address " + nm + L" source=dhcp");
            if (c) {
                ok = false;
                log += fmt(L"netsh (DHCP): Fehler %lu\n", c);
            }
        }
    } else {
        if (a->dhcp) {
            viaNetsh = true;
            const Ip4& f = p.ips[0];
            wstring cmd = netsh() + L"interface ipv4 set address " + nm + L" source=static address=" +
                          ipStr(f.addr) + L" mask=" + maskStr(f.prefix) +
                          (p.hasGw ? L" gateway=" + ipStr(p.gw) + L" gwmetric=0" : wstring(L" gateway=none"));
            DWORD c = runHidden(cmd);
            if (c) {
                ok = false;
                log += fmt(L"netsh (statisch): Fehler %lu\n", c);
            }
        }
        ok = applyAddresses(*a, p, log) && ok;
        ok = applyGateway(*a, p, log) && ok;
        ok = persistStatic(*a, p, log) && ok;
    }
    ok = applyDns(*a, p, log) && ok;
    ok = applyIpv6(*a, p, log) && ok;

    double ms = nowMs() - t0;
    msg = fmt(L"%ls → %ls: %ls (%.0f ms%ls)", p.name.c_str(), a->friendly.c_str(), ok ? L"OK" : TR(L"mit Fehlern", L"with errors"), ms,
              viaNetsh ? TR(L", inkl. DHCP-Moduswechsel", L", incl. DHCP mode change") : L"");
    if (!log.empty()) msg += L"\n" + log;
    if (ok) rememberLast(p);
    return ok;
}

// Elevation
static bool isElevated() {
    HANDLE t;
    TOKEN_ELEVATION e{};
    DWORD sz = 0;
    bool r = false;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t)) {
        if (GetTokenInformation(t, TokenElevation, &e, sizeof(e), &sz)) r = e.TokenIsElevated != 0;
        CloseHandle(t);
    }
    return r;
}

static int relaunchElevated(const wstring& params, bool wait) {
    SHELLEXECUTEINFOW s{};
    s.cbSize = sizeof(s);
    s.fMask = SEE_MASK_NOCLOSEPROCESS;
    s.lpVerb = L"runas";
    s.lpFile = g_exePath.c_str();
    s.lpParameters = params.c_str();
    s.nShow = SW_HIDE;
    if (!ShellExecuteExW(&s)) return -1;
    DWORD c = 0;
    if (wait && s.hProcess) {
        WaitForSingleObject(s.hProcess, INFINITE);
        GetExitCodeProcess(s.hProcess, &c);
    }
    if (s.hProcess) CloseHandle(s.hProcess);
    return (int)c;
}

// Autostart via Task Scheduler (elevated, no UAC prompt)
static const wchar_t* kTaskName = L"MilleIpSwitch";

static wstring xmlEsc(const wstring& s) {
    wstring r;
    for (wchar_t c : s) {
        if (c == L'&') r += L"&amp;";
        else if (c == L'<') r += L"&lt;";
        else if (c == L'>') r += L"&gt;";
        else r += c;
    }
    return r;
}

static bool flushDns() {
    typedef BOOL(WINAPI * PFN)();
    static HMODULE m = LoadLibraryW(L"dnsapi.dll");
    static PFN f = m ? (PFN)(void*)GetProcAddress(m, "DnsFlushResolverCache") : nullptr;
    return f && f();
}

static bool dhcpRenew(const string& guid) {
    ULONG len = 0;
    GetInterfaceInfo(nullptr, &len);
    if (!len) return false;
    vector<BYTE> buf(len);
    auto* info = (PIP_INTERFACE_INFO)buf.data();
    if (GetInterfaceInfo(info, &len) != NO_ERROR) return false;
    wstring g = lower(wstring(guid.begin(), guid.end()));
    for (LONG i = 0; i < info->NumAdapters; ++i)
        if (lower(info->Adapter[i].Name).find(g) != wstring::npos) return IpRenewAddress(&info->Adapter[i]) == NO_ERROR;
    return false;
}

static ITaskFolder* taskRoot(ITaskService** svcOut) {
    ITaskService* svc = nullptr;
    if (FAILED(CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (void**)&svc)))
        return nullptr;
    VARIANT v;
    VariantInit(&v);
    ITaskFolder* f = nullptr;
    BSTR root = SysAllocString(L"\\");
    if (FAILED(svc->Connect(v, v, v, v)) || FAILED(svc->GetFolder(root, &f))) f = nullptr;
    SysFreeString(root);
    if (!f) {
        svc->Release();
        return nullptr;
    }
    *svcOut = svc;
    return f;
}

static IRegisteredTask* taskGet(ITaskService** svc, ITaskFolder** folder) {
    *folder = taskRoot(svc);
    if (!*folder) return nullptr;
    IRegisteredTask* t = nullptr;
    BSTR name = SysAllocString(kTaskName);
    if (FAILED((*folder)->GetTask(name, &t))) t = nullptr;
    SysFreeString(name);
    return t;
}

static bool taskExists() {
    ITaskService* svc = nullptr;
    ITaskFolder* f = nullptr;
    IRegisteredTask* t = taskGet(&svc, &f);
    if (t) t->Release();
    if (f) f->Release(), svc->Release();
    return t != nullptr;
}

static bool taskRun() {
    ITaskService* svc = nullptr;
    ITaskFolder* f = nullptr;
    IRegisteredTask* t = taskGet(&svc, &f);
    bool ok = false;
    if (t) {
        VARIANT v;
        VariantInit(&v);
        IRunningTask* r = nullptr;
        ok = SUCCEEDED(t->Run(v, &r));
        if (r) r->Release();
        t->Release();
    }
    if (f) f->Release(), svc->Release();
    return ok;
}

static bool taskRemove() {
    ITaskService* svc = nullptr;
    ITaskFolder* f = taskRoot(&svc);
    if (!f) return false;
    BSTR name = SysAllocString(kTaskName);
    bool ok = SUCCEEDED(f->DeleteTask(name, 0));
    SysFreeString(name);
    f->Release();
    svc->Release();
    return ok;
}

static bool taskInstall(wstring& err) {
    wchar_t dom[256] = L"", usr[256] = L"";
    GetEnvironmentVariableW(L"USERDOMAIN", dom, 256);
    GetEnvironmentVariableW(L"USERNAME", usr, 256);
    wstring user = xmlEsc(wstring(dom) + L"\\" + usr);
    wstring xml =
        L"<?xml version=\"1.0\" encoding=\"UTF-16\"?>"
        L"<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">"
        L"<RegistrationInfo><Author>666mille</Author><Description>MilleIpSwitch – network profile switcher</Description></RegistrationInfo>"
        L"<Triggers><LogonTrigger><Enabled>true</Enabled><UserId>" + user + L"</UserId></LogonTrigger></Triggers>"
        L"<Principals><Principal id=\"Author\"><UserId>" + user +
        L"</UserId><LogonType>InteractiveToken</LogonType><RunLevel>HighestAvailable</RunLevel></Principal></Principals>"
        L"<Settings><MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>"
        L"<DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries><StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>"
        L"<ExecutionTimeLimit>PT0S</ExecutionTimeLimit><AllowStartOnDemand>true</AllowStartOnDemand><Priority>7</Priority></Settings>"
        L"<Actions Context=\"Author\"><Exec><Command>" + xmlEsc(g_exePath) +
        L"</Command><Arguments>--tray</Arguments></Exec></Actions></Task>";
    ITaskService* svc = nullptr;
    ITaskFolder* f = taskRoot(&svc);
    if (!f) {
        err = TR(L"Aufgabenplanung nicht erreichbar.", L"Task Scheduler not available.");
        return false;
    }
    VARIANT empty;
    VariantInit(&empty);
    BSTR name = SysAllocString(kTaskName), text = SysAllocString(xml.c_str());
    IRegisteredTask* t = nullptr;
    HRESULT hr = f->RegisterTask(name, text, TASK_CREATE_OR_UPDATE, empty, empty, TASK_LOGON_INTERACTIVE_TOKEN, empty, &t);
    SysFreeString(name);
    SysFreeString(text);
    if (t) t->Release();
    f->Release();
    svc->Release();
    if (FAILED(hr)) err = fmt(L"HRESULT 0x%08lX", (unsigned long)hr);
    return SUCCEEDED(hr);
}

static HWND startTrayViaTask() {
    if (!taskExists() || !taskRun()) return nullptr;
    for (int i = 0; i < 60; ++i) {
        Sleep(100);
        if (HWND w = FindWindowW(kWndClass, nullptr)) {
            Sleep(200);
            return w;
        }
    }
    return nullptr;
}

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

// UI helpers (icons from Segoe Fluent Icons / Segoe MDL2 Assets)
namespace G {
const wchar_t Ethernet = 0xE839, Wifi = 0xE701, Vpn = 0xE705, Bluetooth = 0xE702, Monitor = 0xE7F4,
              Globe = 0xE774, Network = 0xE968, Check = 0xE73E, Settings = 0xE713, Power = 0xE7E8,
              Refresh = 0xE72C, Sort = 0xE8CB, Delete = 0xE74D, Completed = 0xE930, Error = 0xE783, Info = 0xE946, Add = 0xE710,
              CheckboxOn = 0xE73A, CheckboxOff = 0xE739;
}

namespace C {
const COLORREF Bg = RGB(243, 243, 243), Card = RGB(255, 255, 255), Border = RGB(225, 225, 225),
               Text = RGB(27, 27, 27), Sub = RGB(96, 96, 96), Faint = RGB(150, 150, 150),
               Accent = RGB(0, 95, 184), AccentBg = RGB(224, 238, 250), Green = RGB(16, 124, 16),
               GreenBg = RGB(221, 243, 218), GreyBg = RGB(235, 235, 235), Red = RGB(196, 43, 28),
               RedBg = RGB(253, 231, 233), Hover = RGB(228, 228, 228), Select = RGB(204, 228, 247),
               Static = RGB(124, 77, 196), StaticBg = RGB(240, 233, 250);
}

static UINT dpiOf(HWND h) {
    typedef UINT(WINAPI * PFN)(HWND);
    static PFN f = (PFN)(void*)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    if (f && h) return f(h);
    HDC dc = GetDC(nullptr);
    UINT d = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    return d;
}
static int S(int v, UINT dpi) { return MulDiv(v, (int)dpi, 96); }

static int CALLBACK fontEnumProc(const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM p) {
    *(bool*)p = true;
    return 0;
}
static bool fontExists(const wchar_t* face) {
    LOGFONTW lf{};
    lf.lfCharSet = DEFAULT_CHARSET;
    lstrcpynW(lf.lfFaceName, face, LF_FACESIZE);
    bool found = false;
    HDC dc = GetDC(nullptr);
    EnumFontFamiliesExW(dc, &lf, fontEnumProc, (LPARAM)&found, 0);
    ReleaseDC(nullptr, dc);
    return found;
}
static const wchar_t* glyphFace() {
    static const wchar_t* f = fontExists(L"Segoe Fluent Icons") ? L"Segoe Fluent Icons" : L"Segoe MDL2 Assets";
    return f;
}
static HFONT mkFont(const wchar_t* face, int tenthPt, int weight, UINT dpi) {
    LOGFONTW lf{};
    lf.lfHeight = -MulDiv(tenthPt, (int)dpi, 720);
    lf.lfWeight = weight;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lstrcpynW(lf.lfFaceName, face, LF_FACESIZE);
    return CreateFontIndirectW(&lf);
}
static void delFont(HFONT& f) {
    if (f) DeleteObject(f);
    f = nullptr;
}
static void drawText(HDC dc, const wstring& s, RECT r, COLORREF c, HFONT f, UINT flags) {
    HGDIOBJ o = SelectObject(dc, f);
    SetTextColor(dc, c);
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, s.c_str(), (int)s.size(), &r, flags | DT_NOPREFIX);
    SelectObject(dc, o);
}
static void drawGlyph(HDC dc, wchar_t g, RECT r, COLORREF c, HFONT f) {
    drawText(dc, wstring(1, g), r, c, f, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}
static int textWidth(HDC dc, const wstring& s, HFONT f) {
    HGDIOBJ o = SelectObject(dc, f);
    SIZE sz{};
    GetTextExtentPoint32W(dc, s.c_str(), (int)s.size(), &sz);
    SelectObject(dc, o);
    return sz.cx;
}
static void fillRect(HDC dc, RECT r, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, &r, b);
    DeleteObject(b);
}
static void fillRound(HDC dc, RECT r, int rad, COLORREF fill, COLORREF border) {
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, rad, rad);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
}
static void fillCircle(HDC dc, int cx, int cy, int rad, COLORREF fill) {
    RECT r{cx - rad, cy - rad, cx + rad + 1, cy + rad + 1};
    fillRound(dc, r, rad * 2 + 1, fill, fill);
}
static int pillWidth(HDC dc, const wstring& t, HFONT f, UINT dpi) { return textWidth(dc, t, f) + S(18, dpi); }
static void drawPill(HDC dc, const wstring& t, RECT r, COLORREF fg, COLORREF bg, HFONT f) {
    fillRound(dc, r, r.bottom - r.top, bg, bg);
    drawText(dc, t, r, fg, f, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void drawFlag(HDC dc, int lang, RECT r) {
    int w = r.right - r.left, h = r.bottom - r.top;
    if (lang == 2) {
        fillRect(dc, r, RGB(170, 21, 27));
        fillRect(dc, RECT{r.left, r.top + h / 4, r.right, r.bottom - h / 4}, RGB(241, 191, 0));
    } else if (lang == 3) {
        for (int i = 0; i < 3; ++i) {
            RECT st{r.left + w * i / 3, r.top, r.left + w * (i + 1) / 3, r.bottom};
            fillRect(dc, st, i == 0 ? RGB(0, 35, 149) : i == 1 ? RGB(255, 255, 255) : RGB(237, 41, 57));
        }
    } else if (lang == 0) {
        for (int i = 0; i < 3; ++i) {
            RECT st{r.left, r.top + h * i / 3, r.right, r.top + h * (i + 1) / 3};
            fillRect(dc, st, i == 0 ? RGB(0, 0, 0) : i == 1 ? RGB(221, 0, 0) : RGB(255, 206, 0));
        }
    } else {
        fillRect(dc, r, RGB(1, 33, 105));
        HRGN clip = CreateRectRgn(r.left, r.top, r.right, r.bottom);
        SelectClipRgn(dc, clip);
        auto diag = [&](int width, COLORREF c) {
            HPEN pen = CreatePen(PS_SOLID, std::max(1, width), c);
            HGDIOBJ o = SelectObject(dc, pen);
            MoveToEx(dc, r.left, r.top, nullptr);
            LineTo(dc, r.right, r.bottom);
            MoveToEx(dc, r.left, r.bottom, nullptr);
            LineTo(dc, r.right, r.top);
            SelectObject(dc, o);
            DeleteObject(pen);
        };
        diag(h / 4 + 1, RGB(255, 255, 255));
        diag(h / 10, RGB(200, 16, 46));
        int cw = std::max(3, h / 3), rw = std::max(1, h / 5);
        fillRect(dc, RECT{r.left, r.top + (h - cw) / 2, r.right, r.top + (h + cw) / 2}, RGB(255, 255, 255));
        fillRect(dc, RECT{r.left + (w - cw) / 2, r.top, r.left + (w + cw) / 2, r.bottom}, RGB(255, 255, 255));
        fillRect(dc, RECT{r.left, r.top + (h - rw) / 2, r.right, r.top + (h + rw) / 2}, RGB(200, 16, 46));
        fillRect(dc, RECT{r.left + (w - rw) / 2, r.top, r.left + (w + rw) / 2, r.bottom}, RGB(200, 16, 46));
        SelectClipRgn(dc, nullptr);
        DeleteObject(clip);
    }
    HBRUSH fb = CreateSolidBrush(RGB(190, 190, 190));
    FrameRect(dc, &r, fb);
    DeleteObject(fb);
}
static const wchar_t* langName(int lang) {
    static const wchar_t* n[] = {L"Deutsch", L"English", L"Español", L"Français"};
    return n[lang];
}

static void drawCheck(HDC dc, RECT r, HFONT f, UINT dpi) {
    int o = std::max(1, S(1, dpi));
    drawGlyph(dc, G::Check, r, C::Green, f);
    OffsetRect(&r, o, 0);
    drawGlyph(dc, G::Check, r, C::Green, f);
    OffsetRect(&r, -o, o);
    drawGlyph(dc, G::Check, r, C::Green, f);
}

static wchar_t kindGlyph(int kind, bool bt = false) {
    if (bt) return G::Bluetooth;
    switch (kind) {
    case K_ETH: return G::Ethernet;
    case K_WLAN: return G::Wifi;
    case K_VPN: return G::Vpn;
    case K_VIRT: return G::Monitor;
    default: return G::Globe;
    }
}
static const wchar_t* kindName(int kind) {
    switch (kind) {
    case K_ETH: return L"Ethernet";
    case K_WLAN: return TR(L"WLAN", L"Wi-Fi");
    case K_VPN: return L"VPN";
    case K_VIRT: return TR(L"Virtuelle Adapter", L"Virtual adapters");
    default: return TR(L"Bluetooth & Sonstige", L"Bluetooth & other");
    }
}
static wchar_t profileGlyph(const Profile& p, const vector<Adapter>& ads) {
    const Adapter* a = findAdapter(ads, p.adapter);
    return a ? kindGlyph(a->kind, a->bluetooth) : G::Ethernet;
}
static wstring profileDetail(const Profile& p) {
    return p.dhcp ? wstring(L"DHCP") : (p.ips.empty() ? wstring(TR(L"statisch", L"static")) : ipStr(p.ips[0].addr));
}
static wstring speedStr(ULONG64 b) {
    if (b >= 1000000000ULL) return (b % 1000000000ULL ? fmt(L"%.1f", b / 1e9) : fmt(L"%llu", b / 1000000000ULL)) + L" Gbit/s";
    if (b >= 1000000ULL) return fmt(L"%llu Mbit/s", b / 1000000ULL);
    return fmt(L"%llu kbit/s", b / 1000ULL);
}
static wstring firstLine(const wstring& s) { return s.substr(0, s.find(L'\n')); }

static DWORD regFlag(const wchar_t* name) {
    DWORD v = 0, sz = sizeof(v);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\MilleIpSwitch", name, RRF_RT_REG_DWORD, nullptr, &v, &sz);
    return v;
}
static void regSetFlag(const wchar_t* name) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\MilleIpSwitch", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k,
                        nullptr) != ERROR_SUCCESS)
        return;
    DWORD one = 1;
    RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE*)&one, sizeof(one));
    RegCloseKey(k);
}

#define WM_TRAY (WM_APP + 1)
#define WM_APPLYDONE (WM_APP + 2)
#define WM_OPENEDITOR (WM_APP + 3)
#define WM_FIRSTRUN (WM_APP + 4)
enum { ID_STATUS = 1, ID_EDITOR, ID_EXIT, ID_AUTOSTART, ID_LANG0 = 10, ID_UPD_OPEN = 20, ID_UPD_CHECK, ID_UPD_AUTO, ID_AUTOSW, ID_PROFILE0 = 1000 };

static HWND g_hwnd, g_hDlg, g_hStatus;
static HICON g_icon, g_iconBig, g_iconMenu, g_iconWinSm, g_iconWinBig;
static NOTIFYICONDATAW g_nid;
static UINT g_taskbarCreated;
static vector<Profile> g_profiles;
static volatile LONG g_busy = 0;
static bool g_autostart = false;
static bool g_balloonUpdate = false;
static bool g_autoEnabled = true;
static int g_applyKind = 0;
static vector<Profile> g_restoreQueue;
static std::map<wstring, DWORD> g_autoHold;
static wstring g_updateTag, g_updateUrl;
static bool g_updateAuto = true;
static void setLanguage(int lang);
static void langPopup(HWND anchor);

#define TIMER_TRAYCHECK 9

static bool trayAdd() {
    g_nid = NOTIFYICONDATAW{};
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon = g_icon;
    lstrcpynW(g_nid.szTip, L"MilleIpSwitch", ARRAYSIZE(g_nid.szTip));
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    if (!Shell_NotifyIconW(NIM_ADD, &g_nid)) return false;
    g_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_nid);
    return true;
}

static void trayEnsure() {
    NOTIFYICONDATAW probe = g_nid;
    probe.uFlags = NIF_TIP;
    if (!Shell_NotifyIconW(NIM_MODIFY, &probe)) trayAdd();
}

static void trayBalloon(const wstring& title, const wstring& text, bool err) {
    g_balloonUpdate = false;
    g_nid.uFlags = NIF_INFO;
    lstrcpynW(g_nid.szInfoTitle, title.c_str(), ARRAYSIZE(g_nid.szInfoTitle));
    lstrcpynW(g_nid.szInfo, text.c_str(), ARRAYSIZE(g_nid.szInfo));
    g_nid.dwInfoFlags = err ? NIIF_ERROR : NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

#define WM_UPDATEDONE (WM_APP + 5)
#define TIMER_UPDATE 10

struct UpdateResult {
    int status;
    wstring tag;
    bool manual;
};

static void regSetDword(const wchar_t* name, DWORD v) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\MilleIpSwitch", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) !=
        ERROR_SUCCESS)
        return;
    RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    RegCloseKey(k);
}

static DWORD todayDays() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return (DWORD)(u.QuadPart / 864000000000ULL);
}

static void parseVersion(const wstring& s, int v[3]) {
    v[0] = v[1] = v[2] = 0;
    size_t i = (!s.empty() && (s[0] == L'v' || s[0] == L'V')) ? 1 : 0;
    for (int idx = 0; i < s.size() && idx < 3; ++i) {
        if (s[i] >= L'0' && s[i] <= L'9') v[idx] = v[idx] * 10 + (s[i] - L'0');
        else if (s[i] == L'.') ++idx;
        else break;
    }
}

static bool isNewerVersion(const wstring& tag) {
    int a[3], b[3];
    parseVersion(tag, a);
    parseVersion(NS_VERSION, b);
    for (int i = 0; i < 3; ++i)
        if (a[i] != b[i]) return a[i] > b[i];
    return false;
}

static string httpsGet(const wchar_t* host, const wchar_t* path, DWORD& status) {
    string body;
    status = 0;
    HINTERNET ses = WinHttpOpen(L"MilleIpSwitch/" NS_VERSION, 4, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses)
        ses = WinHttpOpen(L"MilleIpSwitch/" NS_VERSION, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                          WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) return body;
    WinHttpSetTimeouts(ses, 5000, 5000, 8000, 8000);
    HINTERNET con = WinHttpConnect(ses, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET req = con ? WinHttpOpenRequest(con, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             WINHTTP_FLAG_SECURE)
                        : nullptr;
    if (req && WinHttpSendRequest(req, L"Accept: application/vnd.github+json\r\n", (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(req, nullptr)) {
        DWORD sz = sizeof(status);
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status,
                            &sz, WINHTTP_NO_HEADER_INDEX);
        DWORD avail = 0;
        while (body.size() < 200000 && WinHttpQueryDataAvailable(req, &avail) && avail) {
            string chunk(avail, '\0');
            DWORD got = 0;
            if (!WinHttpReadData(req, &chunk[0], avail, &got) || !got) break;
            body.append(chunk, 0, got);
        }
    }
    if (req) WinHttpCloseHandle(req);
    if (con) WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return body;
}

static string jsonString(const string& j, const char* key) {
    string k = string("\"") + key + "\"";
    size_t p = j.find(k);
    if (p == string::npos) return "";
    p = j.find(':', p + k.size());
    if (p == string::npos) return "";
    p = j.find('"', p);
    if (p == string::npos) return "";
    size_t e = j.find('"', p + 1);
    return e == string::npos ? "" : j.substr(p + 1, e - p - 1);
}

static DWORD WINAPI updateWorker(LPVOID manual) {
    DWORD st = 0;
    string body = httpsGet(L"api.github.com", L"/repos/666mille/MilleIpSwitch/releases/latest", st);
    auto* r = new UpdateResult{0, L"", manual != nullptr};
    if (st == 200) {
        string tag = jsonString(body, "tag_name");
        bool valid = !tag.empty() && tag.size() < 32;
        for (char c : tag)
            if (!isalnum((unsigned char)c) && c != '.' && c != '-' && c != '_') valid = false;
        if (valid) {
            r->tag = wstring(tag.begin(), tag.end());
            r->status = isNewerVersion(r->tag) ? 2 : 1;
        }
    } else if (st == 404) {
        r->status = 3;
    }
    PostMessageW(g_hwnd, WM_UPDATEDONE, 0, (LPARAM)r);
    return 0;
}

static void startUpdateCheck(bool manual) {
    HANDLE h = CreateThread(nullptr, 0, updateWorker, manual ? (LPVOID)1 : nullptr, 0, nullptr);
    if (h) CloseHandle(h);
}

static void openUpdatePage() {
    if (g_updateUrl.rfind(L"https://github.com/666mille/MilleIpSwitch/", 0) != 0) return;
    wstring arg = L"\"" + g_updateUrl + L"\"";
    ShellExecuteW(nullptr, L"open", L"explorer.exe", arg.c_str(), nullptr, SW_SHOWNORMAL);
}

static void updateFinished(UpdateResult* r) {
    if (r->status != 0) regSetDword(L"LastUpdateCheck", todayDays());
    if (r->status == 2) {
        g_updateTag = r->tag;
        g_updateUrl = L"https://github.com/666mille/MilleIpSwitch/releases/tag/" + r->tag;
        trayBalloon(TR(L"Update verfügbar", L"Update available"),
                    fmt(TR(L"Version %ls ist verfügbar – klicken, um die Download-Seite zu öffnen.",
                           L"Version %ls is available – click to open the download page."),
                        r->tag.c_str()),
                    false);
        g_balloonUpdate = true;
    } else if (r->manual) {
        if (r->status == 1) g_updateTag.clear();
        wstring msg = r->status == 1   ? fmt(TR(L"Du hast die neueste Version (%ls).", L"You have the latest version (%ls)."), NS_VERSION)
                      : r->status == 3 ? wstring(TR(L"Noch keine veröffentlichte Version gefunden.", L"No published release found yet."))
                                       : wstring(TR(L"Update-Prüfung fehlgeschlagen – GitHub nicht erreichbar.",
                                                    L"Update check failed – could not reach GitHub."));
        trayBalloon(L"MilleIpSwitch", msg, r->status == 0);
    }
    delete r;
}

static bool reloadProfiles(bool showError) {
    vector<Profile> ps;
    wstring err;
    if (loadProfiles(ps, err)) {
        g_profiles = ps;
        return true;
    }
    if (showError) MessageBoxW(g_hDlg ? g_hDlg : g_hwnd, err.c_str(), TR(L"MilleIpSwitch – Profildatei", L"MilleIpSwitch – profile file"), MB_ICONERROR);
    return false;
}

static DWORD WINAPI applyWorker(LPVOID arg) {
    Profile* p = (Profile*)arg;
    wstring msg;
    bool ok = applyProfile(*p, msg);
    delete p;
    InterlockedExchange(&g_busy, 0);
    PostMessageW(g_hwnd, WM_APPLYDONE, 0, (LPARAM) new std::pair<bool, wstring>(ok, msg));
    return 0;
}

static bool startApply(const Profile& p, int kind = 0) {
    if (InterlockedCompareExchange(&g_busy, 1, 0) != 0) return false;
    Profile* job = new Profile(p);
    HANDLE h = CreateThread(nullptr, 0, applyWorker, job, 0, nullptr);
    if (h) {
        CloseHandle(h);
        g_applyKind = kind;
        if (kind == 0) g_autoHold[lower(p.adapter)] = GetTickCount() + 600000;
        return true;
    }
    delete job;
    InterlockedExchange(&g_busy, 0);
    return false;
}

static bool openAdapterProperties(const wstring& name, HWND owner) {
    bool ok = false;
    IShellFolder* desk = nullptr;
    PIDLIST_ABSOLUTE folder = nullptr;
    if (SUCCEEDED(SHGetDesktopFolder(&desk)) &&
        SUCCEEDED(SHParseDisplayName(L"::{7007ACC7-3202-11D1-AAD2-00805FC1270E}", nullptr, &folder, 0, nullptr))) {
        IShellFolder* nc = nullptr;
        if (SUCCEEDED(desk->BindToObject(folder, nullptr, IID_IShellFolder, (void**)&nc)) && nc) {
            IEnumIDList* en = nullptr;
            if (nc->EnumObjects(owner, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &en) == S_OK && en) {
                PITEMID_CHILD child = nullptr;
                while (!ok && en->Next(1, &child, nullptr) == S_OK) {
                    STRRET sr;
                    wchar_t buf[512];
                    if (SUCCEEDED(nc->GetDisplayNameOf(child, SHGDN_NORMAL, &sr)) &&
                        SUCCEEDED(StrRetToBufW(&sr, child, buf, 512)) && ieq(buf, name)) {
                        PIDLIST_ABSOLUTE full = ILCombine(folder, child);
                        SHELLEXECUTEINFOW se{};
                        se.cbSize = sizeof(se);
                        se.fMask = SEE_MASK_INVOKEIDLIST;
                        se.lpVerb = L"properties";
                        se.lpIDList = full;
                        se.hwnd = owner;
                        se.nShow = SW_SHOWNORMAL;
                        ok = ShellExecuteExW(&se) != FALSE;
                        ILFree(full);
                    }
                    CoTaskMemFree(child);
                }
                en->Release();
            }
            nc->Release();
        }
    }
    if (folder) CoTaskMemFree(folder);
    if (desk) desk->Release();
    if (!ok) ShellExecuteW(owner, L"open", L"control.exe", L"ncpa.cpl", nullptr, SW_SHOWNORMAL);
    return ok;
}

// Status window
static vector<Adapter> g_stAds;
static int g_stScroll = 0, g_stContentH = 0;
static bool g_stShowDown = true, g_stShowDisabled = true, g_stSortMetric = false;
static RECT g_rcRefresh, g_rcToggle, g_rcToggle2, g_rcToggle3, g_rcFlush;
static wstring g_stToast;
static bool g_stToastErr = false;
static string g_renewBusy;
struct GearHit {
    RECT r;
    wstring name;
    int action;
    string guid;
    bool enabled;
};
static string g_toggleBusy;
#define WM_TOGGLEDONE (WM_APP + 20)
#define WM_RENEWDONE (WM_APP + 21)
static vector<GearHit> g_gears;
static int g_gearHover = -1;
static bool g_stTracking = false;
static UINT g_stDpi = 96;
static HICON g_stLogo;
static HFONT g_fTitle, g_fBold, g_fText, g_fSmall, g_fGlyph, g_fGlyphBig, g_fGlyphPower, g_fGlyphSmall, g_fBoldSmall;

static int stHeaderH() { return S(64, g_stDpi); }

static void stFonts(UINT dpi) {
    for (HFONT* f : {&g_fTitle, &g_fBold, &g_fText, &g_fSmall, &g_fGlyph, &g_fGlyphBig, &g_fGlyphPower, &g_fGlyphSmall, &g_fBoldSmall}) delFont(*f);
    g_stDpi = dpi;
    g_fTitle = mkFont(L"Segoe UI", 150, 600, dpi);
    g_fBold = mkFont(L"Segoe UI", 100, 600, dpi);
    g_fText = mkFont(L"Segoe UI", 95, 400, dpi);
    g_fSmall = mkFont(L"Segoe UI", 85, 400, dpi);
    g_fGlyph = mkFont(glyphFace(), 110, 400, dpi);
    g_fGlyphBig = mkFont(glyphFace(), 135, 400, dpi);
    g_fGlyphPower = mkFont(glyphFace(), 95, 700, dpi);
    g_fGlyphSmall = mkFont(glyphFace(), 95, 400, dpi);
    if (g_stLogo) DestroyIcon(g_stLogo);
    g_stLogo = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_DETAIL), IMAGE_ICON, S(36, dpi), S(36, dpi), 0);
    g_fBoldSmall = mkFont(L"Segoe UI", 85, 600, dpi);
}

static wstring metricText(const Adapter& a) {
    return fmt(L"%lu", (unsigned long)a.metric) + L" (" + (a.autoMetric ? TR(L"automatisch", L"automatic") : TR(L"manuell", L"manual")) + L")";
}

static int adapterRank(const Adapter& a) { return a.up ? 0 : a.enabled ? 1 : 2; }

static vector<std::pair<wstring, wstring>> stRows(const Adapter* a) {
    vector<std::pair<wstring, wstring>> rows;
    wstring v;
    for (size_t i = 0; i < a->ips.size(); ++i) v += (i ? L", " : L"") + cidrStr(a->ips[i]);
    if (!v.empty()) rows.push_back({L"IPv4", v});
    v.clear();
    for (size_t i = 0; i < a->gws.size(); ++i) v += (i ? L", " : L"") + ipStr(a->gws[i]);
    if (!v.empty()) rows.push_back({L"Gateway", v});
    if (a->dhcp && a->hasDhcpSrv) rows.push_back({TR(L"DHCP-Server", L"DHCP server"), ipStr(a->dhcpSrv)});
    v.clear();
    for (auto& x : a->ips6)
        if (!x.linkLocal && !x.temp) v += (v.empty() ? L"" : L", ") + cidr6Str(x);
    if (!v.empty()) rows.push_back({L"IPv6", v});
    if (!a->gws6.empty()) rows.push_back({TR(L"IPv6-Gateway", L"IPv6 gateway"), ip6Str(a->gws6[0])});
    v.clear();
    for (size_t i = 0; i < a->dns.size(); ++i) v += (i ? L", " : L"") + ipStr(a->dns[i]);
    for (auto& d6 : a->dns6) v += (v.empty() ? L"" : L", ") + ip6Str(d6);
    if (!v.empty()) rows.push_back({L"DNS", v});
    if (a->hasMetric) rows.push_back({TR(L"Metrik", L"Metric"), metricText(*a)});
    if (a->up && a->speed && a->speed != ~0ULL) rows.push_back({TR(L"Verbindung", L"Link speed"), speedStr(a->speed)});
    if (!a->mac.empty()) rows.push_back({L"MAC", a->mac});
    return rows;
}

static int stBoxCount(const Adapter* a) { return 2 + (a->dhcp && a->enabled ? 1 : 0); }

static int stCardHeight(const Adapter* a, size_t nRows) {
    UINT d = g_stDpi;
    int nb = stBoxCount(a);
    int rowsH = S(50, d) + (int)nRows * S(18, d) + S(8, d);
    int colH = S(34, d) + nb * S(42, d) + (nb - 1) * S(4, d) + S(10, d);
    return std::max(rowsH, colH);
}

static void stPaint(HDC dc, RECT rc) {
    UINT dpi = g_stDpi;
    auto s = [dpi](int v) { return S(v, dpi); };
    fillRect(dc, rc, C::Bg);
    int hdrH = stHeaderH();
    int y = hdrH + s(10) - g_stScroll;
    int x0 = s(12), x1 = rc.right - s(12);
    int nUp = 0, shown = 0;
    for (auto& a : g_stAds) nUp += a.up;
    g_gears.clear();

    for (int k = g_stSortMetric ? -1 : 0; k < (g_stSortMetric ? 0 : K_COUNT); ++k) {
        vector<const Adapter*> list;
        for (auto& a : g_stAds)
            if ((k < 0 || a.kind == k) && (a.up || (a.enabled ? g_stShowDown : g_stShowDisabled))) list.push_back(&a);
        if (list.empty()) continue;
        if (k < 0)
            std::stable_sort(list.begin(), list.end(), [](const Adapter* a, const Adapter* b) {
                if (adapterRank(*a) != adapterRank(*b)) return adapterRank(*a) < adapterRank(*b);
                ULONG ma = a->hasMetric ? a->metric : ~0UL, mb = b->hasMetric ? b->metric : ~0UL;
                return ma < mb;
            });
        else
            std::stable_sort(list.begin(), list.end(),
                             [](const Adapter* a, const Adapter* b) { return adapterRank(*a) < adapterRank(*b); });
        shown += (int)list.size();

        vector<vector<std::pair<wstring, wstring>>> rowsOf;
        vector<int> hs;
        int secH = s(32) + s(8);
        for (auto* a : list) {
            rowsOf.push_back(stRows(a));
            hs.push_back(stCardHeight(a, rowsOf.back().size()));
            secH += hs.back() + s(6);
        }
        secH += s(2);

        RECT sec{x0, y, x1, y + secH};
        if (sec.bottom > hdrH && sec.top < rc.bottom) {
            fillRound(dc, sec, s(12), RGB(247, 250, 254), RGB(196, 220, 243));
            RECT hb{x0, y, x1, y + s(32)};
            fillRound(dc, hb, s(12), C::AccentBg, RGB(196, 220, 243));
            fillRect(dc, RECT{hb.left + 1, hb.bottom - s(8), hb.right - 1, hb.bottom}, C::AccentBg);
            fillRect(dc, RECT{hb.left, hb.bottom - 1, hb.right, hb.bottom}, RGB(196, 220, 243));
            drawGlyph(dc, k < 0 ? G::Sort : kindGlyph(k), RECT{x0 + s(8), y, x0 + s(32), y + s(32)}, C::Accent, g_fGlyph);
            drawText(dc, k < 0 ? TR(L"Nach Metrik sortiert", L"Sorted by metric") : kindName(k),
                     RECT{x0 + s(38), y, x1 - s(60), y + s(32)}, C::Accent, g_fBold,
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            wstring cnt = fmt(L"%u", (unsigned)list.size());
            int cw = std::max(pillWidth(dc, cnt, g_fSmall, dpi), s(26));
            drawPill(dc, cnt, RECT{x1 - s(10) - cw, y + s(7), x1 - s(10), y + s(25)}, C::Card, C::Accent, g_fSmall);
        }

        int cy0 = y + s(32) + s(8);
        int rank = 0;
        for (size_t ci = 0; ci < list.size(); ++ci) {
            const Adapter* a = list[ci];
            auto& rows = rowsOf[ci];
            int cardH = hs[ci];
            int myRank = (k < 0 && a->up) ? ++rank : 0;
            RECT card{x0 + s(8), cy0, x1 - s(8), cy0 + cardH};
            if (card.bottom > hdrH && card.top < rc.bottom) {
                fillRound(dc, card, s(10), C::Card, C::Border);

                int icx = card.left + s(30), icy = card.top + s(28);
                fillCircle(dc, icx, icy, s(18), a->up ? C::AccentBg : C::GreyBg);
                drawGlyph(dc, kindGlyph(a->kind, a->bluetooth), RECT{icx - s(18), icy - s(18), icx + s(18), icy + s(18)},
                          a->up ? C::Accent : C::Faint, g_fGlyphBig);
                fillCircle(dc, icx + s(13), icy + s(13), s(6), C::Card);
                fillCircle(dc, icx + s(13), icy + s(13), s(4), a->up ? C::Green : !a->enabled ? C::Red : C::Faint);

                int colR = card.right - s(10), colL = colR - s(78);
                int ph = s(18), pt = card.top + s(10);
                wstring p1 = !a->enabled ? TR(L"Deaktiviert", L"Disabled")
                             : a->up     ? TR(L"Verbunden", L"Connected")
                                         : TR(L"Getrennt", L"Disconnected");
                wstring p2 = a->dhcp ? L"DHCP" : TR(L"Statisch", L"Static");
                int w1 = pillWidth(dc, p1, g_fSmall, dpi), w2 = pillWidth(dc, p2, g_fSmall, dpi);
                int p2l = colL + (colR - colL - w2) / 2;
                drawPill(dc, p2, RECT{p2l, pt, p2l + w2, pt + ph}, a->dhcp ? C::Accent : C::AccentBg,
                         a->dhcp ? C::AccentBg : C::Accent, g_fSmall);
                int pr = p2l - s(6);
                drawPill(dc, p1, RECT{pr - w1, pt, pr, pt + ph}, a->up ? C::Green : !a->enabled ? C::Red : C::Sub,
                         a->up ? C::GreenBg : !a->enabled ? C::RedBg : C::GreyBg, g_fSmall);

                int nb = stBoxCount(a);
                for (int b = 0; b < nb; ++b) {
                    RECT bx{colL, card.top + s(34) + b * s(46), colR, card.top + s(34) + b * s(46) + s(42)};
                    if (bx.top < hdrH) continue;
                    bool hov = g_gearHover == (int)g_gears.size();
                    fillRound(dc, bx, s(8), hov ? C::Hover : C::Bg, hov ? RGB(205, 205, 205) : C::Border);
                    RECT lr{bx.left + s(2), bx.bottom - s(17), bx.right - s(2), bx.bottom - s(3)};
                    if (b == 0) {
                        bool busy = !g_toggleBusy.empty() && g_toggleBusy == a->guidStr;
                        bool on = a->enabled;
                        COLORREF bc = busy ? C::Faint : on ? (hov ? RGB(46, 160, 67) : RGB(67, 181, 85))
                                                           : (hov ? RGB(205, 45, 45) : RGB(232, 72, 72));
                        int pcx = (bx.left + bx.right) / 2, pcy = bx.top + s(13), pr2 = s(10);
                        fillCircle(dc, pcx, pcy + s(1), pr2, RGB(215, 215, 215));
                        fillCircle(dc, pcx, pcy, pr2, bc);
                        drawGlyph(dc, G::Power, RECT{pcx - pr2, pcy - pr2, pcx + pr2 + 1, pcy + pr2 + 1}, C::Card,
                                  g_fGlyphPower);
                        drawText(dc, busy ? wstring(L"…") : wstring(on ? TR(L"Aktiviert", L"Enabled") : TR(L"Deaktiviert", L"Disabled")),
                                 lr, busy ? C::Sub : on ? RGB(30, 130, 50) : C::Red, g_fSmall,
                                 DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                    } else if (b == 1) {
                        drawGlyph(dc, G::Settings, RECT{bx.left, bx.top + s(2), bx.right, bx.top + s(24)},
                                  hov ? C::Accent : C::Sub, g_fGlyph);
                        drawText(dc, TR(L"Einstellungen", L"Settings"), lr, C::Text, g_fSmall,
                                 DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                    } else {
                        bool busy = !g_renewBusy.empty() && g_renewBusy == a->guidStr;
                        drawGlyph(dc, G::Refresh, RECT{bx.left, bx.top + s(2), bx.right, bx.top + s(24)},
                                  busy ? C::Faint : C::Accent, g_fGlyph);
                        drawText(dc, busy ? wstring(L"…") : wstring(TR(L"DHCP erneuern", L"Renew DHCP")), lr, C::Text,
                                 g_fSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                    }
                    g_gears.push_back({bx, a->friendly, b == 0 ? 1 : b == 1 ? 0 : 2, a->guidStr, a->enabled});
                }

                int tx = card.left + s(58);
                int nameX = tx;
                if (myRank) {
                    wstring rt = fmt(L"#%d", myRank);
                    int rw = std::max(pillWidth(dc, rt, g_fSmall, dpi), s(30));
                    drawPill(dc, rt, RECT{tx, card.top + s(10), tx + rw, card.top + s(28)},
                             myRank == 1 ? C::Card : C::Accent, myRank == 1 ? C::Green : C::AccentBg, g_fSmall);
                    nameX = tx + rw + s(6);
                }
                drawText(dc, a->friendly, RECT{nameX, card.top + s(8), pr - w1 - s(8), card.top + s(28)},
                         a->enabled ? C::Text : C::Sub, g_fBold, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                drawText(dc, a->desc, RECT{tx, card.top + s(28), colL - s(10), card.top + s(46)}, C::Sub, g_fSmall,
                         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                int ry = card.top + s(50);
                for (auto& r : rows) {
                    drawText(dc, r.first, RECT{tx, ry, tx + s(82), ry + s(18)}, C::Sub, g_fSmall,
                             DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                    drawText(dc, r.second, RECT{tx + s(84), ry, colL - s(10), ry + s(18)}, a->up ? C::Text : C::Faint,
                             g_fText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                    ry += s(18);
                }
            }
            cy0 += cardH + s(6);
        }
        y += secH + s(12);
    }
    if (!shown)
        drawText(dc, TR(L"Keine verbundenen Adapter", L"No connected adapters"),
                 RECT{x0, hdrH + s(40), x1, hdrH + s(80)}, C::Sub, g_fText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    g_stContentH = y + g_stScroll - hdrH;

    RECT hr{0, 0, rc.right, hdrH};
    fillRect(dc, hr, C::Bg);
    if (g_stLogo) DrawIconEx(dc, s(14), (hdrH - s(36)) / 2, g_stLogo, s(36), s(36), 0, nullptr, DI_NORMAL);
    drawText(dc, TR(L"Netzwerkstatus", L"Network status"), RECT{s(60), s(8), rc.right, s(36)}, C::Text, g_fTitle,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (!g_stToast.empty())
        drawText(dc, g_stToast, RECT{s(60), s(36), rc.right, s(54)}, g_stToastErr ? C::Red : C::Green, g_fBoldSmall,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    else
        drawText(dc, fmt(TR(L"%d von %d Adaptern verbunden", L"%d of %d adapters connected"), nUp, (int)g_stAds.size()),
                 RECT{s(60), s(36), rc.right, s(54)}, C::Sub, g_fSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    g_rcRefresh = RECT{rc.right - s(14) - s(34), s(15), rc.right - s(14), s(49)};
    fillRound(dc, g_rcRefresh, s(8), C::Card, C::Border);
    drawGlyph(dc, G::Refresh, g_rcRefresh, C::Accent, g_fGlyph);

    wstring ft = TR(L"DNS-Cache leeren", L"Flush DNS cache");
    int fw = textWidth(dc, ft, g_fSmall) + s(38);
    g_rcFlush = RECT{g_rcRefresh.left - s(8) - fw, s(15), g_rcRefresh.left - s(8), s(49)};
    fillRound(dc, g_rcFlush, s(8), C::Card, C::Border);
    drawGlyph(dc, G::Delete, RECT{g_rcFlush.left + s(6), g_rcFlush.top, g_rcFlush.left + s(28), g_rcFlush.bottom},
              C::Red, g_fGlyph);
    drawText(dc, ft, RECT{g_rcFlush.left + s(30), g_rcFlush.top, g_rcFlush.right, g_rcFlush.bottom}, C::Text, g_fSmall,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    wstring tt[3] = {TR(L"Getrennte anzeigen", L"Show disconnected"), TR(L"Deaktivierte anzeigen", L"Show disabled"),
                     TR(L"Nach Metrik sortieren", L"Sort by metric")};
    int tw = std::max({textWidth(dc, tt[0], g_fSmall), textWidth(dc, tt[1], g_fSmall), textWidth(dc, tt[2], g_fSmall)}) + s(30);
    int tl = g_rcFlush.left - s(12) - tw;
    RECT* trc[3] = {&g_rcToggle, &g_rcToggle2, &g_rcToggle3};
    bool ton[3] = {g_stShowDown, g_stShowDisabled, g_stSortMetric};
    for (int i = 0; i < 3; ++i) {
        RECT r{tl, s(3) + i * s(20), tl + tw, s(3) + i * s(20) + s(19)};
        *trc[i] = r;
        if (i == 2 && ton[i]) fillRound(dc, r, s(6), C::AccentBg, C::AccentBg);
        drawGlyph(dc, i == 2 ? G::Sort : ton[i] ? G::CheckboxOn : G::CheckboxOff, RECT{r.left + s(2), r.top, r.left + s(20), r.bottom},
                  ton[i] || i == 2 ? C::Accent : C::Sub, g_fGlyphSmall);
        drawText(dc, tt[i], RECT{r.left + s(24), r.top, r.right, r.bottom}, i == 2 && ton[i] ? C::Accent : C::Text, g_fSmall,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    fillRect(dc, RECT{0, hdrH - 1, rc.right, hdrH}, C::Border);
}

static void stUpdateScroll(HWND h) {
    RECT rc;
    GetClientRect(h, &rc);
    int view = std::max(1, (int)rc.bottom - stHeaderH());
    int mx = std::max(0, g_stContentH - view);
    if (g_stScroll > mx) {
        g_stScroll = mx;
        InvalidateRect(h, nullptr, FALSE);
    }
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
    si.nMin = 0;
    si.nMax = std::max(0, g_stContentH - 1);
    si.nPage = view;
    si.nPos = g_stScroll;
    SetScrollInfo(h, SB_VERT, &si, TRUE);
}

static void stScrollTo(HWND h, int pos) {
    RECT rc;
    GetClientRect(h, &rc);
    int mx = std::max(0, g_stContentH - ((int)rc.bottom - stHeaderH()));
    pos = std::max(0, std::min(pos, mx));
    if (pos != g_stScroll) {
        g_stScroll = pos;
        InvalidateRect(h, nullptr, FALSE);
    }
    stUpdateScroll(h);
}

static LRESULT CALLBACK stProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_CREATE:
        stFonts(dpiOf(h));
        g_stAds = getAdapters(true);
        SetTimer(h, 1, 3000, nullptr);
        return 0;
    case WM_TIMER:
        if (wp == 2) {
            KillTimer(h, 2);
            g_stToast.clear();
            InvalidateRect(h, nullptr, FALSE);
        } else if (!IsIconic(h)) {
            g_stAds = getAdapters(true);
            InvalidateRect(h, nullptr, FALSE);
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bm = CreateCompatibleBitmap(dc, std::max(1L, rc.right), std::max(1L, rc.bottom));
        HGDIOBJ ob = SelectObject(mem, bm);
        stPaint(mem, rc);
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, ob);
        DeleteObject(bm);
        DeleteDC(mem);
        EndPaint(h, &ps);
        stUpdateScroll(h);
        return 0;
    }
    case WM_SIZE:
        stUpdateScroll(h);
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_MOUSEWHEEL:
        stScrollTo(h, g_stScroll - GET_WHEEL_DELTA_WPARAM(wp) * S(60, g_stDpi) / WHEEL_DELTA);
        return 0;
    case WM_VSCROLL: {
        RECT rc;
        GetClientRect(h, &rc);
        int page = rc.bottom - stHeaderH(), pos = g_stScroll;
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_TRACKPOS;
        GetScrollInfo(h, SB_VERT, &si);
        switch (LOWORD(wp)) {
        case SB_LINEUP: pos -= S(40, g_stDpi); break;
        case SB_LINEDOWN: pos += S(40, g_stDpi); break;
        case SB_PAGEUP: pos -= page; break;
        case SB_PAGEDOWN: pos += page; break;
        case SB_TOP: pos = 0; break;
        case SB_BOTTOM: pos = g_stContentH; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: pos = si.nTrackPos; break;
        }
        stScrollTo(h, pos);
        return 0;
    }
    case WM_MOUSEMOVE: {
        POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        int hv = -1;
        for (size_t i = 0; i < g_gears.size(); ++i)
            if (PtInRect(&g_gears[i].r, pt)) hv = (int)i;
        if (hv != g_gearHover) {
            g_gearHover = hv;
            InvalidateRect(h, nullptr, FALSE);
        }
        if (!g_stTracking) {
            TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, h, 0};
            g_stTracking = TrackMouseEvent(&tme) != FALSE;
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        g_stTracking = false;
        if (g_gearHover >= 0) {
            g_gearHover = -1;
            InvalidateRect(h, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONUP: {
        POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        for (auto& g : g_gears)
            if (PtInRect(&g.r, pt)) {
                GearHit hit = g;
                if (hit.action == 0) {
                    openAdapterProperties(hit.name, h);
                } else if (hit.action == 2) {
                    if (g_renewBusy.empty()) {
                        g_renewBusy = hit.guid;
                        InvalidateRect(h, nullptr, FALSE);
                        struct RJob {
                            HWND h;
                            string guid;
                        };
                        RJob* j = new RJob{h, hit.guid};
                        HANDLE th = CreateThread(
                            nullptr, 0,
                            [](LPVOID p) -> DWORD {
                                RJob* j = (RJob*)p;
                                bool ok = dhcpRenew(j->guid);
                                PostMessageW(j->h, WM_RENEWDONE, ok, 0);
                                delete j;
                                return 0;
                            },
                            j, 0, nullptr);
                        if (th) CloseHandle(th);
                        else {
                            delete j;
                            g_renewBusy.clear();
                        }
                    }
                } else if (g_toggleBusy.empty()) {
                    bool enable = !hit.enabled;
                    if (!enable &&
                        MessageBoxW(h,
                                    (fmt(TR(L"„%ls“ deaktivieren?", L"Disable “%ls”?"), hit.name.c_str()) + L"\n\n" +
                                     TR(L"Alle Verbindungen über diesen Adapter werden getrennt.",
                                        L"All connections over this adapter will be dropped."))
                                        .c_str(),
                                    L"MilleIpSwitch", MB_YESNO | MB_ICONQUESTION) != IDYES)
                        return 0;
                    g_toggleBusy = hit.guid;
                    InvalidateRect(h, nullptr, FALSE);
                    struct Job {
                        HWND h;
                        string guid;
                        bool enable;
                    };
                    Job* j = new Job{h, hit.guid, enable};
                    HANDLE th = CreateThread(
                        nullptr, 0,
                        [](LPVOID p) -> DWORD {
                            Job* j = (Job*)p;
                            bool ok = setAdapterEnabled(j->guid, j->enable);
                            PostMessageW(j->h, WM_TOGGLEDONE, ok, 0);
                            delete j;
                            return 0;
                        },
                        j, 0, nullptr);
                    if (th) CloseHandle(th);
                    else {
                        delete j;
                        g_toggleBusy.clear();
                    }
                }
                return 0;
            }
        if (PtInRect(&g_rcRefresh, pt)) {
            g_stAds = getAdapters(true);
            InvalidateRect(h, nullptr, FALSE);
        } else if (PtInRect(&g_rcFlush, pt)) {
            if (MessageBoxW(h,
                            TR(L"Der DNS-Cache von Windows wird geleert.\nGespeicherte Namensauflösungen gehen verloren und "
                               L"werden beim nächsten Zugriff neu abgefragt.\n\nFortfahren?",
                               L"The Windows DNS cache will be flushed.\nCached name resolutions are discarded and will be "
                               L"looked up again on next access.\n\nContinue?"),
                            TR(L"DNS-Cache leeren", L"Flush DNS cache"), MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2) == IDOK) {
                bool ok = flushDns();
                g_stToast = ok ? TR(L"✓ DNS-Cache geleert", L"✓ DNS cache flushed")
                               : TR(L"DNS-Cache konnte nicht geleert werden", L"Could not flush the DNS cache");
                g_stToastErr = !ok;
                SetTimer(h, 2, 3500, nullptr);
                InvalidateRect(h, nullptr, FALSE);
            }
        } else if (PtInRect(&g_rcToggle3, pt)) {
            g_stSortMetric = !g_stSortMetric;
            g_stScroll = 0;
            InvalidateRect(h, nullptr, FALSE);
        } else if (PtInRect(&g_rcToggle, pt) || PtInRect(&g_rcToggle2, pt)) {
            bool& f = PtInRect(&g_rcToggle, pt) ? g_stShowDown : g_stShowDisabled;
            f = !f;
            g_stScroll = 0;
            InvalidateRect(h, nullptr, FALSE);
        }
        return 0;
    }
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(h, &pt);
            bool hand = PtInRect(&g_rcRefresh, pt) || PtInRect(&g_rcToggle, pt) || PtInRect(&g_rcToggle2, pt) || PtInRect(&g_rcToggle3, pt) || PtInRect(&g_rcFlush, pt);
            for (auto& g : g_gears)
                if (PtInRect(&g.r, pt)) hand = true;
            if (hand) {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
        }
        break;
    case WM_RENEWDONE:
        g_renewBusy.clear();
        g_stAds = getAdapters(true);
        g_stToast = wp ? TR(L"✓ DHCP-Adresse erneuert", L"✓ DHCP lease renewed")
                       : TR(L"DHCP-Erneuerung fehlgeschlagen", L"DHCP renewal failed");
        g_stToastErr = !wp;
        SetTimer(h, 2, 3500, nullptr);
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_TOGGLEDONE:
        g_toggleBusy.clear();
        g_disabledStamp = 0;
        g_stAds = getAdapters(true);
        InvalidateRect(h, nullptr, FALSE);
        if (!wp)
            MessageBoxW(h, TR(L"Der Adapter konnte nicht umgeschaltet werden.", L"The adapter could not be switched."),
                        L"MilleIpSwitch", MB_ICONERROR);
        return 0;
    case WM_GETMINMAXINFO: {
        auto* mm = (MINMAXINFO*)lp;
        mm->ptMinTrackSize.x = S(560, g_stDpi);
        mm->ptMinTrackSize.y = S(320, g_stDpi);
        return 0;
    }
    case WM_DPICHANGED: {
        stFonts(HIWORD(wp));
        RECT* r = (RECT*)lp;
        SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(h, 1);
        g_hStatus = nullptr;
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

static void openStatus() {
    if (g_hStatus) {
        ShowWindow(g_hStatus, SW_RESTORE);
        SetForegroundWindow(g_hStatus);
        return;
    }
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = stProc;
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hIcon = g_iconWinBig;
        wc.hIconSm = g_iconWinSm;
        wc.lpszClassName = L"MilleIpSwitchStatusWnd";
        RegisterClassExW(&wc);
        registered = true;
    }
    UINT dpi = dpiOf(g_hDlg ? g_hDlg : g_hwnd);
    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int w = S(600, dpi), hgt = std::min(S(780, dpi), (int)(wa.bottom - wa.top) - S(40, dpi));
    int x = wa.left + (wa.right - wa.left - w) / 2, y = wa.top + (wa.bottom - wa.top - hgt) / 2;
    g_stScroll = 0;
    g_hStatus = CreateWindowExW(0, L"MilleIpSwitchStatusWnd", TR(L"MilleIpSwitch – Netzwerkstatus", L"MilleIpSwitch – Network status"), WS_OVERLAPPEDWINDOW | WS_VSCROLL,
                                x, y, w, hgt, nullptr, nullptr, g_hInst, nullptr);
    if (g_hStatus) {
        ShowWindow(g_hStatus, SW_SHOW);
        SetForegroundWindow(g_hStatus);
    }
}

// Profile editor
static vector<Profile> g_ed;
static vector<Adapter> g_edAds;
static int g_edSel = -1;
static bool g_edDirty = false, g_edLoading = false, g_edListChanged = false;
static std::set<int> g_changed;
static vector<int> g_flash;
static HFONT g_eGlyph, g_eSmall, g_eBold, g_eFbGlyph, g_eTitle;
static HICON g_eLogo;
static int g_eBanner = 0;

static BOOL CALLBACK shiftChild(HWND c, LPARAM dy) {
    if (GetParent(c) != GetAncestor(c, GA_ROOT)) return TRUE;
    RECT r;
    GetWindowRect(c, &r);
    MapWindowPoints(nullptr, GetParent(c), (POINT*)&r, 2);
    SetWindowPos(c, nullptr, r.left, r.top + (int)dy, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    return TRUE;
}
enum { FB_NONE, FB_OK, FB_ERR, FB_INFO };
static int g_fbKind = FB_NONE;
static wstring g_fbText;

static const int kRightIds[] = {IDC_NAME,     IDC_ADAPTER,  IDC_READCUR,  IDC_DHCP,      IDC_STATICIP,
                                IDC_IP,       IDC_MASK,     IDC_GW,       IDC_EXTRA,     IDC_DNSAUTO,
                                IDC_DNSMAN,   IDC_DNS1,     IDC_DNS2,     IDC_LBL_NAME,  IDC_LBL_ADAPTER,
                                IDC_LBL_IP,   IDC_LBL_MASK, IDC_LBL_GW,   IDC_LBL_EXTRA, IDC_LBL_DNS1,
                                IDC_LBL_DNS2, IDC_GRP_IP,   IDC_GRP_DNS,  IDC_DUP,       IDC_DEL,
                                IDC_UP,       IDC_DOWN,     IDC_ACTIVATE, IDC_IPV6BTN,   IDC_AUTOBTN};

static wstring dlgText(HWND d, int id) {
    HWND h = GetDlgItem(d, id);
    int n = GetWindowTextLengthW(h);
    wstring s(n + 1, L'\0');
    GetWindowTextW(h, &s[0], n + 1);
    s.resize(n);
    return s;
}

static int ipGet(HWND d, int id, IN_ADDR& a) {
    HWND h = GetDlgItem(d, id);
    if (SendMessageW(h, IPM_ISBLANK, 0, 0)) return 0;
    DWORD v = 0;
    LRESULT filled = SendMessageW(h, IPM_GETADDRESS, 0, (LPARAM)&v);
    if (filled != 4) return -1;
    a.s_addr = htonl(v);
    return 1;
}

static void ipSet(HWND d, int id, const IN_ADDR* a) {
    HWND h = GetDlgItem(d, id);
    if (!a)
        SendMessageW(h, IPM_CLEARADDRESS, 0, 0);
    else
        SendMessageW(h, IPM_SETADDRESS, 0, (LPARAM)ntohl(a->s_addr));
}

static void edFeedback(HWND d, int kind, const wstring& text) {
    g_fbKind = kind;
    g_fbText = text;
    InvalidateRect(GetDlgItem(d, IDC_FEEDBACK), nullptr, TRUE);
}

static void edSetDirty(HWND d, bool dirty) {
    g_edDirty = dirty;
    wstring t = wstring(L"MilleIpSwitch ") + NS_VERSION + TR(L" – Profile", L" – Profiles");
    if (dirty) t += TR(L"  (nicht gespeichert)", L"  (not saved)");
    SetWindowTextW(d, t.c_str());
}

static void edChanged(HWND d, int id) {
    if (g_edLoading) return;
    g_changed.insert(id);
    edSetDirty(d, true);
}

static void edUpdateEnable(HWND d) {
    bool any = g_edSel >= 0;
    for (int id : kRightIds) EnableWindow(GetDlgItem(d, id), any);
    if (!any) return;
    bool stat = IsDlgButtonChecked(d, IDC_STATICIP) == BST_CHECKED;
    for (int id : {IDC_IP, IDC_MASK, IDC_GW, IDC_EXTRA, IDC_LBL_IP, IDC_LBL_MASK, IDC_LBL_GW, IDC_LBL_EXTRA})
        EnableWindow(GetDlgItem(d, id), stat);
    if (stat) {
        CheckRadioButton(d, IDC_DNSAUTO, IDC_DNSMAN, IDC_DNSMAN);
        EnableWindow(GetDlgItem(d, IDC_DNSAUTO), FALSE);
    }
    bool man = IsDlgButtonChecked(d, IDC_DNSMAN) == BST_CHECKED;
    for (int id : {IDC_DNS1, IDC_DNS2, IDC_LBL_DNS1, IDC_LBL_DNS2}) EnableWindow(GetDlgItem(d, id), man);
    EnableWindow(GetDlgItem(d, IDC_UP), g_edSel > 0);
    EnableWindow(GetDlgItem(d, IDC_DOWN), g_edSel + 1 < (int)g_ed.size());
}

static void edFillAdapters(HWND d) {
    HWND c = GetDlgItem(d, IDC_ADAPTER);
    wstring cur = dlgText(d, IDC_ADAPTER);
    g_edAds = getAdapters();
    SendMessageW(c, CB_RESETCONTENT, 0, 0);
    for (auto& a : g_edAds) SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)a.friendly.c_str());
    SetWindowTextW(c, cur.c_str());
}

static void edExtraButtons(HWND d, const Profile* p) {
    wstring v6 = TR(L"IPv6 …", L"IPv6 …");
    if (p && p->ip6mode == 1) v6 = TR(L"IPv6: automatisch", L"IPv6: automatic");
    else if (p && p->ip6mode == 2) v6 = TR(L"IPv6: statisch", L"IPv6: static");
    SetDlgItemTextW(d, IDC_IPV6BTN, v6.c_str());
    wstring as = (p && p->autoOn) ? wstring(TR(L"AutoSwitch: an", L"AutoSwitch: on")) : wstring(TR(L"AutoSwitch …", L"AutoSwitch …"));
    SetDlgItemTextW(d, IDC_AUTOBTN, as.c_str());
}

static void edFill(HWND d, const Profile* p) {
    g_edLoading = true;
    g_changed.clear();
    SetDlgItemTextW(d, IDC_NAME, p ? p->name.c_str() : L"");
    SetDlgItemTextW(d, IDC_ADAPTER, p ? p->adapter.c_str() : L"");
    CheckRadioButton(d, IDC_DHCP, IDC_STATICIP, (!p || p->dhcp) ? IDC_DHCP : IDC_STATICIP);

    if (p && !p->ips.empty()) {
        ipSet(d, IDC_IP, &p->ips[0].addr);
        ULONG m = 0;
        m = prefixToMask(p->ips[0].prefix);
        IN_ADDR ma;
        ma.s_addr = m;
        ipSet(d, IDC_MASK, &ma);
    } else {
        ipSet(d, IDC_IP, nullptr);
        ipSet(d, IDC_MASK, nullptr);
    }
    ipSet(d, IDC_GW, (p && p->hasGw) ? &p->gw : nullptr);

    wstring extra;
    if (p)
        for (size_t i = 1; i < p->ips.size(); ++i) extra += (i > 1 ? L"\r\n" : L"") + cidrStr(p->ips[i]);
    SetDlgItemTextW(d, IDC_EXTRA, extra.c_str());

    bool dnsAuto = !p || (p->dhcp && p->dns.empty());
    EnableWindow(GetDlgItem(d, IDC_DNSAUTO), TRUE);
    CheckRadioButton(d, IDC_DNSAUTO, IDC_DNSMAN, dnsAuto ? IDC_DNSAUTO : IDC_DNSMAN);
    ipSet(d, IDC_DNS1, (p && p->dns.size() > 0) ? &p->dns[0] : nullptr);
    ipSet(d, IDC_DNS2, (p && p->dns.size() > 1) ? &p->dns[1] : nullptr);

    edExtraButtons(d, p);
    edUpdateEnable(d);
    g_edLoading = false;
}

static bool edFail(HWND d, int focusId, const wstring& msg) {
    edFeedback(d, FB_ERR, msg);
    MessageBoxW(d, msg.c_str(), TR(L"MilleIpSwitch – Eingabe prüfen", L"MilleIpSwitch – check input"), MB_ICONWARNING);
    SetFocus(GetDlgItem(d, focusId));
    return false;
}

static bool edCommit(HWND d) {
    if (g_edSel < 0) return true;
    Profile p = g_ed[g_edSel];

    p.name = trim(dlgText(d, IDC_NAME));
    if (p.name.empty()) return edFail(d, IDC_NAME, TR(L"Bitte einen Profilnamen eingeben.", L"Please enter a profile name."));
    if (p.name.find_first_of(L"[]") != wstring::npos)
        return edFail(d, IDC_NAME, TR(L"Der Profilname darf keine eckigen Klammern enthalten.", L"The profile name must not contain square brackets."));
    for (size_t i = 0; i < g_ed.size(); ++i)
        if ((int)i != g_edSel && ieq(g_ed[i].name, p.name))
            return edFail(d, IDC_NAME, TR(L"Ein Profil mit diesem Namen gibt es schon.", L"A profile with this name already exists."));

    p.adapter = trim(dlgText(d, IDC_ADAPTER));
    if (p.adapter.empty()) return edFail(d, IDC_ADAPTER, TR(L"Bitte einen Adapter auswählen.", L"Please select an adapter."));

    p.dhcp = IsDlgButtonChecked(d, IDC_DHCP) == BST_CHECKED;
    p.ips.clear();
    p.hasGw = false;
    if (!p.dhcp) {
        Ip4 first;
        IN_ADDR m;
        if (ipGet(d, IDC_IP, first.addr) != 1) return edFail(d, IDC_IP, TR(L"Bitte eine vollständige IP-Adresse eingeben.", L"Please enter a complete IP address."));
        UINT8 len = 0;
        if (ipGet(d, IDC_MASK, m) != 1 || !maskToPrefix(m.s_addr, len) || len == 0)
            return edFail(d, IDC_MASK, TR(L"Die Subnetzmaske ist ungültig.", L"The subnet mask is invalid."));
        first.prefix = len;
        p.ips.push_back(first);

        wstring extra = dlgText(d, IDC_EXTRA);
        for (auto& c : extra)
            if (c == L'\r' || c == L'\n') c = L',';
        for (auto& t : splitList(extra)) {
            Ip4 r;
            wstring e;
            if (!parseCidr(t, r, e)) return edFail(d, IDC_EXTRA, TR(L"Weitere IP-Adressen: ", L"Additional IP addresses: ") + e);
            p.ips.push_back(r);
        }

        int g = ipGet(d, IDC_GW, p.gw);
        if (g < 0) return edFail(d, IDC_GW, TR(L"Das Standardgateway ist unvollständig.", L"The default gateway is incomplete."));
        p.hasGw = g == 1;
    }

    vector<IN_ADDR> oldDns = p.dns;
    p.dns.clear();
    if (IsDlgButtonChecked(d, IDC_DNSMAN) == BST_CHECKED) {
        for (int id : {IDC_DNS1, IDC_DNS2}) {
            IN_ADDR a;
            int r = ipGet(d, id, a);
            if (r < 0) return edFail(d, id, TR(L"Die DNS-Serveradresse ist unvollständig.", L"The DNS server address is incomplete."));
            if (r == 1) p.dns.push_back(a);
        }
        for (size_t i = 2; i < oldDns.size(); ++i) p.dns.push_back(oldDns[i]);
    }

    g_ed[g_edSel] = p;
    return true;
}

static void edRefreshList(HWND d) {
    HWND l = GetDlgItem(d, IDC_LIST);
    SendMessageW(l, LB_RESETCONTENT, 0, 0);
    for (auto& p : g_ed) SendMessageW(l, LB_ADDSTRING, 0, (LPARAM)p.name.c_str());
    SendMessageW(l, LB_SETCURSEL, g_edSel, 0);
}

static void edSelect(HWND d, int idx) {
    g_edSel = idx;
    SendMessageW(GetDlgItem(d, IDC_LIST), LB_SETCURSEL, idx, 0);
    edFill(d, idx >= 0 ? &g_ed[idx] : nullptr);
}

static wstring uniqueName(const wstring& base) {
    wstring n = base;
    for (int i = 2;; ++i) {
        bool used = false;
        for (auto& p : g_ed)
            if (ieq(p.name, n)) used = true;
        if (!used) return n;
        n = base + fmt(L" %d", i);
    }
}

static void edFlash(HWND d) {
    g_flash.assign(g_changed.begin(), g_changed.end());
    if (g_edListChanged) g_flash.push_back(IDC_LIST);
    g_changed.clear();
    g_edListChanged = false;
    if (g_flash.empty()) return;
    SetTimer(d, 1, 1800, nullptr);
    RedrawWindow(d, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

static bool edSave(HWND d) {
    bool wasDirty = g_edDirty;
    if (!edCommit(d)) return false;
    wstring err;
    if (!saveProfiles(g_ed, err)) {
        edFeedback(d, FB_ERR, TR(L"Speichern fehlgeschlagen", L"Saving failed"));
        MessageBoxW(d, err.c_str(), L"MilleIpSwitch", MB_ICONERROR);
        return false;
    }
    g_profiles = g_ed;
    size_t n = g_changed.size() + (g_edListChanged ? 1 : 0);
    edFlash(d);
    edSetDirty(d, false);
    InvalidateRect(GetDlgItem(d, IDC_LIST), nullptr, FALSE);
    if (!wasDirty)
        edFeedback(d, FB_INFO, TR(L"Keine Änderungen – alles ist bereits gespeichert.", L"No changes – everything is already saved."));
    else
        edFeedback(d, FB_OK, (n == 0 ? wstring(TR(L"Änderungen gespeichert.", L"Changes saved.")) : n == 1 ? wstring(TR(L"1 Änderung gespeichert.", L"1 change saved."))
                                       : fmt(TR(L"%u Änderungen gespeichert.", L"%u changes saved."), (unsigned)n)) +
                                 TR(L"  „Aktivieren“ wendet das Profil auf den Adapter an.", L"  “Activate” applies the profile to the adapter."));
    return true;
}

static vector<wstring> regMultiSz(HKEY k, const wchar_t* name) {
    vector<wstring> r;
    DWORD type = 0, sz = 0;
    if (RegQueryValueExW(k, name, nullptr, &type, nullptr, &sz) != ERROR_SUCCESS || !sz) return r;
    wstring buf(sz / sizeof(wchar_t) + 2, L'\0');
    if (RegQueryValueExW(k, name, nullptr, &type, (BYTE*)&buf[0], &sz) != ERROR_SUCCESS) return r;
    if (type == REG_SZ) return splitList(wstring(buf.c_str()));
    for (const wchar_t* p = buf.c_str(); *p; p += wcslen(p) + 1) r.push_back(p);
    return r;
}

static void profileFromAdapter(const Adapter& a, Profile& p) {
    p.adapter = a.friendly;
    p.dhcp = a.dhcp;
    p.ips.clear();
    p.hasGw = false;
    p.dns.clear();
    wstring guid(a.guidStr.begin(), a.guidStr.end());
    HKEY k;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      (L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces\\" + guid).c_str(), 0,
                      KEY_READ, &k) == ERROR_SUCCESS) {
        if (!p.dhcp) {
            auto ips = regMultiSz(k, L"IPAddress"), masks = regMultiSz(k, L"SubnetMask");
            for (size_t i = 0; i < ips.size() && i < masks.size(); ++i) {
                Ip4 w;
                IN_ADDR m;
                UINT8 len;
                if (parseIp(ips[i], w.addr) && parseIp(masks[i], m) && w.addr.s_addr &&
                    maskToPrefix(m.s_addr, len)) {
                    w.prefix = len;
                    p.ips.push_back(w);
                }
            }
            auto gws = regMultiSz(k, L"DefaultGateway");
            if (!gws.empty() && parseIp(gws[0], p.gw)) p.hasGw = true;
        }
        for (auto& s : regMultiSz(k, L"NameServer")) {
            for (auto& c : s)
                if (c == L' ') c = L',';
            for (auto& t : splitList(s)) {
                IN_ADDR x;
                if (parseIp(t, x)) p.dns.push_back(x);
            }
        }
        RegCloseKey(k);
    }
    if (!p.dhcp && p.ips.empty()) {
        p.ips = a.ips;
        if (!a.gws.empty()) {
            p.gw = a.gws[0];
            p.hasGw = true;
        }
        if (p.dns.empty()) p.dns = a.dns;
    }
    p.ip6mode = 0;
    p.ips6.clear();
    p.hasGw6 = false;
    p.dns6.clear();
    vector<Ip6> manual6;
    bool global6 = false;
    for (auto& x : a.ips6) {
        if (x.manual && !x.linkLocal) manual6.push_back(x);
        if (!x.linkLocal && !x.temp) global6 = true;
    }
    if (!manual6.empty()) {
        p.ip6mode = 2;
        p.ips6 = manual6;
        if (!a.gws6.empty()) {
            p.gw6 = a.gws6[0];
            p.hasGw6 = true;
        }
        p.dns6 = a.dns6;
    } else if (global6) {
        p.ip6mode = 1;
    }
}

static void edReadCurrent(HWND d) {
    wstring name = trim(dlgText(d, IDC_ADAPTER));
    g_edAds = getAdapters();
    const Adapter* a = findAdapter(g_edAds, name);
    if (!a) {
        edFeedback(d, FB_ERR, TR(L"Adapter nicht gefunden: ", L"Adapter not found: ") + name);
        return;
    }
    Profile p = g_ed[g_edSel];
    p.name = trim(dlgText(d, IDC_NAME));
    profileFromAdapter(*a, p);
    g_ed[g_edSel] = p;
    edFill(d, &p);
    for (int id : {IDC_ADAPTER, IDC_DHCP, IDC_STATICIP, IDC_IP, IDC_MASK, IDC_GW, IDC_EXTRA, IDC_DNS1, IDC_DNS2})
        g_changed.insert(id);
    edSetDirty(d, true);
    edFeedback(d, FB_INFO, TR(L"Einstellungen von „", L"Settings of “") + a->friendly + TR(L"“ übernommen – zum Sichern „Speichern“ klicken.", L"” loaded – click “Save” to keep them."));
}

static void edApplyLang(HWND d) {
    struct { int id; const wchar_t *de, *en; } t[] = {
        {IDC_LBL_PROFILES, L"Profile:", L"Profiles:"},
        {IDC_NEW, L"Neu", L"New"},
        {IDC_DUP, L"Kopieren", L"Copy"},
        {IDC_DEL, L"Löschen", L"Delete"},
        {IDC_UP, L"Nach oben", L"Move up"},
        {IDC_DOWN, L"Nach unten", L"Move down"},
        {IDC_LBL_NAME, L"Profilname:", L"Profile name:"},
        {IDC_LBL_ADAPTER, L"Adapter:", L"Adapter:"},
        {IDC_READCUR, L"Aktuelle Einstellungen des Adapters übernehmen", L"Load the adapter's current settings"},
        {IDC_GRP_IP, L"IP-Adresse", L"IP address"},
        {IDC_DHCP, L"IP-Adresse automatisch beziehen (DHCP)", L"Obtain an IP address automatically (DHCP)"},
        {IDC_STATICIP, L"Folgende IP-Adresse verwenden:", L"Use the following IP address:"},
        {IDC_LBL_IP, L"IP-Adresse:", L"IP address:"},
        {IDC_LBL_MASK, L"Subnetzmaske:", L"Subnet mask:"},
        {IDC_LBL_GW, L"Standardgateway:", L"Default gateway:"},
        {IDC_LBL_EXTRA, L"Weitere IP-Adressen (eine pro Zeile, z. B. 10.0.0.5/8):",
         L"Additional IP addresses (one per line, e.g. 10.0.0.5/8):"},
        {IDC_GRP_DNS, L"DNS-Server", L"DNS servers"},
        {IDC_DNSAUTO, L"DNS-Serveradresse automatisch beziehen", L"Obtain DNS server address automatically"},
        {IDC_DNSMAN, L"Folgende DNS-Serveradressen verwenden:", L"Use the following DNS server addresses:"},
        {IDC_LBL_DNS1, L"Bevorzugter DNS-Server:", L"Preferred DNS server:"},
        {IDC_LBL_DNS2, L"Alternativer DNS-Server:", L"Alternate DNS server:"},
        {IDC_ACTIVATE, L"Aktivieren", L"Activate"},
        {IDC_STATUSBTN, L"Netzwerkstatus", L"Network status"},
        {IDCANCEL, L"Schließen", L"Close"},
        {IDC_APPLY, L"Speichern", L"Save"},
        {IDC_IMPORT, L"Importieren …", L"Import …"},
        {IDC_EXPORT, L"Exportieren …", L"Export …"},
    };
    for (auto& x : t) SetDlgItemTextW(d, x.id, TR(x.de, x.en));
    edExtraButtons(d, (g_edSel >= 0 && g_edSel < (int)g_ed.size()) ? &g_ed[g_edSel] : nullptr);
    edSetDirty(d, g_edDirty);
    g_fbKind = FB_NONE;
    InvalidateRect(GetDlgItem(d, IDC_FEEDBACK), nullptr, TRUE);
    InvalidateRect(GetDlgItem(d, IDC_LANG), nullptr, TRUE);
    InvalidateRect(GetDlgItem(d, IDC_LIST), nullptr, TRUE);
}

static void edDrawLang(HWND d, DRAWITEMSTRUCT* di) {
    UINT dpi = dpiOf(d);
    HDC dc = di->hDC;
    RECT r = di->rcItem;
    fillRect(dc, r, GetSysColor(COLOR_3DFACE));
    RECT b = r;
    InflateRect(&b, -1, -1);
    bool down = (di->itemState & ODS_SELECTED) != 0;
    fillRound(dc, b, S(6, dpi), down ? C::Hover : C::Card, (di->itemState & ODS_FOCUS) ? C::Accent : C::Border);
    int fw = S(18, dpi), fh = S(12, dpi), cy = (r.top + r.bottom) / 2;
    RECT fr{r.left + S(9, dpi), cy - fh / 2, r.left + S(9, dpi) + fw, cy - fh / 2 + fh};
    drawFlag(dc, g_lang, fr);
    RECT tr{fr.right + S(7, dpi), r.top, r.right - S(4, dpi), r.bottom};
    drawText(dc, langName(g_lang), tr, C::Text, (HFONT)SendMessageW(d, WM_GETFONT, 0, 0),
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

static void edClose(HWND d) {
    if (g_edDirty) {
        int r = MessageBoxW(d, TR(L"Änderungen an den Profilen speichern?", L"Save changes to the profiles?"), L"MilleIpSwitch", MB_YESNOCANCEL | MB_ICONQUESTION);
        if (r == IDCANCEL) return;
        if (r == IDYES && !edSave(d)) return;
    }
    DestroyWindow(d);
}

static void edApplied(HWND d, bool ok, const wstring& msg) {
    g_edAds = getAdapters();
    InvalidateRect(GetDlgItem(d, IDC_LIST), nullptr, FALSE);
    edFeedback(d, ok ? FB_OK : FB_ERR, firstLine(msg));
}

static void edDrawList(HWND d, DRAWITEMSTRUCT* di) {
    if (di->itemID == (UINT)-1) return;
    UINT dpi = dpiOf(d);
    HDC dc = di->hDC;
    RECT r = di->rcItem;
    bool sel = (di->itemState & ODS_SELECTED) != 0;
    fillRect(dc, r, GetSysColor(COLOR_WINDOW));
    bool flash = std::find(g_flash.begin(), g_flash.end(), IDC_LIST) != g_flash.end() && sel;
    if (sel || flash) {
        RECT hr = r;
        InflateRect(&hr, -S(3, dpi), -S(1, dpi));
        COLORREF c = flash ? C::GreenBg : C::Select;
        fillRound(dc, hr, S(8, dpi), c, c);
    }
    int n = (int)SendMessageW(di->hwndItem, LB_GETTEXTLEN, di->itemID, 0);
    wstring name(std::max(n, 0) + 1, L'\0');
    SendMessageW(di->hwndItem, LB_GETTEXT, di->itemID, (LPARAM)&name[0]);
    name.resize(std::max(n, 0));

    const Profile* p = di->itemID < g_ed.size() ? &g_ed[di->itemID] : nullptr;
    bool active = p && isActive(*p, g_edAds);
    RECT gr{r.left + S(8, dpi), r.top, r.left + S(30, dpi), r.bottom};
    drawGlyph(dc, p ? profileGlyph(*p, g_edAds) : G::Ethernet, gr, active ? C::Green : C::Accent, g_eGlyph);
    RECT cr{r.right - S(26, dpi), r.top, r.right - S(6, dpi), r.bottom};
    if (active) drawCheck(dc, cr, g_eGlyph, dpi);

    RECT nr{r.left + S(36, dpi), r.top + S(3, dpi), cr.left - S(2, dpi), r.top + S(20, dpi)};
    drawText(dc, name, nr, C::Text, active ? g_eBold : (HFONT)SendMessageW(d, WM_GETFONT, 0, 0),
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (p) {
        RECT dr{r.left + S(36, dpi), r.top + S(19, dpi), cr.left - S(2, dpi), r.bottom - S(3, dpi)};
        wstring mode = profileDetail(*p);
        if (p->ip6mode) mode += L" · IPv6";
        if (p->autoOn) mode += L" · Auto";
        drawText(dc, mode, dr, C::Accent, g_eSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        dr.left += textWidth(dc, mode, g_eSmall);
        drawText(dc, p->adapter.empty() ? wstring() : L" · " + p->adapter, dr, C::Sub, g_eSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
}

static void edDrawFeedback(HWND d, DRAWITEMSTRUCT* di) {
    UINT dpi = dpiOf(d);
    HDC dc = di->hDC;
    RECT r = di->rcItem;
    fillRect(dc, r, GetSysColor(COLOR_3DFACE));
    if (g_fbKind == FB_NONE || g_fbText.empty()) return;
    COLORREF fg = g_fbKind == FB_OK ? C::Green : g_fbKind == FB_ERR ? C::Red : C::Accent;
    COLORREF bg = g_fbKind == FB_OK ? C::GreenBg : g_fbKind == FB_ERR ? C::RedBg : C::AccentBg;
    wchar_t g = g_fbKind == FB_OK ? G::Completed : g_fbKind == FB_ERR ? G::Error : G::Info;
    fillRound(dc, r, S(8, dpi), bg, bg);
    RECT gr{r.left + S(6, dpi), r.top, r.left + S(28, dpi), r.bottom};
    drawGlyph(dc, g, gr, fg, g_eFbGlyph);
    RECT tr{gr.right + S(4, dpi), r.top, r.right - S(8, dpi), r.bottom};
    drawText(dc, g_fbText, tr, fg, (HFONT)SendMessageW(d, WM_GETFONT, 0, 0),
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static bool inFlash(int id) { return std::find(g_flash.begin(), g_flash.end(), id) != g_flash.end(); }

static Profile* g_dlgProfile = nullptr;

static void v6Texts(HWND d) {
    SetWindowTextW(d, TR(L"IPv6-Einstellungen", L"IPv6 settings"));
    struct { int id; const wchar_t *de, *en; } t[] = {
        {IDC_V6_KEEP, L"IPv6 nicht ändern", L"Leave IPv6 unchanged"},
        {IDC_V6_AUTO, L"IPv6 automatisch beziehen (Router-Advertisement / DHCPv6)", L"Obtain IPv6 automatically (router advertisement / DHCPv6)"},
        {IDC_V6_STATIC, L"Folgende IPv6-Adressen verwenden:", L"Use the following IPv6 addresses:"},
        {IDC_V6_LBL_ADDR, L"Adressen (eine pro Zeile, z. B. 2001:db8::10/64):", L"Addresses (one per line, e.g. 2001:db8::10/64):"},
        {IDC_V6_LBL_GW, L"Standardgateway:", L"Default gateway:"},
        {IDC_V6_LBL_DNS1, L"Bevorzugter DNS-Server:", L"Preferred DNS server:"},
        {IDC_V6_LBL_DNS2, L"Alternativer DNS-Server:", L"Alternate DNS server:"},
        {IDC_V6_HINT, L"DNS leer lassen = automatisch.", L"Leave DNS empty for automatic."},
        {IDOK, L"OK", L"OK"},
        {IDCANCEL, L"Abbrechen", L"Cancel"},
    };
    for (auto& x : t) SetDlgItemTextW(d, x.id, TR(x.de, x.en));
}

static void v6Enable(HWND d) {
    bool st = IsDlgButtonChecked(d, IDC_V6_STATIC) == BST_CHECKED;
    bool any = IsDlgButtonChecked(d, IDC_V6_KEEP) != BST_CHECKED;
    for (int id : {IDC_V6_ADDR, IDC_V6_GW, IDC_V6_LBL_ADDR, IDC_V6_LBL_GW}) EnableWindow(GetDlgItem(d, id), st);
    for (int id : {IDC_V6_DNS1, IDC_V6_DNS2, IDC_V6_LBL_DNS1, IDC_V6_LBL_DNS2, IDC_V6_HINT}) EnableWindow(GetDlgItem(d, id), any);
}

static bool v6Fail(HWND d, int focusId, const wstring& msg) {
    MessageBoxW(d, msg.c_str(), L"MilleIpSwitch", MB_ICONWARNING);
    SetFocus(GetDlgItem(d, focusId));
    return false;
}

static bool v6Parse(HWND d, Profile& p) {
    Profile q = p;
    q.ip6mode = IsDlgButtonChecked(d, IDC_V6_STATIC) == BST_CHECKED ? 2 : IsDlgButtonChecked(d, IDC_V6_AUTO) == BST_CHECKED ? 1 : 0;
    q.ips6.clear();
    q.hasGw6 = false;
    q.dns6.clear();
    if (q.ip6mode == 2) {
        wstring t = dlgText(d, IDC_V6_ADDR);
        for (auto& c : t)
            if (c == L'\r' || c == L'\n') c = L',';
        for (auto& part : splitList(t)) {
            Ip6 r;
            wstring e;
            if (!parseCidr6(part, r, e)) return v6Fail(d, IDC_V6_ADDR, e);
            q.ips6.push_back(r);
        }
        if (q.ips6.empty()) return v6Fail(d, IDC_V6_ADDR, TR(L"Bitte mindestens eine IPv6-Adresse angeben.", L"Please enter at least one IPv6 address."));
        wstring g = trim(dlgText(d, IDC_V6_GW));
        if (!g.empty()) {
            if (!parseIp6(g, q.gw6)) return v6Fail(d, IDC_V6_GW, TR(L"Ungültiges IPv6-Gateway.", L"Invalid IPv6 gateway."));
            q.hasGw6 = true;
        }
    }
    if (q.ip6mode != 0)
        for (int id : {IDC_V6_DNS1, IDC_V6_DNS2}) {
            wstring sv = trim(dlgText(d, id));
            if (sv.empty()) continue;
            IN6_ADDR a;
            if (!parseIp6(sv, a)) return v6Fail(d, id, TR(L"Ungültiger IPv6-DNS-Server.", L"Invalid IPv6 DNS server."));
            q.dns6.push_back(a);
        }
    p = q;
    return true;
}

static INT_PTR CALLBACK v6Proc(HWND d, UINT msg, WPARAM wp, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        SendMessageW(d, WM_SETICON, ICON_SMALL, (LPARAM)g_iconWinSm);
        SendMessageW(d, WM_SETICON, ICON_BIG, (LPARAM)g_iconWinBig);
        v6Texts(d);
        const Profile& p = *g_dlgProfile;
        CheckRadioButton(d, IDC_V6_KEEP, IDC_V6_STATIC, IDC_V6_KEEP + p.ip6mode);
        wstring addr;
        for (size_t i = 0; i < p.ips6.size(); ++i) addr += (i ? L"\r\n" : L"") + cidr6Str(p.ips6[i]);
        SetDlgItemTextW(d, IDC_V6_ADDR, addr.c_str());
        SetDlgItemTextW(d, IDC_V6_GW, p.hasGw6 ? ip6Str(p.gw6).c_str() : L"");
        SetDlgItemTextW(d, IDC_V6_DNS1, p.dns6.size() > 0 ? ip6Str(p.dns6[0]).c_str() : L"");
        SetDlgItemTextW(d, IDC_V6_DNS2, p.dns6.size() > 1 ? ip6Str(p.dns6[1]).c_str() : L"");
        v6Enable(d);
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_V6_KEEP:
        case IDC_V6_AUTO:
        case IDC_V6_STATIC:
            if (HIWORD(wp) == BN_CLICKED) v6Enable(d);
            return TRUE;
        case IDOK:
            if (v6Parse(d, *g_dlgProfile)) EndDialog(d, IDOK);
            return TRUE;
        case IDCANCEL:
            EndDialog(d, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static void asTexts(HWND d) {
    SetWindowTextW(d, L"AutoSwitch");
    struct { int id; const wchar_t *de, *en; } t[] = {
        {IDC_AS_ENABLE, L"Dieses Profil automatisch aktivieren", L"Activate this profile automatically"},
        {IDC_AS_INFO, L"Das Profil wird aktiviert, sobald der Adapter verbunden ist und alle angehakten Bedingungen zutreffen. Passen mehrere Profile desselben Adapters, gewinnt das oberste in der Liste. Nach einer manuellen Umschaltung pausiert AutoSwitch für diesen Adapter 10 Minuten. Gateway-MAC und Ping gelten für das aktuell verbundene Netz; die ARP-Prüfung findet auch Geräte in einem anderen Subnetz am selben Switch.", L"The profile is activated as soon as the adapter is connected and all checked conditions are met. If several profiles of the same adapter match, the topmost in the list wins. After a manual switch, AutoSwitch pauses for that adapter for 10 minutes. Gateway MAC and ping apply to the currently connected network; the ARP check also finds devices in a different subnet on the same switch."},
        {IDC_AS_GRP, L"Bedingungen (zusätzlich zu „Adapter verbunden“)", L"Conditions (in addition to “adapter connected”)"},
        {IDC_AS_MACCHK, L"Das Gateway hat die MAC-Adresse:", L"The gateway has the MAC address:"},
        {IDC_AS_MACCUR, L"Aktuelle übernehmen", L"Use current"},
        {IDC_AS_PINGCHK, L"Dieser Host antwortet auf Ping:", L"This host answers ping:"},
        {IDC_AS_ARPCHK, L"Ein Gerät antwortet auf ARP (geht auch aus einem anderen Subnetz am selben Switch):", L"A device answers ARP (also works from a different subnet on the same switch):"},
        {IDC_AS_ARPTEST, L"Testen", L"Test"},
        {IDOK, L"OK", L"OK"},
        {IDCANCEL, L"Abbrechen", L"Cancel"},
    };
    for (auto& x : t) SetDlgItemTextW(d, x.id, TR(x.de, x.en));
}

static void asEnable(HWND d) {
    bool on = IsDlgButtonChecked(d, IDC_AS_ENABLE) == BST_CHECKED;
    for (int id : {IDC_AS_GRP, IDC_AS_MACCHK, IDC_AS_PINGCHK, IDC_AS_MACCUR}) EnableWindow(GetDlgItem(d, id), on);
    EnableWindow(GetDlgItem(d, IDC_AS_MAC), on && IsDlgButtonChecked(d, IDC_AS_MACCHK) == BST_CHECKED);
    EnableWindow(GetDlgItem(d, IDC_AS_PING), on && IsDlgButtonChecked(d, IDC_AS_PINGCHK) == BST_CHECKED);
    EnableWindow(GetDlgItem(d, IDC_AS_ARPCHK), on);
    bool arp = on && IsDlgButtonChecked(d, IDC_AS_ARPCHK) == BST_CHECKED;
    for (int id : {IDC_AS_ARPIP, IDC_AS_ARPMAC, IDC_AS_ARPTEST, IDC_AS_ARPL1, IDC_AS_ARPL2}) EnableWindow(GetDlgItem(d, id), arp);
}

static INT_PTR CALLBACK asProc(HWND d, UINT msg, WPARAM wp, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        SendMessageW(d, WM_SETICON, ICON_SMALL, (LPARAM)g_iconWinSm);
        SendMessageW(d, WM_SETICON, ICON_BIG, (LPARAM)g_iconWinBig);
        asTexts(d);
        const Profile& p = *g_dlgProfile;
        CheckDlgButton(d, IDC_AS_ENABLE, p.autoOn ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(d, IDC_AS_MACCHK, p.autoMac.empty() ? BST_UNCHECKED : BST_CHECKED);
        CheckDlgButton(d, IDC_AS_PINGCHK, p.autoPing.empty() ? BST_UNCHECKED : BST_CHECKED);
        SetDlgItemTextW(d, IDC_AS_MAC, p.autoMac.c_str());
        SetDlgItemTextW(d, IDC_AS_PING, p.autoPing.c_str());
        CheckDlgButton(d, IDC_AS_ARPCHK, p.autoArpIp.empty() ? BST_UNCHECKED : BST_CHECKED);
        SetDlgItemTextW(d, IDC_AS_ARPIP, p.autoArpIp.c_str());
        SetDlgItemTextW(d, IDC_AS_ARPMAC, p.autoArpMac.c_str());
        asEnable(d);
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_AS_ENABLE:
        case IDC_AS_MACCHK:
        case IDC_AS_PINGCHK:
        case IDC_AS_ARPCHK:
            if (HIWORD(wp) == BN_CLICKED) asEnable(d);
            return TRUE;
        case IDC_AS_ARPTEST: {
            IN_ADDR ip{};
            vector<Adapter> ads = getAdapters();
            const Adapter* a = findAdapter(ads, g_dlgProfile->adapter);
            if (!parseIp(trim(dlgText(d, IDC_AS_ARPIP)), ip)) {
                SetDlgItemTextW(d, IDC_AS_STATUS, TR(L"Bitte eine gültige IPv4-Adresse eingeben.", L"Please enter a valid IPv4 address."));
                return TRUE;
            }
            wstring mac;
            HCURSOR old = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
            bool ok = a && a->up && arpFresh(*a, ip.s_addr, mac);
            SetCursor(old);
            if (ok) {
                if (trim(dlgText(d, IDC_AS_ARPMAC)).empty()) SetDlgItemTextW(d, IDC_AS_ARPMAC, mac.c_str());
                SetDlgItemTextW(d, IDC_AS_STATUS, fmt(TR(L"Gerät antwortet: MAC %ls", L"Device answers: MAC %ls"), mac.c_str()).c_str());
            } else {
                SetDlgItemTextW(d, IDC_AS_STATUS, TR(L"Keine Antwort – Gerät am selben Switch und eingeschaltet?", L"No answer – is the device on the same switch and powered on?"));
            }
            return TRUE;
        }
        case IDC_AS_MACCUR: {
            vector<Adapter> ads = getAdapters();
            const Adapter* a = findAdapter(ads, g_dlgProfile->adapter);
            wstring mac;
            HCURSOR old = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
            bool ok = a && a->up && gatewayMac(*a, mac);
            SetCursor(old);
            if (ok) {
                SetDlgItemTextW(d, IDC_AS_MAC, mac.c_str());
                CheckDlgButton(d, IDC_AS_MACCHK, BST_CHECKED);
                asEnable(d);
                SetDlgItemTextW(d, IDC_AS_STATUS, fmt(TR(L"Gateway-MAC übernommen: %ls", L"Gateway MAC captured: %ls"), mac.c_str()).c_str());
            } else {
                SetDlgItemTextW(d, IDC_AS_STATUS, TR(L"Keine MAC ermittelt – Adapter verbunden und Gateway erreichbar?", L"No MAC found – is the adapter connected and the gateway reachable?"));
            }
            return TRUE;
        }
        case IDOK: {
            Profile& p = *g_dlgProfile;
            bool on = IsDlgButtonChecked(d, IDC_AS_ENABLE) == BST_CHECKED;
            if (!on) {
                p.autoOn = false;
                EndDialog(d, IDOK);
                return TRUE;
            }
            bool mc = IsDlgButtonChecked(d, IDC_AS_MACCHK) == BST_CHECKED;
            bool pc = IsDlgButtonChecked(d, IDC_AS_PINGCHK) == BST_CHECKED;
            wstring mac = normMac(dlgText(d, IDC_AS_MAC)), ping = trim(dlgText(d, IDC_AS_PING));
            if (mc && mac.empty()) {
                v6Fail(d, IDC_AS_MAC, TR(L"Bitte eine gültige MAC-Adresse eingeben (z. B. AA-BB-CC-DD-EE-FF).", L"Please enter a valid MAC address (e.g. AA-BB-CC-DD-EE-FF)."));
                return TRUE;
            }
            if (pc && ping.empty()) {
                v6Fail(d, IDC_AS_PING, TR(L"Bitte einen Host für den Ping angeben.", L"Please enter a host to ping."));
                return TRUE;
            }
            p.autoOn = true;
            p.autoMac = mc ? mac : L"";
            p.autoPing = pc ? ping : L"";
            bool ac = IsDlgButtonChecked(d, IDC_AS_ARPCHK) == BST_CHECKED;
            wstring arpIp = trim(dlgText(d, IDC_AS_ARPIP)), arpMac = normMac(dlgText(d, IDC_AS_ARPMAC));
            IN_ADDR tmpIp{};
            if (ac && !parseIp(arpIp, tmpIp)) {
                v6Fail(d, IDC_AS_ARPIP, TR(L"Bitte eine gültige IPv4-Adresse eingeben.", L"Please enter a valid IPv4 address."));
                return TRUE;
            }
            p.autoArpIp = ac ? arpIp : L"";
            p.autoArpMac = ac ? arpMac : L"";
            EndDialog(d, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(d, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static bool pickIniFile(HWND owner, bool save, wstring& path) {
    wchar_t buf[MAX_PATH] = L"";
    if (save) lstrcpynW(buf, L"milleipswitch-profiles.ini", MAX_PATH);
    wstring filter;
    filter += TR(L"Profildateien (*.ini)", L"Profile files (*.ini)");
    filter.push_back(L'\0');
    filter += L"*.ini";
    filter.push_back(L'\0');
    filter += TR(L"Alle Dateien (*.*)", L"All files (*.*)");
    filter.push_back(L'\0');
    filter += L"*.*";
    filter.push_back(L'\0');
    OPENFILENAMEW o{};
    o.lStructSize = sizeof(o);
    o.hwndOwner = owner;
    o.lpstrFilter = filter.c_str();
    o.lpstrFile = buf;
    o.nMaxFile = MAX_PATH;
    o.lpstrDefExt = L"ini";
    o.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    BOOL ok = save ? GetSaveFileNameW(&o) : GetOpenFileNameW(&o);
    if (ok) path = buf;
    return ok != 0;
}

static void edExport(HWND d) {
    if (!edCommit(d)) return;
    wstring path, err;
    if (!pickIniFile(d, true, path)) return;
    if (writeProfilesTo(path, g_ed, err)) {
        edFeedback(d, FB_OK, fmt(TR(L"%u Profile exportiert.", L"%u profiles exported."), (unsigned)g_ed.size()));
    } else {
        edFeedback(d, FB_ERR, firstLine(err));
        MessageBoxW(d, err.c_str(), L"MilleIpSwitch", MB_ICONERROR);
    }
}

static void edImport(HWND d) {
    if (!edCommit(d)) return;
    wstring path, err;
    if (!pickIniFile(d, false, path)) return;
    vector<Profile> in;
    if (!loadProfilesFrom(path, in, err)) {
        edFeedback(d, FB_ERR, firstLine(err));
        MessageBoxW(d, err.c_str(), L"MilleIpSwitch", MB_ICONERROR);
        return;
    }
    if (in.empty()) {
        edFeedback(d, FB_INFO, TR(L"Keine Profile in dieser Datei gefunden.", L"No profiles found in this file."));
        return;
    }
    int first = (int)g_ed.size();
    for (auto& p : in) {
        p.name = uniqueName(p.name);
        g_ed.push_back(p);
    }
    g_edListChanged = true;
    edRefreshList(d);
    edSelect(d, first);
    edSetDirty(d, true);
    edFeedback(d, FB_INFO,
               fmt(TR(L"%u Profile importiert – mit „Speichern“ übernehmen.", L"%u profiles imported – click “Save” to keep them."),
                   (unsigned)in.size()));
}

static INT_PTR CALLBACK edProc(HWND d, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        SendMessageW(d, WM_SETICON, ICON_SMALL, (LPARAM)g_iconWinSm);
        SendMessageW(d, WM_SETICON, ICON_BIG, (LPARAM)g_iconWinBig);
        UINT dpi = dpiOf(d);
        for (HFONT* f : {&g_eGlyph, &g_eSmall, &g_eBold, &g_eFbGlyph}) delFont(*f);
        g_eGlyph = mkFont(glyphFace(), 120, 400, dpi);
        g_eFbGlyph = mkFont(glyphFace(), 100, 400, dpi);
        g_eSmall = mkFont(L"Segoe UI", 80, 400, dpi);
        g_eBold = mkFont(L"Segoe UI", 90, 600, dpi);
        delFont(g_eTitle);
        g_eTitle = mkFont(L"Segoe UI", 120, 600, dpi);
        if (g_eLogo) DestroyIcon(g_eLogo);
        g_eLogo = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_DETAIL), IMAGE_ICON, S(32, dpi), S(32, dpi), 0);
        g_eBanner = S(52, dpi);
        EnumChildWindows(d, shiftChild, g_eBanner);
        RECT wr;
        GetWindowRect(d, &wr);
        SetWindowPos(d, nullptr, wr.left, wr.top - g_eBanner / 2, wr.right - wr.left, wr.bottom - wr.top + g_eBanner,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        edApplyLang(d);
        g_ed = g_profiles;
        g_edSel = g_ed.empty() ? -1 : 0;
        g_flash.clear();
        g_edListChanged = false;
        edFillAdapters(d);
        bool generated = false;
        if (g_ed.empty()) {
            for (auto& a : g_edAds)
                if (a.up && (a.kind == K_ETH || a.kind == K_WLAN)) {
                    Profile p;
                    profileFromAdapter(a, p);
                    p.name = uniqueName(a.friendly + (p.dhcp ? TR(L" – DHCP", L" – DHCP") : TR(L" – statisch", L" – static")));
                    g_ed.push_back(p);
                    generated = true;
                }
            g_edSel = g_ed.empty() ? -1 : 0;
        }
        edRefreshList(d);
        edSelect(d, g_edSel);
        edSetDirty(d, generated);
        g_edListChanged = generated;
        if (generated)
            edFeedback(d, FB_INFO, fmt(TR(L"%u Profile aus den aktuellen Adaptereinstellungen vorbereitet – mit „Speichern“ übernehmen.", L"%u profiles prepared from the current adapter settings – click “Save” to keep them."),
                                       (unsigned)g_ed.size()));
        else
            edFeedback(d, g_ed.empty() ? FB_INFO : FB_NONE,
                       g_ed.empty() ? TR(L"Noch keine Profile – mit „Neu“ das erste anlegen.", L"No profiles yet – create the first one with “New”.") : L"");
        return TRUE;
    }
    case WM_MEASUREITEM: {
        auto* mi = (MEASUREITEMSTRUCT*)lp;
        if (mi->CtlID == IDC_LIST) {
            mi->itemHeight = S(38, dpiOf(d));
            return TRUE;
        }
        return FALSE;
    }
    case WM_DRAWITEM: {
        auto* di = (DRAWITEMSTRUCT*)lp;
        if (di->CtlID == IDC_LIST) edDrawList(d, di);
        else if (di->CtlID == IDC_LANG) edDrawLang(d, di);
        else if (di->CtlID == IDC_FEEDBACK) edDrawFeedback(d, di);
        else return FALSE;
        return TRUE;
    }
    case WM_PAINT: {
        UINT dpi = dpiOf(d);
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(d, &ps);
        FillRect(dc, &ps.rcPaint, GetSysColorBrush(COLOR_3DFACE));
        RECT cr;
        GetClientRect(d, &cr);
        fillRect(dc, RECT{0, 0, cr.right, g_eBanner}, C::Card);
        fillRect(dc, RECT{0, g_eBanner - 1, cr.right, g_eBanner}, C::Border);
        int ls = S(32, dpi);
        if (g_eLogo) DrawIconEx(dc, S(12, dpi), (g_eBanner - ls) / 2, g_eLogo, ls, ls, 0, nullptr, DI_NORMAL);
        drawText(dc, L"MilleIpSwitch", RECT{S(52, dpi), S(4, dpi), cr.right, S(30, dpi)}, C::Text, g_eTitle,
                 DT_LEFT | DT_BOTTOM | DT_SINGLELINE);
        drawText(dc, wstring(L"v") + NS_VERSION, RECT{S(52, dpi), S(30, dpi), cr.right, S(48, dpi)}, C::Faint, g_eSmall,
                 DT_LEFT | DT_TOP | DT_SINGLELINE);
        for (int id : g_flash) {
            HWND c = GetDlgItem(d, id);
            if (!c || !IsWindowVisible(c)) continue;
            RECT r;
            GetWindowRect(c, &r);
            MapWindowPoints(nullptr, d, (POINT*)&r, 2);
            InflateRect(&r, S(4, dpi), S(4, dpi));
            fillRound(dc, r, S(8, dpi), C::GreenBg, C::Green);
        }
        EndPaint(d, &ps);
        return TRUE;
    }
    case WM_CTLCOLOREDIT: {
        if (g_flash.empty()) return FALSE;
        HWND c = (HWND)lp;
        if (inFlash(GetDlgCtrlID(c)) || inFlash(GetDlgCtrlID(GetParent(c)))) {
            static HBRUSH b = CreateSolidBrush(C::GreenBg);
            SetBkColor((HDC)wp, C::GreenBg);
            return (INT_PTR)b;
        }
        return FALSE;
    }
    case WM_TIMER:
        if (wp == 1) {
            KillTimer(d, 1);
            g_flash.clear();
            RedrawWindow(d, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
        }
        return TRUE;

    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);
        switch (id) {
        case IDC_LIST:
            if (code == LBN_SELCHANGE) {
                int n = (int)SendMessageW(GetDlgItem(d, IDC_LIST), LB_GETCURSEL, 0, 0);
                if (n == g_edSel) break;
                if (!edCommit(d)) {
                    SendMessageW(GetDlgItem(d, IDC_LIST), LB_SETCURSEL, g_edSel, 0);
                    break;
                }
                edSelect(d, n);
            }
            break;
        case IDC_NEW:
        case IDC_DUP: {
            if (!edCommit(d)) break;
            Profile p;
            if (id == IDC_DUP && g_edSel >= 0) {
                p = g_ed[g_edSel];
                p.name = uniqueName(p.name + TR(L" (Kopie)", L" (copy)"));
                p.autoOn = false;
            } else {
                p.name = uniqueName(TR(L"Neues Profil", L"New profile"));
                p.dhcp = true;
                if (g_edSel >= 0)
                    p.adapter = g_ed[g_edSel].adapter;
                else {
                    for (auto& a : g_edAds)
                        if (a.up && (a.kind == K_ETH || a.kind == K_WLAN)) { p.adapter = a.friendly; break; }
                    if (p.adapter.empty() && !g_edAds.empty()) p.adapter = g_edAds[0].friendly;
                }
                g_edAds = getAdapters();
                if (const Adapter* a = findAdapter(g_edAds, p.adapter)) profileFromAdapter(*a, p);
            }
            g_ed.insert(g_ed.begin() + (g_edSel + 1), p);
            g_edSel++;
            g_edListChanged = true;
            edRefreshList(d);
            edSelect(d, g_edSel);
            edSetDirty(d, true);
            if (id == IDC_DUP)
                edFeedback(d, FB_INFO, TR(L"Kopie angelegt – anpassen und „Speichern“ klicken.", L"Copy created – adjust and click “Save”."));
            else if (p.adapter.empty())
                edFeedback(d, FB_INFO, TR(L"Neues Profil angelegt – Adapter wählen und „Speichern“ klicken.",
                                          L"New profile created – choose an adapter and click “Save”."));
            else
                edFeedback(d, FB_INFO, TR(L"Neues Profil mit den aktuellen Werten von „", L"New profile with the current values of “") +
                                           p.adapter + TR(L"“ angelegt – anpassen und „Speichern“ klicken.", L"” created – adjust and click “Save”."));
            SetFocus(GetDlgItem(d, IDC_NAME));
            SendDlgItemMessageW(d, IDC_NAME, EM_SETSEL, 0, -1);
            break;
        }
        case IDC_DEL:
            if (g_edSel >= 0 &&
                MessageBoxW(d, (TR(L"Profil „", L"Delete profile “") + g_ed[g_edSel].name + TR(L"“ löschen?", L"”?")).c_str(), L"MilleIpSwitch",
                            MB_YESNO | MB_ICONQUESTION) == IDYES) {
                g_ed.erase(g_ed.begin() + g_edSel);
                if (g_edSel >= (int)g_ed.size()) g_edSel = (int)g_ed.size() - 1;
                g_edListChanged = true;
                edRefreshList(d);
                edSelect(d, g_edSel);
                edSetDirty(d, true);
            }
            break;
        case IDC_UP:
        case IDC_DOWN: {
            int to = g_edSel + (id == IDC_UP ? -1 : 1);
            if (g_edSel < 0 || to < 0 || to >= (int)g_ed.size() || !edCommit(d)) break;
            std::swap(g_ed[g_edSel], g_ed[to]);
            g_edSel = to;
            g_edListChanged = true;
            edRefreshList(d);
            edSelect(d, to);
            edSetDirty(d, true);
            break;
        }
        case IDC_READCUR:
            if (g_edSel >= 0) edReadCurrent(d);
            break;
        case IDC_ADAPTER:
            if (code == CBN_DROPDOWN) edFillAdapters(d);
            if (code == CBN_EDITCHANGE || code == CBN_SELCHANGE) edChanged(d, id);
            break;
        case IDC_DHCP:
        case IDC_STATICIP:
        case IDC_DNSAUTO:
        case IDC_DNSMAN:
            if (code == BN_CLICKED && !g_edLoading) {
                edUpdateEnable(d);
                if (id == IDC_DHCP) EnableWindow(GetDlgItem(d, IDC_DNSAUTO), TRUE);
                edChanged(d, id);
            }
            break;
        case IDC_IP:
            if (code == EN_KILLFOCUS) {
                IN_ADDR a, m;
                if (ipGet(d, IDC_IP, a) == 1 && ipGet(d, IDC_MASK, m) == 0) {
                    BYTE b0 = (BYTE)(ntohl(a.s_addr) >> 24);
                    m.s_addr = htonl(b0 < 128 ? 0xFF000000u : b0 < 192 ? 0xFFFF0000u : 0xFFFFFF00u);
                    ipSet(d, IDC_MASK, &m);
                }
            }
            [[fallthrough]];
        case IDC_NAME:
        case IDC_MASK:
        case IDC_GW:
        case IDC_EXTRA:
        case IDC_DNS1:
        case IDC_DNS2:
            if (code == EN_CHANGE && !g_edLoading) {
                edChanged(d, id);
                if (id == IDC_NAME && g_edSel >= 0) {
                    HWND l = GetDlgItem(d, IDC_LIST);
                    wstring n = dlgText(d, IDC_NAME);
                    SendMessageW(l, LB_DELETESTRING, g_edSel, 0);
                    SendMessageW(l, LB_INSERTSTRING, g_edSel, (LPARAM)n.c_str());
                    SendMessageW(l, LB_SETCURSEL, g_edSel, 0);
                }
            }
            break;
        case IDC_ACTIVATE:
            if (g_edSel >= 0 && edSave(d)) {
                if (startApply(g_profiles[g_edSel]))
                    edFeedback(d, FB_INFO, L"„" + g_profiles[g_edSel].name + TR(L"“ wird aktiviert …", L"” is being activated …"));
                else
                    edFeedback(d, FB_INFO, TR(L"Es läuft gerade schon ein Wechsel.", L"A switch is already in progress."));
            }
            break;
        case IDC_STATUSBTN:
            openStatus();
            break;
        case IDC_LANG:
            langPopup(GetDlgItem(d, IDC_LANG));
            break;
        case IDC_IMPORT:
            edImport(d);
            break;
        case IDC_IPV6BTN:
        case IDC_AUTOBTN:
            if (g_edSel >= 0 && edCommit(d)) {
                Profile tmp = g_ed[g_edSel];
                g_dlgProfile = &tmp;
                if (DialogBoxParamW(g_hInst, MAKEINTRESOURCEW(id == IDC_IPV6BTN ? IDD_IPV6 : IDD_AUTOSWITCH), d,
                                    id == IDC_IPV6BTN ? v6Proc : asProc, 0) == IDOK) {
                    g_ed[g_edSel] = tmp;
                    g_changed.insert(id);
                    edSetDirty(d, true);
                    edExtraButtons(d, &g_ed[g_edSel]);
                    InvalidateRect(GetDlgItem(d, IDC_LIST), nullptr, FALSE);
                }
                g_dlgProfile = nullptr;
            }
            break;
        case IDC_EXPORT:
            edExport(d);
            break;
        case IDC_APPLY:
            edSave(d);
            break;
        case IDOK:
            if (edSave(d)) DestroyWindow(d);
            break;
        case IDCANCEL:
            edClose(d);
            break;
        }
        return TRUE;
    }
    case WM_CLOSE:
        edClose(d);
        return TRUE;
    case WM_DESTROY:
        KillTimer(d, 1);
        g_hDlg = nullptr;
        if (!regFlag(L"CloseHintDone")) {
            trayBalloon(TR(L"MilleIpSwitch läuft weiter", L"MilleIpSwitch keeps running"), TR(L"Profile wechselst du über das Symbol im Infobereich.", L"Switch profiles via the icon in the notification area."), false);
            regSetFlag(L"CloseHintDone");
        }
        return TRUE;
    }
    return FALSE;
}

static void openEditor() {
    if (g_hDlg) {
        ShowWindow(g_hDlg, SW_RESTORE);
        SetForegroundWindow(g_hDlg);
        return;
    }
    reloadProfiles(true);
    g_hDlg = CreateDialogParamW(g_hInst, MAKEINTRESOURCEW(IDD_EDITOR), nullptr, edProc, 0);
    if (g_hDlg) {
        ShowWindow(g_hDlg, SW_SHOW);
        SetForegroundWindow(g_hDlg);
    }
}

static bool exeInProtectedFolder() {
    wstring exe = lower(g_exePath);
    for (const KNOWNFOLDERID* id : {&FOLDERID_ProgramFiles, &FOLDERID_ProgramFilesX86, &FOLDERID_Windows}) {
        PWSTR dir = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(*id, 0, nullptr, &dir))) {
            wstring base = lower(wstring(dir) + L"\\");
            CoTaskMemFree(dir);
            if (exe.compare(0, base.size(), base) == 0) return true;
        }
    }
    return false;
}

static bool setAutostart(bool on, HWND owner) {
    wstring err;
    if (on && !exeInProtectedFolder()) {
        wstring dir = g_exePath.substr(0, g_exePath.find_last_of(L'\\'));
        wstring msg = fmt(TR(L"MilleIpSwitch liegt in einem Ordner, den auch Programme ohne Adminrechte verändern können:\n%ls\n\nDer Autostart läuft mit Adminrechten. Ein anderes Programm könnte die EXE austauschen und so Adminrechte erlangen. Sicherer ist es, die EXE nach „C:\\Program Files\\MilleIpSwitch“ zu verschieben und den Autostart dort einzuschalten.\n\nAutostart trotzdem hier einrichten?", L"MilleIpSwitch is located in a folder that programs without administrator rights can modify:\n%ls\n\nAutostart runs with administrator rights. Another program could replace the exe and gain administrator rights that way. It is safer to move the exe to “C:\\Program Files\\MilleIpSwitch” and enable autostart there.\n\nEnable autostart here anyway?"), dir.c_str());
        if (MessageBoxW(owner, msg.c_str(), L"MilleIpSwitch", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return false;
    }
    if (on) {
        if (!taskInstall(err)) {
            MessageBoxW(owner, (TR(L"Autostart konnte nicht eingerichtet werden.\n", L"Autostart could not be set up.\n") + err).c_str(), L"MilleIpSwitch", MB_ICONERROR);
            return false;
        }
        g_autostart = true;
        trayBalloon(TR(L"Autostart eingerichtet", L"Autostart enabled"),
                    TR(L"MilleIpSwitch startet jetzt bei der Anmeldung – ohne UAC-Abfrage.", L"MilleIpSwitch now starts at sign-in – without a UAC prompt."), false);
    } else {
        taskRemove();
        g_autostart = taskExists();
    }
    return true;
}

static HRESULT CALLBACK firstRunCallback(HWND, UINT, WPARAM, LPARAM, LONG_PTR);

static void firstRunConfig(TASKDIALOGCONFIG& tc) {
    static TASKDIALOG_BUTTON btn[2];
    static TASKDIALOG_BUTTON radio[LANG_COUNT];
    btn[0] = {1001, TR(L"Mit Windows starten\n"
                       L"Startet bei der Anmeldung im Infobereich, ohne UAC-Abfrage.",
                       L"Start with Windows\n"
                       L"Starts in the notification area at sign-in, without a UAC prompt.")};
    btn[1] = {1002, TR(L"Taskleisten-Einstellungen öffnen\n"
                       L"Unter „Andere Taskleistensymbole“ MilleIpSwitch einschalten, damit das Symbol immer sichtbar ist.",
                       L"Open taskbar settings\n"
                       L"Turn on MilleIpSwitch under “Other system tray icons” to keep the icon visible.")};
    for (int l = 0; l < LANG_COUNT; ++l) radio[l] = {2000 + l, langName(l)};
    tc = TASKDIALOGCONFIG{};
    tc.cbSize = sizeof(tc);
    tc.hwndParent = g_hDlg;
    tc.hInstance = g_hInst;
    tc.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
    tc.dwCommonButtons = TDCBF_CLOSE_BUTTON;
    tc.pszWindowTitle = TR(L"Willkommen bei MilleIpSwitch", L"Welcome to MilleIpSwitch");
    tc.pszMainIcon = TD_INFORMATION_ICON;
    tc.pszMainInstruction = TR(L"MilleIpSwitch läuft im Infobereich der Taskleiste", L"MilleIpSwitch runs in the notification area");
    tc.pszContent = TR(
        L"Das Symbol findest du unten rechts neben der Uhr – eventuell hinter dem kleinen Pfeil ˄.\n"
        L"Ein Klick darauf zeigt deine Profile, aktive sind grün markiert.\n\n"
        L"Diese Profilverwaltung öffnest du jederzeit über „Profile verwalten …“ im Symbol-Menü "
        L"oder indem du MilleIpSwitch erneut startest.",
        L"You'll find the icon at the bottom right next to the clock – possibly behind the small arrow ˄.\n"
        L"Clicking it shows your profiles; active ones are marked green.\n\n"
        L"You can open this profile manager at any time via “Manage profiles …” in the icon menu "
        L"or by starting MilleIpSwitch again.");
    tc.pRadioButtons = radio;
    tc.cRadioButtons = LANG_COUNT;
    tc.nDefaultRadioButton = 2000 + g_lang;
    tc.pButtons = btn;
    tc.cButtons = 2;
    tc.nDefaultButton = IDCLOSE;
    tc.pfCallback = firstRunCallback;
}

static HRESULT CALLBACK firstRunCallback(HWND h, UINT n, WPARAM wp, LPARAM, LONG_PTR) {
    if (n == TDN_RADIO_BUTTON_CLICKED) {
        int l = (int)wp - 2000;
        if (l >= 0 && l < LANG_COUNT && l != g_lang) {
            setLanguage(l);
            TASKDIALOGCONFIG tc;
            firstRunConfig(tc);
            SendMessageW(h, TDM_NAVIGATE_PAGE, 0, (LPARAM)&tc);
        }
    }
    return S_OK;
}

static void firstRunHint() {
    TASKDIALOGCONFIG tc;
    firstRunConfig(tc);
    int pressed = 0;
    if (SUCCEEDED(TaskDialogIndirect(&tc, &pressed, nullptr, nullptr))) {
        if (pressed == 1001) setAutostart(true, g_hDlg);
        if (pressed == 1002) ShellExecuteW(nullptr, L"open", L"ms-settings:taskbar", nullptr, nullptr, SW_SHOWNORMAL);
    }
    regSetFlag(L"FirstRunDone");
}

// Tray hover popup
static HWND g_hPop;
static HICON g_iconPop;
static HFONT g_pTitle, g_pBold, g_pText, g_pSmall, g_pGlyph, g_pGlyphSmall;
static UINT g_pDpi = 96;
static bool g_menuOpen = false;
struct PopRow {
    const Adapter* a;
    wstring profile;
    vector<std::pair<wstring, wstring>> rows;
};
static vector<Adapter> g_pAds;
static vector<PopRow> g_pRows;

static int popRowHeight(const PopRow& r) {
    UINT d = g_pDpi;
    return S(38, d) + S(18, d) + S(22, d) + (int)r.rows.size() * S(17, d) + (r.profile.empty() ? 0 : S(20, d)) + S(8, d);
}

static int popHeight() {
    UINT d = g_pDpi;
    int h = S(52, d) + S(6, d);
    for (auto& r : g_pRows) h += popRowHeight(r) + S(6, d);
    if (g_pRows.empty()) h += S(40, d);
    return h + S(4, d);
}

static void popPaint(HDC dc, RECT rc) {
    UINT d = g_pDpi;
    auto s = [d](int v) { return S(v, d); };
    fillRect(dc, rc, C::Card);

    int is = s(32);
    DrawIconEx(dc, s(12), (s(52) - is) / 2, g_iconPop ? g_iconPop : g_iconBig, is, is, 0, nullptr, DI_NORMAL);
    RECT tr{s(52), s(4), rc.right - s(12), s(30)};
    drawText(dc, L"MilleIpSwitch", tr, C::Text, g_pTitle, DT_LEFT | DT_BOTTOM | DT_SINGLELINE);
    drawText(dc, wstring(L"v") + NS_VERSION, RECT{tr.left, s(30), tr.right, s(48)}, C::Faint, g_pSmall,
             DT_LEFT | DT_TOP | DT_SINGLELINE);
    fillRect(dc, RECT{0, s(52) - 1, rc.right, s(52)}, C::Border);

    int y = s(58), x0 = s(8), x1 = rc.right - s(8);
    if (g_pRows.empty())
        drawText(dc, TR(L"Keine Verbindung", L"No connection"), RECT{0, y, rc.right, y + s(40)}, C::Sub, g_pText,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    for (auto& r : g_pRows) {
        const Adapter* a = r.a;
        int h = popRowHeight(r);
        fillRound(dc, RECT{x0, y, x1, y + h}, s(10), C::Card, C::Border);

        RECT hb{x0, y, x1, y + s(38)};
        COLORREF hbg = a->up ? C::AccentBg : C::GreyBg;
        fillRound(dc, hb, s(10), hbg, hbg);
        fillRect(dc, RECT{hb.left, hb.bottom - s(6), hb.right, hb.bottom}, hbg);
        drawGlyph(dc, kindGlyph(a->kind, a->bluetooth), RECT{x0 + s(6), y, x0 + s(30), y + s(38)},
                  a->up ? C::Accent : C::Faint, g_pGlyph);
        fillCircle(dc, x1 - s(14), y + s(19), s(5), a->up ? C::Green : C::Faint);
        int nameL = x0 + s(34), nameR = x1 - s(26);
        wstring off = a->up ? L"" : wstring(L"(") + TR(L"Getrennt", L"Disconnected") + L")";
        int offW = off.empty() ? 0 : textWidth(dc, off, g_pSmall) + s(6);
        int nameT = a->hasMetric ? y + s(3) : y, nameB = a->hasMetric ? y + s(21) : y + s(38);
        drawText(dc, a->friendly, RECT{nameL, nameT, nameR - offW, nameB}, a->up ? C::Text : C::Sub, g_pBold,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (!off.empty()) {
            int nw = std::min(textWidth(dc, a->friendly, g_pBold), nameR - offW - nameL);
            drawText(dc, off, RECT{nameL + nw + s(6), nameT, nameR, nameB}, C::Faint, g_pSmall,
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
        if (a->hasMetric)
            drawText(dc, TR(L"Schnittstellenmetrik", L"Interface metric") + wstring(L" ") + metricText(*a),
                     RECT{nameL, y + s(20), nameR, y + s(36)}, C::Sub, g_pSmall,
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        int bx = x0 + s(10), br = x1 - s(10), by = y + s(40);
        drawText(dc, a->desc, RECT{bx, by, br, by + s(18)}, C::Sub, g_pSmall,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        by += s(18);

        wstring pill = a->dhcp ? L"DHCP" : TR(L"Statisch", L"Static");
        int pw = pillWidth(dc, pill, g_pSmall, d);
        drawPill(dc, pill, RECT{bx, by + s(2), bx + pw, by + s(19)}, a->dhcp ? C::Accent : C::AccentBg,
                 a->dhcp ? C::AccentBg : C::Accent, g_pSmall);
        wstring extra;
        if (a->dhcp && a->hasDhcpSrv) extra = L"Server " + ipStr(a->dhcpSrv);
        if (a->up && a->speed && a->speed != ~0ULL) extra += (extra.empty() ? L"" : L"  ·  ") + speedStr(a->speed);
        drawText(dc, extra, RECT{bx + pw + s(8), by, br, by + s(21)}, C::Sub, g_pSmall,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        by += s(22);

        for (auto& row : r.rows) {
            drawText(dc, row.first, RECT{bx, by, bx + s(54), by + s(17)}, C::Sub, g_pSmall,
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            drawText(dc, row.second, RECT{bx + s(56), by, br, by + s(17)}, a->up ? C::Text : C::Faint, g_pSmall,
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            by += s(17);
        }
        if (!r.profile.empty()) {
            drawCheck(dc, RECT{bx - s(2), by + s(1), bx + s(16), by + s(19)}, g_pGlyphSmall, d);
            drawText(dc, r.profile, RECT{bx + s(20), by + s(1), br, by + s(19)}, C::Green, g_pBold,
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        y += h + s(6);
    }
    HBRUSH b = CreateSolidBrush(C::Border);
    FrameRect(dc, &rc, b);
    DeleteObject(b);
}

static LRESULT CALLBACK popProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bm = CreateCompatibleBitmap(dc, std::max(1L, rc.right), std::max(1L, rc.bottom));
        HGDIOBJ ob = SelectObject(mem, bm);
        popPaint(mem, rc);
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, ob);
        DeleteObject(bm);
        DeleteDC(mem);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_LBUTTONUP:
        ShowWindow(h, SW_HIDE);
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

static void hidePopup() {
    if (g_hPop) ShowWindow(g_hPop, SW_HIDE);
}

static void showPopup() {
    if (g_menuOpen) return;
    if (!g_hPop) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_DROPSHADOW;
        wc.lpfnWndProc = popProc;
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"MilleIpSwitchPopup";
        RegisterClassExW(&wc);
        g_hPop = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE, wc.lpszClassName, L"", WS_POPUP, 0,
                                 0, 10, 10, nullptr, nullptr, g_hInst, nullptr);
        if (!g_hPop) return;
        typedef HRESULT(WINAPI * PFN)(HWND, DWORD, LPCVOID, DWORD);
        if (PFN f = (PFN)(void*)GetProcAddress(LoadLibraryW(L"dwmapi.dll"), "DwmSetWindowAttribute")) {
            DWORD round = 2;
            f(g_hPop, 33, &round, sizeof(round));
        }
    }
    if (!g_hDlg) reloadProfiles(false);
    g_pAds = getAdapters();
    g_pRows.clear();
    for (int pass = 0; pass < 2; ++pass)
        for (auto& a : g_pAds) {
            if (a.up != (pass == 0)) continue;
            if (a.kind != K_ETH && a.kind != K_WLAN && a.kind != K_VPN) continue;
            PopRow r{&a, L"", {}};
            bool first = true;
            for (auto& x : a.ips) {
                r.rows.push_back({first ? L"IP" : L"", cidrStr(x)});
                first = false;
            }
            for (auto& x : a.ips6)
                if (!x.linkLocal && !x.temp) {
                    r.rows.push_back({L"IPv6", ip6Str(x.addr)});
                    break;
                }
            if (!a.gws.empty()) r.rows.push_back({L"Gateway", ipStr(a.gws[0])});
            wstring dns;
            for (size_t i = 0; i < a.dns.size(); ++i) dns += (i ? L", " : L"") + ipStr(a.dns[i]);
            if (!dns.empty()) r.rows.push_back({L"DNS", dns});
            if (!a.mac.empty()) r.rows.push_back({L"MAC", a.mac});
            for (auto& p : g_profiles)
                if (ieq(p.adapter, a.friendly) && isActive(p, g_pAds)) {
                    r.profile = p.name;
                    break;
                }
            g_pRows.push_back(r);
        }

    NOTIFYICONIDENTIFIER id{};
    id.cbSize = sizeof(id);
    id.hWnd = g_hwnd;
    id.uID = 1;
    RECT ir;
    if (FAILED(Shell_NotifyIconGetRect(&id, &ir))) {
        POINT pt;
        GetCursorPos(&pt);
        ir = RECT{pt.x, pt.y, pt.x + 1, pt.y + 1};
    }
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(MonitorFromRect(&ir, MONITOR_DEFAULTTONEAREST), &mi);
    RECT wa = mi.rcWork;

    g_pDpi = dpiOf(g_hPop);
    for (HFONT* f : {&g_pTitle, &g_pBold, &g_pText, &g_pSmall, &g_pGlyph, &g_pGlyphSmall}) delFont(*f);
    g_pTitle = mkFont(L"Segoe UI", 120, 600, g_pDpi);
    g_pBold = mkFont(L"Segoe UI", 90, 600, g_pDpi);
    g_pText = mkFont(L"Segoe UI", 90, 400, g_pDpi);
    g_pSmall = mkFont(L"Segoe UI", 82, 400, g_pDpi);
    g_pGlyph = mkFont(glyphFace(), 100, 400, g_pDpi);
    g_pGlyphSmall = mkFont(glyphFace(), 80, 400, g_pDpi);

    if (g_iconPop) DestroyIcon(g_iconPop);
    g_iconPop = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_DETAIL), IMAGE_ICON, S(32, g_pDpi), S(32, g_pDpi), 0);
    int gap = S(10, g_pDpi), w = S(272, g_pDpi);
    int h = std::min(popHeight(), (int)(wa.bottom - wa.top) - 2 * gap);
    int x = (ir.left + ir.right) / 2 - w / 2, y;
    if (ir.top >= wa.bottom - 2)
        y = wa.bottom - h - gap;
    else if (ir.bottom <= wa.top + 2)
        y = wa.top + gap;
    else {
        y = (ir.top + ir.bottom) / 2 - h / 2;
        x = ir.left >= wa.right - 2 ? wa.right - w - gap : wa.left + gap;
    }
    x = std::max((int)wa.left + gap, std::min(x, (int)wa.right - w - gap));
    y = std::max((int)wa.top + gap, std::min(y, (int)wa.bottom - h - gap));
    SetWindowPos(g_hPop, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(g_hPop, nullptr, FALSE);
}

// Tray menu
struct MenuEntry {
    wchar_t glyph = 0;
    wstring text, detail;
    bool check = false, header = false, popup = false;
    bool section = false, inBox = false, boxLast = false;
    int flag = -1;
    wstring pill;
    bool pillDark = false;
    COLORREF glyphColor = C::Accent, detailColor = C::Faint;
};
static vector<MenuEntry> g_menu;
static HFONT g_mFont, g_mBold, g_mSmall, g_mGlyph, g_mHeader, g_mTitle;
static UINT g_mDpi = 96;
static int g_mPillW = 0, g_mDetailW = 0;

static void menuMeasure(MEASUREITEMSTRUCT* mi) {
    const MenuEntry& e = g_menu[mi->itemData];
    UINT d = g_mDpi;
    HDC dc = GetDC(g_hwnd);
    int w = S(30, d) + textWidth(dc, e.text, e.header ? g_mTitle : e.section ? g_mHeader : g_mBold) + S(16, d);
    if (!e.pill.empty())
        w += g_mPillW + S(8, d) + g_mDetailW + S(8, d);
    else if (!e.detail.empty())
        w += textWidth(dc, e.detail, g_mSmall) + S(10, d);
    if (e.header) w += S(32, d);
    else if (e.glyph || e.flag >= 0) w += S(26, d);
    if (e.popup) w += S(14, d);
    if (e.inBox || e.section) w += S(12, d);
    ReleaseDC(g_hwnd, dc);
    mi->itemWidth = std::max(w, S(190, d));
    mi->itemHeight = S(e.header ? 46 : e.section ? 30 : 26, d) + (e.boxLast ? S(6, d) : 0);
}

static void menuDraw(DRAWITEMSTRUCT* di) {
    const MenuEntry& e = g_menu[di->itemData];
    UINT d = g_mDpi;
    HDC dc = di->hDC;
    RECT r = di->rcItem;
    bool gray = (di->itemState & (ODS_GRAYED | ODS_DISABLED)) != 0;
    bool sel = (di->itemState & ODS_SELECTED) && !gray && !e.header && !e.section;
    fillRect(dc, r, GetSysColor(COLOR_MENU));

    int bx0 = r.left + S(6, d), bx1 = r.right - S(6, d), rad = S(10, d);
    if (e.section || e.inBox) {
        int save = SaveDC(dc);
        IntersectClipRect(dc, r.left, r.top, r.right, r.bottom);
        if (e.section) {
            fillRound(dc, RECT{bx0, r.top + S(4, d), bx1, r.bottom + rad}, rad, C::AccentBg, C::Border);
        } else if (e.boxLast) {
            fillRound(dc, RECT{bx0, r.top - rad, bx1, r.bottom - S(4, d)}, rad, C::Card, C::Border);
        } else {
            fillRect(dc, RECT{bx0, r.top, bx1, r.bottom}, C::Card);
            fillRect(dc, RECT{bx0, r.top, bx0 + 1, r.bottom}, C::Border);
            fillRect(dc, RECT{bx1 - 1, r.top, bx1, r.bottom}, C::Border);
        }
        RestoreDC(dc, save);
        if (e.section) {
            RECT gr{bx0 + S(6, d), r.top + S(4, d), bx0 + S(28, d), r.bottom};
            drawGlyph(dc, e.glyph, gr, C::Accent, g_mGlyph);
            RECT tr{gr.right + S(4, d), r.top + S(4, d), bx1 - S(10, d), r.bottom};
            drawText(dc, e.text, tr, C::Accent, g_mHeader, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            drawText(dc, e.detail, tr, C::Accent, g_mSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            return;
        }
        r.left = bx0 + S(2, d);
        r.right = bx1 - S(2, d);
        if (e.boxLast) r.bottom -= S(6, d);
    }

    if (sel || (e.check && !e.header)) {
        RECT hr = r;
        InflateRect(&hr, -S(3, d), -S(1, d));
        COLORREF bg = sel ? C::Hover : RGB(239, 239, 239);
        fillRound(dc, hr, S(6, d), bg, bg);
    }
    if (e.header) {
        int is = S(32, d);
        RECT gr{r.left + S(8, d), r.top, r.left + S(8, d) + is, r.bottom};
        if (g_iconMenu) DrawIconEx(dc, gr.left, (r.top + r.bottom - is) / 2, g_iconMenu, is, is, 0, nullptr, DI_NORMAL);
        int tx = gr.right + S(8, d);
        drawText(dc, e.text, RECT{tx, r.top + S(5, d), r.right - S(10, d), r.top + S(26, d)}, C::Text, g_mTitle,
                 DT_LEFT | DT_BOTTOM | DT_SINGLELINE);
        drawText(dc, e.detail, RECT{tx, r.top + S(26, d), r.right - S(10, d), r.bottom - S(4, d)}, C::Faint, g_mSmall,
                 DT_LEFT | DT_TOP | DT_SINGLELINE);
        return;
    }
    if (e.check) {
        RECT cr{r.left + S(6, d), r.top, r.left + S(26, d), r.bottom};
        drawCheck(dc, cr, g_mGlyph, d);
    }
    int right = r.right - S(e.popup ? 22 : 8, d);
    RECT gr{right - S(24, d), r.top, right, r.bottom};
    if (e.flag >= 0) {
        int fw = S(18, d), fh = S(12, d), cx = (gr.left + gr.right) / 2, cy = (r.top + r.bottom) / 2;
        drawFlag(dc, e.flag, RECT{cx - fw / 2, cy - fh / 2, cx - fw / 2 + fw, cy - fh / 2 + fh});
    } else if (e.glyph) {
        drawGlyph(dc, e.glyph, gr, gray ? C::Faint : (e.check ? C::Green : e.glyphColor), g_mGlyph);
    }
    int textRight;
    if (!e.pill.empty()) {
        int detL = gr.left - S(8, d) - g_mDetailW;
        drawText(dc, e.detail, RECT{detL, r.top, gr.left - S(8, d), r.bottom}, gray ? C::Faint : e.detailColor, g_mSmall,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        int ph = S(17, d), cy = (r.top + r.bottom) / 2, pl = detL - S(8, d) - g_mPillW;
        drawPill(dc, e.pill, RECT{pl, cy - ph / 2, pl + g_mPillW, cy - ph / 2 + ph}, e.pillDark ? C::AccentBg : C::Accent,
                 e.pillDark ? C::Accent : C::AccentBg, g_mSmall);
        textRight = pl - S(10, d);
    } else {
        int detailW = 0;
        if (!e.detail.empty()) {
            detailW = textWidth(dc, e.detail, g_mSmall);
            RECT dr{gr.left - S(6, d) - detailW, r.top, gr.left - S(6, d), r.bottom};
            drawText(dc, e.detail, dr, gray ? C::Faint : e.detailColor, g_mSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
        textRight = gr.left - S(12, d) - detailW;
    }
    RECT tr{r.left + S(30, d), r.top, textRight, r.bottom};
    drawText(dc, e.text, tr, gray ? C::Faint : C::Text, e.check ? g_mBold : g_mFont,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static void menuFonts() {
    g_mDpi = dpiOf(g_hwnd);
    for (HFONT* f : {&g_mFont, &g_mBold, &g_mSmall, &g_mGlyph, &g_mHeader, &g_mTitle}) delFont(*f);
    g_mFont = mkFont(L"Segoe UI", 90, 400, g_mDpi);
    g_mBold = mkFont(L"Segoe UI", 90, 600, g_mDpi);
    g_mHeader = mkFont(L"Segoe UI", 95, 600, g_mDpi);
    g_mTitle = mkFont(L"Segoe UI", 120, 600, g_mDpi);
    g_mSmall = mkFont(L"Segoe UI", 80, 400, g_mDpi);
    g_mGlyph = mkFont(glyphFace(), 105, 400, g_mDpi);
    if (g_iconMenu) DestroyIcon(g_iconMenu);
    g_iconMenu = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_DETAIL), IMAGE_ICON, S(32, g_mDpi), S(32, g_mDpi), 0);
}

static void edApplyLang(HWND d);
static void setLanguage(int lang);
static void langPopup(HWND anchor);
static void langPopup(HWND anchor) {
    menuFonts();
    g_menu.clear();
    HMENU m = CreatePopupMenu();
    for (int l = 0; l < LANG_COUNT; ++l) {
        MenuEntry le;
        le.text = langName(l);
        le.flag = l;
        le.check = g_lang == l;
        g_menu.push_back(le);
        AppendMenuW(m, MF_STRING | MF_OWNERDRAW, ID_LANG0 + l, (LPCWSTR)(ULONG_PTR)(g_menu.size() - 1));
    }
    RECT r;
    GetWindowRect(anchor, &r);
    UINT cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_BOTTOMALIGN, r.left, r.top, 0, g_hwnd, nullptr);
    DestroyMenu(m);
    if (cmd >= ID_LANG0 && cmd < ID_LANG0 + LANG_COUNT) setLanguage((int)(cmd - ID_LANG0));
}

static void setLanguage(int lang) {
    g_lang = lang;
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\MilleIpSwitch", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) ==
        ERROR_SUCCESS) {
        DWORD v = (DWORD)lang + 1;
        RegSetValueExW(k, L"Language", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
        RegCloseKey(k);
    }
    if (g_hDlg) edApplyLang(g_hDlg);
    if (g_hStatus) {
        SetWindowTextW(g_hStatus, TR(L"MilleIpSwitch – Netzwerkstatus", L"MilleIpSwitch – Network status"));
        InvalidateRect(g_hStatus, nullptr, FALSE);
    }
}

static void showMenu() {
    if (!g_hDlg) reloadProfiles(false);
    vector<Adapter> ads = getAdapters();

    menuFonts();
    g_menu.clear();
    HMENU m = CreatePopupMenu();
    auto addTo = [&](HMENU mm, UINT flags, UINT_PTR id, const MenuEntry& e) {
        g_menu.push_back(e);
        AppendMenuW(mm, flags | MF_OWNERDRAW, id, (LPCWSTR)(ULONG_PTR)(g_menu.size() - 1));
    };
    auto add = [&](UINT flags, UINT id, const MenuEntry& e) { addTo(m, flags, id, e); };

    MenuEntry h;
    h.header = true;
    h.glyph = G::Network;
    h.text = L"MilleIpSwitch";
    h.detail = g_busy ? wstring(TR(L"schaltet um …", L"switching …")) : wstring(L"v") + NS_VERSION;
    add(MF_DISABLED, 0, h);

    MenuEntry sec;
    sec.section = true;
    sec.glyph = 0xE8FD;
    sec.text = TR(L"Profile", L"Profiles");
    int nActive = 0;
    for (auto& p : g_profiles) nActive += isActive(p, ads);
    if (!g_profiles.empty()) sec.detail = fmt(TR(L"%d aktiv", L"%d active"), nActive);
    add(MF_DISABLED, 0, sec);

    if (g_profiles.empty()) {
        MenuEntry e;
        e.text = TR(L"Noch keine Profile – jetzt anlegen …", L"No profiles yet – create one …");
        e.glyph = G::Add;
        e.inBox = e.boxLast = true;
        add(MF_STRING, ID_EDITOR, e);
    }
    for (size_t i = 0; i < g_profiles.size(); ++i) {
        MenuEntry e;
        e.text = g_profiles[i].name;
        if (g_profiles[i].dhcp) {
            e.pill = L"DHCP";
            const Adapter* a = findAdapter(ads, g_profiles[i].adapter);
            if (a && a->dhcp)
                for (auto& x : a->ips)
                    if ((x.addr.s_addr & 0xFFFF) != 0xFEA9) {
                        e.detail = ipStr(x.addr);
                        break;
                    }
        } else {
            e.pill = TR(L"Statisch", L"Static");
            e.pillDark = true;
            if (!g_profiles[i].ips.empty()) e.detail = ipStr(g_profiles[i].ips[0].addr);
        }
        e.glyph = profileGlyph(g_profiles[i], ads);
        e.check = isActive(g_profiles[i], ads);
        e.detailColor = C::Accent;
        e.inBox = true;
        e.boxLast = i + 1 == g_profiles.size();
        add(MF_STRING | (g_busy ? MF_GRAYED : 0), ID_PROFILE0 + (UINT)i, e);
    }
    {
        HDC dc = GetDC(g_hwnd);
        g_mPillW = g_mDetailW = 0;
        for (auto& e : g_menu)
            if (!e.pill.empty()) {
                g_mPillW = std::max(g_mPillW, pillWidth(dc, e.pill, g_mSmall, g_mDpi));
                g_mDetailW = std::max(g_mDetailW, textWidth(dc, e.detail, g_mSmall));
            }
        ReleaseDC(g_hwnd, dc);
    }
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    MenuEntry ed;
    ed.text = TR(L"Profile verwalten …", L"Manage profiles …");
    ed.glyph = G::Settings;
    ed.glyphColor = C::Sub;
    add(MF_STRING, ID_EDITOR, ed);
    MenuEntry st;
    st.text = TR(L"Netzwerkstatus", L"Network status");
    st.glyph = G::Network;
    st.glyphColor = C::Sub;
    add(MF_STRING, ID_STATUS, st);
    MenuEntry as;
    as.text = TR(L"Mit Windows starten", L"Start with Windows");
    as.detail = g_autostart ? TR(L"ohne UAC", L"no UAC") : L"";
    as.check = g_autostart;
    as.glyph = 0xE768;
    as.glyphColor = C::Sub;
    add(MF_STRING, ID_AUTOSTART, as);

    HMENU langMenu = CreatePopupMenu();
    for (int l = 0; l < LANG_COUNT; ++l) {
        MenuEntry le;
        le.text = langName(l);
        le.flag = l;
        le.check = g_lang == l;
        addTo(langMenu, MF_STRING, ID_LANG0 + l, le);
    }
    MenuEntry asw;
    asw.text = L"AutoSwitch";
    asw.glyph = 0xE945;
    asw.glyphColor = C::Sub;
    asw.check = g_autoEnabled;
    int nAuto = 0;
    for (auto& pr : g_profiles) nAuto += pr.autoOn ? 1 : 0;
    if (nAuto) asw.detail = fmt(TR(L"%d Profile", L"%d profiles"), nAuto);
    {
        DWORD now = GetTickCount();
        LONG left = 0;
        for (auto& pr : g_profiles) {
            if (!pr.autoOn) continue;
            auto it = g_autoHold.find(lower(pr.adapter));
            if (it != g_autoHold.end()) left = std::max(left, (LONG)(it->second - now));
        }
        if (g_autoEnabled && left > 0)
            asw.detail = fmt(TR(L"pausiert %d min", L"paused %d min"), (int)((left + 59999) / 60000));
    }
    add(MF_STRING, ID_AUTOSW, asw);

    HMENU updMenu = CreatePopupMenu();
    bool haveUpd = !g_updateTag.empty();
    if (haveUpd) {
        MenuEntry ue;
        ue.text = fmt(TR(L"Download %ls …", L"Download %ls …"), g_updateTag.c_str());
        ue.glyph = 0xE896;
        ue.glyphColor = C::Green;
        addTo(updMenu, MF_STRING, ID_UPD_OPEN, ue);
    }
    MenuEntry uc;
    uc.text = TR(L"Jetzt nach Updates suchen", L"Check for updates now");
    uc.glyph = 0xE895;
    uc.glyphColor = C::Sub;
    addTo(updMenu, MF_STRING, ID_UPD_CHECK, uc);
    MenuEntry ua;
    ua.text = TR(L"Automatisch suchen", L"Check automatically");
    ua.check = g_updateAuto;
    addTo(updMenu, MF_STRING, ID_UPD_AUTO, ua);
    MenuEntry um;
    um.text = TR(L"Updates", L"Updates");
    um.popup = true;
    um.glyph = haveUpd ? 0xE896 : 0xE895;
    um.glyphColor = haveUpd ? C::Green : C::Sub;
    if (haveUpd) {
        um.detail = g_updateTag;
        um.detailColor = C::Green;
    }
    addTo(m, MF_POPUP, (UINT_PTR)updMenu, um);

    MenuEntry lg;
    lg.text = TR(L"Sprache", L"Language");
    lg.flag = g_lang;
    lg.popup = true;
    addTo(m, MF_POPUP, (UINT_PTR)langMenu, lg);
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    MenuEntry ex;
    ex.text = TR(L"Beenden", L"Exit");
    ex.glyph = G::Power;
    ex.glyphColor = C::Red;
    add(MF_STRING, ID_EXIT, ex);

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(g_hwnd);
    hidePopup();
    g_menuOpen = true;
    UINT cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, pt.x, pt.y, 0, g_hwnd, nullptr);
    g_menuOpen = false;
    PostMessageW(g_hwnd, WM_NULL, 0, 0);
    DestroyMenu(m);

    if (cmd >= ID_PROFILE0 && cmd < ID_PROFILE0 + g_profiles.size())
        startApply(g_profiles[cmd - ID_PROFILE0]);
    else if (cmd == ID_EDITOR)
        openEditor();
    else if (cmd == ID_STATUS)
        openStatus();
    else if (cmd == ID_AUTOSTART)
        setAutostart(!g_autostart, nullptr);
    else if (cmd == ID_AUTOSW) {
        g_autoEnabled = !g_autoEnabled;
        regSetDword(L"AutoSwitchOff", g_autoEnabled ? 0 : 1);
        g_autoHold.clear();
    } else if (cmd == ID_UPD_OPEN)
        openUpdatePage();
    else if (cmd == ID_UPD_CHECK)
        startUpdateCheck(true);
    else if (cmd == ID_UPD_AUTO) {
        g_updateAuto = !g_updateAuto;
        regSetDword(L"UpdateCheckOff", g_updateAuto ? 0 : 1);
    }
    else if (cmd >= ID_LANG0 && cmd < ID_LANG0 + LANG_COUNT)
        setLanguage((int)(cmd - ID_LANG0));
    else if (cmd == ID_EXIT) {
        if (g_hDlg) {
            edClose(g_hDlg);
            if (g_hDlg) return;
        }
        if (g_hStatus) DestroyWindow(g_hStatus);
        DestroyWindow(g_hwnd);
    }
}

static void applyFinished(bool ok, const wstring& msg) {
    int kind = g_applyKind;
    g_applyKind = 0;
    if (!(kind == 2 && ok))
        trayBalloon(ok ? (kind == 1 ? TR(L"AutoSwitch: Profil gewechselt", L"AutoSwitch: profile switched")
                                    : TR(L"Profil gewechselt", L"Profile switched"))
                       : TR(L"Fehler beim Umschalten", L"Switching failed"),
                    msg, !ok);
    if (g_hDlg) edApplied(g_hDlg, ok, msg);
    if (g_hStatus) {
        g_stAds = getAdapters(true);
        InvalidateRect(g_hStatus, nullptr, FALSE);
    }
    if (!g_restoreQueue.empty()) {
        Profile next = g_restoreQueue.front();
        g_restoreQueue.erase(g_restoreQueue.begin());
        startApply(next, 2);
    }
}

static void restoreLast() {
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\MilleIpSwitch\\Last", 0, KEY_READ, &k) != ERROR_SUCCESS) return;
    vector<Adapter> ads = getAdapters();
    for (DWORD i = 0;; ++i) {
        wchar_t nm[256], val[256];
        DWORD nl = 256, vl = sizeof(val) - sizeof(wchar_t), type = 0;
        memset(val, 0, sizeof(val));
        if (RegEnumValueW(k, i, nm, &nl, nullptr, &type, (BYTE*)val, &vl) != ERROR_SUCCESS) break;
        if (type != REG_SZ) continue;
        for (auto& p : g_profiles)
            if (p.ip6mode == 2 && ieq(p.adapter, nm) && ieq(p.name, val) && !isActive(p, ads)) {
                g_restoreQueue.push_back(p);
                break;
            }
    }
    RegCloseKey(k);
    if (!g_restoreQueue.empty()) {
        Profile next = g_restoreQueue.front();
        g_restoreQueue.erase(g_restoreQueue.begin());
        if (!startApply(next, 2)) g_restoreQueue.clear();
    }
}

#define TIMER_AUTO 11
#define TIMER_RESTORE 12
#define WM_AUTODONE (WM_APP + 6)

struct AutoJob {
    vector<Profile> ps;
};
struct AutoResult {
    vector<wstring> names, adapters;
    vector<char> match, active, up;
};
struct AutoState {
    wstring wanted;
    int cnt = 0;
    DWORD last = 0;
};
static std::map<wstring, AutoState> g_autoState;
static std::map<wstring, bool> g_wasUp;
static bool g_autoBusy = false;

static DWORD WINAPI autoWorker(LPVOID arg) {
    AutoJob* job = (AutoJob*)arg;
    vector<Adapter> ads = getAdapters();
    auto* r = new AutoResult;
    for (auto& pr : job->ps) {
        bool m = false;
        const Adapter* a = findAdapter(ads, pr.adapter);
        if (a && a->up) {
            m = true;
            if (!pr.autoMac.empty()) {
                wstring mac;
                m = gatewayMac(*a, mac) && mac == pr.autoMac;
            }
            if (m && !pr.autoPing.empty()) m = pingHost(pr.autoPing);
            if (m && !pr.autoArpIp.empty()) {
                IN_ADDR ip{};
                wstring mac;
                m = parseIp(pr.autoArpIp, ip) && arpFresh(*a, ip.s_addr, mac) && (pr.autoArpMac.empty() || mac == pr.autoArpMac);
            }
        }
        r->names.push_back(pr.name);
        r->adapters.push_back(pr.adapter);
        r->match.push_back(m);
        r->active.push_back(isActive(pr, ads));
        r->up.push_back(a && a->up);
    }
    delete job;
    PostMessageW(g_hwnd, WM_AUTODONE, 0, (LPARAM)r);
    return 0;
}

static void autoTick() {
    if (!g_autoEnabled || g_autoBusy || g_busy) return;
    AutoJob* job = new AutoJob;
    for (auto& p : g_profiles)
        if (p.autoOn) job->ps.push_back(p);
    if (job->ps.empty()) {
        delete job;
        return;
    }
    g_autoBusy = true;
    HANDLE h = CreateThread(nullptr, 0, autoWorker, job, 0, nullptr);
    if (h) {
        CloseHandle(h);
    } else {
        delete job;
        g_autoBusy = false;
    }
}

static void autoDecide(AutoResult* r) {
    DWORD now = GetTickCount();
    std::set<wstring> done;
    for (size_t i = 0; i < r->names.size(); ++i) {
        wstring key = lower(r->adapters[i]);
        if (!done.insert(key).second) continue;
        AutoState& st = g_autoState[key];
        bool& was = g_wasUp[key];
        if (r->up[i] && !was) g_autoHold.erase(key);
        was = r->up[i] != 0;
        auto hold = g_autoHold.find(key);
        if (hold != g_autoHold.end() && (LONG)(hold->second - now) > 0) {
            st.wanted.clear();
            st.cnt = 0;
            continue;
        }
        int w = -1;
        for (size_t j = i; j < r->names.size(); ++j)
            if (lower(r->adapters[j]) == key && r->match[j]) {
                w = (int)j;
                break;
            }
        if (w < 0 || r->active[w]) {
            st.wanted.clear();
            st.cnt = 0;
            continue;
        }
        if (st.wanted == r->names[w]) {
            ++st.cnt;
        } else {
            st.wanted = r->names[w];
            st.cnt = 1;
        }
        if (st.cnt < 2 || (st.last && (LONG)(now - st.last) < 30000) || g_busy) continue;
        for (auto& p : g_profiles)
            if (p.name == st.wanted && p.autoOn && startApply(p, 1)) {
                st.last = now;
                st.cnt = 0;
                st.wanted.clear();
                break;
            }
    }
}

static LRESULT CALLBACK wndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_TRAY:
        switch (LOWORD(lp)) {
        case NIN_BALLOONUSERCLICK:
            if (g_balloonUpdate) openUpdatePage();
            g_balloonUpdate = false;
            break;
        case NIN_POPUPOPEN: showPopup(); break;
        case NIN_POPUPCLOSE: hidePopup(); break;
        case NIN_SELECT:
        case NIN_KEYSELECT:
        case WM_CONTEXTMENU:
            hidePopup();
            showMenu();
            break;
        }
        return 0;
    case WM_MEASUREITEM: {
        auto* mi = (MEASUREITEMSTRUCT*)lp;
        if (mi->CtlType == ODT_MENU && mi->itemData < g_menu.size()) {
            menuMeasure(mi);
            return TRUE;
        }
        break;
    }
    case WM_DRAWITEM: {
        auto* di = (DRAWITEMSTRUCT*)lp;
        if (di->CtlType == ODT_MENU && di->itemData < g_menu.size()) {
            menuDraw(di);
            return TRUE;
        }
        break;
    }
    case WM_OPENEDITOR:
        openEditor();
        return 0;
    case WM_TIMER:
        if (wp == TIMER_TRAYCHECK) trayEnsure();
        else if (wp == TIMER_UPDATE) {
            KillTimer(h, TIMER_UPDATE);
            if (g_updateAuto && regFlag(L"LastUpdateCheck") != todayDays()) startUpdateCheck(false);
        } else if (wp == TIMER_AUTO) {
            autoTick();
        } else if (wp == TIMER_RESTORE) {
            KillTimer(h, TIMER_RESTORE);
            restoreLast();
        }
        return 0;
    case WM_AUTODONE: {
        auto* ar = (AutoResult*)lp;
        g_autoBusy = false;
        if (g_autoEnabled) autoDecide(ar);
        delete ar;
        return 0;
    }
    case WM_UPDATEDONE:
        updateFinished((UpdateResult*)lp);
        return 0;
    case WM_FIRSTRUN:
        firstRunHint();
        return 0;
    case WM_APPLYDONE: {
        auto* r = (std::pair<bool, wstring>*)lp;
        applyFinished(r->first, r->second);
        delete r;
        return 0;
    }
    case WM_COPYDATA: {
        auto* cds = (COPYDATASTRUCT*)lp;
        if (cds->dwData != kCopyDataId || !cds->lpData) return 0;
        wstring name((const wchar_t*)cds->lpData, cds->cbData / sizeof(wchar_t));
        while (!name.empty() && name.back() == L'\0') name.pop_back();
        reloadProfiles(false);
        const Profile* p = findProfile(g_profiles, name);
        if (!p) return 100 + RC_UNKNOWN;
        g_autoHold[lower(p->adapter)] = GetTickCount() + 600000;
        if (InterlockedCompareExchange(&g_busy, 1, 0) != 0) return 100 + RC_BUSY;
        wstring m;
        bool ok = applyProfile(*p, m);
        InterlockedExchange(&g_busy, 0);
        applyFinished(ok, m);
        return 100 + (ok ? RC_OK : RC_FAIL);
    }
    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;
    default:
        if (msg == g_taskbarCreated) trayAdd();
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static int runTray(bool showWindow) {
    if (HWND w = FindWindowW(kWndClass, nullptr)) {
        if (showWindow) PostMessageW(w, WM_OPENEDITOR, 0, 0);
        return RC_OK;
    }
    if (!isElevated()) {
        if (HWND w = startTrayViaTask()) {
            if (showWindow)
                for (int i = 0; i < 10 && !PostMessageW(w, WM_OPENEDITOR, 0, 0); ++i) Sleep(200);
            return RC_OK;
        }
        relaunchElevated(showWindow ? L"" : L"--tray", false);
        return RC_OK;
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES | ICC_INTERNET_CLASSES};
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wndProc;
    wc.hInstance = g_hInst;
    wc.lpszClassName = kWndClass;
    RegisterClassExW(&wc);
    g_hwnd = CreateWindowExW(0, kWndClass, L"MilleIpSwitch", 0, 0, 0, 0, 0, nullptr, nullptr, g_hInst, nullptr);
    if (!g_hwnd) return RC_FAIL;

    ChangeWindowMessageFilterEx(g_hwnd, WM_COPYDATA, MSGFLT_ALLOW, nullptr);
    ChangeWindowMessageFilterEx(g_hwnd, WM_OPENEDITOR, MSGFLT_ALLOW, nullptr);
    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    ChangeWindowMessageFilterEx(g_hwnd, g_taskbarCreated, MSGFLT_ALLOW, nullptr);

    g_icon = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                               GetSystemMetrics(SM_CYSMICON), 0);
    g_iconBig = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXICON),
                                  GetSystemMetrics(SM_CYICON), 0);
    g_iconWinSm = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_DETAIL), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                    GetSystemMetrics(SM_CYSMICON), 0);
    g_iconWinBig = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_DETAIL), IMAGE_ICON, GetSystemMetrics(SM_CXICON),
                                     GetSystemMetrics(SM_CYICON), 0);
    if (!g_icon) g_icon = LoadIconW(nullptr, IDI_APPLICATION);
    if (!g_iconBig) g_iconBig = g_icon;
    if (!g_iconWinSm) g_iconWinSm = g_icon;
    if (!g_iconWinBig) g_iconWinBig = g_iconBig;

    reloadProfiles(true);
    trayAdd();
    SetTimer(g_hwnd, TIMER_TRAYCHECK, 3000, nullptr);
    g_autoEnabled = !regFlag(L"AutoSwitchOff");
    SetTimer(g_hwnd, TIMER_AUTO, 5000, nullptr);
    SetTimer(g_hwnd, TIMER_RESTORE, 12000, nullptr);
    g_updateAuto = !regFlag(L"UpdateCheckOff");
    if (g_updateAuto) SetTimer(g_hwnd, TIMER_UPDATE, 15000, nullptr);
    g_autostart = taskExists();
    bool firstRun = !regFlag(L"FirstRunDone");
    if (showWindow || firstRun || g_profiles.empty()) openEditor();
    if (firstRun) PostMessageW(g_hwnd, WM_FIRSTRUN, 0, 0);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        if (g_hDlg && IsDialogMessageW(g_hDlg, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return RC_OK;
}

// Command line
static void report(const wstring& text, bool err) {
    if (g_console)
        out(text + L"\n");
    else if (err)
        MessageBoxW(nullptr, text.c_str(), L"MilleIpSwitch", MB_ICONERROR);
}

static int runSelfTest() {
    vector<wstring> fails;
    auto check = [&](bool cond, const wchar_t* what) {
        if (!cond) fails.push_back(what);
    };
    wstring err;
    Ip4 a4;
    check(parseCidr(L"192.168.1.10/24", a4, err) && a4.prefix == 24 && ipStr(a4.addr) == L"192.168.1.10", L"parseCidr prefix");
    check(parseCidr(L"10.0.0.5/255.0.0.0", a4, err) && a4.prefix == 8, L"parseCidr mask");
    check(!parseCidr(L"10.0.0.5", a4, err), L"parseCidr missing prefix");
    check(!parseCidr(L"300.1.1.1/24", a4, err), L"parseCidr invalid ip");
    Ip6 a6;
    check(parseCidr6(L"2001:db8::10/64", a6, err) && a6.prefix == 64 && ip6Str(a6.addr) == L"2001:db8::10", L"parseCidr6");
    check(parseCidr6(L"fe80::1", a6, err) && a6.prefix == 64, L"parseCidr6 default prefix");
    check(!parseCidr6(L"2001:db8::zz/64", a6, err), L"parseCidr6 invalid");
    check(normMac(L"aa:bb:cc:dd:ee:ff") == L"AA-BB-CC-DD-EE-FF", L"normMac colon");
    check(normMac(L"AABBCCDDEEFF") == L"AA-BB-CC-DD-EE-FF", L"normMac plain");
    check(normMac(L"aa-bb").empty(), L"normMac invalid");
    check(splitList(L" a, b ;c ,, ").size() == 3, L"splitList");
    check(trim(L"  x \r\n") == L"x", L"trim");
    int v[3];
    parseVersion(L"v1.12.3", v);
    check(v[0] == 1 && v[1] == 12 && v[2] == 3, L"parseVersion");
    check(isNewerVersion(L"v99.0.0") && !isNewerVersion(L"v0.0.1") && !isNewerVersion(wstring(L"v") + NS_VERSION), L"isNewerVersion");

    vector<Profile> ps(2);
    ps[0].name = L"Lab \u00dcn\u00efcode";
    ps[0].adapter = L"Ethernet 2";
    parseCidr(L"192.168.10.50/24", a4, err);
    ps[0].ips.push_back(a4);
    parseCidr(L"10.0.0.5/8", a4, err);
    ps[0].ips.push_back(a4);
    ps[0].hasGw = parseIp(L"192.168.10.1", ps[0].gw);
    IN_ADDR d4{};
    parseIp(L"1.1.1.1", d4);
    ps[0].dns.push_back(d4);
    ps[0].ip6mode = 2;
    parseCidr6(L"2001:db8:10::50/64", a6, err);
    ps[0].ips6.push_back(a6);
    ps[0].hasGw6 = parseIp6(L"2001:db8:10::1", ps[0].gw6);
    IN6_ADDR d6{};
    parseIp6(L"2001:4860:4860::8888", d6);
    ps[0].dns6.push_back(d6);
    ps[0].autoOn = true;
    ps[0].autoMac = L"AA-BB-CC-DD-EE-FF";
    ps[0].autoPing = L"192.168.10.1";
    ps[0].autoArpIp = L"192.168.10.15";
    ps[0].autoArpMac = L"11-22-33-44-55-66";
    ps[1].name = L"Office";
    ps[1].adapter = L"Wi-Fi";
    ps[1].dhcp = true;
    ps[1].ip6mode = 1;

    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    wstring file = wstring(tmp) + L"milleipswitch-selftest.ini";
    vector<Profile> in;
    bool io = writeProfilesTo(file, ps, err) && loadProfilesFrom(file, in, err);
    check(io, L"write/load profiles");
    if (io && in.size() == 2) {
        const Profile& q = in[0];
        check(q.name == ps[0].name && q.adapter == ps[0].adapter && !q.dhcp, L"round trip basics");
        check(q.ips.size() == 2 && q.ips[1].prefix == 8 && q.hasGw && ipStr(q.gw) == L"192.168.10.1" && q.dns.size() == 1, L"round trip ipv4");
        check(q.ip6mode == 2 && q.ips6.size() == 1 && q.ips6[0].prefix == 64 && q.hasGw6 && q.dns6.size() == 1, L"round trip ipv6");
        check(q.autoOn && q.autoMac == ps[0].autoMac && q.autoPing == ps[0].autoPing && q.autoArpIp == ps[0].autoArpIp &&
                  q.autoArpMac == ps[0].autoArpMac,
              L"round trip autoswitch");
        check(in[1].dhcp && in[1].ip6mode == 1 && in[1].ips.empty() && !in[1].autoOn, L"round trip dhcp profile");
    } else if (io) {
        check(false, L"round trip count");
    }
    {
        const char bad[] = "[Bad]\r\nadapter = X\r\ndhcp = no\r\n";
        HANDLE f = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
        if (f != INVALID_HANDLE_VALUE) {
            DWORD wr = 0;
            WriteFile(f, bad, sizeof(bad) - 1, &wr, nullptr);
            CloseHandle(f);
            check(!loadProfilesFrom(file, in, err), L"reject static profile without ip");
        }
    }
    DeleteFileW(file.c_str());

    wstring rep = fails.empty() ? L"selftest: OK\n" : L"selftest: FAILED\n";
    for (auto& f : fails) rep += L"  - " + f + L"\n";
    out(rep);
    string utf8(WideCharToMultiByte(CP_UTF8, 0, rep.c_str(), (int)rep.size(), nullptr, 0, nullptr, nullptr), '\0');
    WideCharToMultiByte(CP_UTF8, 0, rep.c_str(), (int)rep.size(), &utf8[0], (int)utf8.size(), nullptr, nullptr);
    HANDLE lf = CreateFileW((wstring(tmp) + L"milleipswitch-selftest.log").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (lf != INVALID_HANDLE_VALUE) {
        DWORD wr = 0;
        WriteFile(lf, utf8.data(), (DWORD)utf8.size(), &wr, nullptr);
        CloseHandle(lf);
    }
    return fails.empty() ? 0 : 1;
}

static int runCli(const wstring& arg) {
    wstring a = lower(arg);
    if (a == L"--help" || a == L"-h" || a == L"/?") {
        out(wstring(L"MilleIpSwitch ") + NS_VERSION +
            TR(L" – schneller Netzwerkprofil-Umschalter\n\n"
               L"  milleipswitch                 Starten (Tray + Profilverwaltung)\n"
               L"  milleipswitch --tray          Nur ins Tray starten (für Autostart)\n"
               L"  milleipswitch \"<Profil>\"      Profil anwenden\n"
               L"  milleipswitch --list          Profile auflisten\n"
               L"  milleipswitch --status        Adapterstatus anzeigen\n"
               L"  milleipswitch --edit          Profilverwaltung öffnen\n"
               L"  milleipswitch --version       Version anzeigen\n\n",
               L" – fast network profile switcher\n\n"
               L"  milleipswitch                 Start (tray + profile manager)\n"
               L"  milleipswitch --tray          Start to tray only (for autostart)\n"
               L"  milleipswitch \"<profile>\"     Apply profile\n"
               L"  milleipswitch --list          List profiles\n"
               L"  milleipswitch --status        Show adapter status\n"
               L"  milleipswitch --edit          Open profile manager\n"
               L"  milleipswitch --version       Show version\n\n") +
            TR(L"Profile: ", L"Profiles: ") + g_iniPath + L"\n");
        return RC_OK;
    }
    if (a == L"--selftest") return runSelfTest();
    if (a == L"--version" || a == L"-v") {
        out(wstring(L"MilleIpSwitch ") + NS_VERSION + L"\n");
        return RC_OK;
    }
    if (a == L"--edit" || a == L"-e") return runTray(true);
    if (a == L"--tray" || a == L"-t") return runTray(false);
    if (a == L"--status" || a == L"-s") {
        out(statusText());
        return RC_OK;
    }

    vector<Profile> ps;
    wstring err;
    if (!loadProfiles(ps, err)) {
        report(err, true);
        return RC_CONFIG;
    }

    if (a == L"--list" || a == L"-l") {
        vector<Adapter> ads = getAdapters();
        for (auto& p : ps)
            out((isActive(p, ads) ? L" * " : L"   ") + p.name + L"   [" + p.adapter + L", " +
                (p.dhcp ? L"DHCP" : TR(L"statisch", L"static")) + L"]\n");
        return RC_OK;
    }

    const Profile* p = findProfile(ps, arg);
    if (!p) {
        report(TR(L"Unbekanntes Profil: ", L"Unknown profile: ") + arg, true);
        return RC_UNKNOWN;
    }

    HWND w = FindWindowW(kWndClass, nullptr);
    if (!w && !isElevated()) w = startTrayViaTask();
    if (w) {
        COPYDATASTRUCT cds{};
        cds.dwData = kCopyDataId;
        cds.cbData = (DWORD)((p->name.size() + 1) * sizeof(wchar_t));
        cds.lpData = (void*)p->name.c_str();
        DWORD_PTR res = 0;
        if (SendMessageTimeoutW(w, WM_COPYDATA, 0, (LPARAM)&cds, SMTO_ABORTIFHUNG, 30000, &res) && res >= 100) {
            int rc = (int)res - 100;
            if (rc == RC_OK) report(p->name + TR(L": OK (über Tray)", L": OK (via tray)"), false);
            else if (rc == RC_BUSY) report(TR(L"Es läuft gerade schon ein Wechsel.", L"A switch is already in progress."), true);
            else report(p->name + TR(L": Fehler (Details in der Tray-Benachrichtigung)", L": error (details in the tray notification)"), true);
            return rc;
        }
    }

    if (!isElevated()) {
        int rc = relaunchElevated(L"\"" + p->name + L"\"", true);
        if (rc == -1) report(TR(L"Adminrechte wurden nicht erteilt.", L"Administrator rights were not granted."), true);
        else if (rc == RC_OK) report(p->name + L": OK", false);
        else report(p->name + TR(L": Fehler", L": error"), true);
        return rc < 0 ? RC_FAIL : rc;
    }

    wstring msg;
    bool ok = applyProfile(*p, msg);
    report(msg, !ok);
    return ok ? RC_OK : RC_FAIL;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    g_hInst = hInst;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    WSADATA wsd;
    WSAStartup(MAKEWORD(2, 2), &wsd);
    {
        DWORD v = 0, sz = sizeof(v);
        RegGetValueW(HKEY_CURRENT_USER, L"Software\\MilleIpSwitch", L"Language", RRF_RT_REG_DWORD, nullptr, &v, &sz);
        g_lang = (v >= 1 && v <= LANG_COUNT) ? (int)v - 1 : 1;
    }
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    g_exePath = exe;
    {
        wstring dir = g_exePath.substr(0, g_exePath.find_last_of(L'\\'));
        g_iniPath = dir + L"\\profiles.ini";
        if (GetFileAttributesW(g_iniPath.c_str()) == INVALID_FILE_ATTRIBUTES &&
            GetFileAttributesW((dir + L"\\portable").c_str()) == INVALID_FILE_ATTRIBUTES) {
            PWSTR appData = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData))) {
                wstring d2 = wstring(appData) + L"\\MilleIpSwitch";
                CoTaskMemFree(appData);
                CreateDirectoryW(d2.c_str(), nullptr);
                g_iniPath = d2 + L"\\profiles.ini";
            }
        }
    }

    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (argc >= 2 && AttachConsole(ATTACH_PARENT_PROCESS)) {
        g_conOut = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, 0, nullptr);
        g_console = g_conOut != INVALID_HANDLE_VALUE;
        out(L"\n");
    }

    int rc = (argc < 2) ? runTray(true) : runCli(argv[1]);
    LocalFree(argv);
    return rc;
}
