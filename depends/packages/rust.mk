package=rust
$(package)_version=1.36.0
$(package)_download_path=https://static.rust-lang.org/dist
$(package)_file_name_linux=rust-$($(package)_version)-x86_64-unknown-linux-gnu.tar.gz
$(package)_sha256_hash_linux=15e592ec52f14a0586dcebc87a957e472c4544e07359314f6354e2b8bd284c55
$(package)_file_name_darwin=rust-$($(package)_version)-x86_64-apple-darwin.tar.gz
$(package)_sha256_hash_darwin=91f151ec7e24f5b0645948d439fc25172ec4012f0584dd16c3fb1acb709aa325
$(package)_file_name_mingw32=rust-$($(package)_version)-x86_64-pc-windows-gnu.tar.gz
$(package)_sha256_hash_mingw32=55c07426f791c51c8a2b6934b35784175c4abb4e03f123f3e847109c4dc1ad8b

ifeq ($(host_os),mingw32)
$(package)_build_subdir=buildos
$(package)_extra_sources = $($(package)_file_name_$(build_os))

define $(package)_fetch_cmds
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_download_file),$($(package)_file_name),$($(package)_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_file_name_$(build_os)),$($(package)_file_name_$(build_os)),$($(package)_sha256_hash_$(build_os)))
endef

define $(package)_extract_cmds
  mkdir -p $($(package)_extract_dir) && \
  echo "$($(package)_sha256_hash)  $($(package)_source)" > $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_sha256_hash_$(build_os))  $($(package)_source_dir)/$($(package)_file_name_$(build_os))" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  $(build_SHA256SUM) -c $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  mkdir mingw32 && \
  tar --strip-components=1 -xf $($(package)_source) -C mingw32 && \
  mkdir buildos && \
  tar --strip-components=1 -xf $($(package)_source_dir)/$($(package)_file_name_$(build_os)) -C buildos
endef

define $(package)_stage_cmds
  ./install.sh --destdir=$($(package)_staging_dir) --prefix=$(host_prefix)/native --disable-ldconfig && \
  cp -r ../mingw32/rust-std-x86_64-pc-windows-gnu/lib/rustlib/x86_64-pc-windows-gnu $($(package)_staging_dir)$(host_prefix)/native/lib/rustlib
endef
else

define $(package)_stage_cmds
  ./install.sh --destdir=$($(package)_staging_dir) --prefix=$(host_prefix)/native --disable-ldconfig
endef
endif
