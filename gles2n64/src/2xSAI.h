#ifndef GLN64_2XSAI_H
#define GLN64_2XSAI_H

#include <stddef.h>
#include <stdint.h>

/*
 * 2xSAI texture scaler adapted from Mupen64Plus FZ's glN64 plugin.
 *
 * FZ carries three near-identical implementations for RGBA4444, RGBA5551
 * and RGBA8888. ReARMed keeps the same pixel decisions and interpolation
 * masks, but shares the neighbourhood walk so the C port stays compact.
 */
enum gln64_2xsai_format
{
   GLN64_2XSAI_4444 = 0,
   GLN64_2XSAI_5551 = 1,
   GLN64_2XSAI_8888 = 2
};

static inline int16_t gln64_2xsai_result1(
      uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
   int16_t x = 0;
   int16_t y = 0;
   int16_t r = 0;

   if (a == c) x += 1; else if (b == c) y += 1;
   if (a == d) x += 1; else if (b == d) y += 1;
   if (x <= 1) r += 1;
   if (y <= 1) r -= 1;
   return r;
}

static inline int16_t gln64_2xsai_result2(
      uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
   int16_t x = 0;
   int16_t y = 0;
   int16_t r = 0;

   if (a == c) x += 1; else if (b == c) y += 1;
   if (a == d) x += 1; else if (b == d) y += 1;
   if (x <= 1) r -= 1;
   if (y <= 1) r += 1;
   return r;
}

static inline uint32_t gln64_2xsai_interpolate(
      enum gln64_2xsai_format format, uint32_t a, uint32_t b)
{
   if (a == b)
      return a;

   switch (format)
   {
      case GLN64_2XSAI_4444:
         return ((a & 0xEEEEu) >> 1) +
                (((b & 0xEEEEu) >> 1) | (a & b & 0x1111u));
      case GLN64_2XSAI_5551:
         return ((a & 0xF7BCu) >> 1) +
                (((b & 0xF7BCu) >> 1) | (a & b & 0x0843u));
      default:
         return ((a & 0xFEFEFEFEu) >> 1) +
                (((b & 0xFEFEFEFEu) >> 1) | (a & b & 0x01010101u));
   }
}

static inline uint32_t gln64_2xsai_q_interpolate(
      enum gln64_2xsai_format format,
      uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
   switch (format)
   {
      case GLN64_2XSAI_4444:
      {
         const uint32_t x = ((a & 0xCCCCu) >> 2) +
                            ((b & 0xCCCCu) >> 2) +
                            ((c & 0xCCCCu) >> 2) +
                            ((d & 0xCCCCu) >> 2);
         const uint32_t y = (((a & 0x3333u) + (b & 0x3333u) +
                              (c & 0x3333u) + (d & 0x3333u)) >> 2) & 0x3333u;
         return x | y;
      }
      case GLN64_2XSAI_5551:
      {
         const uint32_t x = ((a & 0xE738u) >> 2) +
                            ((b & 0xE738u) >> 2) +
                            ((c & 0xE738u) >> 2) +
                            ((d & 0xE738u) >> 2);
         const uint32_t y = (((a & 0x18C6u) + (b & 0x18C6u) +
                              (c & 0x18C6u) + (d & 0x18C6u)) >> 2) & 0x18C6u;
         const uint32_t z = ((a & 1u) + (b & 1u) + (c & 1u) + (d & 1u)) > 2u;
         return x | y | z;
      }
      default:
      {
         const uint32_t x = ((a & 0xFCFCFCFCu) >> 2) +
                            ((b & 0xFCFCFCFCu) >> 2) +
                            ((c & 0xFCFCFCFCu) >> 2) +
                            ((d & 0xFCFCFCFCu) >> 2);
         const uint32_t y = (((a & 0x03030303u) + (b & 0x03030303u) +
                              (c & 0x03030303u) + (d & 0x03030303u)) >> 2) &
                            0x03030303u;
         return x | y;
      }
   }
}

static inline uint32_t gln64_2xsai_read(
      const void *src, ptrdiff_t index, unsigned bytes_per_pixel)
{
   if (bytes_per_pixel == 4)
      return ((const uint32_t *)src)[index];
   return ((const uint16_t *)src)[index];
}

