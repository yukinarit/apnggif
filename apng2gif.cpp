/* apng2gif version 1.7
 *
 * This program converts APNG animations into animated GIF format.
 * Wu64 quantizer is used for true-color files.
 *
 * http://apng2gif.sourceforge.net/
 *
 * Copyright (c) 2010-2015 Max Stepin
 * maxst at users.sourceforge.net
 *
 * zlib license
 * ------------
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <png.h>     /* original (unpatched) libpng is ok */
#include <zlib.h>

#define notabc(c) ((c) < 65 || (c) > 122 || ((c) > 90 && (c) < 97))

#define id_IHDR 0x52444849
#define id_acTL 0x4C546361
#define id_fcTL 0x4C546366
#define id_IDAT 0x54414449
#define id_fdAT 0x54416466
#define id_IEND 0x444E4549

#define indx(r, g, b) ((r << 12) + (r << 7) + r + (g << 6) + g + b)

#define RED     2
#define GREEN   1
#define BLUE    0

unsigned char   pal[256][3];
unsigned int    palsize;
int             trns_idx;
unsigned int    accum;
unsigned char   bits, codesize;
unsigned char   buf[288];
unsigned char   bigcube[128][128][128];
int             wt[65*65*65];
int             mr[65*65*65];
int             mg[65*65*65];
int             mb[65*65*65];
double          m2[65*65*65];
unsigned char   tag[65*65*65];
typedef struct  {int r0; int r1; int g0; int g1; int b0; int b1; int vol; } box;
int             tlevel, bcolor, back_r, back_g, back_b;

struct CHUNK { unsigned char * p; unsigned int size; };
struct APNGFrame { unsigned char * p, ** rows; unsigned int w, h, delay_num, delay_den; };

const unsigned long cMaxPNGSize = 1000000UL;

void info_fn(png_structp png_ptr, png_infop info_ptr)
{
  png_set_expand(png_ptr);
  png_set_strip_16(png_ptr);
  png_set_gray_to_rgb(png_ptr);
  png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
  (void)png_set_interlace_handling(png_ptr);
  png_read_update_info(png_ptr, info_ptr);
}

void row_fn(png_structp png_ptr, png_bytep new_row, png_uint_32 row_num, int pass)
{
  APNGFrame * frame = (APNGFrame *)png_get_progressive_ptr(png_ptr);
  png_progressive_combine_row(png_ptr, frame->rows[row_num], new_row);
}

void compose_frame(unsigned char ** rows_dst, unsigned char ** rows_src, unsigned char bop, unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
  unsigned int  i, j;
  int u, v, al;

  for (j=0; j<h; j++)
  {
    unsigned char * sp = rows_src[j];
    unsigned char * dp = rows_dst[j+y] + x*4;

    if (bop == 0)
      memcpy(dp, sp, w*4);
    else
    for (i=0; i<w; i++, sp+=4, dp+=4)
    {
      if (sp[3] == 255)
        memcpy(dp, sp, 4);
      else
      if (sp[3] != 0)
      {
        if (dp[3] != 0)
        {
          u = sp[3]*255;
          v = (255-sp[3])*dp[3];
          al = u + v;
          dp[0] = (sp[0]*u + dp[0]*v)/al;
          dp[1] = (sp[1]*u + dp[1]*v)/al;
          dp[2] = (sp[2]*u + dp[2]*v)/al;
          dp[3] = al/255;
        }
        else
          memcpy(dp, sp, 4);
      }
    }
  }
}

inline unsigned int read_chunk(FILE * f, CHUNK * pChunk)
{
  unsigned char len[4];
  pChunk->size = 0;
  pChunk->p = 0;
  if (fread(&len, 4, 1, f) == 1)
  {
    pChunk->size = png_get_uint_32(len) + 12;
    pChunk->p = new unsigned char[pChunk->size];
    memcpy(pChunk->p, len, 4);
    if (fread(pChunk->p + 4, pChunk->size - 4, 1, f) == 1)
      return *(unsigned int *)(pChunk->p + 4);
  }
  return 0;
}

int processing_start(png_structp & png_ptr, png_infop & info_ptr, void * frame_ptr, bool hasInfo, CHUNK & chunkIHDR, std::vector<CHUNK>& chunksInfo)
{
  unsigned char header[8] = {137, 80, 78, 71, 13, 10, 26, 10};

  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  info_ptr = png_create_info_struct(png_ptr);
  if (!png_ptr || !info_ptr)
    return 1;

  if (setjmp(png_jmpbuf(png_ptr)))
  {
    png_destroy_read_struct(&png_ptr, &info_ptr, 0);
    return 1;
  }

  png_set_crc_action(png_ptr, PNG_CRC_QUIET_USE, PNG_CRC_QUIET_USE);
  png_set_progressive_read_fn(png_ptr, frame_ptr, info_fn, row_fn, NULL);

  png_process_data(png_ptr, info_ptr, header, 8);
  png_process_data(png_ptr, info_ptr, chunkIHDR.p, chunkIHDR.size);

  if (hasInfo)
    for (unsigned int i=0; i<chunksInfo.size(); i++)
      png_process_data(png_ptr, info_ptr, chunksInfo[i].p, chunksInfo[i].size);
  return 0;
}

