/*
 * GDI definitions
 *
 * Copyright 1993 Alexandre Julliard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef __WINE_NTGDI_PRIVATE_H
#define __WINE_NTGDI_PRIVATE_H

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include "win32u_private.h"

/* extra stock object: default 1x1 bitmap for memory DCs */
#define DEFAULT_BITMAP (STOCK_LAST+1)

struct gdi_obj_funcs
{
    INT     (*pGetObjectW)( HGDIOBJ handle, INT count, LPVOID buffer );
    BOOL    (*pUnrealizeObject)( HGDIOBJ handle );
    BOOL    (*pDeleteObject)( HGDIOBJ handle );
};

struct gdi_obj_header
{
    const struct gdi_obj_funcs *funcs;       /* type-specific functions */
    WORD                        selcount;    /* number of times the object is selected in a DC */
    WORD                        system : 1;  /* system object flag */
    WORD                        deleted : 1; /* whether DeleteObject has been called on this object */
};

typedef struct tagDC
{
    struct gdi_obj_header obj;     /* object header */
    HDC          hSelf;            /* Handle to this DC */
    struct gdi_physdev nulldrv;    /* physdev for the null driver */
    PHYSDEV      physDev;          /* current top of the physdev stack */
    DWORD        thread;           /* thread owning the DC */
    LONG         refcount;         /* thread refcount */
    LONG         dirty;            /* dirty flag */
    DC_ATTR     *attr;             /* DC attributes accessible by client */
    struct tagDC *saved_dc;
    DWORD_PTR    dwHookData;
    DCHOOKPROC   hookProc;         /* DC hook */
    BOOL         bounds_enabled:1; /* bounds tracking is enabled */
    BOOL         path_open:1;      /* path is currently open (only for saved DCs) */

    RECT         device_rect;      /* rectangle for the whole device */
    int          pixel_format;     /* pixel format (for memory DCs) */
    UINT         aa_flags;         /* anti-aliasing flags to pass to GetGlyphOutline for current font */
    WCHAR        display[CCHDEVICENAME]; /* Display name when created for a specific display device */

    int           flags;
    HRGN          hClipRgn;      /* Clip region */
    HRGN          hMetaRgn;      /* Meta region */
    HRGN          hVisRgn;       /* Visible region */
    HRGN          region;        /* Total DC region (intersection of clip and visible) */
    HPEN          hPen;
    HBRUSH        hBrush;
    HFONT         hFont;
    HBITMAP       hBitmap;
    HPALETTE      hPalette;

    struct gdi_path *path;

    const struct font_gamma_ramp *font_gamma_ramp;

    INT           breakExtra;        /* breakTotalExtra / breakCount */
    INT           breakRem;          /* breakTotalExtra % breakCount */
    XFORM         xformWorld2Wnd;    /* World-to-window transformation */
    XFORM         xformWorld2Vport;  /* World-to-viewport transformation */
    XFORM         xformVport2World;  /* Inverse of the above transformation */
    BOOL          vport2WorldValid;  /* Is xformVport2World valid? */
    RECT          bounds;            /* Current bounding rect */
} DC;

/* Certain functions will do no further processing if the driver returns this.
   Used by mfdrv for example. */
#define GDI_NO_MORE_WORK 2

/* Rounds a floating point number to integer. The world-to-viewport
 * transformation process is done in floating point internally. This function
 * is then used to round these coordinates to integer values.
 */
static inline INT GDI_ROUND(double val)
{
   return (int)floor(val + 0.5);
}

#define GET_DC_PHYSDEV(dc,func) \
    get_physdev_entry_point( (dc)->physDev, FIELD_OFFSET(struct gdi_dc_funcs,func))

static inline PHYSDEV pop_dc_driver( DC *dc, const struct gdi_dc_funcs *funcs )
{
    PHYSDEV dev, *pdev = &dc->physDev;
    while (*pdev && (*pdev)->funcs != funcs) pdev = &(*pdev)->next;
    if (!*pdev) return NULL;
    dev = *pdev;
    *pdev = dev->next;
    return dev;
}

static inline PHYSDEV find_dc_driver( DC *dc, const struct gdi_dc_funcs *funcs )
{
    PHYSDEV dev;

    for (dev = dc->physDev; dev; dev = dev->next) if (dev->funcs == funcs) return dev;
    return NULL;
}

