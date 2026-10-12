# Getting it into distributions

Where things stand, and the steps that remain. The steps marked **you** need an
account in your own name; nobody else can do them for you.

## State of the package

- `packaging/screensaver-service.spec` follows the Fedora packaging guidelines:
  Meson macros, systemd user-unit scriptlets, `desktop-file-validate` and
  `appstream-util validate-relax` in `%check`, licence file, manual pages.
- `rpmlint` on the spec, the source RPM and both binary RPMs reports no errors
  (one warning: the `-kcm` subpackage has no documentation of its own).
- `Source0` is the GitHub tag tarball, so the source RPM can be rebuilt from the
  public repository.

## 1. A Copr repository (you, about ten minutes)

Copr is Fedora's build service for personal repositories. It gives people a
one-line install today, and reviewers something to try.

1. Make a Fedora account at https://accounts.fedoraproject.org and sign the
   Fedora Project Contributor Agreement there.
2. At https://copr.fedorainfracloud.org choose *New Project*, name it
   `screensaver-service`, and tick the Fedora releases to build for.
3. *Builds*, *New Build*, *Upload*: upload `screensaver-service-0.2.1-1.fc44.src.rpm`
   from the GitHub release.

People can then install it with:

    sudo dnf copr enable YOURNAME/screensaver-service
    sudo dnf install screensaver-service screensaver-service-kcm

## 2. Fedora package review (you, then weeks of waiting)

1. Read https://docs.fedoraproject.org/en-US/package-maintainers/Joining_the_Package_Maintainers/
2. File a review request at
   https://bugzilla.redhat.com/enter_bug.cgi?product=Fedora&component=Package%20Review
   using the text below. Because this would be your first package, also set the
   bug to block `FE-NEEDSPONSOR`: a sponsor has to vouch for a new packager.
3. A reviewer runs `fedora-review` and asks for changes. Answer in the bug.
4. Once approved and sponsored, request the repository and build it. The Joining
   page above walks through that.

Finding a sponsor is the slow part. It helps to do an informal review of someone
else's package in the meantime, and to post an introduction on the `devel`
mailing list.

### Review request text

    Summary: Review Request: screensaver-service - Screen savers for Wayland desktops

    Spec URL: https://raw.githubusercontent.com/mottipython/screensaver-service/v0.2.1/packaging/screensaver-service.spec
    SRPM URL: https://github.com/mottipython/screensaver-service/releases/download/v0.2.1/screensaver-service-0.2.1-1.fc44.src.rpm

    Description:
    A small service that starts a screen saver when a Wayland session has been
    idle, and a classic settings window to choose one.

    Idleness is reported by the compositor (ext-idle-notify-v1), so programs
    that inhibit idling, such as video players, hold the screen saver off. The
    service runs X11 screen savers through Xwayland and standalone full-screen
    ones directly. It does not lock the screen; that is left to the desktop.

    Fedora Account System Username: YOURNAME

    This is my first package and I need a sponsor.

    It lets the screen savers already in Fedora (xscreensaver-extras-gss,
    xscreensaver-gl-extras-gss) be used in a Wayland session, where at present
    nothing runs them. A -kcm subpackage adds a page to KDE System Settings.

## 3. Nobara (you)

Nobara is maintained by one person and takes suggestions through its Discord and
its issue tracker. A working Copr repository is the best argument. Suggested text:

    Screen savers on Plasma Wayland: screensaver-service

    Plasma on Wayland has no screen saver, and the xscreensaver packages Fedora
    ships have nothing to run them. screensaver-service is a small idle service
    plus a classic chooser, with a page under System Settings, Display & Monitor.
    It uses the compositor's idle protocol, so games and video hold it off, and
    it never locks the screen.

    Repository: https://github.com/mottipython/screensaver-service
    Copr: https://copr.fedorainfracloud.org/coprs/YOURNAME/screensaver-service/

    Would you consider adding it to the Nobara repositories?

## 4. In Plasma itself

Being part of KDE's own System Settings would be KDE's decision, not a
distribution's. KDE removed screen savers on purpose when it moved to Wayland, so
this is a long shot; a separately installed page is what is realistic.

## Before a wider release

- Each screen saver should ship its own entry in
  `/usr/share/screensaver-service/savers/` in its own package. `contrib/anathem/`
  holds those for the Anathem screen savers until their packages do.
- X11 screen savers have been tested on an off-screen X server, not yet on a
  KWin session.
