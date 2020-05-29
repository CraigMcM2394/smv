#include "options.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "string_util.h"
#include "file_util.h"
#include "datadefs.h"
#include "MALLOCC.h"
#include "gd.h"
#include "gdfontg.h"
#include "dem_grid.h"
#include "dem_util.h"

#define LONGLATREF_NONE -1
#define LONGLATREF_ORIG 0
#define LONGLATREF_CENTER 1
#define LONGLATREF_MINMAX 2

/* ------------------ SphereDistance ------------------------ */

float SphereDistance(float llong1, float llat1, float llong2, float llat2){
  // https://en.wikipedia.org/wiki/Great-circle_distance
  // a = sin(dlat/2)^2 + cos(lat1)*cos(lat2)*sin(dlong/2)^2
  // c = 2*asin(sqrt(a))
  // d = R*c
  // R = RAD_EARTH

  float deg2rad;
  float a, c;
  float dlat, dlong;

  deg2rad = 4.0*atan(1.0)/180.0;
  llat1 *= deg2rad;
  llat2 *= deg2rad;
  llong1 *= deg2rad;
  llong2 *= deg2rad;
  dlat = llat2-llat1;
  dlong = llong2-llong1;
  a = pow(sin(dlat/2.0), 2)+cos(llat1)*cos(llat2)*pow(sin(dlong/2.0), 2);
  c = 2.0 * asin(sqrt(ABS(a)));
  return EARTH_RADIUS*c;
}

/* ------------------ ParseInput ------------------------ */

griddata *ParseInput(char *file){
  FILE *stream_in;
  int nlongs = 100, nlats = 100;
  int kbar=10;
  float xmax = -1000.0, ymax = -1000.0, zmin = -1000.0, zmax = -1000.0;
  float longref = -1000.0, latref = -1000.0;
  float xref = 0.0, yref = 0.0;
  float dlat, dlong;
  float fds_long_min, fds_long_max, fds_lat_min, fds_lat_max;
  griddata *inputdata;

  if(file==NULL||strlen(file)==0)return NULL;
  NewMemory((void **)&(inputdata), sizeof(griddata));
  NewMemory((void **)&(inputdata->file), strlen(file)+1);
  NewMemory((void **)&(inputdata->image_file), strlen(file)+5);
  inputdata->vals = NULL;
  strcpy(inputdata->file, file);
  strcpy(inputdata->image_file, file);
  inputdata->image_ncols = 2000;
  inputdata->image_nrows = 2000;
  char *ext = strrchr(inputdata->image_file, '.');
  if(ext!=NULL){
    ext[0] = 0;
  }
  strcat(inputdata->image_file, ".png");
  inputdata->image = NULL;

  stream_in = fopen(file, "r");
  if(stream_in==NULL) {
    FreeGridData(inputdata);
    fprintf(stderr, "***error: unable to open file %s for input\n", file);
    return NULL;
  }

  // pass 1

  nexcludeinfo = 0;
  while(!feof(stream_in)){
    char buffer[LEN_BUFFER], *buffer2;

    CheckMemory;

    if(fgets(buffer, LEN_BUFFER, stream_in)==NULL)break;
    buffer2 = strstr(buffer, "//");
    if(buffer2!=NULL)buffer2[0] = 0;
    buffer2 = TrimFrontBack(buffer);
    if(strlen(buffer2)==0)continue;

    if(Match(buffer, "BUFF_DIST")==1) {
      if(fgets(buffer, LEN_BUFFER, stream_in)==NULL)break;
      sscanf(buffer, "%f", &buff_dist);
      continue;
    }
    if(Match(buffer, "GRID")==1){
      nlongs = 10;
      nlats = 10;
      kbar = 10;
      if(fgets(buffer, LEN_BUFFER, stream_in)==NULL)break;
      sscanf(buffer, "%i %i %i %f %f %f %f", &nlongs, &nlats, &kbar, &xmax, &ymax, &zmin, &zmax);
      continue;
    }
    if(Match(buffer, "EXCLUDE")==1){
      nexcludeinfo++;
      continue;
    }
  }

  if(nexcludeinfo>0){
    NewMemory((void **)&excludeinfo, nexcludeinfo*sizeof(excludedata));
  }

  // pass 2

  rewind(stream_in);
  nexcludeinfo = 0;
  while(!feof(stream_in)){
    char buffer[LEN_BUFFER], *buffer2;

    CheckMemory;

    if(fgets(buffer, LEN_BUFFER, stream_in)==NULL)break;
    buffer2 = strstr(buffer, "//");
    if(buffer2!=NULL)buffer2[0] = 0;
    buffer2 = TrimFrontBack(buffer);
    if(strlen(buffer2)==0)continue;

    if(Match(buffer, "LONGLATORIG")==1){
      if(fgets(buffer, LEN_BUFFER, stream_in)==NULL)break;
      sscanf(buffer, "%f %f", &longref, &latref);

      xref = 0.0;
      yref = 0.0;
      dlat = ymax/EARTH_RADIUS;
      fds_lat_max = latref+RAD2DEG*dlat;
      fds_lat_min = latref;
      dlong = ABS(2.0*asin(sin(xmax/(2.0*EARTH_RADIUS))/cos(DEG2RAD*latref)));
      fds_long_max = longref+RAD2DEG*dlong;
      fds_long_min = longref;
      continue;
    }

    // a = sin(dlat/2)^2 + cos(lat1)*cos(lat2)*sin(dlong/2)^2
    // c = 2*asin(sqrt(a))
    // d = R*c
    // R = RAD_EARTH

    // dlat = 0 ==> d = 2*R*asin(cos(lat1)*sin(dlong/2))
    //              dlong = 2*asin(sin(d/(2*R))/cos(lat1))
    // dlong = 0 ==> d = R*dlat
    //               dlat = d/R

    if(Match(buffer, "LONGLATCENTER")==1){
      if(fgets(buffer, LEN_BUFFER, stream_in)==NULL)break;
      sscanf(buffer, "%f %f", &longref, &latref);
      xref = xmax/2.0;
      yref = ymax/2.0;
      dlat = yref/EARTH_RADIUS;
      fds_lat_max = latref+RAD2DEG*dlat;
      fds_lat_min = latref-RAD2DEG*dlat;
      dlong = ABS(2.0*asin(sin(xref/(2.0*EARTH_RADIUS))/cos(DEG2RAD*latref)));
      fds_long_max = longref+RAD2DEG*dlong;
      fds_long_min = longref-RAD2DEG*dlong;
      continue;
    }

    if(Match(buffer, "MESH")==1){
      if(fgets(buffer, LEN_BUFFER, stream_in)==NULL)break;
      sscanf(buffer, "%i %i", &nmeshx, &nmeshy);
      nmeshx = MAX(1, nmeshx);
      nmeshy = MAX(1, nmeshy);
    }

    if(Match(buffer, "LONGLATMINMAX")==1){
      fds_long_min = -1000.0;
      fds_long_max = -1000.0;
      fds_lat_min = -1000.0;
      fds_lat_max = -1000.0;
      if(fgets(buffer, LEN_BUFFER, stream_in)==NULL)break;
      sscanf(buffer, "%f %f %f %f", &fds_long_min, &fds_long_max, &fds_lat_min, &fds_lat_max);
      longref = (fds_long_min+fds_long_max)/2.0;
      latref = (fds_lat_min+fds_lat_max)/2.0;
      xmax = SphereDistance(fds_long_min, latref, fds_long_max, latref);
      ymax = SphereDistance(longref, fds_lat_min, longref, fds_lat_max);
      xref = xmax/2.0;
      yref = ymax/2.0;
      continue;
    }

    if(Match(buffer, "EXCLUDE")==1){
      excludedata *exi;

      exi = excludeinfo+nexcludeinfo++;
      if(fgets(buffer, LEN_BUFFER, stream_in)==NULL)break;
      exi->xmin = -1.0;
      exi->xmax = -1.0;
      exi->ymin = -1.0;
      exi->ymax = -1.0;
      sscanf(buffer, "%f %f %f %f", &exi->xmin, &exi->ymin, &exi->xmax, &exi->ymax);
      continue;
    }
  }
  fclose(stream_in);

  inputdata->latmin = fds_lat_min;
  inputdata->have_latmin = 1;

  inputdata->latmax = fds_lat_max;
  inputdata->have_latmax = 1;

  inputdata->longmin = fds_long_min;
  inputdata->have_longmin = 1;

  inputdata->longmax = fds_long_max;
  inputdata->have_longmax = 1;

  inputdata->image_nrows = inputdata->image_ncols*(fds_lat_max-fds_lat_min)/(fds_long_max-fds_long_min);

  inputdata->ncols = nlongs;
  inputdata->have_ncols = 1;

  inputdata->nz = kbar;

  inputdata->nrows = nlats;
  inputdata->have_nrows = 1;

  inputdata->xmax = xmax;
  inputdata->ymax = ymax;

  inputdata->zmin = zmin;
  inputdata->zmax = zmax;

  return inputdata;
}

/* ------------------ CopyString ------------------------ */

void CopyString(char *cval, char **p, int len, int *val) {
  if(**p == '0') {
    strncpy(cval, *p + 1, len - 1);
    cval[len - 1] = 0;
  }
  else {
    strncpy(cval, *p, len);
    cval[len] = 0;
  }
  *p += len;

  if(val == NULL)return;
  sscanf(cval, "%i", val);
}

/* ------------------ GetLongLats ------------------------ */

void GetLongLats(
  float longref, float latref, float xref, float yref,
  float xmax, float ymax, int nxmax, int nymax,
  float *longlats
) {
  int j;
  float dx, dy;

  dy = ymax / (float)(nymax - 1);
  dx = xmax / (float)(nxmax - 1);

  for(j = nymax - 1; j >= 0; j--) {
    int i;
    float dlat, dyval, coslat;

    dyval = (float)j*dy - yref;
    dlat = dyval / EARTH_RADIUS;
    coslat = cos(DEG2RAD*latref + dlat);
    for(i = 0; i < nxmax; i++) {
      float dxval, top, dlong;

      dxval = (float)i*dx - xref;
      top = sin(dxval / (2.0*EARTH_RADIUS));
      dlong = 2.0*asin(top / coslat);
      *longlats++ = longref + RAD2DEG*dlong;
      *longlats++ = latref + RAD2DEG*dlat;
    }
  }
}