/* bitmap object */

typedef struct tagBITMAPOBJ
{
    struct gdi_obj_header obj;
    DIBSECTION            dib;
    SIZE                  size;   /* For SetBitmapDimension() */
    RGBQUAD              *color_table;  /* DIB color table if <= 8bpp (always 1 << bpp in size) */
} BITMAPOBJ;

static inline BOOL is_bitmapobj_dib( const BITMAPOBJ *bmp )
{
    return bmp->dib.dsBmih.biSize != 0;
}

/* bitblt.c */
extern DWORD convert_bits( const BITMAPINFO *src_info, struct bitblt_coords *src,
                           BITMAPINFO *dst_info, struct gdi_image_bits *bits ) DECLSPEC_HIDDEN;
extern BOOL intersect_vis_rectangles( struct bitblt_coords *dst, struct bitblt_coords *src ) DECLSPEC_HIDDEN;
extern DWORD stretch_bits( const BITMAPINFO *src_info, struct bitblt_coords *src,
                           BITMAPINFO *dst_info, struct bitblt_coords *dst,
                           struct gdi_image_bits *bits, int mode ) DECLSPEC_HIDDEN;
extern void get_mono_dc_colors( DC *dc, int color_table_size, BITMAPINFO *info, int count ) DECLSPEC_HIDDEN;

/* brush.c */
extern HBRUSH create_brush( const LOGBRUSH *brush );
extern BOOL store_brush_pattern( LOGBRUSH *brush, struct brush_pattern *pattern ) DECLSPEC_HIDDEN;
extern void free_brush_pattern( struct brush_pattern *pattern ) DECLSPEC_HIDDEN;

/* clipping.c */
extern BOOL clip_device_rect( DC *dc, RECT *dst, const RECT *src ) DECLSPEC_HIDDEN;
extern BOOL clip_visrect( DC *dc, RECT *dst, const RECT *src ) DECLSPEC_HIDDEN;
extern void update_dc_clipping( DC * dc ) DECLSPEC_HIDDEN;

/* Return the total DC region (if any) */
static inline HRGN get_dc_region( DC *dc )
{
    if (dc->region) return dc->region;
    if (dc->hVisRgn) return dc->hVisRgn;
    if (dc->hClipRgn) return dc->hClipRgn;
    return dc->hMetaRgn;
}

/* dc.c */
extern DC *alloc_dc_ptr( DWORD magic ) DECLSPEC_HIDDEN;
extern void free_dc_ptr( DC *dc ) DECLSPEC_HIDDEN;
extern DC *get_dc_ptr( HDC hdc ) DECLSPEC_HIDDEN;
extern void release_dc_ptr( DC *dc ) DECLSPEC_HIDDEN;
extern void update_dc( DC *dc ) DECLSPEC_HIDDEN;
extern void DC_InitDC( DC * dc ) DECLSPEC_HIDDEN;
extern void DC_UpdateXforms( DC * dc ) DECLSPEC_HIDDEN;

/* dib.c */
extern BOOL fill_color_table_from_pal_colors( BITMAPINFO *info, HDC hdc ) DECLSPEC_HIDDEN;
extern const RGBQUAD *get_default_color_table( int bpp ) DECLSPEC_HIDDEN;
extern void fill_default_color_table( BITMAPINFO *info ) DECLSPEC_HIDDEN;
extern void get_ddb_bitmapinfo( BITMAPOBJ *bmp, BITMAPINFO *info ) DECLSPEC_HIDDEN;
extern BITMAPINFO *copy_packed_dib( const BITMAPINFO *src_info, UINT usage ) DECLSPEC_HIDDEN;
extern DWORD convert_bitmapinfo( const BITMAPINFO *src_info, void *src_bits, struct bitblt_coords *src,
                                 const BITMAPINFO *dst_info, void *dst_bits ) DECLSPEC_HIDDEN;

extern DWORD stretch_bitmapinfo( const BITMAPINFO *src_info, void *src_bits, struct bitblt_coords *src,
                                 const BITMAPINFO *dst_info, void *dst_bits, struct bitblt_coords *dst,
                                 INT mode ) DECLSPEC_HIDDEN;
