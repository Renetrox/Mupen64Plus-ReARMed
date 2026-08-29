#ifndef TEXTURES_H
#define TEXTURES_H

#include <stdint.h>
#include <stdlib.h>
#include <retro_inline.h>

#include <glsm/glsmsym.h>

#include "Config.h"
#include "convert.h"
#include "2xSAI.h"

#ifdef __cplusplus
extern "C" {
#endif

struct gDPTile;

typedef uint32_t (*GetTexelFunc)( uint64_t *src, uint16_t x, uint16_t i, uint8_t palette );

typedef struct CachedTexture
{
   GLuint  glName;
   uint32_t     address;
   uint32_t     crc;
   float   offsetS, offsetT;
   uint32_t     maskS, maskT;
   uint32_t     clampS, clampT;
   uint32_t     mirrorS, mirrorT;
   uint32_t     line;
   uint32_t     size;
   uint32_t     format;
   uint32_t     tMem;
   uint32_t     palette;
   uint32_t     width, height;            // N64 width and height
   uint32_t     clampWidth, clampHeight;  // Size to clamp to
   uint32_t     realWidth, realHeight;    // Actual texture size
   float     scaleS, scaleT;           // Scale to map to 0.0-1.0
   float     shiftScaleS, shiftScaleT; // Scale to shift
   uint32_t     textureBytes;

   struct CachedTexture   *lower, *higher;
   uint32_t     lastDList;
   uint8_t      max_level;
   uint8_t      frameBufferTexture;
} CachedTexture;

/*
 * FZ's 2xSAI path expands each uploaded texture to twice its width and height.
 * The legacy cache records pre-filter byte counts, so use a quarter-sized
 * logical limit while the filter is enabled. This keeps the effective GPU
 * texture budget close to the original 8 MiB instead of silently allowing
 * roughly 32 MiB of filtered texture data on low-end hardware.
 */
#define TEXTURECACHE_MAX ((config.texture.sai2x ? 2u : 8u) * 1024u * 1024u)
#define TEXTUREBUFFER_SIZE (512 * 1024)

typedef struct TextureCache
{
    CachedTexture   *current[2];
    CachedTexture   *bottom, *top;
    CachedTexture   *dummy;

    uint32_t             cachedBytes;
    uint32_t             numCached;
    uint32_t             hits, misses;
    GLuint          glNoiseNames[32];
} TextureCache;

extern TextureCache cache;

static INLINE uint8_t TextureCache_SizeToBPP(uint8_t size)
{
   switch (size)
   {
      case 0:
         return 4;
      case 1:
         return 8;
      case 2:
         return 16;
      default:
         break;
   }

   return 32;
}

static INLINE uint32_t pow2( uint32_t dim )
{
    uint32_t i = 1;

    while (i < dim) i <<= 1;

    return i;
}

static INLINE uint32_t powof( uint32_t dim )
{
    uint32_t num, i;
    num = 1;
    i = 0;

    while (num < dim)
    {
        num <<= 1;
        i++;
    }

    return i;
}

/*
 * Intercept gles2n64's CPU-decoded RGBA texture uploads and apply the same
 * three pixel-format variants used by Mupen64Plus FZ. Noise textures use
 * GL_LUMINANCE_ALPHA and pass straight through; framebuffer textures in this
 * renderer are not CPU-uploaded through this path. FZ's corrected dispatch is
 * deliberately used here: BYTE / 4444 / 5551 are mutually exclusive.
 *
 * The scaler clamps its 4x4 neighbourhood at the physical decoded texture
 * edge. Mirroring/repetition has already been materialized by the texture-cache
 * decoder before this upload, so wrapping again inside 2xSAI would create edge
 * bleed.
 */
static INLINE void gln64_2xsai_glTexImage2D(
      GLenum target, GLint level, GLint internalformat,
      GLsizei width, GLsizei height, GLint border,
      GLenum format, GLenum type, const GLvoid *pixels)
{
   void *scaled;
   size_t bytes_per_pixel;
   size_t scaled_bytes;

   if (!config.texture.sai2x ||
       target != GL_TEXTURE_2D ||
       pixels == NULL ||
       format != GL_RGBA ||
       width <= 0 || height <= 0 ||
       width > 32767 || height > 32767 ||
       (cache.dummy && cache.top == cache.dummy) ||
       (type != GL_UNSIGNED_BYTE &&
        type != GL_UNSIGNED_SHORT_4_4_4_4 &&
        type != GL_UNSIGNED_SHORT_5_5_5_1))
   {
      glTexImage2D(target, level, internalformat, width, height,
                   border, format, type, pixels);
      return;
   }

   bytes_per_pixel = (type == GL_UNSIGNED_BYTE) ? 4u : 2u;

   if ((size_t)width > SIZE_MAX / (size_t)height / bytes_per_pixel / 4u)
   {
      glTexImage2D(target, level, internalformat, width, height,
                   border, format, type, pixels);
      return;
   }

   scaled_bytes = (size_t)width * (size_t)height * bytes_per_pixel * 4u;
   scaled = malloc(scaled_bytes);
   if (!scaled)
   {
      glTexImage2D(target, level, internalformat, width, height,
                   border, format, type, pixels);
      return;
   }

   if (type == GL_UNSIGNED_BYTE)
      _2xSaI8888((uint32_t *)pixels, (uint32_t *)scaled,
                  (uint16_t)width, (uint16_t)height, 1, 1);
   else if (type == GL_UNSIGNED_SHORT_4_4_4_4)
      _2xSaI4444((uint16_t *)pixels, (uint16_t *)scaled,
                  (uint16_t)width, (uint16_t)height, 1, 1);
   else
      _2xSaI5551((uint16_t *)pixels, (uint16_t *)scaled,
                  (uint16_t)width, (uint16_t)height, 1, 1);

   /* FZ changes the cached half-texel offset from 0.5 to 0.25 when the
    * physical texture dimensions are doubled. TextureCache_AddTop() has
    * already made the texture being uploaded cache.top here. Apply the
    * correction only once at level zero, and never to the dummy or an FBO. */
   if (level == 0 && cache.top && cache.top != cache.dummy &&
       !cache.top->frameBufferTexture)
   {
      cache.top->offsetS *= 0.5f;
      cache.top->offsetT *= 0.5f;
   }

   glTexImage2D(target, level, GL_RGBA, width << 1, height << 1,
                border, GL_RGBA, type, scaled);
   free(scaled);
}

/* glsmsym.h maps glTexImage2D to rglTexImage2D. The wrapper body above was
 * preprocessed while that mapping was active, so its internal calls still hit
 * the real glsm entry point. Remove the alias before installing our local
 * gles2n64 interception macro to avoid a macro-redefinition warning. */
#ifdef glTexImage2D
#undef glTexImage2D
#endif
#define glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels) \
   gln64_2xsai_glTexImage2D((target), (level), (internalformat), (width), (height), \
                            (border), (format), (type), (pixels))

CachedTexture *TextureCache_AddTop();
void TextureCache_MoveToTop( CachedTexture *newtop );
void TextureCache_Remove( CachedTexture *texture );
void TextureCache_RemoveBottom();
void TextureCache_Init();
void TextureCache_Destroy();
void TextureCache_Update( uint32_t t );
void TextureCache_ActivateTexture( uint32_t t, CachedTexture *texture );
void TextureCache_ActivateNoise( uint32_t t );
void TextureCache_ActivateDummy( uint32_t t );
bool TextureCache_Verify();

#ifdef __cplusplus
}
#endif

#endif