int processing_data(png_structp png_ptr, png_infop info_ptr, unsigned char * p, unsigned int size)
{
  if (!png_ptr || !info_ptr)
    return 1;

  if (setjmp(png_jmpbuf(png_ptr)))
  {
    png_destroy_read_struct(&png_ptr, &info_ptr, 0);
    return 1;
  }

  png_process_data(png_ptr, info_ptr, p, size);
  return 0;
}

int processing_finish(png_structp png_ptr, png_infop info_ptr)
{
  unsigned char footer[12] = {0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130};

  if (!png_ptr || !info_ptr)
    return 1;

  if (setjmp(png_jmpbuf(png_ptr)))
  {
    png_destroy_read_struct(&png_ptr, &info_ptr, 0);
    return 1;
  }

  png_process_data(png_ptr, info_ptr, footer, 12);
  png_destroy_read_struct(&png_ptr, &info_ptr, 0);

  return 0;
}

int load_apng(const char * szIn, std::vector<APNGFrame>& frames, unsigned int * num_loops)
{
  FILE         * f;
  unsigned int   id, i, j, w, h, w0, h0, x0, y0;
  unsigned int   delay_num, delay_den, dop, bop, rowbytes, imagesize;
  unsigned char  sig[8];
  png_structp    png_ptr;
  png_infop      info_ptr;
  CHUNK chunk;
  CHUNK chunkIHDR;
  std::vector<CHUNK> chunksInfo;
  bool isAnimated = false;
  bool skipFirst = false;
  bool hasInfo = false;
  APNGFrame      frameRaw = {0};
  APNGFrame      frameCur = {0};
  APNGFrame      frameNext = {0};
  int res = -1;

  if ((f = fopen(szIn, "rb")) != 0)
  {
    if (fread(sig, 1, 8, f) == 8 && png_sig_cmp(sig, 0, 8) == 0)
    {
      id = read_chunk(f, &chunkIHDR);

      if (id == id_IHDR && chunkIHDR.size == 25)
      {
        w0 = w = png_get_uint_32(chunkIHDR.p + 8);
        h0 = h = png_get_uint_32(chunkIHDR.p + 12);

        if (w > cMaxPNGSize || h > cMaxPNGSize)
        {
          fclose(f);
          return res;
        }

        x0 = 0;
        y0 = 0;
        delay_num = 1;
        delay_den = 10;
        dop = 0;
        bop = 0;
        rowbytes = w * 4;
        imagesize = h * rowbytes;

        frameRaw.p = new unsigned char[imagesize];
        frameRaw.rows = new png_bytep[h * sizeof(png_bytep)];
        for (j=0; j<h; j++)
          frameRaw.rows[j] = frameRaw.p + j * rowbytes;

        if (!processing_start(png_ptr, info_ptr, (void *)&frameRaw, hasInfo, chunkIHDR, chunksInfo))
        {
          frameCur.w = w;
          frameCur.h = h;
          frameCur.p = new unsigned char[imagesize];
          frameCur.rows = new png_bytep[h * sizeof(png_bytep)];
          for (j=0; j<h; j++)
            frameCur.rows[j] = frameCur.p + j * rowbytes;

          while ( !feof(f) )
          {
            id = read_chunk(f, &chunk);
            if (!id)
              break;

            if (id == id_acTL && !hasInfo && !isAnimated)
            {
              *num_loops = png_get_uint_32(chunk.p + 12);
              isAnimated = true;
              skipFirst = true;
            }
            else
            if (id == id_fcTL && (!hasInfo || isAnimated))
            {
              if (hasInfo)
              {
                if (!processing_finish(png_ptr, info_ptr))
                {
                  frameNext.p = new unsigned char[imagesize];
                  frameNext.rows = new png_bytep[h * sizeof(png_bytep)];
                  for (j=0; j<h; j++)
                    frameNext.rows[j] = frameNext.p + j * rowbytes;

                  if (dop == 2)
                    memcpy(frameNext.p, frameCur.p, imagesize);

                  compose_frame(frameCur.rows, frameRaw.rows, bop, x0, y0, w0, h0);
                  frameCur.delay_num = delay_num;
                  frameCur.delay_den = delay_den;
                  frames.push_back(frameCur);

                  if (dop != 2)
                  {
                    memcpy(frameNext.p, frameCur.p, imagesize);
                    if (dop == 1)
                      for (j=0; j<h0; j++)
                        memset(frameNext.rows[y0 + j] + x0*4, 0, w0*4);
                  }
                  frameCur.p = frameNext.p;
                  frameCur.rows = frameNext.rows;
                }
                else
                {
                  delete[] frameCur.rows;
                  delete[] frameCur.p;
                  delete[] chunk.p;
                  break;
                }
              }

              // At this point the old frame is done. Let's start a new one.
              w0 = png_get_uint_32(chunk.p + 12);
              h0 = png_get_uint_32(chunk.p + 16);
              x0 = png_get_uint_32(chunk.p + 20);
              y0 = png_get_uint_32(chunk.p + 24);
              delay_num = png_get_uint_16(chunk.p + 28);
              delay_den = png_get_uint_16(chunk.p + 30);
              dop = chunk.p[32];
              bop = chunk.p[33];

              if (w0 > cMaxPNGSize || h0 > cMaxPNGSize || x0 > cMaxPNGSize || y0 > cMaxPNGSize
                  || x0 + w0 > w || y0 + h0 > h || dop > 2 || bop > 1)
              {
                delete[] frameCur.rows;
                delete[] frameCur.p;
                delete[] chunk.p;
                break;
              }

              if (hasInfo)
              {
                memcpy(chunkIHDR.p + 8, chunk.p + 12, 8);
                if (processing_start(png_ptr, info_ptr, (void *)&frameRaw, hasInfo, chunkIHDR, chunksInfo))
                {
                  delete[] frameCur.rows;
                  delete[] frameCur.p;
                  delete[] chunk.p;
                  break;
                }
              }
              else
                skipFirst = false;

              if (frames.size() == (skipFirst ? 1 : 0))
              {
                bop = 0;
                if (dop == 2)
                  dop = 1;
              }
            }
            else
            if (id == id_IDAT)
            {
              hasInfo = true;
              if (processing_data(png_ptr, info_ptr, chunk.p, chunk.size))
              {
                delete[] frameCur.rows;
                delete[] frameCur.p;
                delete[] chunk.p;
                break;
              }
            }
            else
            if (id == id_fdAT && isAnimated)
            {
              png_save_uint_32(chunk.p + 4, chunk.size - 16);
              memcpy(chunk.p + 8, "IDAT", 4);
              if (processing_data(png_ptr, info_ptr, chunk.p + 4, chunk.size - 4))
              {
                delete[] frameCur.rows;
                delete[] frameCur.p;
                delete[] chunk.p;
                break;
              }
            }
            else
            if (id == id_IEND)
            {
              if (hasInfo && !processing_finish(png_ptr, info_ptr))
              {
                compose_frame(frameCur.rows, frameRaw.rows, bop, x0, y0, w0, h0);
                frameCur.delay_num = delay_num;
                frameCur.delay_den = delay_den;
                frames.push_back(frameCur);
              }
              else
              {
                delete[] frameCur.rows;
                delete[] frameCur.p;
              }
              delete[] chunk.p;
              break;
            }
            else
            if (notabc(chunk.p[4]) || notabc(chunk.p[5]) || notabc(chunk.p[6]) || notabc(chunk.p[7]))
            {
              delete[] chunk.p;
              break;
            }
            else
            if (!hasInfo)
            {
              if (processing_data(png_ptr, info_ptr, chunk.p, chunk.size))
              {
                delete[] frameCur.rows;
                delete[] frameCur.p;
                delete[] chunk.p;
                break;
              }
              chunksInfo.push_back(chunk);
              continue;
            }
            delete[] chunk.p;
          }
        }
        delete[] frameRaw.rows;
        delete[] frameRaw.p;

        if (!frames.empty())
          res = (skipFirst) ? 0 : 1;
      }

      for (i=0; i<chunksInfo.size(); i++)
        delete[] chunksInfo[i].p;

      chunksInfo.clear();
      delete[] chunkIHDR.p;
    }

    fclose(f);
  }

  return res;
}

