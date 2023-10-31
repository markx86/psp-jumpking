BUILDDIR = $(abspath ./build)

.PHONY: build clean

build: $(BUILDDIR)
	@$(MAKE) -C $<

clean:
	rm -rf $(BUILDDIR)

fmt:
	clang-format -i src/*

$(BUILDDIR):
	@mkdir -p $@ && cd $@ && psp-cmake ..
	@cd $@ && ln -s ../assets
