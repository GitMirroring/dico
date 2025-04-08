# SYNOPSIS
#   make release
#
# DESRIPTION
#   Publishes a new release.  Release type and destination are
#   determined automatically.
#
# PREREQUISITES
#   Main Makefile from the project top-level source directory must be
#   included before this one.
#
#   Grayupload (https://puszcza.gnu.org.ua/projects/grayupload) must
#   be installed.
#
#   Release announce generator needs the following programs: md5sum,
#   sha1sum, sha256sum.

release:
	@$(MAKE) -f $(firstword $(MAKEFILE_LIST)) release_archives

UPLOAD_OPTIONS =

release_archives: release_announce .grayupload
	@echo "Releasing $(DIST_ARCHIVES)"; \
        grayupload $(UPLOAD_OPTIONS) $(DIST_ARCHIVES)

release_announce: $(DIST_ARCHIVES)
	@maint/mkann -p $(PACKAGE) -v $(VERSION) -o $(PACKAGE)-$(VERSION).ann $(DIST_ARCHIVES)
	@echo "Announce template: $(PACKAGE)-$(VERSION).ann"

%.tar.gz:
	test -f $@ || $(MAKE) distcheck

