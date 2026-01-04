# AUR Packaging for KDECodexBar

This directory contains the `PKGBUILD` for creating an Arch Linux package.

## Building Locally

To build the package from the local source (useful for testing):

1.  Navigate to this directory:
    ```bash
    cd packaging/aur
    ```

2.  Run `makepkg`:
    ```bash
    makepkg -si
    ```
    This will compile the application and install dependencies.

## Publishing to AUR

1.  Create an account on [aur.archlinux.org](https://aur.archlinux.org/).
2.  Setup SSH keys for AUR.
3.  Clone your package repo:
    ```bash
    git clone ssh://aur@aur.archlinux.org/kdecodexbar.git
    ```
4.  Copy the `PKGBUILD` and `.SRCINFO` into that repo.
    *   Note: You must verify the `source` array point to a valid public URL (e.g., GitHub release or git tag) before publishing. Local `dir://` sources won't work for others.
5.  Generate `.SRCINFO`:
    ```bash
    makepkg --printsrcinfo > .SRCINFO
    ```
6.  Commit and push:
    ```bash
    git add PKGBUILD .SRCINFO
    git commit -m "Initial release"
    git push origin master
    ```