/* ------------------ GetElevationFile ------------------------ */

elevdata *GetElevationFile(elevdata *elevinfo, int nelevinfo, float longval, float latval){
  int i;

  for(i = 0; i < nelevinfo; i++){
    elevdata *elevi;

    elevi = elevinfo + i;
    if(longval<elevi->long_min || longval>elevi->long_max)continue;
    if(latval<elevi->lat_min || latval>elevi->lat_max)continue;
    return elevi;
  }
  return NULL;
}

/* ------------------ GetElevation ------------------------ */

float GetElevation(elevdata *elevinfo, int nelevinfo, float longval, float latval, int interp_option, int *have_val){
  elevdata *elevi;
  int ival, jval;
  int ival2, jval2;
  int index11, index12, index21, index22;
  float val11, val12, val21, val22;
  float val1, val2;
  float factor_x, factor_y;
  float return_val;
  float ivalx, jvaly;

  *have_val = 0;
  elevi = GetElevationFile(elevinfo, nelevinfo, longval, latval);
  if(elevi == NULL)return 0.0;
  if(elevi->valbuffer == NULL){
    FILE *stream;
    float *data_buffer;

    stream = fopen(elevi->datafile, "rb");
    if(stream == NULL)return 0.0;
    NewMemory((void **)&data_buffer, elevi->ncols*elevi->nrows * sizeof(float));
    elevi->valbuffer = data_buffer;
    fread(data_buffer, sizeof(float), elevi->ncols*elevi->nrows, stream);
    fclose(stream);
  }

  ivalx = (longval - elevi->long_min) / elevi->cellsize;
  ival = CLAMP(ivalx, 0, elevi->ncols - 1);

  jvaly = (elevi->lat_max - latval) / elevi->cellsize;
  jval = CLAMP(jvaly, 0, elevi->nrows - 1);

  index11 = jval*elevi->ncols + ival;
  val11 = elevi->valbuffer[index11];
  if(interp_option == 0) {
    *have_val = 1;
    return val11;
  }

  ival2 = CLAMP(ival + 1, 0, elevi->ncols - 1);
  jval2 = CLAMP(jval - 1, 0, elevi->nrows - 1);

  index12 = jval*elevi->ncols + ival2;
  val12 = elevi->valbuffer[index12];

  index21 = jval2*elevi->ncols + ival;
  val21 = elevi->valbuffer[index21];

  index22 = jval2*elevi->ncols + ival2;
  val22 = elevi->valbuffer[index22];

  factor_x = CLAMP(ivalx - ival, 0.0, 1.0);
  factor_y = CLAMP(jvaly - jval, 0.0, 1.0);

  val1 = INTERP1D(factor_x, val11, val12);
  val2 = INTERP1D(factor_x, val21, val22);

  return_val = INTERP1D(factor_y, val2, val1);
  *have_val = 1;
  return return_val;
}

/* ------------------ GetJPEGImage ------------------------ */

gdImagePtr GetJPEGImage(const char *filename, int *width, int *height) {

  FILE *file;
  gdImagePtr image;

  *width = 0;
  *height = 0;
  file = fopen(filename, "rb");
  if(file == NULL)return NULL;
  image = gdImageCreateFromJpeg(file);
  fclose(file);
  if(image != NULL){
    *width = gdImageSX(image);
    *height = gdImageSY(image);
  }
  return image;
}

/* ------------------ GetColor ------------------------ */

#ifdef pp_FASTCOLOR
int GetColor(elevdata *imagei, float llong, float llat) {
  if(imagei->long_min<=llong&&llong<=imagei->long_max&&imagei->lat_min<=llat&&llat<=imagei->lat_max) {
    int irow, icol;
    float latfact, longfact;

    if(imagei->image==NULL)imagei->image = GetJPEGImage(imagei->datafile, &imagei->ncols, &imagei->nrows);

    latfact = (llat-imagei->lat_min)/(imagei->lat_max-imagei->lat_min);
    longfact = (llong-imagei->long_min)/(imagei->long_max-imagei->long_min);

    irow = overlap_size+(imagei->nrows-1-2*overlap_size)*latfact;
    irow = imagei->nrows-1-irow;
    irow = CLAMP(irow, 0, imagei->nrows-1);

    icol = overlap_size+(imagei->ncols-1-2*overlap_size)*longfact;
    icol = CLAMP(icol, 0, imagei->ncols-1);
    return gdImageGetPixel(imagei->image, icol, irow);
  }
  return -1;
}
#else
int GetColor(float llong, float llat, elevdata *imageinfo, int nimageinfo) {
  int i;

  for(i = 0; i < nimageinfo; i++) {
    elevdata *imagei;

    imagei = imageinfo + i;
    if(imagei->long_min <= llong&&llong <= imagei->long_max&&imagei->lat_min <= llat&&llat <= imagei->lat_max) {
      int irow, icol;
      float latfact, longfact;

      if(imagei->image == NULL)imagei->image = GetJPEGImage(imagei->datafile, &imagei->ncols, &imagei->nrows);

      latfact = (llat - imagei->lat_min) / (imagei->lat_max - imagei->lat_min);
      longfact = (llong - imagei->long_min) / (imagei->long_max - imagei->long_min);

      irow = overlap_size + (imagei->nrows - 1 - 2 * overlap_size)*latfact;
      irow = imagei->nrows - 1 - irow;
      irow = CLAMP(irow, 0, imagei->nrows - 1);

      icol = overlap_size + (imagei->ncols - 1 - 2 * overlap_size)*longfact;
      icol = CLAMP(icol, 0, imagei->ncols - 1);
      return gdImageGetPixel(imagei->image, icol, irow);
    }
  }
  return       (122 << 16) | (117 << 8) | 48;
}
#endif

/* ------------------ GenerateMapImage ------------------------ */

void GenerateMapImage(char *image_file, char *image_file_type, elevdata *fds_elevs, elevdata *imageinfo, int nimageinfo) {
  int nrows, ncols, j;
  gdImagePtr RENDERimage;
  float dx, dy;
#ifdef pp_FASTCOLOR
  int ii;
#endif

  ncols = terrain_image_width;
  nrows = terrain_image_height;

  dx = (fds_elevs->long_max - fds_elevs->long_min) / (float)ncols;
  dy = (fds_elevs->lat_max - fds_elevs->lat_min) / (float)nrows;

  RENDERimage = gdImageCreateTrueColor(ncols, nrows);
#ifdef pp_FASTCOLOR
  for(ii = 0; ii<nimageinfo; ii++) {
    elevdata *imagei;

    imagei = imageinfo+ii;
    printf(" processing image file %i of %i\n",ii+1,nimageinfo);

    for(j = 0; j<nrows; j++) {
      int i;
      float llat;

      llat = fds_elevs->lat_max-(float)j*dy;
      for(i = 0; i<ncols; i++) {
        float llong;
        int rgb_local;

        llong = fds_elevs->long_min+(float)i*dx;

        rgb_local = GetColor(imagei, llong, llat);
        if(rgb_local>=0)gdImageSetPixel(RENDERimage, i, j, rgb_local);
      }
    }
    if(imagei->image!=NULL)gdImageDestroy(imagei->image);
  }
#else
  for(j = 0; j < nrows; j++) {
    int i;
    float llat;

    llat = fds_elevs->lat_max - (float)j*dy;
    for(i = 0; i < ncols; i++) {
      float llong;
      int rgb_local;

      llong = fds_elevs->long_min + (float)i*dx;

      rgb_local = GetColor(llong, llat, imageinfo, nimageinfo);
      gdImageSetPixel(RENDERimage, i, j, rgb_local);
    }
  }
#endif

#define SCALE_IMAGE_I(ival) (CLAMP(ncols*((ival) - fds_elevs->long_min) / (fds_elevs->long_max - fds_elevs->long_min),0,ncols-1))
#define SCALE_IMAGE_J(jval) (CLAMP(nrows*(fds_elevs->lat_max - (jval)) / (fds_elevs->lat_max - fds_elevs->lat_min), 0, nrows - 1))

  if(show_maps == 1){
    int i;

    // draw outline of each terrain map (downloaded from USGS website)

    for(i = 0; i < nimageinfo; i++) {
      elevdata *imagei;
      float latcen;
      int ileft, jcen;
      int textline_color;
      int imin, imax, jmin, jmax;
      int ii, jj;

      imagei = imageinfo + i;
      ileft = 10+SCALE_IMAGE_I(imagei->long_min);

      latcen = (imagei->lat_max + imagei->lat_min) / 2.0;
      jcen = SCALE_IMAGE_J(latcen);
      textline_color = (0 << 16) | (0 << 8) | 0;

      // gdFontTiny, gdFontSmall, gdFontMediumBold, gdFontLarge, and gdFontGiant
      gdImageString(RENDERimage, gdFontGiant, ileft, jcen, (unsigned char *)imagei->filelabel, textline_color);

      imin = SCALE_IMAGE_I(imagei->long_min);
      imax = SCALE_IMAGE_I(imagei->long_max);

      jmin = SCALE_IMAGE_J(imagei->lat_max); // max latitude occurs first in file (ie has a smaller 'j' index)
      jmax = SCALE_IMAGE_J(imagei->lat_min);

      for(jj = jmin; jj <= jmax; jj++){
        int kk;

        for(kk = -1; kk < 2; kk++){
          gdImageSetPixel(RENDERimage, imin + kk, jj, textline_color);
          gdImageSetPixel(RENDERimage, imax + kk, jj, textline_color);
        }
      }
      for(ii = imin; ii <= imax; ii++){
        int kk;

        for(kk = -1; kk < 2; kk++){
          gdImageSetPixel(RENDERimage, ii, jmin + kk, textline_color);
          gdImageSetPixel(RENDERimage, ii, jmax + kk, textline_color);
        }
      }
    }
    {
    // draw outline of fds domain

      int imin, imax;
      int jmin, jmax;
      int red_color;
      int ii, jj;

      imin = SCALE_IMAGE_I(fds_elevs->long_min_orig);
      imax = SCALE_IMAGE_I(fds_elevs->long_max_orig);
      jmin = SCALE_IMAGE_J(fds_elevs->lat_max_orig);
      jmax = SCALE_IMAGE_J(fds_elevs->lat_min_orig);

      red_color = (255 << 16) | (0 << 8) | 0;

      for(jj = jmin; jj <= jmax; jj++){
        int kk;

        for(kk = -1; kk < 2; kk++){
          gdImageSetPixel(RENDERimage, imin + kk, jj, red_color);
          gdImageSetPixel(RENDERimage, imax + kk, jj, red_color);
        }
      }
      for(ii = imin; ii <= imax; ii++){
        int kk;

        for(kk = -1; kk < 2; kk++){
          gdImageSetPixel(RENDERimage, ii, jmin + kk, red_color);
          gdImageSetPixel(RENDERimage, ii, jmax + kk, red_color);
        }
      }
    }
  }

  {
    FILE *stream=NULL;

    if(image_file!=NULL)stream = fopen(image_file, "wb");
    if(stream!=NULL){
      if(strcmp(image_file_type,".png")==0)gdImagePng(RENDERimage, stream);
      if(strcmp(image_file_type,".jpg")==0)gdImageJpeg(RENDERimage, stream, 100);
    }
    gdImageDestroy(RENDERimage);
    if(stream!=NULL)fclose(stream);
  }
}

