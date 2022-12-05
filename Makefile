PPSSPP = $(abspath ../ppsspp/bin/PPSSPPSDL)

BUILDDIR = $(abspath ./build)

.PHONY: run clean

run: $(BUILDDIR)
	@$(MAKE) -C $<
	$(PPSSPP) $</EBOOT.PBP

clean:
	rm -rf $(BUILDDIR)

$(BUILDDIR):
	@mkdir -p $@ && cd $@ && psp-cmake ..