extern DWORD blend_bitmapinfo( const BITMAPINFO *src_info, void *src_bits, struct bitblt_coords *src,
                               const BITMAPINFO *dst_info, void *dst_bits, struct bitblt_coords *dst,
                               BLENDFUNCTION blend ) DECLSPEC_HIDDEN;
extern DWORD gradient_bitmapinfo( const BITMAPINFO *info, void *bits, TRIVERTEX *vert_array, ULONG nvert,
                                  void *grad_array, ULONG ngrad, ULONG mode, const POINT *dev_pts, HRGN rgn ) DECLSPEC_HIDDEN;
extern COLORREF get_pixel_bitmapinfo( const BITMAPINFO *info, void *bits, struct bitblt_coords *src ) DECLSPEC_HIDDEN;
extern BOOL render_aa_text_bitmapinfo( DC *dc, BITMAPINFO *info, struct gdi_image_bits *bits,
                                       struct bitblt_coords *src, INT x, INT y, UINT flags,
                                       UINT aa_flags, LPCWSTR str, UINT count, const INT *dx ) DECLSPEC_HIDDEN;
extern DWORD get_image_from_bitmap( BITMAPOBJ *bmp, BITMAPINFO *info,
                                    struct gdi_image_bits *bits, struct bitblt_coords *src ) DECLSPEC_HIDDEN;
extern DWORD put_image_into_bitmap( BITMAPOBJ *bmp, HRGN clip, BITMAPINFO *info,
                                    const struct gdi_image_bits *bits, struct bitblt_coords *src,
                                    struct bitblt_coords *dst ) DECLSPEC_HIDDEN;
extern UINT get_dib_dc_color_table( HDC hdc, UINT startpos, UINT entries,
                                    RGBQUAD *colors ) DECLSPEC_HIDDEN;
extern UINT set_dib_dc_color_table( HDC hdc, UINT startpos, UINT entries,
                                    const RGBQUAD *colors ) DECLSPEC_HIDDEN;
extern void dibdrv_set_window_surface( DC *dc, struct window_surface *surface ) DECLSPEC_HIDDEN;

/* driver.c */
extern const struct gdi_dc_funcs null_driver DECLSPEC_HIDDEN;
extern const struct gdi_dc_funcs dib_driver DECLSPEC_HIDDEN;
extern const struct gdi_dc_funcs path_driver DECLSPEC_HIDDEN;
extern const struct gdi_dc_funcs font_driver DECLSPEC_HIDDEN;
extern const struct gdi_dc_funcs *get_display_driver(void) DECLSPEC_HIDDEN;

/* font.c */

struct font_gamma_ramp
{
    DWORD gamma;
    BYTE  encode[256];
    BYTE  decode[256];
};

typedef struct { FLOAT eM11, eM12, eM21, eM22; } FMAT2;

struct bitmap_font_size
{
    int  width;
    int  height;
    int  size;
    int  x_ppem;
    int  y_ppem;
    int  internal_leading;
};

struct gdi_font
{
    struct list            entry;
    struct list            unused_entry;
    DWORD                  refcount;
    DWORD                  gm_size;
    struct glyph_metrics **gm;
    OUTLINETEXTMETRICW     otm;
    KERNINGPAIR           *kern_pairs;
    int                    kern_count;
    /* the following members can be accessed without locking, they are never modified after creation */
    void                  *private;  /* font backend private data */
    struct list            child_fonts;
    DWORD                  handle;
    DWORD                  cache_num;
    DWORD                  hash;
    UINT                   charset;
    UINT                   codepage;
    FONTSIGNATURE          fs;
    LOGFONTW               lf;
    FMAT2                  matrix;
    UINT                   face_index;
    INT                    scale_y;
    INT                    aveWidth;
    INT                    ppem;
    SHORT                  yMax;
    SHORT                  yMin;
    UINT                   ntmFlags;
    UINT                   ntmAvgWidth;
    UINT                   aa_flags;
    ULONG                  ttc_item_offset;    /* 0 if font is not a part of TrueType collection */
    BOOL                   can_use_bitmap : 1;
    BOOL                   fake_italic : 1;
    BOOL                   fake_bold : 1;
    BOOL                   scalable : 1;
    BOOL                   use_logfont_name : 1;
    struct gdi_font       *base_font;
    void                  *gsub_table;
    void                  *vert_feature;
    void                  *data_ptr;
    SIZE_T                 data_size;
    FILETIME               writetime;
    WCHAR                  file[1];
};