/* ------------------ SetImageSize ------------------------ */

void SetImageSize(elevdata *fds_elevs){
  int ncols, nrows;

  if(terrain_image_width<=0&&terrain_image_height<=0)terrain_image_width = 2000;

  if(terrain_image_width>0){
    ncols = terrain_image_width;
    nrows = ncols*fds_elevs->ymax / fds_elevs->xmax;
  }
  else{
    if(terrain_image_height<=0)terrain_image_height=2000;
    nrows = terrain_image_height;
    ncols = nrows*fds_elevs->xmax / fds_elevs->ymax;
  }
  terrain_image_width = ncols;
  terrain_image_height = nrows;
}

/* ------------------ GetElevations ------------------------ */

int GetElevations(char *input_file, char *image_file, char *image_file_type, elevdata *fds_elevs){
  int nelevinfo, nimageinfo, i, j;
  filelistdata *headerfiles, *imagefiles;
  FILE *stream_in;
  elevdata *elevinfo=NULL, *imageinfo=NULL;
  int kbar;
  int nlongs = 100, nlats = 100;
  float dlat, dlong;
  int *have_vals;
  float valmin=0.0, valmax=1.0, *vals;
  char *ext;
  float longref = -1000.0, latref = -1000.0;
  float xref = 0.0, yref = 0.0;
  float xmax = -1000.0, ymax = -1000.0, zmin = -1000.0, zmax = -1000.0;
  float *longlats = NULL, *longlatsorig;
  float image_long_min=0.0, image_long_max=1.0, image_lat_min=0.0, image_lat_max=1.0;
  float fds_long_min, fds_long_max, fds_lat_min, fds_lat_max;
  int longlatref_mode = LONGLATREF_NONE;
  int xymax_defined=0;

  nimageinfo = GetFileListSize(image_dir, "m_*.jpg");
  if(nimageinfo > 0){
    NewMemory((void **)&imagefiles, nimageinfo * sizeof(filelistdata));
    NewMemory((void **)&imageinfo, nimageinfo * sizeof(elevdata));
    MakeFileList(image_dir, "m_*.jpg", nimageinfo, NO, &imagefiles);
  }
  for(i = 0; i < nimageinfo; i++){
    elevdata *imagei;
    filelistdata *imagefilei;
    char dummy[2], clat[3], clong[4], coffset[4], cquarter[3];
    int llat, llong, offset, icol, irow;
    char *p;
    char imagefilename[1024];

    imagei = imageinfo + i;
    imagefilei = imagefiles + i;
    imagei->datafile = imagefilei->file;
    strcpy(imagefilename, "");
    if(strcmp(image_dir, ".") != 0) {
      strcat(imagefilename, image_dir);
      strcat(imagefilename, dirseparator);
    }
    strcat(imagefilename, imagefilei->file);
    NewMemory((void **)&imagei->datafile, strlen(imagefilename) + 1);
    strcpy(imagei->datafile, imagefilename);

    p = imagefilei->file + 2;
    strncpy(imagei->filelabel, imagefilei->file, 12);
    imagei->filelabel[12] = 0;

    CopyString(clat, &p, 2, &llat);
    CopyString(clong, &p, 3, &llong);
    CopyString(coffset, &p, 2, &offset);
    CopyString(dummy, &p, 1, NULL);
    CopyString(cquarter, &p, 2, NULL);

    irow = 7 - (offset - 1) / 8;
    icol = 7 - (offset - 1) % 8;
    imagei->lat_min = (float)llat + (float)irow*7.5 / 60;
    imagei->long_min = (float)llong + (float)icol*7.5 / 60.0;
    if(strcmp(cquarter, "ne") == 0) {
      imagei->lat_min += 3.75 / 60;
      imagei->long_min += 3.75 / 60;
    }
    else if(strcmp(cquarter, "nw") == 0) {
      imagei->lat_min += 3.75 / 60;
      imagei->long_min += 7.5 / 60;
    }
    else if(strcmp(cquarter, "se") == 0) {
      imagei->long_min += 3.75 / 60;
    }
    else if(strcmp(cquarter, "sw") == 0) {
      imagei->long_min += 7.5 / 60;
    }
    imagei->long_min = -imagei->long_min;
    imagei->long_max = imagei->long_min + 3.75 / 60.0;
    imagei->lat_max = imagei->lat_min + 3.75 / 60.0;
    imagei->image = NULL;

#ifdef _DEBUG
    printf("file: %s\n", imagefilei->file);
    printf("long min/max %f %f\n", imagei->long_min, imagei->long_max);
    printf(" lat min/max %f %f\n", imagei->lat_min, imagei->lat_max);
#endif
    if(i == 0){
      image_lat_min = imagei->lat_min;
      image_lat_max = imagei->lat_max;
      image_long_min = imagei->long_min;
      image_long_max = imagei->long_max;
    }
    else{
      image_lat_min = MIN(imagei->lat_min, image_lat_min);
      image_lat_max = MAX(imagei->lat_max, image_lat_max);
      image_long_min = MIN(imagei->long_min, image_long_min);
      image_long_max = MAX(imagei->long_max, image_long_max);
    }
  }

  nelevinfo = GetFileListSize(elev_dir, "*.hdr");
  if(nelevinfo == 0){
    fprintf(stderr, "***error: unable to create an FDS input file, elevation files\n");
    fprintf(stderr, "          not found in directory: %s\n",elev_dir);
    return 0;
  }

  MakeFileList(elev_dir, "*.hdr", nelevinfo, NO, &headerfiles);
  NewMemory((void **)&elevinfo, nelevinfo * sizeof(elevdata));
  for(i = 0; i < nelevinfo; i++){
    filelistdata *headerfilei;
    elevdata *elevi;
    char basefile[LEN_BUFFER], *datafile, *headerfile;
    int lenfile;

    headerfilei = headerfiles + i;
    elevi = elevinfo + i;

    strcpy(basefile, headerfilei->file);
    ext = strrchr(basefile, '.');
    if(ext != NULL)ext[0] = 0;

    lenfile = strlen(elev_dir) + strlen(dirseparator) + strlen(basefile) + 4 + 1;

    NewMemory((void **)&datafile, lenfile);
    strcpy(datafile, "");
    if(strcmp(elev_dir, ".") != 0){
      strcat(datafile, elev_dir);
      strcat(datafile, dirseparator);
    }
    strcat(datafile, basefile);
    strcat(datafile, ".flt");

    NewMemory((void **)&headerfile, lenfile);
    strcpy(headerfile, "");
    if(strcmp(elev_dir, ".") != 0){
      strcat(headerfile, elev_dir);
      strcat(headerfile, dirseparator);
    }
    strcat(headerfile, basefile);
    strcat(headerfile, ".hdr");

    elevi->headerfile = headerfile;
    elevi->datafile = datafile;
  }
  for(i = 0; i < nelevinfo; i++){
    elevdata *elevi;
    char buffer[LEN_BUFFER];

    elevi = elevinfo + i;
    elevi->use_it = 0;

    stream_in = fopen(elevi->headerfile, "r");
    if(stream_in == NULL)continue;

    if(fgets(buffer, LEN_BUFFER, stream_in) == NULL)continue;
    TrimBack(buffer);
    sscanf(buffer + 5, " %i", &elevi->ncols);

    if(fgets(buffer, LEN_BUFFER, stream_in) == NULL)continue;
    TrimBack(buffer);
    sscanf(buffer + 5, " %i", &elevi->nrows);

    if(fgets(buffer, LEN_BUFFER, stream_in) == NULL)continue;
    TrimBack(buffer);
    sscanf(buffer + 9, " %f", &elevi->xllcorner);

    if(fgets(buffer, LEN_BUFFER, stream_in) == NULL)continue;
    TrimBack(buffer);
    sscanf(buffer + 9, " %f", &elevi->yllcorner);

    if(fgets(buffer, LEN_BUFFER, stream_in) == NULL)continue;
    TrimBack(buffer);
    sscanf(buffer + 8, " %f", &elevi->cellsize);

    elevi->long_min = elevi->xllcorner;
    elevi->long_max = elevi->long_min + (float)elevi->ncols*elevi->cellsize;

    elevi->lat_min = elevi->yllcorner;
    elevi->lat_max = elevi->lat_min + (float)elevi->nrows*elevi->cellsize;

    elevi->valbuffer = NULL;

    elevi->use_it = 1;

    fclose(stream_in);
  }

  stream_in = fopen(input_file, "r");
  if(stream_in == NULL) {
    fprintf(stderr, "***error: unable to open file %s for input\n", input_file);
    return 0;
  }

  // pass 1

  nexcludeinfo = 0;
  while(!feof(stream_in)){
    char buffer[LEN_BUFFER], *buffer2;

    CheckMemory;

    if(fgets(buffer, LEN_BUFFER, stream_in) == NULL)break;
    buffer2 = strstr(buffer, "//");
    if(buffer2 != NULL)buffer2[0] = 0;
    buffer2 = TrimFrontBack(buffer);
    if(strlen(buffer2) == 0)continue;

    if (Match(buffer, "BUFF_DIST") == 1) {
      if (fgets(buffer, LEN_BUFFER, stream_in) == NULL)break;
      sscanf(buffer, "%f", &buff_dist);
      continue;
    }
    if(Match(buffer, "GRID") == 1){
      nlongs = 10;
      nlats = 10;
      kbar = 10;
      if(fgets(buffer, LEN_BUFFER, stream_in) == NULL)break;
      sscanf(buffer, "%i %i %i %f %f %f %f", &nlongs, &nlats, &kbar, &xmax, &ymax, &zmin, &zmax);
      if(xmax > 0.0&&ymax > 0.0)xymax_defined = 1;
      continue;
    }
    if(Match(buffer, "EXCLUDE") == 1){
      nexcludeinfo++;
      continue;
    }
  }

  if(nexcludeinfo > 0){
    NewMemory((void **)&excludeinfo, nexcludeinfo * sizeof(excludedata));
  }

  // pass 2

  rewind(stream_in);
  nexcludeinfo = 0;
  while(!feof(stream_in)){
    char buffer[LEN_BUFFER], *buffer2;

    CheckMemory;

    if(fgets(buffer, LEN_BUFFER, stream_in) == NULL)break;
    buffer2 = strstr(buffer, "//");
    if(buffer2 != NULL)buffer2[0] = 0;
    buffer2 = TrimFrontBack(buffer);
    if(strlen(buffer2) == 0)continue;

    if(Match(buffer, "LONGLATORIG") == 1){
      if(fgets(buffer, LEN_BUFFER, stream_in) == NULL)break;
      sscanf(buffer, "%f %f", &longref, &latref);
      longlatref_mode = LONGLATREF_ORIG;

      xref = 0.0;
      yref = 0.0;
      dlat = ymax / EARTH_RADIUS;
      fds_lat_max = latref + RAD2DEG*dlat;
      fds_lat_min = latref;
      dlong = ABS(2.0*asin(sin(xmax / (2.0*EARTH_RADIUS)) / cos(DEG2RAD*latref)));
      fds_long_max = longref + RAD2DEG*dlong;
      fds_long_min = longref;
      continue;
    }

      // a = sin(dlat/2)^2 + cos(lat1)*cos(lat2)*sin(dlong/2)^2
      // c = 2*asin(sqrt(a))
      // d = R*c
      // R = RAD_EARTH

      // dlat = 0 ==> d = 2*R*asin(cos(lat1)*sin(dlong/2))
      //              dlong = 2*asin(sin(d/(2*R))/cos(lat1))
      // dlong = 0 ==> d = R*dlat
      //               dlat = d/R

    if(Match(buffer, "LONGLATCENTER") == 1){
      if(fgets(buffer, LEN_BUFFER, stream_in) == NULL)break;
      sscanf(buffer, "%f %f", &longref, &latref);
      longlatref_mode = LONGLATREF_CENTER;
      xref = xmax/2.0;
      yref = ymax/2.0;
      dlat = yref / EARTH_RADIUS;
      fds_lat_max = latref + RAD2DEG*dlat;
      fds_lat_min = latref - RAD2DEG*dlat;
      dlong = ABS(2.0*asin(sin(xref / (2.0*EARTH_RADIUS)) / cos(DEG2RAD*latref)));
      fds_long_max = longref + RAD2DEG*dlong;
      fds_long_min = longref - RAD2DEG*dlong;
      continue;
    }

    if(Match(buffer, "MESH")==1){
      if(fgets(buffer, LEN_BUFFER, stream_in)==NULL)break;
      sscanf(buffer, "%i %i", &nmeshx, &nmeshy);
      nmeshx = MAX(1, nmeshx);
      nmeshy = MAX(1, nmeshy);
    }

    if(Match(buffer, "LONGLATMINMAX")==1){
      fds_long_min = -1000.0;
      fds_long_max = -1000.0;
      fds_lat_min = -1000.0;
      fds_lat_max = -1000.0;
      if(fgets(buffer, LEN_BUFFER, stream_in) == NULL)break;
      sscanf(buffer, "%f %f %f %f", &fds_long_min, &fds_long_max, &fds_lat_min, &fds_lat_max);
      longlatref_mode = LONGLATREF_MINMAX;
      longref = (fds_long_min + fds_long_max) / 2.0;
      latref = (fds_lat_min + fds_lat_max) / 2.0;
      xmax = SphereDistance(fds_long_min, latref, fds_long_max, latref);
      ymax = SphereDistance(longref, fds_lat_min, longref, fds_lat_max);
      xref = xmax / 2.0;
      yref = ymax / 2.0;
      xymax_defined = 1;
      continue;
    }

    if(Match(buffer, "EXCLUDE") == 1){
      excludedata *exi;

      exi = excludeinfo + nexcludeinfo++;
      if(fgets(buffer, LEN_BUFFER, stream_in) == NULL)break;
      exi->xmin = -1.0;
      exi->xmax = -1.0;
      exi->ymin = -1.0;
      exi->ymax = -1.0;
      sscanf(buffer, "%f %f %f %f", &exi->xmin, &exi->ymin, &exi->xmax, &exi->ymax);
      continue;
    }
  }
  fclose(stream_in);

  if(longlatref_mode==LONGLATREF_NONE){
    fprintf(stderr, "***error: A longitude and latitude must be specified with either the LONGLATMINMAX, \n");
    fprintf(stderr,"           LONGLATORIG or LONGLATCENTER keywords\n");
    return 0;
  }

  if(show_maps==1){
    longlatref_mode = LONGLATREF_MINMAX;
    longref = (image_long_min + image_long_max) / 2.0;
    latref = (image_lat_min + image_lat_max) / 2.0;
    xmax = SphereDistance(image_long_min, latref, image_long_max, latref);
    ymax = SphereDistance(longref, image_lat_min, longref, image_lat_max);
    xref = xmax / 2.0;
    yref = ymax / 2.0;
    xymax_defined = 1;
  }
  if(xymax_defined==0){
    fprintf(stderr,"***error: The domain size must be defined by using the GRID or LONGLATMINMAX keyword\n");
    return 0;
  }

  NewMemory((void **)&longlatsorig, 2 * nlongs*nlats * sizeof(float));
  longlats = longlatsorig;
  GetLongLats(longref, latref, xref, yref,
    xmax, ymax, nlongs, nlats, longlats);

  for(i = 0; i < nlongs*nlats; i++) {
    float llong, llat;

    llong = *longlats++;
    llat = *longlats++;
    if(GetElevationFile(elevinfo, nelevinfo, llong, llat) == NULL) {
      fprintf(stderr, "***error: elevation data not available for \n");
      fprintf(stderr, "    longitude/latitude: (%f %f) \n", llong, llat);
      for(i = 0; i < nelevinfo; i++) {
        elevdata *elevi;

        elevi = elevinfo + i;
        fprintf(stderr, " header file: %s bounds: %f %f %f %f\n",
          elevi->headerfile, elevi->long_min, elevi->lat_min, elevi->long_max, elevi->lat_max);
      }
      FREEMEMORY(longlatsorig);
      return 0;
    }
  }

  if(show_maps==1){
    fds_elevs->long_min = image_long_min;
    fds_elevs->long_max = image_long_max;
    fds_elevs->lat_min = image_lat_min;
    fds_elevs->lat_max = image_lat_max;

    fds_elevs->long_min_orig = fds_long_min;
    fds_elevs->long_max_orig = fds_long_max;
    fds_elevs->lat_min_orig = fds_lat_min;
    fds_elevs->lat_max_orig = fds_lat_max;
  }
  else{
    fds_elevs->long_min = fds_long_min;
    fds_elevs->long_max = fds_long_max;
    fds_elevs->lat_min = fds_lat_min;
    fds_elevs->lat_max = fds_lat_max;
  }
  dlat = (fds_elevs->lat_max - fds_elevs->lat_min) / (float)(nlats - 1);
  dlong = (fds_elevs->long_max - fds_elevs->long_min) / (float)(nlongs - 1);
  NewMemory((void **)&vals, nlongs*nlats * sizeof(float));
  NewMemory((void **)&have_vals, nlongs*nlats * sizeof(int));
  longlats = longlatsorig;
  fds_elevs->valbuffer = vals;
  fds_elevs->nrows = nlats;
  fds_elevs->ncols = nlongs;
  fds_elevs->nz = kbar;
  fds_elevs->xmax = xmax;
  fds_elevs->ymax = ymax;
  fds_elevs->xref = xref;
  fds_elevs->yref = yref;
  fds_elevs->longref = longref;
  fds_elevs->latref = latref;

  {
    int count=0;

    for(j = 0; j<nlats; j++) {
      for(i = 0; i<nlongs; i++){
        float llong, llat, elevij;
        int have_val;

        llong = *longlats++;
        llat = *longlats++;
        elevij = GetElevation(elevinfo, nelevinfo, llong, llat, INTERPOLATE, &have_val);
        vals[count] = elevij;
        have_vals[count] = have_val;
        if(have_val==1){
          if(count==0){
            valmin = elevij;
            valmax = elevij;
          }
          else{
            valmin = MIN(valmin, elevij);
            valmax = MAX(valmax, elevij);
          }
        }
        count++;
      }
    }
  }
  fds_elevs->val_min = valmin;
  fds_elevs->val_max = valmax;
  if(zmin>-1000.0){
    fds_elevs->zmin = zmin;
  }
  else{
    fds_elevs->zmin = valmin-(valmax-valmin)/10.0;
  }
  if(zmax>-1000.0){
    fds_elevs->zmax = zmax;
  }
  else{
    fds_elevs->zmax = valmax+(valmax-valmin)/10.0;
  }
  FREEMEMORY(have_vals);
  FREEMEMORY(longlatsorig);

  SetImageSize(fds_elevs);

  fprintf(stderr, "\nmap properties:\n");
  fprintf(stderr, "        input file: %s\n", input_file);
  fprintf(stderr, "         image dir: %s\n", image_dir);
  fprintf(stderr, "  image dimensions: %i x %i\n", terrain_image_width,terrain_image_height);
  fprintf(stderr, "     elevation dir: %s\n", elev_dir);
  if(nimageinfo > 0){
    fprintf(stderr, "  longitude bounds: %f %f\n", image_long_min, image_long_max);
    fprintf(stderr, "   latitude bounds: %f %f\n", image_lat_min, image_lat_max);
  }

  GenerateMapImage(image_file, image_file_type, fds_elevs, imageinfo, nimageinfo);
  return 1;
}