static inline void gln64_2xsai_write(
      void *dst, ptrdiff_t index, unsigned bytes_per_pixel, uint32_t value)
{
   if (bytes_per_pixel == 4)
      ((uint32_t *)dst)[index] = value;
   else
      ((uint16_t *)dst)[index] = (uint16_t)value;
}

static void gln64_2xsai_scale(
      const void *src,
      void *dst,
      uint16_t width,
      uint16_t height,
      int32_t clamp_s,
      int32_t clamp_t,
      enum gln64_2xsai_format format)
{
   uint16_t y;
   const unsigned bytes_per_pixel =
      (format == GLN64_2XSAI_8888) ? 4u : 2u;
   const ptrdiff_t dest_width = (ptrdiff_t)width << 1;

   if (!src || !dst || width == 0 || height == 0)
      return;

   for (y = 0; y < height; ++y)
   {
      int32_t row0, row1, row2, row3;
      uint16_t x;

      row0 = (y > 0) ? -(int32_t)width :
             (clamp_t ? 0 : (int32_t)(height - 1) * width);
      row1 = 0;

      if (y < height - 1)
      {
         row2 = width;
         row3 = (y < height - 2) ? ((int32_t)width << 1) :
                (clamp_t ? (int32_t)width : -(int32_t)y * width);
      }
      else
      {
         row2 = clamp_t ? 0 : -(int32_t)y * width;
         row3 = clamp_t ? 0 : (1 - (int32_t)y) * width;
      }

      for (x = 0; x < width; ++x)
      {
         int32_t col0, col1, col2, col3;
         uint32_t color_a, color_b, color_c, color_d;
         uint32_t color_e, color_f, color_g, color_h;
         uint32_t color_i, color_j, color_k, color_l;
         uint32_t color_m, color_n, color_o, color_p;
         uint32_t product, product1, product2;
         const ptrdiff_t current = (ptrdiff_t)y * width + x;
         const ptrdiff_t dest = ((ptrdiff_t)y << 1) * dest_width +
                                ((ptrdiff_t)x << 1);

         col0 = (x > 0) ? -1 : (clamp_s ? 0 : (int32_t)width - 1);
         col1 = 0;

         if (x < width - 1)
         {
            col2 = 1;
            col3 = (x < width - 2) ? 2 :
                   (clamp_s ? 1 : -(int32_t)x);
         }
         else
         {
            col2 = clamp_s ? 0 : -(int32_t)x;
            col3 = clamp_s ? 0 : 1 - (int32_t)x;
         }

         /* Same 4x4 neighbourhood used by FZ's glN64 2xSAI code:
          *
          *   I E F J
          *   G A B K
          *   H C D L
          *   M N O P
          */
         color_i = gln64_2xsai_read(src, current + col0 + row0, bytes_per_pixel);
         color_e = gln64_2xsai_read(src, current + col1 + row0, bytes_per_pixel);
         color_f = gln64_2xsai_read(src, current + col2 + row0, bytes_per_pixel);
         color_j = gln64_2xsai_read(src, current + col3 + row0, bytes_per_pixel);

         color_g = gln64_2xsai_read(src, current + col0 + row1, bytes_per_pixel);
         color_a = gln64_2xsai_read(src, current + col1 + row1, bytes_per_pixel);
         color_b = gln64_2xsai_read(src, current + col2 + row1, bytes_per_pixel);
         color_k = gln64_2xsai_read(src, current + col3 + row1, bytes_per_pixel);

         color_h = gln64_2xsai_read(src, current + col0 + row2, bytes_per_pixel);
         color_c = gln64_2xsai_read(src, current + col1 + row2, bytes_per_pixel);
         color_d = gln64_2xsai_read(src, current + col2 + row2, bytes_per_pixel);
         color_l = gln64_2xsai_read(src, current + col3 + row2, bytes_per_pixel);

         color_m = gln64_2xsai_read(src, current + col0 + row3, bytes_per_pixel);
         color_n = gln64_2xsai_read(src, current + col1 + row3, bytes_per_pixel);
         color_o = gln64_2xsai_read(src, current + col2 + row3, bytes_per_pixel);
         color_p = gln64_2xsai_read(src, current + col3 + row3, bytes_per_pixel);

         if (color_a == color_d && color_b != color_c)
         {
            if ((color_a == color_e && color_b == color_l) ||
                (color_a == color_c && color_a == color_f &&
                 color_b != color_e && color_b == color_j))
               product = color_a;
            else
               product = gln64_2xsai_interpolate(format, color_a, color_b);

            if ((color_a == color_g && color_c == color_o) ||
                (color_a == color_b && color_a == color_h &&
                 color_g != color_c && color_c == color_m))
               product1 = color_a;
            else
               product1 = gln64_2xsai_interpolate(format, color_a, color_c);

            product2 = color_a;
         }
         else if (color_b == color_c && color_a != color_d)
         {
            if ((color_b == color_f && color_a == color_h) ||
                (color_b == color_e && color_b == color_d &&
                 color_a != color_f && color_a == color_i))
               product = color_b;
            else
               product = gln64_2xsai_interpolate(format, color_a, color_b);

            if ((color_c == color_h && color_a == color_f) ||
                (color_c == color_g && color_c == color_d &&
                 color_a != color_h && color_a == color_i))
               product1 = color_c;
            else
               product1 = gln64_2xsai_interpolate(format, color_a, color_c);

            product2 = color_b;
         }
         else if (color_a == color_d && color_b == color_c)
         {
            if (color_a == color_b)
            {
               product = color_a;
               product1 = color_a;
               product2 = color_a;
            }
            else
            {
               int16_t r = 0;
               product1 = gln64_2xsai_interpolate(format, color_a, color_c);
               product = gln64_2xsai_interpolate(format, color_a, color_b);

               r += gln64_2xsai_result1(color_a, color_b, color_g, color_e);
               r += gln64_2xsai_result2(color_b, color_a, color_k, color_f);
               r += gln64_2xsai_result2(color_b, color_a, color_h, color_n);
               r += gln64_2xsai_result1(color_a, color_b, color_l, color_o);

               if (r > 0)
                  product2 = color_a;
               else if (r < 0)
                  product2 = color_b;
               else
                  product2 = gln64_2xsai_q_interpolate(
                     format, color_a, color_b, color_c, color_d);
            }
         }
         else
         {
            product2 = gln64_2xsai_q_interpolate(
               format, color_a, color_b, color_c, color_d);

            if (color_a == color_c && color_a == color_f &&
                color_b != color_e && color_b == color_j)
               product = color_a;
            else if (color_b == color_e && color_b == color_d &&
                     color_a != color_f && color_a == color_i)
               product = color_b;
            else
               product = gln64_2xsai_interpolate(format, color_a, color_b);

            if (color_a == color_b && color_a == color_h &&
                color_g != color_c && color_c == color_m)
               product1 = color_a;
            else if (color_c == color_g && color_c == color_d &&
                     color_a != color_h && color_a == color_i)
               product1 = color_c;
            else
               product1 = gln64_2xsai_interpolate(format, color_a, color_c);
         }

         gln64_2xsai_write(dst, dest, bytes_per_pixel, color_a);
         gln64_2xsai_write(dst, dest + 1, bytes_per_pixel, product);
         gln64_2xsai_write(dst, dest + dest_width, bytes_per_pixel, product1);
         gln64_2xsai_write(dst, dest + dest_width + 1, bytes_per_pixel, product2);
      }
   }
}

static inline void _2xSaI4444(
      uint16_t *src, uint16_t *dst, uint16_t width, uint16_t height,
      int32_t clamp_s, int32_t clamp_t)
{
   gln64_2xsai_scale(src, dst, width, height, clamp_s, clamp_t,
                     GLN64_2XSAI_4444);
}

static inline void _2xSaI5551(
      uint16_t *src, uint16_t *dst, uint16_t width, uint16_t height,
      int32_t clamp_s, int32_t clamp_t)
{
   gln64_2xsai_scale(src, dst, width, height, clamp_s, clamp_t,
                     GLN64_2XSAI_5551);
}

static inline void _2xSaI8888(
      uint32_t *src, uint32_t *dst, uint16_t width, uint16_t height,
      int32_t clamp_s, int32_t clamp_t)
{
   gln64_2xsai_scale(src, dst, width, height, clamp_s, clamp_t,
                     GLN64_2XSAI_8888);
}

#endif /* GLN64_2XSAI_H */