#define MS_MAKE_TAG(ch1,ch2,ch3,ch4) \
    (((DWORD)ch4 << 24) | ((DWORD)ch3 << 16) | ((DWORD)ch2 << 8) | (DWORD)ch1)

#define MS_GASP_TAG MS_MAKE_TAG('g', 'a', 's', 'p')
#define MS_GSUB_TAG MS_MAKE_TAG('G', 'S', 'U', 'B')
#define MS_KERN_TAG MS_MAKE_TAG('k', 'e', 'r', 'n')
#define MS_TTCF_TAG MS_MAKE_TAG('t', 't', 'c', 'f')
#define MS_VDMX_TAG MS_MAKE_TAG('V', 'D', 'M', 'X')

#define FS_DBCS_MASK (FS_JISJAPAN | FS_CHINESESIMP | FS_WANSUNG | FS_CHINESETRAD | FS_JOHAB)

#define ADDFONT_EXTERNAL_FONT 0x01
#define ADDFONT_ALLOW_BITMAP  0x02
#define ADDFONT_ADD_TO_CACHE  0x04
#define ADDFONT_ADD_RESOURCE  0x08  /* added through AddFontResource */
#define ADDFONT_VERTICAL_FONT 0x10
#define ADDFONT_EXTERNAL_FOUND 0x20
#define ADDFONT_AA_FLAGS(flags) ((flags) << 16)

struct font_backend_funcs
{
    void  (*load_fonts)(void);
    BOOL  (*enum_family_fallbacks)( DWORD pitch_and_family, int index, WCHAR buffer[LF_FACESIZE] );
    INT   (*add_font)( const WCHAR *file, DWORD flags );
    INT   (*add_mem_font)( void *ptr, SIZE_T size, DWORD flags );

    BOOL  (*load_font)( struct gdi_font *gdi_font );
    DWORD (*get_font_data)( struct gdi_font *gdi_font, DWORD table, DWORD offset,
                            void *buf, DWORD count );
    UINT  (*get_aa_flags)( struct gdi_font *font, UINT aa_flags, BOOL antialias_fakes );
    BOOL  (*get_glyph_index)( struct gdi_font *gdi_font, UINT *glyph, BOOL use_encoding );
    UINT  (*get_default_glyph)( struct gdi_font *gdi_font );
    DWORD (*get_glyph_outline)( struct gdi_font *font, UINT glyph, UINT format,
                                GLYPHMETRICS *gm, ABC *abc, DWORD buflen, void *buf,
                                const MAT2 *mat, BOOL tategaki );
    DWORD (*get_unicode_ranges)( struct gdi_font *font, GLYPHSET *gs );
    BOOL  (*get_char_width_info)( struct gdi_font *font, struct char_width_info *info );
    BOOL  (*set_outline_text_metrics)( struct gdi_font *font );
    BOOL  (*set_bitmap_text_metrics)( struct gdi_font *font );
    DWORD (*get_kerning_pairs)( struct gdi_font *gdi_font, KERNINGPAIR **kern_pair );
    void  (*destroy_font)( struct gdi_font *font );
};

extern int add_gdi_face( const WCHAR *family_name, const WCHAR *second_name,
                         const WCHAR *style, const WCHAR *fullname, const WCHAR *file,
                         void *data_ptr, SIZE_T data_size, UINT index, FONTSIGNATURE fs,
                         DWORD ntmflags, DWORD version, DWORD flags,
                         const struct bitmap_font_size *size ) DECLSPEC_HIDDEN;
extern UINT font_init(void) DECLSPEC_HIDDEN;
extern UINT get_acp(void) DECLSPEC_HIDDEN;
extern CPTABLEINFO *get_cptable( WORD cp ) DECLSPEC_HIDDEN;
extern const struct font_backend_funcs *init_freetype_lib(void) DECLSPEC_HIDDEN;

/* opentype.c */

struct ttc_sfnt_v1;
struct tt_name_v0;

struct opentype_name
{
    DWORD codepage;
    DWORD length;
    const void *bytes;
};

extern BOOL opentype_get_ttc_sfnt_v1( const void *data, size_t size, DWORD index, DWORD *count,
                                      const struct ttc_sfnt_v1 **ttc_sfnt_v1 ) DECLSPEC_HIDDEN;