/* ------------------ GetAndersonSurfIndex ------------------------ */

#define NFIRE_TYPES 20
int GetAndersonSurfIndex(int val){
  int j;
  int fire_type[NFIRE_TYPES] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 90, 91, 92, 93, 98, 99, 9999};

  for(j = 0; j<NFIRE_TYPES-1; j++){
    if(val==fire_type[j])return j+1;
  }
  return NFIRE_TYPES;
}

/* ------------------ FIRE2PNG ------------------------ */

void FIRE2PNG(char *basename, int *vals, int nrows, int ncols){
  int i;
  gdImagePtr RENDERimage;
  FILE *RENDERfile = NULL;

  RENDERimage = gdImageCreateTrueColor(3*ncols, 3*nrows);

  for(i = 0; i<ncols; i++){
    int j;

    for(j = 0; j<nrows; j++){
      unsigned char r, g, b;
      int rgb_local, color_index, ii, jj;
      int val;

      val = vals[j*ncols+i];
      if(val==98){
        val = 98;
      }
      color_index = GetAndersonSurfIndex(val)-1;
      r = (unsigned char)firecolors[3*color_index];
      g = (unsigned char)firecolors[3*color_index+1];
      b = (unsigned char)firecolors[3*color_index+2];
      rgb_local = (r<<16)|(g<<8)|b;
      for(ii = 0; ii<3; ii++){
        for(jj = 0; jj<3; jj++){
          gdImageSetPixel(RENDERimage, 3*i+ii, 3*j+jj, rgb_local);
        }
      }
    }
  }
  {
    char filename[1000];

    strcpy(filename, basename);
    strcat(filename, ".png");
    RENDERfile = fopen(filename, "wb");
  }
  gdImagePng(RENDERimage, RENDERfile);
  fclose(RENDERfile);
}

