include ../../Makefile.include
DEPS= ../../Makefile.include Makefile

LIBNAME=tz
VERSION=2019c
ARCHIVE=$(VERSION).tar.gz
BASE_URL := https://github.com/eggert/tz/archive/

all: .installed-$(PLATFORM)

$(TARBALLS_LOCATION)/$(ARCHIVE):
	cd $(TARBALLS_LOCATION); $(RETRIEVE_TOOL) $(RETRIEVE_TOOL_FLAGS) $(BASE_URL)/$(ARCHIVE)

$(PLATFORM): $(TARBALLS_LOCATION)/$(ARCHIVE) $(DEPS)
	rm -rf $(PLATFORM); mkdir -p $(PLATFORM)
	cd $(PLATFORM); $(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)

.installed-$(PLATFORM): $(PLATFORM)
	$(MAKE) -C $(PLATFORM) TZDIR="$(PREFIX)/usr/share/zoneinfo" REDO=posix_only TOPDIR="$(PREFIX)" install
	touch $@

clean:
	$(MAKE) -C $(PLATFORM) clean
	rm -f .installed-$(PLATFORM)

distclean::
	rm -rf $(PLATFORM) .installed-$(PLATFORM)