extern BOOL opentype_get_tt_name_v0( const void *data, size_t size, const struct ttc_sfnt_v1 *ttc_sfnt_v1,
                                     const struct tt_name_v0 **tt_name_v0 ) DECLSPEC_HIDDEN;

typedef BOOL ( *opentype_enum_names_cb )( LANGID langid, struct opentype_name *name, void *user );
extern BOOL opentype_enum_family_names( const struct tt_name_v0 *tt_name_v0,
                                        opentype_enum_names_cb callback, void *user ) DECLSPEC_HIDDEN;
extern BOOL opentype_enum_style_names( const struct tt_name_v0 *tt_name_v0,
                                       opentype_enum_names_cb callback, void *user ) DECLSPEC_HIDDEN;
extern BOOL opentype_enum_full_names( const struct tt_name_v0 *tt_name_v0,
                                      opentype_enum_names_cb callback, void *user ) DECLSPEC_HIDDEN;

extern BOOL opentype_get_properties( const void *data, size_t size, const struct ttc_sfnt_v1 *ttc_sfnt_v1,
                                     DWORD *version, FONTSIGNATURE *fs, DWORD *ntm_flags ) DECLSPEC_HIDDEN;
extern BOOL translate_charset_info( DWORD *src, CHARSETINFO *cs, DWORD flags ) DECLSPEC_HIDDEN;

/* gdiobj.c */
extern HGDIOBJ alloc_gdi_handle( struct gdi_obj_header *obj, DWORD type,
                                 const struct gdi_obj_funcs *funcs ) DECLSPEC_HIDDEN;
extern void *free_gdi_handle( HGDIOBJ handle ) DECLSPEC_HIDDEN;
extern void *GDI_GetObjPtr( HGDIOBJ, DWORD ) DECLSPEC_HIDDEN;
extern void *get_any_obj_ptr( HGDIOBJ, DWORD * ) DECLSPEC_HIDDEN;
extern void GDI_ReleaseObj( HGDIOBJ ) DECLSPEC_HIDDEN;
extern UINT GDI_get_ref_count( HGDIOBJ handle ) DECLSPEC_HIDDEN;
extern HGDIOBJ GDI_inc_ref_count( HGDIOBJ handle ) DECLSPEC_HIDDEN;
extern BOOL GDI_dec_ref_count( HGDIOBJ handle ) DECLSPEC_HIDDEN;
extern DWORD get_system_dpi(void) DECLSPEC_HIDDEN;
extern HGDIOBJ get_stock_object( INT obj ) DECLSPEC_HIDDEN;
extern DWORD get_gdi_object_type( HGDIOBJ obj ) DECLSPEC_HIDDEN;

/* mapping.c */
extern BOOL dp_to_lp( DC *dc, POINT *points, INT count ) DECLSPEC_HIDDEN;
extern void lp_to_dp( DC *dc, POINT *points, INT count ) DECLSPEC_HIDDEN;
extern BOOL set_map_mode( DC *dc, int mode ) DECLSPEC_HIDDEN;
extern void combine_transform( XFORM *result, const XFORM *xform1,
                               const XFORM *xform2 ) DECLSPEC_HIDDEN;
extern int muldiv( int a, int b, int c ) DECLSPEC_HIDDEN;

/* driver.c */
extern BOOL is_display_device( LPCWSTR name ) DECLSPEC_HIDDEN;

/* path.c */

extern void free_gdi_path( struct gdi_path *path ) DECLSPEC_HIDDEN;
extern struct gdi_path *get_gdi_flat_path( DC *dc, HRGN *rgn ) DECLSPEC_HIDDEN;
extern int get_gdi_path_data( struct gdi_path *path, POINT **points, BYTE **flags ) DECLSPEC_HIDDEN;
extern BOOL PATH_SavePath( DC *dst, DC *src ) DECLSPEC_HIDDEN;
extern BOOL PATH_RestorePath( DC *dst, DC *src ) DECLSPEC_HIDDEN;

/* painting.c */
extern POINT *GDI_Bezier( const POINT *Points, INT count, INT *nPtsOut ) DECLSPEC_HIDDEN;