/* ------------------ ReadIGrid ------------------------ */

int ReadIGrid(char *directory, char *file, wuigriddata *wuifireinfo){
  FILE *stream;
  int i;
  int *vals_local = NULL, nvals_local;
  char *buffer = NULL;
  int size_buffer;

  wuifireinfo->vals_ntypes = 0;

  stream = fopen_indir(directory, file, "r");
  if(stream==NULL)return 1;

  size_buffer = 100;
  NewMemory((void **)&buffer, size_buffer*sizeof(char));

  fgets(buffer, size_buffer, stream);
  sscanf(buffer+13, "%i", &wuifireinfo->ncols);

  fgets(buffer, size_buffer, stream);
  sscanf(buffer+13, "%i", &wuifireinfo->nrows);

  fgets(buffer, size_buffer, stream);
  sscanf(buffer+13, "%f", &wuifireinfo->long_min);

  fgets(buffer, size_buffer, stream);
  sscanf(buffer+13, "%f", &wuifireinfo->lat_min);

  fgets(buffer, size_buffer, stream);
  sscanf(buffer+13, "%f", &wuifireinfo->dlong);

  fgets(buffer, size_buffer, stream);
  sscanf(buffer+13, "%f", &wuifireinfo->dlat);

  wuifireinfo->long_max = wuifireinfo->long_min+(float)wuifireinfo->ncols*wuifireinfo->dlong;
  wuifireinfo->lat_max = wuifireinfo->lat_min+(float)wuifireinfo->nrows*wuifireinfo->dlat;

  fgets(buffer, size_buffer, stream);

  nvals_local = wuifireinfo->ncols*wuifireinfo->nrows;
  if(nvals_local<=0)return 1;

  FREEMEMORY(buffer);
  size_buffer = 5*wuifireinfo->ncols;
  NewMemory((void **)&buffer, size_buffer*sizeof(char));

  NewMemory((void **)&vals_local, nvals_local*sizeof(int));
  wuifireinfo->vals = vals_local;
  for(i = 0; i<wuifireinfo->nrows; i++){
    int j;
    char *tok;

    if(fgets(buffer, size_buffer, stream)==NULL)break;
    tok = strtok(buffer, " ");
    for(j = 0; j<wuifireinfo->ncols; j++){
      sscanf(tok, "%i", vals_local);
      tok = strtok(NULL, " ");
      vals_local++;
    }
  }
  return 0;
}


/* ------------------ GetFireData ------------------------ */

wuigriddata *GetFireData(char *directory, char *casename){
  wuigriddata *wuifireinfo;

  NewMemory((void **)&wuifireinfo, sizeof(wuigriddata));

  if(ReadIGrid(directory, "anderson13.asc", wuifireinfo)!=0){
    FREEMEMORY(wuifireinfo);
    return NULL;
  }
  FIRE2PNG(casename, wuifireinfo->vals, wuifireinfo->nrows, wuifireinfo->ncols);
  return wuifireinfo;
}

/* ------------------ GetFireIndex ------------------------ */

int GetFireIndex(wuigriddata *wuifireinfo, float longitude, float latitude){
  int ix, iy, index, val;
  float dlong, dlat;
  int nrows, ncols;

  dlong = wuifireinfo->long_max-wuifireinfo->long_min;
  dlat = wuifireinfo->lat_max-wuifireinfo->lat_min;
  nrows = wuifireinfo->nrows;
  ncols = wuifireinfo->ncols;  
  ix = CLAMP(ncols*(longitude-wuifireinfo->long_min)/dlong, 0, wuifireinfo->ncols-1);
  iy = CLAMP(nrows*(wuifireinfo->lat_max-latitude)/dlat,    0, wuifireinfo->nrows-1);
  index = iy*wuifireinfo->ncols+ix;
  val = wuifireinfo->vals[index];
  return val;
}

/* ------------------ GetSurfsFromXY ------------------------ */

void GetSurfsFromXY(wuigriddata *wuifireinfo, struct _elevdata *fds_elevs, float *xy, int *surfs, int nfaces){
  int i;
  float dlong, dlat;

  dlong = fds_elevs->long_max-fds_elevs->long_min;
  dlat  = fds_elevs->lat_max-fds_elevs->lat_min;
  for(i = 0; i<nfaces; i++){
    float xavg, yavg, longitude, latitude;
    int fire_index;

    xavg           = xy[2*i+0];
    yavg           = xy[2*i+1];
    longitude      = fds_elevs->long_min + (xavg/fds_elevs->xmax)*dlong;
    latitude       = fds_elevs->lat_min  + (yavg/fds_elevs->ymax)*dlat;
    fire_index     = GetFireIndex(wuifireinfo, longitude, latitude);
    surfs[i]       = GetAndersonSurfIndex(fire_index);
  }
}

/* ------------------ GetSurfsFromFaces ------------------------ */

void GetFaceCenter(int iface, int *faces, float *verts, float *xy){
  float *v1, *v2, *v3;
  int f1, f2, f3;

  f1 = faces[3*iface+0]-1;
  f2 = faces[3*iface+1]-1;
  f3 = faces[3*iface+2]-1;
  v1 = verts+3*f1;
  v2 = verts+3*f2;
  v3 = verts+3*f3;
  xy[0] = (v1[0]+v2[0]+v3[0])/3.0;
  xy[1] = (v1[1]+v2[1]+v3[1])/3.0;
}

/* ------------------ GetSurfsFromFaces ------------------------ */

void GetSurfsFromFaces(wuigriddata *wuifireinfo, struct _elevdata *fds_elevs, float *verts, int nverts, int *faces, int *surfs, int nfaces){
  int i;
  float *xy;

  NewMemory((void **)&xy, 2*sizeof(float)*nfaces);

  for(i = 0; i<nfaces; i+=2){
    float xy1[2], xy2[2];

    GetFaceCenter(  i, faces, verts, xy1);
    GetFaceCenter(i+1, faces, verts, xy2);
    xy[2*i+0] = (xy1[0]+xy2[0])/2.;
    xy[2*i+1] = (xy1[1]+xy2[1])/2.;
    xy[2*i+2] = xy[2*i+0];
    xy[2*i+3] = xy[2*i+1];
  }
  GetSurfsFromXY(wuifireinfo, fds_elevs, xy, surfs, nfaces);
  FREEMEMORY(xy);
}

/* ------------------ GetImageColor ------------------------ */

int GetImageColor(float x, float y, griddata *imagedata){
  if(imagedata->image==NULL){
    int ncols, nrows;

    imagedata->image = GetJPEGImage(imagedata->image_file, &ncols, &nrows);
    imagedata->image_ncols = ncols;
    imagedata->image_nrows = nrows;
  }

  int icol, irow;
  int rgb_default = (0<<16)|(0<<8)|0;

  if(x<imagedata->longmin||x>imagedata->longmax)return rgb_default;
  if(y<imagedata->latmin||y>imagedata->latmax)return rgb_default;

  icol = (float)(imagedata->ncols-1)*(x-imagedata->longmin)/(imagedata->longmax-imagedata->longmin);
  irow = (float)(imagedata->nrows-1)*(y-imagedata->latmin)/(imagedata->latmax-imagedata->latmin);
  icol = CLAMP(icol, 0, imagedata->ncols-1);
  irow = CLAMP(imagedata->nrows-1-irow, 0, imagedata->nrows-1);
  return gdImageGetPixel(imagedata->image, icol, irow);
}

