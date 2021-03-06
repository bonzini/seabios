#ifndef __STDVGA_H
#define __STDVGA_H

#include "types.h" // u8

// VGA registers
#define VGAREG_ACTL_ADDRESS            0x3c0
#define VGAREG_ACTL_WRITE_DATA         0x3c0
#define VGAREG_ACTL_READ_DATA          0x3c1

#define VGAREG_INPUT_STATUS            0x3c2
#define VGAREG_WRITE_MISC_OUTPUT       0x3c2
#define VGAREG_VIDEO_ENABLE            0x3c3
#define VGAREG_SEQU_ADDRESS            0x3c4
#define VGAREG_SEQU_DATA               0x3c5

#define VGAREG_PEL_MASK                0x3c6
#define VGAREG_DAC_STATE               0x3c7
#define VGAREG_DAC_READ_ADDRESS        0x3c7
#define VGAREG_DAC_WRITE_ADDRESS       0x3c8
#define VGAREG_DAC_DATA                0x3c9

#define VGAREG_READ_FEATURE_CTL        0x3ca
#define VGAREG_READ_MISC_OUTPUT        0x3cc

#define VGAREG_GRDC_ADDRESS            0x3ce
#define VGAREG_GRDC_DATA               0x3cf

#define VGAREG_MDA_CRTC_ADDRESS        0x3b4
#define VGAREG_MDA_CRTC_DATA           0x3b5
#define VGAREG_VGA_CRTC_ADDRESS        0x3d4
#define VGAREG_VGA_CRTC_DATA           0x3d5

#define VGAREG_MDA_WRITE_FEATURE_CTL   0x3ba
#define VGAREG_VGA_WRITE_FEATURE_CTL   0x3da
#define VGAREG_ACTL_RESET              0x3da

#define VGAREG_MDA_MODECTL             0x3b8
#define VGAREG_CGA_MODECTL             0x3d8
#define VGAREG_CGA_PALETTE             0x3d9

/* Video memory */
#define SEG_GRAPH 0xA000
#define SEG_CTEXT 0xB800
#define SEG_MTEXT 0xB000

/*
 * Tables of default values for each mode
 */
#define TEXT       0x80

#define CTEXT      (0x00 | TEXT)
#define MTEXT      (0x01 | TEXT)
#define CGA        0x02
#define PLANAR1    0x03
#define PLANAR4    0x04
#define LINEAR8    0x05

// for SVGA
#define LINEAR15   0x10
#define LINEAR16   0x11
#define LINEAR24   0x12
#define LINEAR32   0x13

struct vgamode_s {
    u8 svgamode;
    u8 memmodel;    /* CTEXT,MTEXT,CGA,PL1,PL2,PL4,P8,P15,P16,P24,P32 */
    u8 twidth;
    u8 theight;
    u8 cheight;
    u8 pixbits;
    u16 sstart;
    u16 slength;

    u8 pelmask;
    u8 *dac;
    u16 dacsize;
    u8 *sequ_regs;
    u8 miscreg;
    u8 *crtc_regs;
    u8 *actl_regs;
    u8 *grdc_regs;
};

struct saveVideoHardware {
    u8 sequ_index;
    u8 crtc_index;
    u8 grdc_index;
    u8 actl_index;
    u8 feature;
    u8 sequ_regs[4];
    u8 sequ0;
    u8 crtc_regs[25];
    u8 actl_regs[20];
    u8 grdc_regs[9];
    u16 crtc_addr;
    u8 plane_latch[4];
};

struct saveDACcolors {
    u8 rwmode;
    u8 peladdr;
    u8 pelmask;
    u8 dac[768];
    u8 color_select;
};

void stdvga_set_border_color(u8 color);
void stdvga_set_overscan_border_color(u8 color);
u8 stdvga_get_overscan_border_color(void);
void stdvga_set_palette(u8 palid);
void stdvga_set_single_palette_reg(u8 reg, u8 val);
u8 stdvga_get_single_palette_reg(u8 reg);
void stdvga_set_all_palette_reg(u16 seg, u8 *data_far);
void stdvga_get_all_palette_reg(u16 seg, u8 *data_far);
void stdvga_toggle_intensity(u8 flag);
void stdvga_select_video_dac_color_page(u8 flag, u8 data);
void stdvga_read_video_dac_state(u8 *pmode, u8 *curpage);
void stdvga_set_dac_regs(u16 seg, u8 *data_far, u8 start, int count);
void stdvga_get_dac_regs(u16 seg, u8 *data_far, u8 start, int count);
void stdvga_set_pel_mask(u8 val);
u8 stdvga_get_pel_mask(void);
void stdvga_save_dac_state(u16 seg, struct saveDACcolors *info);
void stdvga_restore_dac_state(u16 seg, struct saveDACcolors *info);
void stdvga_perform_gray_scale_summing(u16 start, u16 count);
void stdvga_sequ_write(u8 index, u8 value);
void stdvga_grdc_write(u8 index, u8 value);
void stdvga_set_text_block_specifier(u8 spec);
void stdvga_load_font(u16 seg, void *src_far, u16 count
                      , u16 start, u8 destflags, u8 fontsize);
u16 stdvga_get_crtc(void);
void stdvga_set_cursor_shape(u8 start, u8 end);
void stdvga_set_active_page(u16 address);
void stdvga_set_cursor_pos(u16 address);
void stdvga_set_scan_lines(u8 lines);
u16 stdvga_get_vde(void);
void stdvga_save_state(u16 seg, struct saveVideoHardware *info);
void stdvga_restore_state(u16 seg, struct saveVideoHardware *info);
int stdvga_set_mode(int mode, int flags);
void stdvga_enable_video_addressing(u8 disable);
int stdvga_init(void);

#endif // stdvga.h