void WuHistogram(std::vector<APNGFrame>& frames, unsigned char * pAGIF, unsigned int size)
{
  int i, j, r, g, b, inr, ing, inb;
  int table[256];

  for (i=0; i<256; i++)
    table[i]=i*i;

  memset(wt, 0, 65*65*65*sizeof(int));
  memset(mr, 0, 65*65*65*sizeof(int));
  memset(mg, 0, 65*65*65*sizeof(int));
  memset(mb, 0, 65*65*65*sizeof(int));
  memset(m2, 0, 65*65*65*sizeof(double));

  unsigned int * dp = (unsigned int *)pAGIF;
  for (size_t n=0; n<frames.size(); n++)
  {
    unsigned char * sp = frames[n].p;
    for (unsigned int x=0; x<size; x++)
    {
      r = *sp++;
      g = *sp++;
      b = *sp++;
      if (*sp++ >= tlevel)
      {
        inr = (r>>2)+1;
        ing = (g>>2)+1;
        inb = (b>>2)+1;
        *dp++ = j = indx(inr, ing, inb);
        wt[j]++;
        mr[j] += r;
        mg[j] += g;
        mb[j] += b;
        m2[j] += (double)(table[r]+table[g]+table[b]);
      }
      else
        *dp++ = 0;
    }
  }
}