/* palette.c */
extern HPALETTE WINAPI GDISelectPalette( HDC hdc, HPALETTE hpal, WORD wBkg) DECLSPEC_HIDDEN;
extern HPALETTE PALETTE_Init(void) DECLSPEC_HIDDEN;
extern UINT get_palette_entries( HPALETTE hpalette, UINT start, UINT count,
                                 PALETTEENTRY *entries ) DECLSPEC_HIDDEN;

/* pen.c */
extern HPEN create_pen( INT style, INT width, COLORREF color ) DECLSPEC_HIDDEN;

/* region.c */
extern BOOL add_rect_to_region( HRGN rgn, const RECT *rect ) DECLSPEC_HIDDEN;
extern INT mirror_region( HRGN dst, HRGN src, INT width ) DECLSPEC_HIDDEN;
extern BOOL REGION_FrameRgn( HRGN dest, HRGN src, INT x, INT y ) DECLSPEC_HIDDEN;
extern HRGN create_polypolygon_region( const POINT *pts, const INT *count, INT nbpolygons,
                                       INT mode, const RECT *clip_rect ) DECLSPEC_HIDDEN;

#define RGN_DEFAULT_RECTS 4
typedef struct
{
    struct gdi_obj_header obj;
    INT size;
    INT numRects;
    RECT *rects;
    RECT extents;
    RECT rects_buf[RGN_DEFAULT_RECTS];
} WINEREGION;

/* return the region data without making a copy */
static inline const WINEREGION *get_wine_region(HRGN rgn)
{
    return GDI_GetObjPtr( rgn, NTGDI_OBJ_REGION );
}
static inline void release_wine_region(HRGN rgn)
{
    GDI_ReleaseObj(rgn);
}

/**********************************************************
 *     region_find_pt
 *
 * Return either the index of the rectangle that contains (x,y) or the first
 * rectangle after it.  Sets *hit to TRUE if the region contains (x,y).
 * Note if (x,y) follows all rectangles, then the return value will be rgn->numRects.
 */
static inline int region_find_pt( const WINEREGION *rgn, int x, int y, BOOL *hit )
{
    int i, start = 0, end = rgn->numRects - 1;
    BOOL h = FALSE;

    while (start <= end)
    {
        i = (start + end) / 2;

        if (rgn->rects[i].bottom <= y ||
            (rgn->rects[i].top <= y && rgn->rects[i].right <= x))
            start = i + 1;
        else if (rgn->rects[i].top > y ||
                 (rgn->rects[i].bottom > y && rgn->rects[i].left > x))
            end = i - 1;
        else
        {
            h = TRUE;
            break;
        }
    }

    if (hit) *hit = h;
    return h ? i : start;
}

/* null driver entry points */
extern BOOL CDECL nulldrv_AbortPath( PHYSDEV dev ) DECLSPEC_HIDDEN;
extern BOOL CDECL nulldrv_AlphaBlend( PHYSDEV dst_dev, struct bitblt_coords *dst,
                                      PHYSDEV src_dev, struct bitblt_coords *src, BLENDFUNCTION func) DECLSPEC_HIDDEN;
extern BOOL CDECL nulldrv_AngleArc( PHYSDEV dev, INT x, INT y, DWORD radius, FLOAT start, FLOAT sweep ) DECLSPEC_HIDDEN;
extern BOOL CDECL nulldrv_ArcTo( PHYSDEV dev, INT left, INT top, INT right, INT bottom, INT xstart, INT ystart, INT xend, INT yend ) DECLSPEC_HIDDEN;
extern BOOL CDECL nulldrv_BeginPath( PHYSDEV dev ) DECLSPEC_HIDDEN;
extern DWORD CDECL nulldrv_BlendImage( PHYSDEV dev, BITMAPINFO *info, const struct gdi_image_bits *bits,
                                       struct bitblt_coords *src, struct bitblt_coords *dst, BLENDFUNCTION func ) DECLSPEC_HIDDEN;
extern BOOL CDECL nulldrv_CloseFigure( PHYSDEV dev ) DECLSPEC_HIDDEN;
extern BOOL CDECL nulldrv_EndPath( PHYSDEV dev ) DECLSPEC_HIDDEN;
extern BOOL CDECL nulldrv_ExtTextOut( PHYSDEV dev, INT x, INT y, UINT flags, const RECT *rect,
                                      LPCWSTR str, UINT count, const INT *dx ) DECLSPEC_HIDDEN;
