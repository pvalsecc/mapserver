/* $Id$ */

#include "map.h"

#define PSF .8
#define VMARGIN 5 /* margin at top and bottom of legend graphic */
#define HMARGIN 5 /* margin at left and right of legend graphic */


int msDrawLegendIcon(mapObj *map, layerObj *lp, classObj *class, int width, int height, gdImagePtr img, int dstX, int dstY)
{
  int i;
  shapeObj box, zigzag;
  pointObj marker;
  char szPath[MS_MAXPATHLEN];
  imageObj *image = NULL;

  // if the class has a keyimage then load it, scale it and we're done
  if(class->keyimage != NULL) {
    image = msImageLoadGD(msBuildPath(szPath, map->mappath, class->keyimage));
    if(!image) return(MS_FAILURE);

    // TO DO: we may want to handle this differently depending on the relative size of the keyimage
    gdImageCopyResampled(img, image->img.gd, 0, 0, 0, 0,img->sx, img->sy, image->img.gd->sx, image->img.gd->sy);
      
    return(MS_SUCCESS);
  }

  // initialize the shapes used to render the legend
  box.line = (lineObj *)malloc(sizeof(lineObj));
  box.numlines = 1;
  box.line[0].point = (pointObj *)malloc(sizeof(pointObj)*5);
  box.line[0].numpoints = 5;

  zigzag.line = (lineObj *)malloc(sizeof(lineObj));
  zigzag.numlines = 1;
  zigzag.line[0].point = (pointObj *)malloc(sizeof(pointObj)*4);
  zigzag.line[0].numpoints = 4;

  // compute shapes used to render individual legend pieces
  marker.x = dstX + MS_NINT(width / 2.0);
  marker.y = dstY + MS_NINT(height / 2.0);

  zigzag.line[0].point[0].x = dstX;
  zigzag.line[0].point[0].y = dstY + height - 1;
  zigzag.line[0].point[1].x = dstX + MS_NINT(width / 3.0) - 1;
  zigzag.line[0].point[1].y = dstY;
  zigzag.line[0].point[2].x = dstX + MS_NINT(2.0 * width / 3.0) - 1;
  zigzag.line[0].point[2].y = dstY + height - 1;
  zigzag.line[0].point[3].x = dstX + width - 1;
  zigzag.line[0].point[3].y = dstY;
  zigzag.line[0].numpoints = 4;

  box.line[0].point[0].x = dstX;
  box.line[0].point[0].y = dstY;
  box.line[0].point[1].x = dstX + width - 1;
  box.line[0].point[1].y = dstY;
  box.line[0].point[2].x = dstX + width - 1;
  box.line[0].point[2].y = dstY + height - 1;
  box.line[0].point[3].x = dstX;
  box.line[0].point[3].y = dstY + height - 1;
  box.line[0].point[4].x = box.line[0].point[0].x;
  box.line[0].point[4].y = box.line[0].point[0].y;
  box.line[0].numpoints = 5;

  /* 
  ** now draw the appropriate color/symbol/size combination 
  */      
  switch(lp->type) {
  case MS_LAYER_ANNOTATION:
  case MS_LAYER_POINT:
    for(i=0; i<class->numstyles; i++)
      msDrawMarkerSymbolGD(&map->symbolset, img, &marker, &(class->styles[i]), 1.0);          
    break;
  case MS_LAYER_LINE:
    for(i=0; i<class->numstyles; i++)
      msDrawLineSymbolGD(&map->symbolset, img, &zigzag, &(class->styles[i]), 1.0); 	
    break;
  case MS_LAYER_CIRCLE:
  case MS_LAYER_RASTER:
  case MS_LAYER_POLYGON:
    for(i=0; i<class->numstyles; i++) { // TO DO: it may not be this easy
      if(MS_VALID_COLOR(class->styles[0].color))
        msDrawShadeSymbolGD(&map->symbolset, img, &box, &(class->styles[i]), 1.0);
      else
	msDrawLineSymbolGD(&map->symbolset, img, &box, &(class->styles[i]), 1.0);
    }   
    break;
  default:
    return MS_FAILURE;
    break;
  } /* end symbol drawing */
   
  free(box.line[0].point);
  free(box.line);
  free(zigzag.line[0].point);
  free(zigzag.line);

  return MS_SUCCESS;
}