void WuMoments()
{
  unsigned int      ind1, ind2;
  unsigned char     i, r, g, b;
  int               line, line_r, line_g, line_b;
  double            line2;
  int               area[65], area_r[65], area_g[65], area_b[65];
  double            area2[65];

  for (r=1; r<=64; r++)
  {
    for (i=0; i<=64; i++)
    {
      area[i] = area_r[i] = area_g[i] = area_b[i] = 0;
      area2[i] = (double)0.0;
    }

    for (g=1; g<=64; g++)
    {
      line = line_r = line_g = line_b = 0;
      line2 = (double)0.0;

      for (b=1; b<=64; b++)
      {
        ind1 = indx(r, g, b);
        line += wt[ind1];
        line_r += mr[ind1];
        line_g += mg[ind1];
        line_b += mb[ind1];
        line2 += m2[ind1];
        area[b] += line;
        area_r[b] += line_r;
        area_g[b] += line_g;
        area_b[b] += line_b;
        area2[b] += line2;
        ind2 = ind1 - 65*65;
        wt[ind1] = wt[ind2] + area[b];
        mr[ind1] = mr[ind2] + area_r[b];
        mg[ind1] = mg[ind2] + area_g[b];
        mb[ind1] = mb[ind2] + area_b[b];
        m2[ind1] = m2[ind2] + area2[b];
      }
    }
  }
}

int WuCubeStat(box *cube, int *m)
{
  return ( m[indx(cube->r1, cube->g1, cube->b1)]
         - m[indx(cube->r1, cube->g1, cube->b0)]
         - m[indx(cube->r1, cube->g0, cube->b1)]
         + m[indx(cube->r1, cube->g0, cube->b0)]
         - m[indx(cube->r0, cube->g1, cube->b1)]
         + m[indx(cube->r0, cube->g1, cube->b0)]
         + m[indx(cube->r0, cube->g0, cube->b1)]
         - m[indx(cube->r0, cube->g0, cube->b0)] );
}

int WuBottomStat(box * cube, unsigned char dir, int *m)
{
  switch(dir)
  {
    case RED:
      return ( -m[indx(cube->r0, cube->g1, cube->b1)]
               +m[indx(cube->r0, cube->g1, cube->b0)]
               +m[indx(cube->r0, cube->g0, cube->b1)]
               -m[indx(cube->r0, cube->g0, cube->b0)] );
      break;
    case GREEN:
      return ( -m[indx(cube->r1, cube->g0, cube->b1)]
               +m[indx(cube->r1, cube->g0, cube->b0)]
               +m[indx(cube->r0, cube->g0, cube->b1)]
               -m[indx(cube->r0, cube->g0, cube->b0)] );
      break;
    case BLUE:
      return ( -m[indx(cube->r1, cube->g1, cube->b0)]
               +m[indx(cube->r1, cube->g0, cube->b0)]
               +m[indx(cube->r0, cube->g1, cube->b0)]
               -m[indx(cube->r0, cube->g0, cube->b0)] );
      break;
  }
  return 0;
}

int WuTopStat(box * cube, unsigned char dir, int pos, int *m)
{
  switch(dir)
  {
    case RED:
      return ( m[indx(pos, cube->g1, cube->b1)]
              -m[indx(pos, cube->g1, cube->b0)]
              -m[indx(pos, cube->g0, cube->b1)]
              +m[indx(pos, cube->g0, cube->b0)] );
      break;
    case GREEN:
      return ( m[indx(cube->r1, pos, cube->b1)]
              -m[indx(cube->r1, pos, cube->b0)]
              -m[indx(cube->r0, pos, cube->b1)]
              +m[indx(cube->r0, pos, cube->b0)] );
      break;
    case BLUE:
      return ( m[indx(cube->r1, cube->g1, pos)]
              -m[indx(cube->r1, cube->g0, pos)]
              -m[indx(cube->r0, cube->g1, pos)]
              +m[indx(cube->r0, cube->g0, pos)] );
      break;
  }
  return 0;
}

double WuVariance(box * cube)
{
  double dr, dg, db, dt, xx;

  dr = (double)WuCubeStat(cube, mr);
  dg = (double)WuCubeStat(cube, mg);
  db = (double)WuCubeStat(cube, mb);
  dt = (double)WuCubeStat(cube, wt);
  xx =  m2[indx(cube->r1, cube->g1, cube->b1)]
       -m2[indx(cube->r1, cube->g1, cube->b0)]
       -m2[indx(cube->r1, cube->g0, cube->b1)]
       +m2[indx(cube->r1, cube->g0, cube->b0)]
       -m2[indx(cube->r0, cube->g1, cube->b1)]
       +m2[indx(cube->r0, cube->g1, cube->b0)]
       +m2[indx(cube->r0, cube->g0, cube->b1)]
       -m2[indx(cube->r0, cube->g0, cube->b0)];

  return( xx - (dr*dr+dg*dg+db*db)/dt );
}

