#!/bin/bash

# DLS specific http proxy
export http_proxy=wwwcache.rl.ac.uk:8080

# Variables telling us where to get things
HERE="$(dirname "$0")"
VERSION="0.1.13"
SOURCE="http://ftp.gnome.org/pub/GNOME/sources/aravis/0.1/aravis-${VERSION}.tar.xz"

# fail if we can't do anything
set -e

# remove dir if it already exists
rm -rf ${HERE}/aravis

# Now get the the zip file
wget -P "${HERE}" $SOURCE

# untar the source
echo "Untarring source..."
tar Jxvf "${HERE}/$(basename $SOURCE)" -C "${HERE}"

# remove the archives
rm "${HERE}/$(basename $SOURCE)"

# move the untarred archive to the correct name
mv "${HERE}/aravis-${VERSION}" "${HERE}/vendor/aravis"

# patch aravis
patch -d "${HERE}" -p0 < "${HERE}/vendor/locate-by-ip.patch"

echo "You can now type make to build this module"