imageObj *msCreateLegendIcon(mapObj* map, layerObj* lp, classObj* class, int width, int height)
{
  imageObj *image;
  outputFormatObj *format = NULL;

  if(!map->outputformat || !MS_RENDERER_GD(map->outputformat) ) 
  {
      msSetError(MS_GDERR, 
                 "Map outputformat must be set to a GD format!", 
                 "msCreateLegendIcon()");
      return(NULL);
  }

  /* Ensure we have an image format representing the options for the legend.*/
  msApplyOutputFormat( &format, map->outputformat, 
                       map->legend.transparent, 
                       map->legend.interlace, 
                       MS_NOOVERRIDE );

  /* create image */
  image = msImageCreateGD(width, height, map->outputformat,
                          map->web.imagepath, map->web.imageurl);

  /* drop this reference to output format */
  msApplyOutputFormat( &format, NULL, 
                       MS_NOOVERRIDE, MS_NOOVERRIDE, MS_NOOVERRIDE );

  /* did we succeed in creating the image? */
  if(image == NULL) {
    msSetError(MS_GDERR, "Unable to initialize image.","msCreateLegendIcon()");
    return(NULL);
  }

  // allocate the background color
  msImageInitGD( image, &(map->legend.imagecolor));

  // Call drawLegendIcon with destination (0, 0)
  // Return an empty image if lp==NULL || class=NULL
  if (lp && class) {
    msClearLayerPenValues(lp); // just in case the mapfile has already been processed
    msDrawLegendIcon(map, lp, class, width, height, image->img.gd, 0, 0);
  }

  return image;
}


/*
** Creates a GD image of a legend for a specific map. msDrawLegend()
** respects the current scale, and classes without a name are not
** added to the legend.
*/
imageObj *msDrawLegend(mapObj *map)
{
  int status;

  gdImagePtr img; /* image data structure */
  int i,j; /* loop counters */
  pointObj pnt;
  int size_x, size_y;
  layerObj *lp;  
  int maxwidth=0, maxheight=0, n=0;
  int *heights;
  rectObj rect;
  imageObj      *image = NULL;

  map->cellsize = msAdjustExtent(&(map->extent), map->width, map->height);
  status = msCalculateScale(map->extent, map->units, map->width, map->height, map->resolution, &map->scale);
  if(status != MS_SUCCESS) return(NULL);

  /*
  ** allocate heights array
  */
  for(i=0; i<map->numlayers; i++) {
    if((map->layers[i].status == MS_OFF) || (map->layers[i].type == MS_LAYER_QUERY)) /* skip it */
      continue;

    for(j=0;j<map->layers[i].numclasses;j++) {
      if(!map->layers[i].class[j].name)
	continue; /* skip it */
      n++;
    }
  }

  if((heights = (int *)malloc(sizeof(int)*n)) == NULL) {
    msSetError(MS_MEMERR, "Error allocating heights array.", "msDrawLegend()");
    return(NULL);
  }

  /*
  ** Calculate the optimal image size for the legend
  */
  n=0;
  for(i=0; i<map->numlayers; i++) { /* Need to find the longest legend label string */
    if((map->layers[i].status == MS_OFF) || (map->layers[i].type == MS_LAYER_QUERY)) /* skip it */
      continue;

   if(map->scale > 0) {
      if((map->layers[i].maxscale > 0) && (map->scale > map->layers[i].maxscale))
	continue;
      if((map->layers[i].minscale > 0) && (map->scale <= map->layers[i].minscale))
	continue;
    }
 
    for(j=0;j<map->layers[i].numclasses;j++) {
      if(!map->layers[i].class[j].name)
	continue; /* skip it */
      if(msGetLabelSize(map->layers[i].class[j].name, &map->legend.label, &rect, &(map->fontset), 1.0) != 0)
	return(NULL); /* something bad happened */
      maxheight = MS_MAX(maxheight, MS_NINT(rect.maxy - rect.miny));
      maxwidth = MS_MAX(maxwidth, MS_NINT(rect.maxx - rect.minx));
      heights[n] = MS_NINT(rect.maxy - rect.miny);

      n++;
    }
  }
  
  size_x = (2*HMARGIN)+(maxwidth)+(map->legend.keyspacingx)+(map->legend.keysizex);
  size_y = (2*VMARGIN) + ((n-1)*map->legend.keyspacingy);
  for(i=0; i<n; i++) {
    heights[i] = MS_MAX(heights[i], maxheight);
    size_y += MS_MAX(heights[i], map->legend.keysizey);
  }

  /*
  ** Initialize the legend image
  */
  image = msImageCreateGD(size_x, size_y, map->outputformat, map->web.imagepath, map->web.imageurl);

  if (image)
    img = image->img.gd;
  else {
    msSetError(MS_GDERR, "Unable to initialize image.", "msDrawLegend()");
    return(NULL);
  }
  
  /* Set background */
  if(image != NULL)
    msImageInitGD(image, &(map->legend.imagecolor));

  msClearPenValues(map); // just in case the mapfile has already been processed

  pnt.y = VMARGIN;
    
  /* for(i=0; i<map->numlayers; i++) { */
  for(i=map->numlayers-1; i>=0; i--) {

    lp = &(map->layers[i]); /* assign for brevity */

    if((lp->numclasses == 0) || (lp->status == MS_OFF) || (lp->type == MS_LAYER_QUERY))
      continue; /* skip this layer */

    if(map->scale > 0) {
      if((lp->maxscale > 0) && (map->scale > lp->maxscale))
	continue;
      if((lp->minscale > 0) && (map->scale <= lp->minscale))
	continue;
    }

    for(j=0; j<lp->numclasses; j++) { /* always at least 1 class */

      if(!lp->class[j].name) continue; /* skip it */
      
      pnt.x = HMARGIN + map->legend.keysizex + map->legend.keyspacingx;
      
      //TODO
      if (msDrawLegendIcon(map, lp, &(lp->class[j]),  map->legend.keysizex,  map->legend.keysizey, image->img.gd, HMARGIN, pnt.y) != MS_SUCCESS)
         return NULL;

      pnt.y += MS_MAX(map->legend.keysizey, maxheight);
      //TODO
      msDrawLabel(image, pnt, lp->class[j].name, &(map->legend.label), &map->fontset, 1.0);

      pnt.y += map->legend.keyspacingy; /* bump y for next label */
	
    } // next label
  } // next layer

  free(heights);
  return(image);
}