double WuMaximize(box * cube, unsigned char dir, int first, int last, int *cut, int whole_r, int whole_g, int whole_b, int whole_w)
{
  int    half_r, half_g, half_b, half_w;
  int    base_r, base_g, base_b, base_w;
  int    i;
  double temp, res;

  base_r = WuBottomStat(cube, dir, mr);
  base_g = WuBottomStat(cube, dir, mg);
  base_b = WuBottomStat(cube, dir, mb);
  base_w = WuBottomStat(cube, dir, wt);
  res = (double)0.0;
  *cut = -1;

  for (i=first; i<last; i++)
  {
    half_r = base_r + WuTopStat(cube, dir, i, mr);
    half_g = base_g + WuTopStat(cube, dir, i, mg);
    half_b = base_b + WuTopStat(cube, dir, i, mb);
    half_w = base_w + WuTopStat(cube, dir, i, wt);

    if (half_w == 0)
      continue;
    else
      temp = ((double)half_r*half_r + (double)half_g*half_g + (double)half_b*half_b)/half_w;

    half_r = whole_r - half_r;
    half_g = whole_g - half_g;
    half_b = whole_b - half_b;
    half_w = whole_w - half_w;

    if (half_w == 0)
      continue;
    else
      temp += ((double)half_r*half_r + (double)half_g*half_g + (double)half_b*half_b)/half_w;

    if (temp > res) { res = temp; *cut = i; }
  }

  return res;
}

int WuCut(box * set1, box * set2)
{
  unsigned char dir;
  double        maxr, maxg, maxb;
  int           cutr, cutg, cutb;
  int           whole_r, whole_g, whole_b, whole_w;

  whole_r = WuCubeStat(set1, mr);
  whole_g = WuCubeStat(set1, mg);
  whole_b = WuCubeStat(set1, mb);
  whole_w = WuCubeStat(set1, wt);

  maxr = WuMaximize(set1, RED,   set1->r0+1, set1->r1, &cutr, whole_r, whole_g, whole_b, whole_w);
  maxg = WuMaximize(set1, GREEN, set1->g0+1, set1->g1, &cutg, whole_r, whole_g, whole_b, whole_w);
  maxb = WuMaximize(set1, BLUE,  set1->b0+1, set1->b1, &cutb, whole_r, whole_g, whole_b, whole_w);

  if (maxr >= maxg && maxr >= maxb)
  {
    dir = RED;
    if (cutr < 0)
      return 0;
  }
  else
  if (maxg >= maxr && maxg >= maxb)
    dir = GREEN;
  else
    dir = BLUE;

  set2->r1 = set1->r1;
  set2->g1 = set1->g1;
  set2->b1 = set1->b1;

  switch (dir)
  {
    case RED:
      set2->r0 = set1->r1 = cutr;
      set2->g0 = set1->g0;
      set2->b0 = set1->b0;
      break;
    case GREEN:
      set2->g0 = set1->g1 = cutg;
      set2->r0 = set1->r0;
      set2->b0 = set1->b0;
      break;
    case BLUE:
      set2->b0 = set1->b1 = cutb;
      set2->r0 = set1->r0;
      set2->g0 = set1->g0;
      break;
  }

  set1->vol = (set1->r1-set1->r0)*(set1->g1-set1->g0)*(set1->b1-set1->b0);
  set2->vol = (set2->r1-set2->r0)*(set2->g1-set2->g0)*(set2->b1-set2->b0);

  return 1;
}