extern BOOL CDECL nulldrv_FillPath( PHYSDEV dev ) DECLSPEC_HIDDEN;
extern BOOL CDECL nulldrv_FillRgn( PHYSDEV dev, HRGN rgn, HBRUSH brush ) DECLSPEC_HIDDEN;
extern BOOL CDECL nulldrv_FrameRgn( PHYSDEV dev, HRGN rgn, HBRUSH brush, INT width, INT height ) DECLSPEC_HIDDEN;
extern LONG CDECL nulldrv_GetBitmapBits( HBITMAP bitmap, void *bits, LONG size ) DECLSPEC_HIDDEN;
extern COLORREF CDECL nulldrv_GetNearestColor( PHYSDEV dev, COLORREF color ) DECLSPEC_HIDDEN;
extern COLORREF CDECL nulldrv_GetPixel( PHYSDEV dev, INT x, INT y ) DECLSPEC_HIDDEN;
extern UINT CDECL nulldrv_GetSystemPaletteEntries( PHYSDEV dev, UINT start, UINT count, PALETTEENTRY *entries ) DECLSPEC_HIDDEN;
extern BOOL CDECL nulldrv_GradientFill( PHYSDEV dev, TRIVERTEX *vert_array, ULONG nvert,
                                        void * grad_array, ULONG ngrad, ULONG mode ) DECLSPEC_HIDDEN;
extern BOOL CDECL nulldrv_InvertRgn( PHYSDEV dev, HRGN rgn ) DECLSPEC_HIDDEN;
extern BOOL CDECL nulldrv_PolyBezier( PHYSDEV dev, const POINT *points, DWORD count ) DECLSPEC_HIDDEN;
extern BOOL CDECL nulldrv_PolyBezierTo( PHYSDEV dev, const POINT *points, DWORD count ) DECLSPEC_HIDDEN;
extern BOOL CDECL nulldrv_PolyDraw( PHYSDEV dev, const POINT *points, const BYTE *types, DWORD count ) DECLSPEC_HIDDEN;
extern BOOL CDECL nulldrv_PolylineTo( PHYSDEV dev, const POINT *points, INT count ) DECLSPEC_HIDDEN;
extern INT CDECL nulldrv_SetDIBitsToDevice( PHYSDEV dev, INT x_dst, INT y_dst, DWORD width, DWORD height,
                                            INT x_src, INT y_src, UINT start, UINT lines,
                                            const void *bits, BITMAPINFO *info, UINT coloruse ) DECLSPEC_HIDDEN;
extern BOOL CDECL nulldrv_StretchBlt( PHYSDEV dst_dev, struct bitblt_coords *dst,
                                      PHYSDEV src_dev, struct bitblt_coords *src, DWORD rop ) DECLSPEC_HIDDEN;
extern INT  CDECL nulldrv_StretchDIBits( PHYSDEV dev, INT xDst, INT yDst, INT widthDst, INT heightDst,
                                         INT xSrc, INT ySrc, INT widthSrc, INT heightSrc, const void *bits,
                                         BITMAPINFO *info, UINT coloruse, DWORD rop ) DECLSPEC_HIDDEN;
extern BOOL CDECL nulldrv_StrokeAndFillPath( PHYSDEV dev ) DECLSPEC_HIDDEN;
extern BOOL CDECL nulldrv_StrokePath( PHYSDEV dev ) DECLSPEC_HIDDEN;

static inline DC *get_nulldrv_dc( PHYSDEV dev )
{
    return CONTAINING_RECORD( dev, DC, nulldrv );
}

static inline DC *get_physdev_dc( PHYSDEV dev )
{
    while (dev->funcs != &null_driver)
        dev = dev->next;
    return get_nulldrv_dc( dev );
}

BOOL WINAPI FontIsLinked(HDC);

BOOL WINAPI SetVirtualResolution(HDC hdc, DWORD horz_res, DWORD vert_res, DWORD horz_size, DWORD vert_size);

static inline BOOL is_rect_empty( const RECT *rect )
{
    return (rect->left >= rect->right || rect->top >= rect->bottom);
}

static inline BOOL intersect_rect( RECT *dst, const RECT *src1, const RECT *src2 )
{
    dst->left   = max( src1->left, src2->left );
    dst->top    = max( src1->top, src2->top );
    dst->right  = min( src1->right, src2->right );
    dst->bottom = min( src1->bottom, src2->bottom );
    return !is_rect_empty( dst );
}

