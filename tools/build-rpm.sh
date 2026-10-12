#!/bin/sh
# Build the RPMs and the source RPM from the current commit, into dist/. Run inside the build container:
#   podman run --rm --security-opt label=disable -v "$PWD":/src -w /src saver-build tools/build-rpm.sh
set -e
version=$(sed -n "s/^Version: *//p" packaging/screensaver-service.spec)
top=$PWD/build-rpm
rm -rf "$top" && mkdir -p "$top/SOURCES" dist
git archive --format=tar.gz --prefix="screensaver-service-$version/" -o "$top/SOURCES/screensaver-service-$version.tar.gz" HEAD
rpmbuild -ba --define "_topdir $top" packaging/screensaver-service.spec
cp "$top"/RPMS/*/screensaver-service-*"$version"-*.rpm "$top"/SRPMS/*.src.rpm dist/
rm -rf "$top"