//TODO
int msEmbedLegend(mapObj *map, gdImagePtr img)
{
  int s,l;
  pointObj point;
  imageObj *image = NULL;

  s = msGetSymbolIndex(&(map->symbolset), "legend");
  if(s == -1) {
    s = map->symbolset.numsymbols;
    map->symbolset.numsymbols++;
    initSymbol(&(map->symbolset.symbol[s]));
  } else {
    if(map->symbolset.symbol[s].img) 
      gdImageDestroy(map->symbolset.symbol[s].img);
  }
  
  image = msDrawLegend(map);
  map->symbolset.symbol[s].img = image->img.gd; //TODO 
  if(!map->symbolset.symbol[s].img) return(-1); // something went wrong creating scalebar

  map->symbolset.symbol[s].type = MS_SYMBOL_PIXMAP; // intialize a few things
  map->symbolset.symbol[s].name = strdup("legend");  

  // I'm not too sure this test is sufficient ... NFW.
  if(map->legend.transparent == MS_ON)
    gdImageColorTransparent(map->symbolset.symbol[s].img, 0);

  switch(map->legend.position) {
  case(MS_LL):
    point.x = MS_NINT(map->symbolset.symbol[s].img->sx/2.0);
    point.y = map->height - MS_NINT(map->symbolset.symbol[s].img->sy/2.0);
    break;
  case(MS_LR):
    point.x = map->width - MS_NINT(map->symbolset.symbol[s].img->sx/2.0);
    point.y = map->height - MS_NINT(map->symbolset.symbol[s].img->sy/2.0);
    break;
  case(MS_LC):
    point.x = MS_NINT(map->width/2.0);
    point.y = map->height - MS_NINT(map->symbolset.symbol[s].img->sy/2.0);
    break;
  case(MS_UR):
    point.x = map->width - MS_NINT(map->symbolset.symbol[s].img->sx/2.0);
    point.y = MS_NINT(map->symbolset.symbol[s].img->sy/2.0);
    break;
  case(MS_UL):
    point.x = MS_NINT(map->symbolset.symbol[s].img->sx/2.0);
    point.y = MS_NINT(map->symbolset.symbol[s].img->sy/2.0);
    break;
  case(MS_UC):
    point.x = MS_NINT(map->width/2.0);
    point.y = MS_NINT(map->symbolset.symbol[s].img->sy/2.0);
    break;
  }

  l = msGetLayerIndex(map, "legend");
  if(l == -1) {
    l = map->numlayers;
    map->numlayers++;

    if(initLayer(&(map->layers[l]), map) == -1) return(-1);
    map->layers[l].name = strdup("legend");
    map->layers[l].type = MS_LAYER_ANNOTATION;
    map->layers[l].status = MS_ON;

    if(initClass(&(map->layers[l].class[0])) == -1) return(-1);    
      //Update the layer order list with the layer's index.
    map->layerorder[l] = l;
  }

  map->layers[l].class[0].numstyles = 1;
  map->layers[l].class[0].styles[0].symbol = s;
  map->layers[l].class[0].styles[0].color.pen = -1;
  map->layers[l].class[0].label.force = MS_TRUE;
  map->layers[l].class[0].label.size = MS_MEDIUM; // must set a size to have a valid label definition

  if(map->legend.postlabelcache) // add it directly to the image
    msDrawMarkerSymbolGD(&map->symbolset, img, &point, &(map->layers[l].class[0].styles[0]), 1.0);
  else
    msAddLabel(map, l, 0, -1, -1, &point, " ", 1.0);

  return(0);
}



