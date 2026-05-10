/*
 * AmigaOS m68k errno definition.
 *
 * Linking with `-noixemul` (which we always do — see package.yml) means
 * the m68k-amigaos sysroot's libc isn't on the link line, so any code
 * that references `errno` ends up with an unresolved symbol. Lots of
 * downstream code does — AmiSSL most prominently, but also any C code
 * that touches sockets or files via the BSD-shaped APIs that libnix
 * forwards.
 *
 * We define `errno` here in am-lang-core so every AmLang app on
 * amigaos has exactly one copy, regardless of which other libraries
 * (am-ssl, am-crypto, ...) get pulled in. Apps that never reference
 * errno still pay a 4-byte tax in .bss, which is fine.
 */

int errno = 0;
