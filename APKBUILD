# Contributor: Dan Hunsaker <danhunsaker@gmail.com>
# Maintainer: Dan Hunsaker <danhunsaker@gmail.com>
pkgname=narc
pkgver=0.2.2
pkgrel=0
pkgdesc="Small client utility to watch log files and ship to syslog service, like logvac."
url="https://github.com/mu-box/narc"
arch="all"
license="MPL-2.0"
depends="libuv"
makedepends="libuv-dev autoconf automake bash"
checkdepends=""
install=""
subpackages=""
source=""
srcdir="/tmp/abuild/narc/src"
builddir=""

build() {
    autoreconf -fvi
    ./configure
	make
}

check() {
	# Replace with proper check command(s)
	:
}

package() {
	install -D src/narcd "$pkgdir"/sbin/narcd
    # install -D narc.conf "$pkgdir"/etc/narc/narc.conf
}