/* ------------------ GenerateImage ------------------------ */

void GenerateImage(griddata *inputdata, griddata *imagedata) {
  gdImagePtr RENDERimage;
  int j;

  float dx = (inputdata->longmax-inputdata->longmin)/(float)(inputdata->image_ncols-1);
  float dy = (inputdata->latmax-inputdata->latmin)/(float)(inputdata->image_nrows-1);
  RENDERimage = gdImageCreateTrueColor(inputdata->image_ncols, inputdata->image_nrows);
  for(j=0; j<inputdata->image_nrows; j++) {
    int i;

    float latj = inputdata->latmax - (float)j*dy;
    for(i=0; i<inputdata->image_ncols; i++) {
      int rgb_local;

      float longi = inputdata->longmin+(float)i*dx;

      rgb_local = GetImageColor(longi, latj, imagedata);
      gdImageSetPixel(RENDERimage, i, j, rgb_local);
    }
  }


  FILE *stream = fopen(inputdata->image_file, "wb");
  if(stream!=NULL)gdImagePng(RENDERimage, stream);
  gdImageDestroy(RENDERimage);
  if(stream!=NULL)fclose(stream);
}

/* ------------------ GenerateFDSInputFile ------------------------ */

#ifdef pp_GRIDDATA
int GenerateFDSInputFile(int option, char *casename, char *casename_fds, char *casename_bingeom,
  griddata *inputdata, griddata *firedata, griddata *elevdata, griddata *imagedata){
  int *fire_fds = NULL;
  float *elev_fds = NULL;
  int size_fire=0, size_elev=0, size_input=0;
  float dlat_fds, dlong_fds;
  float dlat_elev, dlong_elev;
  float *vals_elev;
  int i, j;
  float valmin_fds, valmax_fds;

  if(inputdata==NULL||elevdata==NULL)return 0;
  size_input = inputdata->ncols*inputdata->nrows;
  if(firedata!=NULL)size_fire = size_input*sizeof(int);
  if(elevdata!=NULL)size_elev = size_input*sizeof(float);

  if(size_fire>0)NewMemory((void **)&fire_fds, sizeof(float)*(size_fire));
  if(size_elev>0)NewMemory((void **)&elev_fds, sizeof(float)*(size_elev));
  dlat_fds   = (inputdata->latmax  - inputdata->latmin)/(float)(inputdata->nrows-1);
  dlong_fds  = (inputdata->longmax - inputdata->longmin)/(float)(inputdata->ncols-1);
  dlat_elev  = (elevdata->latmax   - elevdata->latmin)/(float)(elevdata->nrows-1);
  dlong_elev = (elevdata->longmax  - elevdata->longmin)/(float)(elevdata->ncols-1);

  if(imagedata!=NULL){
    GenerateImage(inputdata, imagedata);
  }

#define IJ3(i,j,nj) ((i)*(nj)+(j))

  vals_elev = (float *)elevdata->vals;
  for(i = 0; i<inputdata->nrows; i++){
    float lati, lat_factor;
    int ii, ii2;
    float yii;

    lati = inputdata->latmax-(float)i*dlat_fds;
    if(i==inputdata->nrows-1)lati = inputdata->latmin;

    yii = (elevdata->latmax - lati)/dlat_elev;
    ii =  CLAMP((int)yii, 0, elevdata->nrows-1);
    ii2 = CLAMP(ii+1, 0, elevdata->nrows-1);
    lat_factor = yii-(float)ii;
    lat_factor = CLAMP(lat_factor,0.0,1.0);

    for(j = 0; j<inputdata->ncols; j++){
      float longj, long_factor;
      int jj, jj2;
      float xjj;
      float val, val1, val2, val11, val12, val21, val22;

      longj = inputdata->longmin+(float)j*dlong_fds;
      if(j==inputdata->ncols-1)longj = inputdata->longmax;

      xjj = (longj-elevdata->longmin)/dlong_elev;
      jj = CLAMP((int)xjj, 0, elevdata->ncols-1);
      jj2 = CLAMP(jj+1, 0, elevdata->ncols-1);
      long_factor = xjj-(float)jj;
      long_factor = CLAMP(long_factor, 0.0, 1.0);

      val11 = vals_elev[IJ3(ii,  jj, elevdata->ncols)];
      val12 = vals_elev[IJ3(ii, jj2, elevdata->ncols)];
      val21 = vals_elev[IJ3(ii2, jj, elevdata->ncols)];
      val22 = vals_elev[IJ3(ii2,jj2, elevdata->ncols)];
      val1 = (1.0-long_factor)*val11 + long_factor*val12;
      val2 = (1.0-long_factor)*val21 + long_factor*val22;
      
      val = (1.0-lat_factor)*val1 + lat_factor*val2;
      elev_fds[IJ3(i,j,inputdata->ncols)] = val;
    }
  }
  valmin_fds = elev_fds[0];
  valmax_fds = valmin_fds;
  for(i = 1; i<inputdata->nrows*inputdata->ncols; i++){
    float val;

    val = elev_fds[i];
    valmin_fds = MIN(valmin_fds, val);
    valmax_fds = MAX(valmax_fds, val);
  }
  if(inputdata->zmin<=-999.0){
    inputdata->zmin = valmin_fds-(valmax_fds-valmin_fds)/10.0;
  }
  if(inputdata->zmax<=-999.0){
    inputdata->zmax = valmax_fds+(valmax_fds-valmin_fds)/10.0;
  }

  char output_file[LEN_BUFFER], output_elev_file[LEN_BUFFER], *ext;
  char basename[LEN_BUFFER];

  char casename_fds_basename[LEN_BUFFER];
  int nz;
  float xmax, ymax, zmin, zmax;
  float *xgrid, *ygrid;
  int count;
  int kbar;
  FILE *streamout = NULL;
  char *last;

  strcpy(casename_fds_basename, casename_fds);
  last = strrchr(casename_fds_basename, '.');
  if(last!=NULL)last[0] = 0;

  strcpy(basename, casename_fds_basename);
  ext = strrchr(basename, '.');
  if(ext!=NULL)ext[0] = 0;

  strcpy(output_file, casename_fds);
  streamout = fopen(output_file, "w");
  if(streamout==NULL){
    fprintf(stderr, "***error: unable to open %s for output\n", output_file);
    return 0;
  }

  strcpy(output_elev_file, basename);
  strcat(output_elev_file, ".elev");

  zmin = inputdata->zmin;
  zmax = inputdata->zmax;
  nz = inputdata->nz;

  xmax = inputdata->xmax;
  ymax = inputdata->ymax;

  kbar = nz;

  NewMemory((void **)&xgrid, sizeof(float)*inputdata->ncols);
  for(i = 0; i<inputdata->ncols-1; i++){
    xgrid[i] = xmax*(float)i/(float)(inputdata->ncols-1);
  }
  xgrid[inputdata->ncols-1] = xmax;

  NewMemory((void **)&ygrid, sizeof(float)*inputdata->nrows);
  for(i = 0; i<inputdata->nrows-1; i++){
    ygrid[i] = ymax*(float)(i)/(float)(inputdata->nrows-1);
  }
  ygrid[inputdata->nrows-1] = ymax;


  if(option==FDS_OBST){
    int ibar = inputdata->ncols - 1;
    int jbar = inputdata->nrows - 1;
    int nvals = ibar*jbar, len;

    len = strlen(output_elev_file);
    FORTelev2geom(output_elev_file, xgrid, &ibar, ygrid, &jbar, elev_fds, &nvals, len);
  }

  fprintf(streamout, "&HEAD CHID='%s', TITLE='created from %s' /\n", basename, casename);

  NewMemory((void **)&xplt, (nmeshx+1)*sizeof(float));
  xplt[0] = 0.0;
  for(i = 1; i<nmeshx-1; i++){
    xplt[i] = xmax*(float)i/(float)nmeshx;
  }
  xplt[nmeshx] = xmax;

  NewMemory((void **)&yplt, (nmeshy+1)*sizeof(float));
  yplt[0] = 0.0;
  for(i = 1; i<nmeshy-1; i++){
    yplt[i] = ymax*(float)i/(float)nmeshy;
  }
  yplt[nmeshy] = ymax;

  for(j = 0; j<nmeshy; j++){
    for(i = 0; i<nmeshx; i++){
      fprintf(streamout, "&MESH IJK = %i, %i, %i, XB = %f, %f, %f, %f, %f, %f /\n",
        inputdata->ncols-1, inputdata->nrows-1, kbar, xplt[i], xplt[i+1], yplt[j], yplt[j+1], zmin, zmax);
    }
  }

  if(option==FDS_OBST) {
    fprintf(streamout, "&MISC TERRAIN_CASE = .TRUE., TERRAIN_IMAGE = '%s.png' /\n", basename);
  }
  if(option==FDS_GEOM) {
    fprintf(streamout, "&MISC TERRAIN_CASE = .TRUE., TERRAIN_IMAGE = '%s.png' /\n", basename);
  }
  fprintf(streamout, "&TIME T_END = 0.0 /\n");
  fprintf(streamout, "&VENT MB = 'XMIN', SURF_ID = 'OPEN' /\n");
  fprintf(streamout, "&VENT MB = 'XMAX', SURF_ID = 'OPEN' /\n");
  fprintf(streamout, "&VENT MB = 'YMIN', SURF_ID = 'OPEN' /\n");
  fprintf(streamout, "&VENT MB = 'YMAX', SURF_ID = 'OPEN' /\n");
  fprintf(streamout, "&VENT MB = 'ZMAX', SURF_ID = 'OPEN' /\n");

  fprintf(streamout, "\nTerrain Geometry\n\n");

  if(option==FDS_GEOM){
    float *verts;
    int *faces, *surfs;
    int nverts, nfaces;

    NewMemory((void **)&verts, 3*inputdata->nrows*inputdata->ncols*sizeof(float));
    NewMemory((void **)&faces, 3*2*(inputdata->nrows-1)*(inputdata->ncols-1)*sizeof(int));
    NewMemory((void **)&surfs, 2*(inputdata->nrows-1)*(inputdata->ncols-1)*sizeof(int));

#define VERTIJ(i,j) ((j)*(inputdata->ncols)+(i))

    nverts = 0;
    for(j = 0; j<inputdata->nrows; j++){
      for(i = 0; i<inputdata->ncols; i++){
        verts[3*nverts+0] = xgrid[i];
        verts[3*nverts+1] = ymax-ygrid[j];
        verts[3*nverts+2] = elev_fds[VERTIJ(i, j)];
        nverts++;
      }
    }

    nfaces = 0;
    for(j = 0; j<inputdata->nrows-1; j++){
      for(i = 0; i<inputdata->ncols-1; i++){
        faces[3*nfaces+0] = 1+VERTIJ(i, j);
        faces[3*nfaces+1] = 1+VERTIJ(i+1, j+1);
        faces[3*nfaces+2] = 1+VERTIJ(i+1, j);
        nfaces++;

        faces[3*nfaces+0] = 1+VERTIJ(i, j);
        faces[3*nfaces+1] = 1+VERTIJ(i, j+1);
        faces[3*nfaces+2] = 1+VERTIJ(i+1, j+1);
        nfaces++;
      }
    }

    fprintf(streamout, " LONGMIN=%f LONGMAX=%f \n", inputdata->longmin, inputdata->longmax);
    fprintf(streamout, " LATMIN=%f LATMAX=%f\n", inputdata->latmin, inputdata->latmax);
    fprintf(streamout, " ZMIN=%f ZMAX=%f\n", valmin_fds, valmax_fds);
    if(firedata==NULL){
      int below_zero = 0;

      for(i = 0; i<nfaces; i++){
        int v1, v2, v3;
        float z1, z2, z3;

        v1 = faces[3*i+0]-1;
        v2 = faces[3*i+1]-1;
        v3 = faces[3*i+2]-1;

        z1 = verts[3*v1+2];
        z2 = verts[3*v2+2];
        z3 = verts[3*v3+2];
        if(z1<0.0||z2<0.0||z3<0.0){
          below_zero = 1;
          break;
        }
      }
      if(below_zero==1){
        for(i = 0; i<nfaces; i++){
          int v1, v2, v3;
          float z1, z2, z3;

          v1 = faces[3*i+0]-1;
          v2 = faces[3*i+1]-1;
          v3 = faces[3*i+2]-1;

          z1 = verts[3*v1+2];
          z2 = verts[3*v2+2];
          z3 = verts[3*v3+2];
          if(z1<0.0||z2<0.0||z3<0.0){
            surfs[i] = 1;
          }
          else{
            surfs[i] = 2;
          }
        }
        fprintf(streamout, "&SURF ID = 'bsl', RGB = 0,0,255 /\n");
        fprintf(streamout, "&SURF ID = '%s', RGB = 122,117,48/\n", surf_id1);
        fprintf(streamout, "&GEOM ID='terrain', IS_TERRAIN=T, SURF_ID='bsl','%s',\n", surf_id1);
      }
      else{
        for(i = 0; i<nfaces; i++){
          surfs[i] = 1;
        }
        fprintf(streamout, "&SURF ID = 'bsl','%s', /\n", surf_id1);
        fprintf(streamout, "&GEOM ID='terrain', IS_TERRAIN=T, SURF_ID='%s',\n", surf_id1);
      }
    }
    else{
      for(i = 0; i<NFIRETYPES; i++){
        fprintf(streamout, "&SURF ID = '%s', RGB = %i, %i, %i /\n", firetypes[i], firecolors[3*i], firecolors[3*i+1], firecolors[3*i+2]);
      }
   //   GetSurfsFromFaces(wuifireinfo, fds_elevs, verts, nverts, faces, surfs, nfaces);

      fprintf(streamout, "&GEOM ID='terrain', IS_TERRAIN=T, SURF_ID=\n");
      for(i = 0; i<NFIRETYPES; i++){
        fprintf(streamout, " '%s', ", firetypes[i]);
        if(i==9||i==NFIRETYPES-1)fprintf(streamout, "\n");
      }
    }

    if(bingeom==1){
      FILE_SIZE filelen;
      int error, nsurfs;

      nsurfs = nfaces;
      filelen = strlen(casename_bingeom);
      FORTwrite_bingeom(casename_bingeom, verts, faces, surfs, &nverts, &nfaces, &nsurfs, &error, filelen);
      fprintf(streamout, " READ_BINARY=T, BINARY_NAME='%s' ", casename_bingeom);
      fprintf(streamout, "/\n");
    }
    else{
      fprintf(streamout, "  VERTS=\n");
      for(i = 0; i<nverts; i++){
        fprintf(streamout, " %f,%f,%f", verts[3*i+0], verts[3*i+1], verts[3*i+2]);
        fprintf(streamout, ",  ");
        if((i+1)%3==0)fprintf(streamout, "\n");
      }
      fprintf(streamout, "\n");

      fprintf(streamout, "  FACES=\n");
      for(i = 0; i<nfaces; i++){
        fprintf(streamout, " %i,%i,%i,%i", faces[3*i+0], faces[3*i+1], faces[3*i+2], surfs[i]);
        if(i!=nfaces-1)fprintf(streamout, ",  ");
        if((i+1)%6==0)fprintf(streamout, "\n");
      }
      fprintf(streamout, "/\n");
    }
    FREEMEMORY(verts);
    FREEMEMORY(faces);
  }

  if(option==FDS_OBST){
    fprintf(streamout, "&SURF ID = '%s', RGB = 122,117,48 /\n", surf_id1);
    fprintf(streamout, "&SURF ID = '%s', RGB = 122,117,48 /\n", surf_id2);
    count = 0;
    float *valsp1 = elev_fds+inputdata->ncols;
    for(j = 0; j<inputdata->nrows-1; j++){
      float ycen;

      ycen = (ygrid[j]+ygrid[j+1])/2.0;
      for(i = 0; i<inputdata->ncols-1; i++){
        float vavg, xcen;
        int k;
        int exclude;
        float x1, x2, y1, y2;

        xcen = (xgrid[i]+xgrid[i+1])/2.0;

        exclude = 0;
        for(k = 0; k<nexcludeinfo; k++){
          excludedata *exi;

          exi = excludeinfo+k;
          if(exi->xmin<=xcen&&xcen<=exi->xmax&&exi->ymin<=ycen&&ycen<=exi->ymax){
            exclude = 1;
            break;
          }
        }
        if(exclude==1)continue;
        vavg = (elev_fds[count]+elev_fds[count+1]+valsp1[count]+valsp1[count+1])/4.0;
        x1 = MIN(xgrid[i], xgrid[i+1]);
        x2 = MAX(xgrid[i], xgrid[i+1]);
        y1 = MIN(ygrid[j], ygrid[j+1]);
        y2 = MAX(ygrid[j], ygrid[j+1]);
        if(ABS(x1)<buff_dist||ABS(x2-xmax)<buff_dist||ABS(y1)<buff_dist||ABS(y2-ymax)<buff_dist) {
          fprintf(streamout, "&OBST XB=%f,%f,%f,%f,0.0,%f SURF_ID='%s'/\n", x1, x2, y1, y2, vavg, surf_id2);
        }
        else {
          fprintf(streamout, "&OBST XB=%f,%f,%f,%f,0.0,%f SURF_ID='%s'/\n", x1, x2, y1, y2, vavg, surf_id1);
        }
        count++;
      }
      count++;
    }
  }
  fprintf(streamout, "\n&TAIL /\n");

  fprintf(stderr, "\n");
  fprintf(stderr, "FDS input file properties:\n");
  fprintf(stderr, "         file name: %s\n", output_file);
  fprintf(stderr, "             max x: %f\n", xmax);
  fprintf(stderr, "             max y: %f\n", ymax);
  fprintf(stderr, "  elevation bounds: %f %f\n", valmin_fds, valmax_fds);


  FREEMEMORY(fire_fds);
  FREEMEMORY(elev_fds);
  fclose(streamout);
  return 1;
}