void apng_to_agif(std::vector<APNGFrame>& frames, unsigned char * pAGIF)
{
  int               i, j, k, m, trans, colors8, colors7, weight;
  unsigned int      c, n, x;
  unsigned char     r, g, b, a;
  unsigned char  *  sp;
  unsigned char  *  dp;
  unsigned int   *  tp;
  unsigned int      bit[256];
  box               cube[256];
  double            temp;
  double            vv[256];
  unsigned int      size = frames[0].w * frames[0].h;

  memset(pal, 0, 256*3);

  for (i=0; i<256; i++)
    bit[i] = ((i>>7)&1) + ((i>>6)&1) + ((i>>5)&1) + ((i>>4)&1) + ((i>>3)&1) + ((i>>2)&1) + ((i>>1)&1) + (i&1);

  memset(bigcube, 0, 128*128*128);
  trans = 0;

  if (bcolor == -1)
  {
    for (n=0; n<frames.size(); n++)
    {
      sp = frames[n].p;
      for (x=0; x<size; x++)
      {
        r = *sp++;
        g = *sp++;
        b = *sp++;
        if (*sp++ >= tlevel)
          bigcube[r>>1][g>>1][b>>1] |= (1<<(((r&1)<<2) + ((g&1)<<1) + (b&1)));
        else
          trans = 1;
      }
    }
  }
  else
  {
    for (n=0; n<frames.size(); n++)
    {
      sp = frames[n].p;
      for (x=0; x<size; x++, sp+=4)
      {
        r = sp[0];
        g = sp[1];
        b = sp[2];
        a = sp[3];
        if (a > 0)
        {
          if (a < 255)
          {
            sp[0] = r = (r*a + back_r*(255-a))/255;
            sp[1] = g = (g*a + back_g*(255-a))/255;
            sp[2] = b = (b*a + back_b*(255-a))/255;
            sp[3] = 255;
          }
          bigcube[r>>1][g>>1][b>>1] |= (1<<(((r&1)<<2) + ((g&1)<<1) + (b&1)));
        }
        else
          trans = 1;
      }
    }
    pal[0][0] = back_r;
    pal[0][1] = back_g;
    pal[0][2] = back_b;
  }

  colors8 = colors7 = trans;

  for (i=0; i<128; i++)
  for (j=0; j<128; j++)
  for (k=0; k<128; k++)
  if ((a = bigcube[i][j][k]) != 0)
  {
    colors8 += bit[a];
    colors7++;
  }

  if (colors8<=256)
  {
    if (colors8<256)
      trans = 1;

    palsize = trans;
    trns_idx = -1;
    if (trans)
      trns_idx = 0;

    for (i=0; i<128; i++)
    for (j=0; j<128; j++)
    for (k=0; k<128; k++)
    if ((a = bigcube[i][j][k]) != 0)
    {
      for (m=0; m<8; m++, a>>=1)
      if (a&1)
      {
        pal[palsize][0] = (i<<1) + ((m>>2)&1);
        pal[palsize][1] = (j<<1) + ((m>>1)&1);
        pal[palsize][2] = (k<<1) + (m&1);
        palsize++;
      }
    }

    dp = pAGIF;
    for (n=0; n<frames.size(); n++)
    {
      sp = frames[n].p;
      for (x=0; x<size; x++)
      {
        r = *sp++;
        g = *sp++;
        b = *sp++;
        if (*sp++ >= tlevel)
        {
          for (c=trans; c<palsize; c++)
            if (r==pal[c][0] && g==pal[c][1] && b==pal[c][2])
              break;
          *dp++ = c;
        }
        else
          *dp++ = 0;
      }
    }
  }
  else
  if (colors7<=256)
  {
    if (colors7<256)
      trans = 1;

    palsize = trans;
    trns_idx = -1;
    if (trans)
      trns_idx = 0;

    for (i=0; i<128; i++)
    for (j=0; j<128; j++)
    for (k=0; k<128; k++)
    if (bigcube[i][j][k] != 0)
    {
      pal[palsize][0] = (i<<1);
      pal[palsize][1] = (j<<1);
      pal[palsize][2] = (k<<1);
      palsize++;
    }

    dp = pAGIF;
    for (n=0; n<frames.size(); n++)
    {
      sp = frames[n].p;
      for (x=0; x<size; x++)
      {
        r = *sp++;
        g = *sp++;
        b = *sp++;
        if (*sp++ >= tlevel)
        {
          r &= 0xFE;
          g &= 0xFE;
          b &= 0xFE;
          for (c=trans; c<palsize; c++)
            if (r==pal[c][0] && g==pal[c][1] && b==pal[c][2])
              break;
          *dp++ = c;
        }
        else
          *dp++ = 0;
      }
    }
  }
  else
  {
    WuHistogram(frames, pAGIF, size);
    WuMoments();

    trns_idx = 0;
    m = 1;
    cube[1].r0 = cube[1].g0 = cube[1].b0 = 0;
    cube[1].r1 = cube[1].g1 = cube[1].b1 = 64;

    for (i=2; i<256; i++)
    {
      if (WuCut(&cube[m], &cube[i]))
      {
        vv[m] = (cube[m].vol>1) ? WuVariance(&cube[m]) : (double)0.0;
        vv[i] = (cube[i].vol>1) ? WuVariance(&cube[i]) : (double)0.0;
      }
      else
      {
        vv[m] = (double)0.0;
        i--;
      }
      palsize = i+1;
      m = 1; temp = vv[1];
      for (k=2; k<=i; k++)
      if (vv[k] > temp)
      {
        temp = vv[k]; m = k;
      }
      if (temp <= 0.0)
        break;
    }

    memset(tag, 0, 65*65*65);

    for (c=1; c<palsize; c++)
    {
      for (i=cube[c].r0+1; i<=cube[c].r1; i++)
      for (j=cube[c].g0+1; j<=cube[c].g1; j++)
      for (k=cube[c].b0+1; k<=cube[c].b1; k++)
        tag[indx(i, j, k)] = c;

      weight = WuCubeStat(&cube[c], wt);

      if (weight)
      {
        pal[c][0] = WuCubeStat(&cube[c], mr) / weight;
        pal[c][1] = WuCubeStat(&cube[c], mg) / weight;
        pal[c][2] = WuCubeStat(&cube[c], mb) / weight;
      }
      else
      {
        pal[c][0] = pal[c][1] = pal[c][2] = 0;
      }
    }

    tp = (unsigned int *)pAGIF;
    dp = pAGIF;
    for (n=0; n<frames.size(); n++)
      for (x=0; x<size; x++)
        *dp++ = tag[*tp++];
  }
}

int outcode(unsigned int code, FILE * f1)
{
  accum += code << bits;
  bits += codesize;
  while (bits >= 8)
  {
    buf[++buf[0]] = accum & 255;
    accum >>= 8;
    bits -= 8;
    if (buf[0] == 255)
    {
      if (fwrite(buf, 1, 256, f1) != 256) return 1;
      buf[0] = 0;
    }
  }
  return 0;
}

