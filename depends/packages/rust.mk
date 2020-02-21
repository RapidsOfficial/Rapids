package=rust
$(package)_version=1.36.0
$(package)_download_path=https://static.rust-lang.org/dist
$(package)_file_name_linux=rust-$($(package)_version)-x86_64-unknown-linux-gnu.tar.gz
$(package)_sha256_hash_linux=15e592ec52f14a0586dcebc87a957e472c4544e07359314f6354e2b8bd284c55
$(package)_file_name_darwin=rust-$($(package)_version)-x86_64-apple-darwin.tar.gz
$(package)_sha256_hash_darwin=91f151ec7e24f5b0645948d439fc25172ec4012f0584dd16c3fb1acb709aa325

define $(package)_stage_cmds
  ./install.sh --destdir=$($(package)_staging_dir) --prefix=$(host_prefix)/native --disable-ldconfig
endef
