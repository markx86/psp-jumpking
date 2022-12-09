PPSSPP = $(abspath ../ppsspp/bin/PPSSPPSDL)

BUILDDIR = $(abspath ./build)

.PHONY: ppsspp build clean

ppsspp: build
	$(PPSSPP) $</EBOOT.PBP

build: $(BUILDDIR)
	@$(MAKE) -C $<

clean:
	rm -rf $(BUILDDIR)

$(BUILDDIR):
	@mkdir -p $@ && cd $@ && psp-cmake ..