#else

/* ------------------ GenerateFDSInputFile ------------------------ */

void GenerateFDSInputFile(char *casename, char *casename_fds, char *casename_bingeom, elevdata *fds_elevs, int option, wuigriddata *wuifireinfo){
  char output_file[LEN_BUFFER], output_elev_file[LEN_BUFFER], *ext;
  char basename[LEN_BUFFER];

  char casename_fds_basename[LEN_BUFFER];
  int nlong, nlat, nz;
  int i, j;
  float xmax, ymax, zmin, zmax;
  float *xgrid, *ygrid;
  int count;
  int ibar, jbar, kbar;
  float *vals, *valsp1;
  FILE *streamout = NULL;
  char *last;

  strcpy(casename_fds_basename, casename_fds);
  last = strrchr(casename_fds_basename, '.');
  if(last != NULL)last[0] = 0;

  strcpy(basename, casename_fds_basename);
  ext = strrchr(basename, '.');
  if(ext != NULL)ext[0] = 0;

  strcpy(output_file, casename_fds);
  streamout = fopen(output_file, "w");
  if(streamout == NULL){
    fprintf(stderr, "***error: unable to open %s for output\n", output_file);
    return;
  }

  strcpy(output_elev_file, basename);
  strcat(output_elev_file, ".elev");

  nlong = fds_elevs->ncols;
  nlat = fds_elevs->nrows;

  zmin = fds_elevs->zmin;
  zmax = fds_elevs->zmax;
  nz = fds_elevs->nz;

  vals = fds_elevs->valbuffer;

  xmax = fds_elevs->xmax;
  ymax = fds_elevs->ymax;

  ibar = nlong - 1;
  jbar = nlat - 1;
  kbar = nz;

  NewMemory((void **)&xgrid, sizeof(float)*(ibar + 1));
  for(i = 0; i < ibar; i++){
    xgrid[i] = xmax*(float)i / (float)ibar;
  }
  xgrid[ibar] = xmax;

  NewMemory((void **)&ygrid, sizeof(float)*(jbar + 1));
  for(i = 0; i < jbar; i++){
    ygrid[i] = ymax*(float)(i)/(float)jbar;
  }
  ygrid[jbar] = ymax;

  if(option==FDS_OBST){
    int nvals = ibar*jbar, len;

    len = strlen(output_elev_file);
    FORTelev2geom(output_elev_file, xgrid, &ibar, ygrid, &jbar, vals, &nvals, len);
  }

  fprintf(streamout, "&HEAD CHID='%s', TITLE='created from %s' /\n", basename,casename);

  NewMemory((void **)&xplt, (nmeshx+1)*sizeof(float));
  xplt[0] = 0.0;
  for(i = 1;i<nmeshx-1;i++){
    xplt[i] = xmax*(float)i/(float)nmeshx;
  }
  xplt[nmeshx] = xmax;

  NewMemory((void **)&yplt, (nmeshy+1)*sizeof(float));
  yplt[0] = 0.0;
  for(i = 1;i<nmeshy-1;i++){
    yplt[i] = ymax*(float)i/(float)nmeshy;
  }
  yplt[nmeshy] = ymax;

  for(j = 0; j<nmeshy; j++){
    for(i = 0; i<nmeshx; i++){
      fprintf(streamout, "&MESH IJK = %i, %i, %i, XB = %f, %f, %f, %f, %f, %f /\n",
      ibar, jbar, kbar, xplt[i],xplt[i+1],yplt[j],yplt[j+1], zmin, zmax);
    }
  }

  if(option == FDS_OBST) {
    fprintf(streamout, "&MISC TERRAIN_CASE = .TRUE., TERRAIN_IMAGE = '%s.png' /\n", basename);
  }
  if(option==FDS_GEOM) {
    fprintf(streamout, "&MISC TERRAIN_CASE = .TRUE., TERRAIN_IMAGE = '%s.png' /\n", basename);
  }
  fprintf(streamout, "&TIME T_END = 0.0 /\n");
  fprintf(streamout, "&VENT MB = 'XMIN', SURF_ID = 'OPEN' /\n");
  fprintf(streamout, "&VENT MB = 'XMAX', SURF_ID = 'OPEN' /\n");
  fprintf(streamout, "&VENT MB = 'YMIN', SURF_ID = 'OPEN' /\n");
  fprintf(streamout, "&VENT MB = 'YMAX', SURF_ID = 'OPEN' /\n");
  fprintf(streamout, "&VENT MB = 'ZMAX', SURF_ID = 'OPEN' /\n");

  fprintf(streamout, "\nTerrain Geometry\n\n");

  if(option == FDS_GEOM){
    float *verts;
    int *faces, *surfs;
    int nverts, nfaces;

    NewMemory((void **)&verts, 3*(ibar+1)*(jbar+1)*sizeof(float));
    NewMemory((void **)&faces, 3*2*ibar*jbar*sizeof(int));
    NewMemory((void **)&surfs, 2*ibar*jbar*sizeof(int));

#define VERTIJ(i,j) ((j)*(ibar+1)+(i))

    nverts = 0;
    for(j = 0; j<jbar+1; j++){
      for(i = 0; i<ibar+1; i++){
        verts[3*nverts+0] = xgrid[i];
        verts[3*nverts+1] = ymax-ygrid[j];
        verts[3*nverts+2] = vals[VERTIJ(i,j)];
        nverts++;
      }
    }

    nfaces = 0;
    for(j = 0; j<jbar; j++){
      for(i = 0; i<ibar; i++){
        faces[3*nfaces+0] = 1+VERTIJ(i, j);
        faces[3*nfaces+1] = 1+VERTIJ(i+1, j+1);
        faces[3*nfaces+2] = 1+VERTIJ(i+1, j);
        nfaces++;

        faces[3*nfaces+0] = 1+VERTIJ(i, j);
        faces[3*nfaces+1] = 1+VERTIJ(i, j+1);
        faces[3*nfaces+2] = 1+VERTIJ(i+1, j+1);
        nfaces++;
      }
    }

    fprintf(streamout, " LONGMIN=%f LONGMAX=%f \n", fds_elevs->long_min, fds_elevs->long_max);
    fprintf(streamout, " LATMIN=%f LATMAX=%f\n", fds_elevs->lat_min, fds_elevs->lat_max);
    fprintf(streamout, " ZMIN=%f ZMAX=%f\n", fds_elevs->val_min, fds_elevs->val_max);
    if(wuifireinfo==NULL){
      for(i = 0; i<nfaces; i++){
        surfs[i] = 1;
      }
      fprintf(streamout, "&SURF ID = '%s', RGB = 122,117,48 /\n", surf_id1);
      fprintf(streamout, "&GEOM ID='terrain', IS_TERRAIN=T, SURF_ID='%s',\n", surf_id1);
    }
    else{
      for(i = 0; i<NFIRETYPES; i++){
        fprintf(streamout, "&SURF ID = '%s', RGB = %i, %i, %i /\n", firetypes[i],firecolors[3*i],firecolors[3*i+1],firecolors[3*i+2]);
      }
      GetSurfsFromFaces(wuifireinfo, fds_elevs, verts, nverts, faces, surfs, nfaces);

      fprintf(streamout, "&GEOM ID='terrain', IS_TERRAIN=T, SURF_ID=\n");
      for(i = 0; i<NFIRETYPES; i++){
        fprintf(streamout, " '%s', ",firetypes[i]);
        if(i==9||i==NFIRETYPES-1)fprintf(streamout,"\n");
      }
    }

    if(bingeom==1){
      FILE_SIZE filelen;
      int error, nsurfs;

      nsurfs = nfaces;
      filelen = strlen(casename_bingeom);
      FORTwrite_bingeom(casename_bingeom, verts, faces, surfs, &nverts, &nfaces, &nsurfs, &error, filelen);
      fprintf(streamout, " READ_BINARY=T, BINARY_NAME='%s' ", casename_bingeom);
      fprintf(streamout, "/\n");
    }
    else{
      fprintf(streamout, "  VERTS=\n");
      for(i = 0; i<nverts; i++){
        fprintf(streamout, " %f,%f,%f", verts[3*i+0], verts[3*i+1], verts[3*i+2]);
        fprintf(streamout, ",  ");
        if((i+1)%3==0)fprintf(streamout, "\n");
      }
      fprintf(streamout,"\n");

      fprintf(streamout, "  FACES=\n");
      for(i = 0; i<nfaces; i++){
        fprintf(streamout, " %i,%i,%i,%i", faces[3*i+0], faces[3*i+1], faces[3*i+2], surfs[i]);
        if(i!=nfaces-1)fprintf(streamout, ",  ");
        if((i+1)%6==0)fprintf(streamout, "\n");
      }
      fprintf(streamout,"/\n");
    }
    FREEMEMORY(verts);
    FREEMEMORY(faces);
  }

  if(option == FDS_OBST){
    fprintf(streamout, "&SURF ID = '%s', RGB = 122,117,48 /\n", surf_id1);
    fprintf(streamout, "&SURF ID = '%s', RGB = 122,117,48 /\n", surf_id2);
    count = 0;
    valsp1 = vals + nlong;
    for(j = 0; j < jbar; j++){
      float ycen;

      ycen = (ygrid[j] + ygrid[j + 1]) / 2.0;
      for(i = 0; i < ibar; i++){
        float vavg, xcen;
        int k;
        int exclude;
        float x1, x2, y1, y2;

        xcen = (xgrid[i] + xgrid[i + 1]) / 2.0;

        exclude = 0;
        for(k = 0; k < nexcludeinfo; k++){
          excludedata *exi;

          exi = excludeinfo + k;
          if(exi->xmin <= xcen&&xcen <= exi->xmax&&exi->ymin <= ycen&&ycen <= exi->ymax){
            exclude = 1;
            break;
          }
        }
        if(exclude == 1)continue;
        vavg = (vals[count] + vals[count + 1] + valsp1[count] + valsp1[count + 1]) / 4.0;
        x1 = MIN(xgrid[i], xgrid[i+1]);
        x2 = MAX(xgrid[i], xgrid[i+1]);
        y1 = MIN(ygrid[j], ygrid[j+1]);
        y2 = MAX(ygrid[j], ygrid[j+1]);
        if (ABS(x1) < buff_dist || ABS(x2 - xmax) < buff_dist || ABS(y1) < buff_dist || ABS(y2 - ymax) < buff_dist) {
          fprintf(streamout, "&OBST XB=%f,%f,%f,%f,0.0,%f SURF_ID='%s'/\n", x1, x2, y1, y2, vavg, surf_id2);
        }
        else {
          fprintf(streamout, "&OBST XB=%f,%f,%f,%f,0.0,%f SURF_ID='%s'/\n", x1, x2, y1, y2, vavg, surf_id1);
        }
        count++;
      }
      count++;
    }
  }
  fprintf(streamout, "\n&TAIL /\n");

  fprintf(stderr, "\n");
  fprintf(stderr, "FDS input file properties:\n");
  fprintf(stderr, "         file name: %s\n", output_file);
  fprintf(stderr, "             max x: %f\n", xmax);
  fprintf(stderr, "             max y: %f\n", ymax);
  fprintf(stderr, "  elevation bounds: %f %f\n", fds_elevs->val_min, fds_elevs->val_max);
  fprintf(stderr, "  longitude=%f at x=%f\n", fds_elevs->longref, fds_elevs->xref);
  fprintf(stderr, "   latitude=%f at y=%f\n", fds_elevs->latref, fds_elevs->yref);
  if(nexcludeinfo>0) {
    int k;

    if(nexcludeinfo==1){
      fprintf(stderr, "  exclude region:\n");
    }
    else{
      fprintf(stderr, "  exclude regions:\n");
    }
    for(k = 0; k < nexcludeinfo; k++){
      excludedata *exi;

      exi = excludeinfo + k;
      fprintf(stderr, "    %i: %f %f %f %f\n", k+1,exi->xmin, exi->xmax, exi->ymin, exi->ymax);
    }
  }
}
#endif
