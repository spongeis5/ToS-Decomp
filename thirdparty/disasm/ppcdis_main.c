/* ppcdis -- disassemble a range of the guest image, including Xenon VMX128.
 *
 * Capstone has no VMX128, so 1,330 functions in this title are unreadable by
 * every other tool here.  binutils' PowerPC disassembler DOES carry the
 * extension (the vaddfp128 / vpkd3d128 / vupkd3d128 family), gated behind the
 * PPC_OPCODE_VMX_128 dialect bit that the "cell" option sets.
 *
 * Using the real opcode table rather than reimplementing the encoding: the
 * VX128 forms use six different field masks (0x3d0, 0x7f3, 0x210, 0x7f0,
 * 0x730, 0x10) and a transcription is exactly the kind of thing that decodes
 * into something plausible while being wrong.
 *
 *     ppcdis <image> <imagebase-hex> <startva-hex> <count>
 *
 * prints one line per instruction:  ADDR  WORD  TEXT
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "dis-asm.h"

extern int print_insn_big_powerpc(bfd_vma, struct disassemble_info *);

/* PPC_OPCODE_* are defined inside ppc-dis.c itself, not in a header, so the
   values are repeated here.  Read from ppc-dis.c lines 58..113; if that file
   is ever updated these must be re-checked against it. */
#define D_PPC       0x00000001   /* PPC_OPCODE_PPC     */
#define D_64        0x00000010   /* PPC_OPCODE_64      */
#define D_601       0x00000020   /* PPC_OPCODE_601     */
#define D_COMMON    0x00000040   /* PPC_OPCODE_COMMON  */
#define D_ALTIVEC   0x00000200   /* PPC_OPCODE_ALTIVEC */
#define D_403       0x00000400   /* PPC_OPCODE_403     */
#define D_CLASSIC   0x00010000   /* PPC_OPCODE_CLASSIC */
#define D_VMX128    0x01000000   /* PPC_OPCODE_VMX_128 */

static char g_buf[512];
static int  g_len;

static int collect(FILE *stream, const char *fmt, ...)
{
    va_list ap;
    int n;
    (void)stream;
    va_start(ap, fmt);
    n = vsnprintf(g_buf + g_len, (size_t)(sizeof(g_buf) - g_len), fmt, ap);
    va_end(ap);
    if (n > 0) {
        g_len += n;
        if (g_len > (int)sizeof(g_buf) - 1)
            g_len = (int)sizeof(g_buf) - 1;
    }
    return n;
}

int main(int argc, char **argv)
{
    FILE *f;
    long size;
    unsigned char *img;
    unsigned long base, start, count, i;
    struct disassemble_info info;

    if (argc < 5) {
        fprintf(stderr,
            "usage: ppcdis <image> <imagebase-hex> <startva-hex> <count>\n");
        return 1;
    }
    base  = strtoul(argv[2], NULL, 16);
    start = strtoul(argv[3], NULL, 16);
    count = strtoul(argv[4], NULL, 10);

    f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    img = (unsigned char *)malloc((size_t)size);
    if (!img) { fprintf(stderr, "out of memory\n"); return 1; }
    if (fread(img, 1, (size_t)size, f) != (size_t)size) {
        fprintf(stderr, "short read\n"); return 1;
    }
    fclose(f);

    memset(&info, 0, sizeof(info));
    info.arch              = bfd_arch_powerpc;
    info.mach              = bfd_mach_ppc_620;
    info.endian            = BFD_ENDIAN_BIG;
    info.read_memory_func  = buffer_read_memory;
    info.memory_error_func = perror_memory;
    info.print_address_func = generic_print_address;
    info.symbol_at_address_func = generic_symbol_at_address;
    info.fprintf_func      = (fprintf_function)collect;
    info.stream            = NULL;
    info.buffer            = img;
    info.buffer_vma        = base;
    info.buffer_length     = (unsigned int)size;
    info.disassembler_options = "cell";
    /* powerpc_dialect() is static; print_insn_big_powerpc reads the dialect
       straight out of private_data, so it is set here explicitly.  "cell" is
       what turns PPC_OPCODE_VMX_128 on in that function. */
    info.private_data = (char *)0 + (D_PPC | D_64 | D_403 | D_601 | D_CLASSIC
                                     | D_COMMON | D_ALTIVEC | D_VMX128);

    for (i = 0; i < count; i++) {
        bfd_vma va = (bfd_vma)(start + i * 4);
        unsigned long w;
        int n;
        if (va < base || va + 4 > base + (unsigned long)size) {
            printf("%08lX  --------  <not backed>\n", (unsigned long)va);
            continue;
        }
        w = ((unsigned long)img[va - base] << 24)
          | ((unsigned long)img[va - base + 1] << 16)
          | ((unsigned long)img[va - base + 2] << 8)
          | ((unsigned long)img[va - base + 3]);
        g_len = 0;
        g_buf[0] = 0;
        n = print_insn_big_powerpc(va, &info);
        if (n <= 0)
            snprintf(g_buf, sizeof(g_buf), "<undecoded>");
        printf("%08lX  %08lX  %s\n", (unsigned long)va, w, g_buf);
    }
    free(img);
    return 0;
}