static inline void offset_rect( RECT *rect, int offset_x, int offset_y )
{
    rect->left   += offset_x;
    rect->top    += offset_y;
    rect->right  += offset_x;
    rect->bottom += offset_y;
}

static inline void set_rect( RECT *rect, int left, int top, int right, int bottom )
{
    rect->left = left;
    rect->top = top;
    rect->right = right;
    rect->bottom = bottom;
}

static inline void order_rect( RECT *rect )
{
    if (rect->left > rect->right)
    {
        int tmp = rect->left;
        rect->left = rect->right;
        rect->right = tmp;
    }
    if (rect->top > rect->bottom)
    {
        int tmp = rect->top;
        rect->top = rect->bottom;
        rect->bottom = tmp;
    }
}

static inline void get_bounding_rect( RECT *rect, int x, int y, int width, int height )
{
    rect->left   = x;
    rect->right  = x + width;
    rect->top    = y;
    rect->bottom = y + height;
    if (rect->left > rect->right)
    {
        int tmp = rect->left;
        rect->left = rect->right + 1;
        rect->right = tmp + 1;
    }
    if (rect->top > rect->bottom)
    {
        int tmp = rect->top;
        rect->top = rect->bottom + 1;
        rect->bottom = tmp + 1;
    }
}

static inline void reset_bounds( RECT *bounds )
{
    bounds->left = bounds->top = INT_MAX;
    bounds->right = bounds->bottom = INT_MIN;
}

static inline void union_rect( RECT *dest, const RECT *src1, const RECT *src2 )
{
    if (is_rect_empty( src1 ))
    {
        if (is_rect_empty( src2 ))
        {
            reset_bounds( dest );
            return;
        }
        else *dest = *src2;
    }
    else
    {
        if (is_rect_empty( src2 )) *dest = *src1;
        else
        {
            dest->left   = min( src1->left, src2->left );
            dest->right  = max( src1->right, src2->right );
            dest->top    = min( src1->top, src2->top );
            dest->bottom = max( src1->bottom, src2->bottom );
        }
    }
}

static inline void add_bounds_rect( RECT *bounds, const RECT *rect )
{
    if (is_rect_empty( rect )) return;
    bounds->left   = min( bounds->left, rect->left );
    bounds->top    = min( bounds->top, rect->top );
    bounds->right  = max( bounds->right, rect->right );
    bounds->bottom = max( bounds->bottom, rect->bottom );
}

static inline int get_bitmap_stride( int width, int bpp )
{
    return ((width * bpp + 15) >> 3) & ~1;
}

static inline int get_dib_stride( int width, int bpp )
{
    return ((width * bpp + 31) >> 3) & ~3;
}

static inline int get_dib_image_size( const BITMAPINFO *info )
{
    return get_dib_stride( info->bmiHeader.biWidth, info->bmiHeader.biBitCount )
        * abs( info->bmiHeader.biHeight );
}

/* only for use on sanitized BITMAPINFO structures */
static inline int get_dib_info_size( const BITMAPINFO *info, UINT coloruse )
{
    if (info->bmiHeader.biCompression == BI_BITFIELDS)
        return sizeof(BITMAPINFOHEADER) + 3 * sizeof(DWORD);
    if (coloruse == DIB_PAL_COLORS)
        return sizeof(BITMAPINFOHEADER) + info->bmiHeader.biClrUsed * sizeof(WORD);
    return FIELD_OFFSET( BITMAPINFO, bmiColors[info->bmiHeader.biClrUsed] );
}

/* only for use on sanitized BITMAPINFO structures */
static inline void copy_bitmapinfo( BITMAPINFO *dst, const BITMAPINFO *src )
{
    memcpy( dst, src, get_dib_info_size( src, DIB_RGB_COLORS ));
}

extern void CDECL free_heap_bits( struct gdi_image_bits *bits ) DECLSPEC_HIDDEN;

void set_gdi_client_ptr( HGDIOBJ handle, void *ptr ) DECLSPEC_HIDDEN;

extern SYSTEM_BASIC_INFORMATION system_info DECLSPEC_HIDDEN;

#endif /* __WINE_NTGDI_PRIVATE_H */
