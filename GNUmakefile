# Maintainer's make file for dico.
# 
ifneq (,$(wildcard Makefile))
 include Makefile
 include maint/release.mk
configure.ac: configure.boot
	./bootstrap
else
$(if $(MAKECMDGOALS),$(MAKECMDGOALS),all):
	$(MAKE) -f maint/bootstrap.mk
	$(MAKE) $(MAKECMDGOALS)
endif
