#!/bin/sh
# Stand-in for checkupdates / pacman used when LINGMO_UPDATER_FAKE is set, so the
# whole flow can be exercised without touching the system.
#   fake-pacman.sh check  <mode>      like `checkupdates`
#   fake-pacman.sh info   <pkg>...    like `pacman -Si`
#   fake-pacman.sh update <mode>      like `pkexec pacman -Syu --noconfirm`
# mode: 1|ok, none, offline, fail, cancel, lock

cmd=$1; shift
mode=${1:-ok}
speed=${LINGMO_UPDATER_FAKE_DELAY:-0.4}

PKGS="firefox 143.0.1-1 -> 143.0.4-1 38.2
glibc 2.42+r17-1 -> 2.42+r33-1 10.1
kwin 6.7.1-1 -> 6.7.2-1 7.9
lingmo-core 2.0.0.r12.abc1234-1 -> 2.0.0.r15.def5678-1 1.3
linux 6.17.3.arch1-1 -> 6.17.5.arch1-1 141.7
mesa 1:25.2.4-1 -> 1:25.2.5-1 12.4
qt6-base 6.11.1-1 -> 6.11.2-1 6.3
systemd 258.1-1 -> 258.2-1 8.8"

case $cmd in
check)
    sleep 1.5
    case $mode in
    none) exit 2 ;;
    offline)
        echo "==> ERROR: Cannot fetch updates" >&2
        exit 1 ;;
    esac
    echo "$PKGS" | cut -d' ' -f1-4
    exit 0 ;;
info)
    for p in "$@"; do
        size=$(echo "$PKGS" | awk -v p="$p" '$1 == p { print $5 }')
        echo "Repository      : core"
        echo "Name            : $p"
        echo "Download Size   : $size MiB"
        echo
    done
    exit 0 ;;
update)
    sleep 1   # "authentication"
    [ "$mode" = cancel ] && { echo "Error executing command as another user: Request dismissed"; exit 126; }
    if [ "$mode" = lock ]; then
        echo ":: Synchronizing package databases..."
        echo "error: failed to synchronize all databases (unable to lock database)"
        exit 1
    fi
    echo ":: Synchronizing package databases..."
    sleep "$speed"
    echo " core downloading..."
    echo " extra downloading..."
    sleep "$speed"
    echo ":: Starting full system upgrade..."
    n=$(echo "$PKGS" | wc -l)
    echo "resolving dependencies..."
    echo "looking for conflicting packages..."
    echo
    echo "Packages ($n) $(echo "$PKGS" | awk '{ printf "%s-%s  ", $1, $4 }')"
    echo
    echo "Total Download Size:   226.58 MiB"
    echo "Total Installed Size:  812.40 MiB"
    echo "Net Upgrade Size:        3.12 MiB"
    echo
    echo ":: Proceed with installation? [Y/n] "
    echo ":: Retrieving packages..."
    while read -r name old arrow new size; do
        sleep "$speed"
        echo " $name-$new-x86_64 downloading..."
        if [ "$mode" = fail ] && [ "$name" = linux ]; then
            echo "error: failed retrieving file '$name-$new-x86_64.pkg.tar.zst' from geo.mirror.pkgbuild.com : Could not resolve host: geo.mirror.pkgbuild.com"
            echo "warning: failed to retrieve some files"
            echo "error: failed to commit transaction (failed to retrieve some files)"
            echo "Errors occurred, no packages were upgraded."
            exit 1
        fi
    done <<EOT
$PKGS
EOT
    i=0
    for step in "checking keys in keyring" "checking package integrity" "loading package files" \
                "checking for file conflicts" "checking available disk space"; do
        i=0
        while [ $i -lt "$n" ]; do i=$((i + 1)); echo "($i/$n) $step"; done
        sleep "$speed"
    done
    echo ":: Processing package changes..."
    i=0
    while read -r name old arrow new size; do
        i=$((i + 1))
        echo "($i/$n) upgrading $name"
        sleep "$speed"
    done <<EOT
$PKGS
EOT
    echo ":: Running post-transaction hooks..."
    echo "(1/4) Reloading system manager configuration..."
    sleep "$speed"
    echo "(2/4) Updating module dependencies..."
    echo "(3/4) Updating linux initcpios..."
    sleep "$speed"
    echo "==> Building image from preset: /etc/mkinitcpio.d/linux.preset: 'default'"
    echo "==> Image generation successful"
    echo "(4/4) Updating icon theme caches..."
    exit 0 ;;
esac
exit 1
