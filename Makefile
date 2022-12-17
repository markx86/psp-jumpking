PPSSPP = $(abspath ../ppsspp/bin/PPSSPPSDL)
LOCALCONFIG = $(abspath ./.config)

BUILDDIR = $(abspath ./build)

.PHONY: ppsspp build clean

build: $(BUILDDIR)
	@$(MAKE) -C $<

ppsspp: build $(LOCALCONFIG)
	env XDG_CONFIG_HOME="$(LOCALCONFIG)" $(PPSSPP) $(BUILDDIR)/EBOOT.PBP

clean:
	rm -rf $(BUILDDIR)
	rm -rf $(LOCALCONFIG)

$(BUILDDIR):
	@mkdir -p $@ && cd $@ && psp-cmake ..

$(LOCALCONFIG):
	@mkdir -p $@
