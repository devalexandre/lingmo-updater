# Lingmo Updater

Keeps an Arch Linux based Lingmo system up to date without a terminal.

- **Session daemon** (`lingmo-updater --background`, started from
  `/etc/xdg/autostart` in Lingmo sessions): about two minutes after login and
  then every 3 hours it runs `checkupdates` (no root needed). When updates are
  available it shows a tray icon ("N updates available") and sends one desktop
  notification with an *Update* action.
- **Window** ("Updates" in the launcher, the tray icon, the notification, or
  running `lingmo-updater` again): lists the pending packages with
  current → new version and the total download size, and runs
  `pkexec pacman -Syu --noconfirm` (the polkit agent asks for the password),
  streaming pacman's output into a collapsible *Details* view. It suggests a
  restart when the kernel, systemd, glibc or lingmo-core were updated.

Only one instance runs per session; it is reachable on D-Bus as
`com.lingmo.Updater` (`/Updater`, methods `show` and `checkNow`).

## Build

```sh
cmake -B build -G Ninja -DCMAKE_INSTALL_PREFIX=/usr
ninja -C build
DESTDIR=/tmp/stage ninja -C build install
```

Dependencies: Qt 6 (Core, Gui, Widgets, Qml, Quick, DBus, LinguistTools),
KF6 WindowSystem, LingmoUI; at runtime `pacman-contrib` (checkupdates),
`fakeroot` and `polkit` with a polkit agent.

## Testing without touching the system

Set `LINGMO_UPDATER_FAKE` to replace both `checkupdates` and
`pkexec pacman -Syu` with `fake/fake-pacman.sh`:

| value           | behaviour                                          |
|-----------------|----------------------------------------------------|
| `1` / `ok`      | 8 fake updates, the update succeeds                |
| `fail`          | the download fails (network error)                 |
| `cancel`        | the authorization dialog is dismissed (exit 126)   |
| `lock`          | the pacman database is locked                      |
| `offline`       | checking for updates fails                         |
| `none`          | the system is up to date                           |

`LINGMO_UPDATER_DELAY` sets the seconds before the first background check
(default 120) and `LINGMO_UPDATER_FAKE_DELAY` the pause between fake pacman
steps.

```sh
LINGMO_UPDATER_FAKE=1 LINGMO_UPDATER_DELAY=3 lingmo-updater --background
```

## License

GPL-3.0-or-later.
