PPSSPP = $(abspath ../ppsspp/bin/PPSSPPSDL)
LOCALCONFIG = $(abspath ./.config/ppsspp/PSP/GAME/psp-jumpking)

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
	@cd $@ && ln -s ../../../../assets
	@cd $@ && ln -s ../../../../build/EBOOT.PBP