int EncodeLZW(unsigned char * data, int datasize, FILE * f1, unsigned char mincodesize)
{
  int hash[65536];
  int dict[65536];
  int i;
  int clearcode, nextcode, cw, addr, w;
  unsigned char c;

  if (mincodesize == 1) mincodesize++;
  if (fwrite(&mincodesize, 1, 1, f1) != 1) return 1;
  memset(&hash, 0, sizeof(hash));
  clearcode = 1 << mincodesize;
  codesize = mincodesize + 1;
  nextcode = clearcode + 2;
  accum = 0;
  bits = 0;
  buf[0] = 0;
  if (outcode(clearcode, f1)) return 1;

  w = *data++;
  for (i=1; i<datasize; i++)
  {
    c = *data++;
    cw = (c << 16) + w;
    addr = (w << 4) | c;

    while (hash[addr] != 0 && dict[addr] != cw)
    {
      addr++;
      addr &= 0xFFFF;
    }

    if (hash[addr] == 0)
    {
      hash[addr] = nextcode++;
      dict[addr] = cw;
      if (outcode(w, f1)) return 1;
      if (nextcode-1 == 1 << codesize)
        codesize++;
      w = c;
      if (nextcode == 4094)
      {
        if (outcode(clearcode, f1)) return 1;
        memset(&hash, 0, sizeof(hash));
        codesize = mincodesize + 1;
        nextcode = clearcode + 2;
      }
    }
    else
      w = hash[addr];
  }
  if (outcode(w, f1)) return 1;
  if (outcode(clearcode + 1, f1)) return 1;

  if (bits)
    buf[++buf[0]] = accum & 255;

  buf[buf[0]+1] = 0;
  if (fwrite(buf, 1, buf[0]+2, f1)-2 != buf[0]) return 1;
  return 0;
}

int save_agif(const char* szOut, std::vector<APNGFrame>& frames, unsigned char * pAGIF, unsigned int num_loops)
{
  unsigned char gif_head[13] = {'G', 'I', 'F', '8', '9', 'a', 0, 0, 0, 0, 0, 0, 0};
  unsigned char netscape[19] = {0x21, 0xFF, 0x0B, 'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0', 3, 1, 0, 0, 0};
  unsigned char gce[8]       = {0x21, 0xF9, 4, 4, 10, 0, 0, 0};
  unsigned char img_head[10] = {0x2C, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  unsigned char bits = 7;
  unsigned char end = 0x3B;
  unsigned char trns_bit = (trns_idx >= 0) ? 1 : 0;
  unsigned char *ptemp;
  unsigned char *pa;
  unsigned char *pb;
  unsigned char *pc;
  unsigned int  i, j, n, disp0, x0, y0, w0, h0, x1, y1, w1, h1;
  unsigned int  x_min, y_min, x_max, y_max;
  unsigned int  num_frames = frames.size();
  unsigned int  w = frames[0].w;
  unsigned int  h = frames[0].h;
  unsigned int  imagesize = w * h;
  int           res = 1;
  FILE        * f1;

  if ((f1 = fopen(szOut, "wb")) == 0)
  {
    printf("Error: can't open '%s'\n", szOut);
    return res;
  }

  ptemp = new unsigned char[imagesize];

  memcpy(gif_head+6, &w, 2);
  memcpy(gif_head+8, &h, 2);
  memcpy(netscape+16, &num_loops, 2);
  memcpy(img_head+5, &w, 2);
  memcpy(img_head+7, &h, 2);
  if (trns_idx >= 0)
  {
    gif_head[11] = (unsigned char)trns_idx;
    gce[6] = (unsigned char)trns_idx;
  }

  do
  {
    if (palsize <= 2)   bits = 0; else
    if (palsize <= 4)   bits = 1; else
    if (palsize <= 8)   bits = 2; else
    if (palsize <= 16)  bits = 3; else
    if (palsize <= 32)  bits = 4; else
    if (palsize <= 64)  bits = 5; else
    if (palsize <= 128) bits = 6;

    gif_head[10] = 0x80 + 0x11*bits;
    bits++;
    palsize = 1 << bits;

    if (fwrite(gif_head, 1, 13, f1) != 13) break;
    if (fwrite(pal, 3, palsize, f1) != palsize) break;
    if (fwrite(netscape, 1, 19, f1) != 19) break;

    x0 = x1 = 0;
    y0 = y1 = 0;
    w0 = w1 = w;
    h0 = h1 = h;

    for (n=0; n<num_frames; n++)
    {
      disp0 = 1;

      if (n < num_frames-1)
      {
        x_min = y_min = w+h;
        x_max = y_max = 0;
        pa = pAGIF + imagesize*n;
        pb = pa + imagesize;

        for (j=0; j<h; j++)
        for (i=0; i<w; i++)
        {
          if (*pa != *pb)
          {
            if (i < x_min) x_min = i;
            if (i > x_max) x_max = i;
            if (j < y_min) y_min = j;
            if (j > y_max) y_max = j;
            if (trns_idx >= 0)
            {
              if (*pb == trns_idx && *pa != trns_idx)
              {
                disp0 = 2;
                if (i < x0)     { w0 += x0-i; x0 = i; };
                if (i >= x0+w0) { w0 = i-x0+1; };
                if (j < y0)     { h0 += y0-j; y0 = j; };
                if (j >= y0+h0) { h0 = j-y0+1; };
              }
            }
          }
          pa++;
          pb++;
        }

        if (x_min <= x_max)
        {
          x1 = x_min;
          y1 = y_min;
          w1 = x_max-x_min+1;
          h1 = y_max-y_min+1;
        }
        else
        {
          x1 = y1 = 0;
          w1 = h1 = 1;
        }
      }

      pc = ptemp;
      pb = pAGIF + imagesize*n + y0*w + x0;

      if (trns_idx >= 0 && n > 0)
      {
        pa = pb - imagesize;
        for (j=0; j<h0; j++, pa += w, pb += w, pc += w0)
          for (i=0; i<w0; i++)
            pc[i] = (pa[i] != pb[i]) ? pb[i] : trns_idx;
      }
      else
      {
        for (j=0; j<h0; j++, pb += w, pc += w0)
          memcpy(pc, pb, w0);
      }

      if (num_frames > 1 || trns_bit)
      {
        gce[3] = (disp0<<2) + trns_bit;
        if (!frames[n].delay_den)
          memcpy(gce+4, &frames[n].delay_num, 2);
        else
        {
          unsigned int delay_ms = frames[n].delay_num * 100 / frames[n].delay_den;
          memcpy(gce+4, &delay_ms, 2);
        }
        if (fwrite(gce, 1, 8, f1) != 8) break;
      }

      memcpy(img_head+1, &x0, 2);
      memcpy(img_head+3, &y0, 2);
      memcpy(img_head+5, &w0, 2);
      memcpy(img_head+7, &h0, 2);
      if (fwrite(img_head, 1, 10, f1) != 10) break;

      if (EncodeLZW(ptemp, w0*h0, f1, bits)) break;

      if (disp0 == 2)
      {
        pb = pAGIF + imagesize*n + y0*w + x0;
        for (j=0; j<h0; j++, pb += w)
          memset(pb, trns_idx, w0);

        x_min = y_min = w+h;
        x_max = y_max = 0;
        pa = pAGIF + imagesize*n;
        pb = pa + imagesize;

        for (j=0; j<h; j++)
        for (i=0; i<w; i++)
        {
          if (*pa++ != *pb++)
          {
            if (i < x_min) x_min = i;
            if (i > x_max) x_max = i;
            if (j < y_min) y_min = j;
            if (j > y_max) y_max = j;
          }
        }

        if (x_min <= x_max)
        {
          x1 = x_min;
          y1 = y_min;
          w1 = x_max-x_min+1;
          h1 = y_max-y_min+1;
        }
        else
        {
          x1 = y1 = 0;
          w1 = h1 = 1;
        }
      }

      x0 = x1;
      y0 = y1;
      w0 = w1;
      h0 = h1;
    }

    if (fwrite(&end, 1, 1, f1) != 1) break;
    res = 0;

  } while (0);

  delete[] ptemp;
  fclose(f1);
  return res;
}

// apng2gif
//
// png: path to source png file.
// gif: path to destination gif file.
// tlevel: transparency threshold level
// bcolor: background blend color (format: "#808080")
int apng2gif(const char* png, const char* gif, int tl, const char* bc)
{
  unsigned int num_loops = 0;
  std::vector<APNGFrame> frames;

  tlevel = 128;
  bcolor = -1;
  back_r = back_g = back_b = 0;

  if (tl < 1 || tl > 255) {
    tlevel = 128;
  }

  if (bc[0] == '#')
  {
    sscanf(bc+1, "%x", (unsigned int*)(&bcolor));
    back_r = (bcolor>>16)&0xFF;
    back_g = (bcolor>>8)&0xFF;
    back_b = (bcolor)&0xFF;
  }

  if (bcolor != -1)
    tlevel = 1;

  int res = load_apng(png, frames, &num_loops);
  if (res < 0)
  {
    printf("load_apng() failed: '%s'\n", png);
    return 1;
  }

  // zero means first frame is hidden, so let's remove it (unless it's the only frame)
  if (!res && frames.size() > 1)
  {
    delete[] frames[0].rows;
    delete[] frames[0].p;
    frames.erase(frames.begin());
  }

  if (!frames.empty())
  {
    unsigned int w = frames[0].w;
    unsigned int h = frames[0].h;
    unsigned int num_frames = frames.size();

    unsigned char * pAGIF = new unsigned char[w * h * num_frames * 4];

    apng_to_agif(frames, pAGIF);

    if (save_agif(gif, frames, pAGIF, num_loops) != 0)
    {
      printf("save_agif() failed: '%s'\n", gif);
      return 1;
    }

    for (size_t n=0; n<frames.size(); n++)
    {
      delete[] frames[n].rows;
      delete[] frames[n].p;
    }
    frames.clear();

    delete[] pAGIF;
  }

  return 0;
}
